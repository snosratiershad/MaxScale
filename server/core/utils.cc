/*
 * Copyright (c) 2016 MariaDB Corporation Ab
 * Copyright (c) 2023 MariaDB plc, Finnish Branch
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2028-01-30
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

/**
 * @file utils.c - General utility functions
 *
 * @verbatim
 * Revision History
 *
 * Date         Who                     Description
 * 10-06-2013   Massimiliano Pinto      Initial implementation
 * 12-06-2013   Massimiliano Pinto      Read function trought
 *                                      the gwbuff strategy
 * 13-06-2013   Massimiliano Pinto      MaxScale local authentication
 *                                      basics
 * 02-09-2014   Martin Brampton         Replaced C++ comments by C comments
 *
 * @endverbatim
 */

#include <maxscale/utils.hh>

#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <netinet/tcp.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <thread>
#include <curl/curl.h>
#include <crypt.h>
#include <sched.h>
#include <unistd.h>
#include <fstream>

#include <maxscale/config.hh>
#include <maxscale/secrets.hh>
#include <maxscale/routingworker.hh>

#if !defined (PATH_MAX)
# if defined (__USE_POSIX)
#   define PATH_MAX _POSIX_PATH_MAX
# else
#   define PATH_MAX 256
# endif
#endif

#define MAX_ERROR_MSG PATH_MAX

namespace
{
using HexLookupTable = std::array<uint8_t, 256>;
HexLookupTable init_hex_lookup_table() noexcept;

// Hex char -> byte val lookup table.
const HexLookupTable hex_lookup_table = init_hex_lookup_table();

/* used in the bin2hex function */
const char hex_upper[] = "0123456789ABCDEF";

HexLookupTable init_hex_lookup_table() noexcept
{
    auto char_val = [](char c) -> uint8_t {
        if (c >= '0' && c <= '9')
        {
            return c - '0';
        }
        else if (c >= 'A' && c <= 'F')
        {
            return c - 'A' + 10;
        }
        else if (c >= 'a' && c <= 'f')
        {
            return c - 'a' + 10;
        }
        else
        {
            return '\177';
        }
    };

    HexLookupTable rval;
    for (size_t i = 0; i < rval.size(); i++)
    {
        rval[i] = char_val(i);
    }
    return rval;
}
}
/**
 * Check if the provided pathname is POSIX-compliant. The valid characters
 * are [a-z A-Z 0-9._-].
 * @param path A null-terminated string
 * @return true if it is a POSIX-compliant pathname, otherwise false
 */
bool is_valid_posix_path(char* path)
{
    char* ptr = path;
    while (*ptr != '\0')
    {
        if (isalnum(*ptr) || *ptr == '/' || *ptr == '.' || *ptr == '-' || *ptr == '_')
        {
            ptr++;
        }
        else
        {
            return false;
        }
    }
    return true;
}

char* gw_strend(const char* s)
{
    while (*s++)
    {
    }
    return (char*) (s - 1);
}

/**********************************************************
* fill a 20 bytes preallocated with SHA1 digest (160 bits)
* for one input on in_len bytes
**********************************************************/
void gw_sha1_str(const uint8_t* in, int in_len, uint8_t* out)
{
    unsigned char hash[SHA_DIGEST_LENGTH];

    SHA1(in, in_len, hash);
    memcpy(out, hash, SHA_DIGEST_LENGTH);
}

/********************************************************
* fill 20 bytes preallocated with SHA1 digest (160 bits)
* for two inputs, in_len and in2_len bytes
********************************************************/
void gw_sha1_2_str(const uint8_t* in, int in_len, const uint8_t* in2, int in2_len, uint8_t* out)
{
    SHA_CTX context;
    SHA1_Init(&context);
    SHA1_Update(&context, in, in_len);
    SHA1_Update(&context, in2, in2_len);
    SHA1_Final(out, &context);
}


/**
 * node Gets errno corresponding to latest socket error
 *
 * Parameters:
 * @param fd - in, use
 *          socket to examine
 *
 * @return errno
 *
 *
 */
int gw_getsockerrno(int fd)
{
    int eno = 0;
    socklen_t elen = sizeof(eno);

    if (fd <= 0)
    {
        goto return_eno;
    }

    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (void*)&eno, &elen) != 0)
    {
        eno = 0;
    }

return_eno:
    return eno;
}

namespace maxscale
{
std::string create_hex_sha1_sha1_passwd(const char* passwd)
{
    uint8_t hash1[SHA_DIGEST_LENGTH] = "";
    uint8_t hash2[SHA_DIGEST_LENGTH] = "";

    int hexsize = SHA_DIGEST_LENGTH * 2 + 1;
    char hexpasswd[hexsize];

    /* hash1 is SHA1(real_password) */
    gw_sha1_str((uint8_t*)passwd, strlen(passwd), hash1);
    /* hash2 is the SHA1(input data), where input_data = SHA1(real_password) */
    gw_sha1_str(hash1, SHA_DIGEST_LENGTH, hash2);
    /* dbpass is the HEX form of SHA1(SHA1(real_password)) */
    mxs::bin2hex(hash2, SHA_DIGEST_LENGTH, hexpasswd);

    return hexpasswd;
}

bool hex2bin(const char* in, unsigned int in_len, uint8_t* out)
{
    // Input length must be multiple of two.
    if (!in || in_len == 0 || in_len % 2 != 0)
    {
        return false;
    }

    const char* in_end = in + in_len;
    while (in < in_end)
    {
        // One byte is formed from two hex chars, with the first char forming the high bits.
        uint8_t high_half = hex_lookup_table[*in++];
        uint8_t low_half = hex_lookup_table[*in++];
        uint8_t total = (high_half << 4) | low_half;
        *out++ = total;
    }
    return true;
}

char* bin2hex(const uint8_t* in, unsigned int len, char* out)
{
    const uint8_t* in_end = in + len;
    if (len == 0 || in == NULL)
    {
        return NULL;
    }

    for (; in != in_end; ++in)
    {
        *out++ = hex_upper[((uint8_t) *in) >> 4];
        *out++ = hex_upper[((uint8_t) *in) & 0x0F];
    }
    *out = '\0';

    return out;
}

void bin_bin_xor(const uint8_t* input1, const uint8_t* input2, unsigned int input_len, uint8_t* output)
{
    const uint8_t* input1_end = input1 + input_len;
    while (input1 < input1_end)
    {
        *output++ = *input1++ ^ *input2++;
    }
}
}

std::string clean_up_pathname(std::string path)
{
    size_t pos;

    while ((pos = path.find("//")) != std::string::npos)
    {
        path.erase(pos, 1);
    }

    while (path.back() == '/')
    {
        path.pop_back();
    }

    return path.substr(0, PATH_MAX);
}

/**
 * @brief Internal helper function for mkdir_all()
 *
 * @param path Path to create
 * @param mask Bitmask to use
 * @return True if directory exists or it was successfully created, false on error
 */
static bool mkdir_all_internal(char* path, mode_t mask, bool log_errors)
{
    bool rval = false;

    if (mkdir(path, mask) == -1 && errno != EEXIST)
    {
        if (errno == ENOENT)
        {
            /** Try to create the parent directory */
            char* ndir = strrchr(path, '/');
            if (ndir)
            {
                *ndir = '\0';
                if (mkdir_all_internal(path, mask, log_errors))
                {
                    /** Creation of the parent directory was successful, try to
                     * create the directory again */
                    *ndir = '/';
                    if (mkdir(path, mask) == 0)
                    {
                        rval = true;
                    }
                    else if (log_errors)
                    {
                        MXB_ERROR("Failed to create directory '%s': %d, %s",
                                  path, errno, mxb_strerror(errno));
                    }
                }
            }
        }
        else if (log_errors)
        {
            MXB_ERROR("Failed to create directory '%s': %d, %s",
                      path, errno, mxb_strerror(errno));
        }
    }
    else
    {
        rval = true;
    }

    return rval;
}

bool mxs_mkdir_all(const char* path, int mask, bool log_errors)
{
    char local_path[strlen(path) + 1];
    strcpy(local_path, path);

    if (local_path[sizeof(local_path) - 2] == '/')
    {
        local_path[sizeof(local_path) - 2] = '\0';
    }

    return mkdir_all_internal(local_path, (mode_t)mask, log_errors);
}

bool configure_network_socket(int so, int type)
{
    int one = 1;

    if (type != AF_UNIX)
    {
        if (setsockopt(so, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) != 0
            || setsockopt(so, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one)) != 0)
        {
            MXB_ERROR("Failed to set socket option: %d, %s.", errno, mxb_strerror(errno));
            mxb_assert(!true);
            return false;
        }
    }
    return true;
}

static bool configure_listener_socket(int so)
{
    int one = 1;

    if (setsockopt(so, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) != 0
        || setsockopt(so, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) != 0)
    {
        MXB_ERROR("Failed to set socket option: %d, %s.", errno, mxb_strerror(errno));
        return false;
    }

#ifdef SO_REUSEPORT
    if (mxs::have_so_reuseport())
    {
        if (setsockopt(so, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one)) != 0)
        {
            MXB_ERROR("Failed to set socket option: %d, %s.", errno, mxb_strerror(errno));
            return false;
        }
    }
#endif
    return true;
}

static void set_port(struct sockaddr_storage* addr, uint16_t port)
{
    if (addr->ss_family == AF_INET)
    {
        struct sockaddr_in* ip = (struct sockaddr_in*)addr;
        ip->sin_port = htons(port);
    }
    else if (addr->ss_family == AF_INET6)
    {
        struct sockaddr_in6* ip = (struct sockaddr_in6*)addr;
        ip->sin6_port = htons(port);
    }
    else
    {
        MXB_ERROR("Unknown address family: %d", (int)addr->ss_family);
        mxb_assert(false);
    }
}

int open_network_socket(mxs_socket_type type, sockaddr_storage* addr, const char* host, uint16_t port)
{
    mxb_assert(type == MXS_SOCKET_NETWORK || type == MXS_SOCKET_LISTENER);
    struct addrinfo* ai = NULL, hint = {};
    int so = 0, rc = 0;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_family = AF_UNSPEC;
    hint.ai_flags = AI_ALL;

    if ((rc = getaddrinfo(host, NULL, &hint, &ai)) != 0)
    {
        MXB_ERROR("Failed to obtain address for host %s: %s", host, gai_strerror(rc));
        return -1;
    }

    /* Take the first one */
    if (ai)
    {
        if ((so = socket(ai->ai_family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0)) == -1)
        {
            MXB_ERROR("Socket creation failed: %d, %s.", errno, mxb_strerror(errno));
        }
        else
        {
            memcpy(addr, ai->ai_addr, ai->ai_addrlen);
            set_port(addr, port);

            if ((type == MXS_SOCKET_NETWORK && !configure_network_socket(so, addr->ss_family))
                || (type == MXS_SOCKET_LISTENER && !configure_listener_socket(so)))
            {
                close(so);
                so = -1;
            }
            else if (type == MXS_SOCKET_LISTENER && bind(so, (struct sockaddr*)addr, sizeof(*addr)) < 0)
            {
                // Try again with IP_FREEBIND in case the network is not up yet.
                int one = 1;
                if (setsockopt(so, SOL_IP, IP_FREEBIND, &one, sizeof(one)) != 0)
                {
                    MXB_ERROR("Failed to set socket option: %d, %s.", errno, mxb_strerror(errno));
                    close(so);
                    so = -1;
                }
                else if (bind(so, (struct sockaddr*)addr, sizeof(*addr)) < 0)
                {
                    MXB_ERROR("Failed to bind on '%s:%u': %d, %s", host, port, errno, mxb_strerror(errno));
                    close(so);
                    so = -1;
                }
                else
                {
                    MXB_WARNING("The interface for '[%s]:%u' might be down or it does not exist. "
                                "Will listen for connections on it regardless of this.", host, port);
                }
            }
            else if (type == MXS_SOCKET_NETWORK)
            {
                const auto& config = mxs::Config::get();

                auto la = config.local_address;

                if (!la.empty())
                {
                    freeaddrinfo(ai);
                    ai = NULL;

                    if ((rc = getaddrinfo(la.c_str(), NULL, &hint, &ai)) == 0)
                    {
                        struct sockaddr_storage local_address = {};

                        memcpy(&local_address, ai->ai_addr, ai->ai_addrlen);

                        // Use SO_REUSEADDR for outbound connections: this prevents conflicts from happening
                        // at the bind() stage but can theoretically cause them to appear in the connect()
                        // stage.
                        int one = 1;
                        setsockopt(so, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

                        if (bind(so, (struct sockaddr*)&local_address, sizeof(local_address)) == 0)
                        {
                            MXB_INFO("Bound connecting socket to \"%s\".", la.c_str());
                        }
                        else
                        {
                            MXB_ERROR("Could not bind connecting socket to local address \"%s\", "
                                      "connecting to server using default local address: %s",
                                      la.c_str(),
                                      mxb_strerror(errno));
                        }
                    }
                    else
                    {
                        MXB_ERROR("Could not get address information for local address \"%s\", "
                                  "connecting to server using default local address: %s",
                                  la.c_str(),
                                  mxb_strerror(errno));
                    }
                }
            }
        }

        freeaddrinfo(ai);
    }

    return so;
}

static bool configure_unix_socket(int so)
{
    int one = 1;

    if (setsockopt(so, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) != 0)
    {
        MXB_ERROR("Failed to set socket option: %d, %s.", errno, mxb_strerror(errno));
        return false;
    }
    return true;
}

int open_unix_socket(mxs_socket_type type, sockaddr_un* addr, const char* path)
{
    int fd = -1;

    if (strlen(path) > sizeof(addr->sun_path) - 1)
    {
        MXB_ERROR("The path %s specified for the UNIX domain socket is too long. "
                  "The maximum length is %lu.",
                  path,
                  sizeof(addr->sun_path) - 1);
    }
    else if ((fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0)) < 0)
    {
        MXB_ERROR("Can't create UNIX socket: %d, %s", errno, mxb_strerror(errno));
    }
    else if (configure_unix_socket(fd))
    {
        addr->sun_family = AF_UNIX;
        strcpy(addr->sun_path, path);

        /* Bind the socket to the Unix domain socket */
        if (type == MXS_SOCKET_LISTENER && bind(fd, (struct sockaddr*)addr, sizeof(*addr)) < 0)
        {
            MXB_ERROR("Failed to bind to UNIX Domain socket '%s': %d, %s",
                      path,
                      errno,
                      mxb_strerror(errno));
            close(fd);
            fd = -1;
        }
    }

    return fd;
}

int connect_socket(const char* host, int port, sockaddr_storage* addr)
{
    // Start the watchdog notifier workaround, the getaddrinfo call done by connect_socket() can take a long
    // time in some corner cases.
    mxb::WatchdogNotifier::Workaround workaround(mxs::RoutingWorker::get_current());
    int so;
    size_t sz;

    if (host[0] == '/')
    {
        so = open_unix_socket(MXS_SOCKET_NETWORK, (struct sockaddr_un*)addr, host);
        sz = sizeof(sockaddr_un);
    }
    else
    {
        so = open_network_socket(MXS_SOCKET_NETWORK, addr, host, port);
        sz = sizeof(sockaddr_storage);
    }

    if (so != -1)
    {
        if (::connect(so, (struct sockaddr*)addr, sz) == -1 && errno != EINPROGRESS)
        {
            MXB_ERROR("Failed to connect backend server [%s]:%d due to: %d, %s.",
                      host, port, errno, mxb_strerror(errno));
            ::close(so);
            so = -1;
        }
    }
    else
    {
        MXB_ERROR("Establishing connection to backend server [%s]:%d failed.", host, port);
    }
    return so;
}

std::string get_current_cgroup()
{
    std::string rv;

    if (std::ifstream cgroup("/proc/self/cgroup"); cgroup)
    {
        for (std::string line; rv.empty() && std::getline(cgroup, line);)
        {
            if (line.substr(0, 3) == "0::")
            {
                // Unified hierarchy cgroups (v2). The format is `0::/path/to/cgroup/`, usually `0::/` for
                // docker or when no cgroups are set.
                rv = line.substr(3);
            }
            else
            {
                // Legacy cgroups (v1). The file will contain multiple lines and the format of each line is:
                //
                //   hierarchy-ID:controller-list:cgroup-path
                //
                // We must find the hierarchy with the `cpu` controller in it and use the cgroup-path for
                // that. For docker this is unnecessary as it's always the root cgroup but for SystemD the
                // path is different depending on the slice the process is in.
                if (auto pos = line.find(':'); pos != std::string::npos)
                {
                    ++pos;

                    if (auto end_pos = line.find(':', pos); end_pos != std::string::npos)
                    {
                        auto controller = line.substr(pos, end_pos - pos);

                        for (const auto& tok : mxb::strtok(controller, ","))
                        {
                            if (tok == "cpu")
                            {
                                rv = line.substr(end_pos + 1);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    return rv;
}

const std::string& get_cgroup()
{
    static std::string cgroup = get_current_cgroup();
    return cgroup;
}

long get_cpu_count()
{
    unsigned int cpus = get_processor_count();

    if (cpus != 1)
    {
        cpu_set_t cpuset;
        if (sched_getaffinity(getpid(), sizeof(cpuset), &cpuset) == 0)
        {
            cpus = std::min((unsigned int)CPU_COUNT(&cpuset), cpus);
        }
    }

    return cpus;
}

bool get_cpu_quota_and_period(int* quotap, int* periodp)
{
    int quota = 0;
    int period = 0;
    const auto& cg = get_cgroup();

    if (std::ifstream cpu_v2("/sys/fs/cgroup/" + cg + "/cpu.max"); cpu_v2)
    {
        if (std::string line; std::getline(cpu_v2, line))
        {
            auto tok = mxb::strtok(line, " ");

            if (tok.size() == 2 && tok[0] != "-1" && tok [0] != "max")
            {
                quota = atoi(tok[0].c_str());
                period = atoi(tok[1].c_str());
            }
        }
    }
    else if (std::ifstream cpu_v1_quota("/sys/fs/cgroup/cpu/" + cg + "/cpu.cfs_quota_us"); cpu_v1_quota)
    {
        if (std::ifstream cpu_v1_period("/sys/fs/cgroup/cpu/" + cg + "/cpu.cfs_period_us"); cpu_v1_period)
        {
            int tmp_quota = 0;
            int tmp_period = 0;

            if ((cpu_v1_quota >> tmp_quota) && (cpu_v1_period >> tmp_period) && tmp_quota > 0)
            {
                quota = tmp_quota;
                period = tmp_period;
            }
        }
    }
    // Workaround for https://github.com/moby/moby/issues/34584
    else if (std::ifstream old_cpu_v1_quota("/sys/fs/cgroup/cpu/cpu.cfs_quota_us"); old_cpu_v1_quota)
    {
        if (std::ifstream cpu_v1_period("/sys/fs/cgroup/cpu/cpu.cfs_period_us"); cpu_v1_period)
        {
            int tmp_quota = 0;
            int tmp_period = 0;

            if ((cpu_v1_quota >> tmp_quota) && (cpu_v1_period >> tmp_period) && tmp_quota > 0)
            {
                quota = tmp_quota;
                period = tmp_period;
            }
        }
    }

    if (quota && period)
    {
        *quotap = quota;
        *periodp = period;
    }

    return quota && period;
}

double get_vcpu_count()
{
    double cpus = get_cpu_count();

    int quota = 0;
    int period = 0;

    if (get_cpu_quota_and_period(&quota, &period))
    {
        double vcpu = (double)quota / period;
        cpus = std::min(vcpu, cpus);
    }

    return cpus;
}

long get_processor_count()
{
    mxb_assert(sysconf(_SC_NPROCESSORS_ONLN) == std::thread::hardware_concurrency());
    return std::max(std::thread::hardware_concurrency(), 1U);
}

int64_t get_available_memory()
{
    int64_t memory = get_total_memory();

    if (memory)
    {
        const auto& cg = get_cgroup();

        for (auto path : {"/sys/fs/cgroup/" + cg + "/memory.max",
                          "/sys/fs/cgroup/memory/" + cg + "/memory.limit_in_bytes",
                            // Workaround for https://github.com/moby/moby/issues/34584
                          std::string {"/sys/fs/cgroup/memory/memory.limit_in_bytes"}})
        {
            if (std::ifstream mem(path); mem)
            {
                if (int64_t mem_tmp = 0; (mem >> mem_tmp))
                {
                    memory = std::min(mem_tmp, memory);
                    break;
                }
            }
        }
    }
    else
    {
        MXB_ERROR("Unable to establish available memory.");
    }

    return std::max(memory, 0L);
}

int64_t get_total_memory()
{
    int64_t pagesize = 0;
    int64_t num_pages = 0;
#if defined _SC_PAGESIZE && defined _SC_PHYS_PAGES
    if ((pagesize = sysconf(_SC_PAGESIZE)) <= 0 || (num_pages = sysconf(_SC_PHYS_PAGES)) <= 0)
    {
        MXB_ERROR("Unable to establish total system memory: %s", mxb_strerror(errno));
        pagesize = 0;
        num_pages = 0;
    }
#else
#error _SC_PAGESIZE and _SC_PHYS_PAGES are not defined
#endif
    mxb_assert(pagesize * num_pages > 0);
    return pagesize * num_pages;
}

namespace maxscale
{

std::string crypt(const std::string& password, const std::string& salt)
{
#if HAVE_GLIBC
    struct crypt_data cdata;
    cdata.initialized = 0;
    return crypt_r(password.c_str(), salt.c_str(), &cdata);
#else
    static std::mutex mxs_crypt_lock;
    std::lock_guard<std::mutex> guard(mxs_crypt_lock);
    std::string pw = crypt(password.c_str(), salt.c_str());
    return pw;
#endif
}

std::vector<uint8_t> from_hex(const std::string& str)
{
    std::vector<uint8_t> data;
    if (str.size() % 2 == 0)
    {
        data.resize(str.size() / 2);
        hex2bin(str.c_str(), str.size(), data.data());
    }

    return data;
}

int get_kernel_version()
{
    int rval = 0;
    utsname name;

    if (uname(&name) == 0)
    {
        std::istringstream rel {name.release};
        int major = 0;
        int minor = 0;
        int patch = 0;
        char dot;
        rel >> major;
        rel >> dot;
        rel >> minor;
        rel >> dot;
        rel >> patch;

        rval = major * 10000 + minor * 100 + patch;
    }

    return rval;
}

namespace
{
// SO_REUSEPORT was added in Linux 3.9. Even if SO_REUSEPORT is defined it doesn't mean the kernel supports it
// which is why we have to check the kernel version.
static const bool kernel_supports_so_reuseport = get_kernel_version() >= 30900;
}

bool have_so_reuseport()
{
    return kernel_supports_so_reuseport;
}

std::vector<uint8_t> from_base64(std::string_view input)
{
    std::vector<uint8_t> rval;
    rval.resize((input.size() / 4) * 3 + 3);
    int n = EVP_DecodeBlock(&rval[0], (uint8_t*)input.data(), input.size());

    // OpenSSL always pads the data with zero bits. This is not something we want when we're
    // converting Base64 encoded data.
    if (input[input.size() - 2] == '=')
    {
        n -= 2;
    }
    else if (input.back() == '=')
    {
        n -= 1;
    }

    rval.resize(n);
    return rval;
}

std::string to_base64(const uint8_t* ptr, size_t len)
{
    std::string rval;
    rval.resize((len / 3) * 4 + 4);
    int n = EVP_EncodeBlock((uint8_t*)&rval[0], ptr, len);
    rval.resize(n);
    return rval;
}
}
