/*
 * Copyright (c) 2018 MariaDB Corporation Ab
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
#pragma once

#include <maxscale/ccdefs.hh>

#include <cstring>
#include <map>

#include <maxscale/buffer.hh>
#include <maxscale/protocol/mariadb/common_constants.hh>

#define MYSQL_HEADER_LEN         4
#define MYSQL_CHECKSUM_LEN       4
#define MYSQL_EOF_PACKET_LEN     9
#define MYSQL_OK_PACKET_MIN_LEN  11
#define MYSQL_ERR_PACKET_MIN_LEN 9

/**
 * Offsets and sizes of various parts of the client packet. If the offset is
 * defined but not the size, the size of the value is one byte.
 */
#define MYSQL_SEQ_OFFSET        3
#define MYSQL_COM_OFFSET        4
#define MYSQL_CHARSET_OFFSET    12
#define MYSQL_CLIENT_CAP_OFFSET 4
#define MYSQL_CLIENT_CAP_SIZE   4
#define MARIADB_CAP_OFFSET      MYSQL_CHARSET_OFFSET + 20

#define GW_MYSQL_PROTOCOL_VERSION 10    // version is 10
#define GW_MYSQL_HANDSHAKE_FILLER 0x00
#define GW_MYSQL_SERVER_LANGUAGE  0x08
#define GW_MYSQL_MAX_PACKET_LEN   0xffffffL
#define GW_MYSQL_SCRAMBLE_SIZE    MYSQL_SCRAMBLE_LEN
#define GW_SCRAMBLE_LENGTH_323    8

/**
 * Prepared statement payload response offsets for a COM_STMT_PREPARE response:
 *
 * [0]     OK (1)            -- always 0x00
 * [1-4]   statement_id (4)  -- statement-id
 * [5-6]   num_columns (2)   -- number of columns
 * [7-8]   num_params (2)    -- number of parameters
 * [9]     filler
 * [10-11] warning_count (2) -- number of warnings
 */
#define MYSQL_PS_ID_OFFSET     MYSQL_HEADER_LEN + 1
#define MYSQL_PS_ID_SIZE       4
#define MYSQL_PS_COLS_OFFSET   MYSQL_HEADER_LEN + 5
#define MYSQL_PS_COLS_SIZE     2
#define MYSQL_PS_PARAMS_OFFSET MYSQL_HEADER_LEN + 7
#define MYSQL_PS_PARAMS_SIZE   2
#define MYSQL_PS_WARN_OFFSET   MYSQL_HEADER_LEN + 10
#define MYSQL_PS_WARN_SIZE     2

/** The statement ID used by binary protocol commands that refer to the latest prepared statement */
#define MARIADB_PS_DIRECT_EXEC_ID 0xffffffff

/** Name of the default server side authentication plugin */
#define DEFAULT_MYSQL_AUTH_PLUGIN "mysql_native_password"

/** All authentication responses are at least this many bytes long */
#define MYSQL_AUTH_PACKET_BASE_SIZE 36

/** Maximum length of a MySQL packet */
#define MYSQL_PACKET_LENGTH_MAX 0x00ffffff

/* Max length of fields in the mysql.user table */
#define MYSQL_PASSWORD_LEN 41
#define MYSQL_HOST_MAXLEN  60
#define MYSQL_TABLE_MAXLEN 64

#define COM_QUIT_PACKET_SIZE (4 + 1)

/** Defines for response codes */
#define MYSQL_REPLY_ERR               0xff
#define MYSQL_REPLY_OK                0x00
#define MYSQL_REPLY_EOF               0xfe
#define MYSQL_REPLY_LOCAL_INFILE      0xfb
#define MYSQL_REPLY_AUTHSWITCHREQUEST 0xfe      /**< Only sent during authentication */

class DCB;
class BackendDCB;

namespace mariadb
{
/**
 * Protocol packing and unpacking functions. The functions read or write unsigned integers from/to
 * MySQL-protocol buffers. MySQL saves integers in lsb-first format, so a conversion to/from host format
 * may be required.
 */

size_t   set_byte1(uint8_t* buffer, uint8_t val);
size_t   set_byte2(uint8_t* buffer, uint16_t val);
size_t   set_byte3(uint8_t* buffer, uint32_t val);
size_t   set_byte4(uint8_t* buffer, uint32_t val);
size_t   set_byte8(uint8_t* buffer, uint64_t val);
uint16_t get_byte2(const uint8_t* buffer);
uint32_t get_byte3(const uint8_t* buffer);
uint32_t get_byte4(const uint8_t* buffer);
uint64_t get_byte8(const uint8_t* buffer);

/**
 * Protocol unpacking functions that take pointer to pointer to buffer and that move
 * the pointer forward as many bytes that are consumed.
 */

inline uint16_t consume_byte2(uint8_t** buffer)
{
    uint16_t rv = get_byte2(*buffer);
    *buffer += 2;
    return rv;
}

inline uint32_t consume_byte3(uint8_t** buffer)
{
    uint32_t rv = get_byte3(*buffer);
    *buffer += 3;
    return rv;
}

inline uint32_t consume_byte4(const uint8_t** buffer)
{
    uint32_t rv = get_byte4(*buffer);
    *buffer += 4;
    return rv;
}

inline uint64_t consume_byte8(uint8_t** buffer)
{
    uint64_t rv = get_byte8(*buffer);
    *buffer += 8;
    return rv;
}


struct HeaderData
{
    uint32_t pl_length {0};
    uint8_t  seq {0};
};

static inline HeaderData get_header(const uint8_t* buffer)
{
    auto bytes = get_byte4(buffer);
    HeaderData rval;
    rval.pl_length = (bytes & 0xFFFFFFu);
    rval.seq = bytes >> 24u;
    return rval;
}


/**
 * Write MySQL-header to buffer.
 *
 * @param buffer Destination buffer
 * @param pl_size Payload size, max 2^24 - 1
 * @param seq Sequence number
 * @return Pointer to next byte
 */
uint8_t* write_header(uint8_t* buffer, uint32_t pl_size, uint8_t seq);

/**
 * Same as mempcpy, but for uint8_t*.
 *
 * @param dest Destination buffer
 * @param src Source buffer
 * @param n Bytes to copy
 * @return Pointer to next byte
 */
uint8_t* copy_bytes(uint8_t* dest, const uint8_t* src, size_t n);

// Same for char source.
uint8_t* copy_chars(uint8_t* dest, const char* src, size_t n);

/**
 * Same as memset.
 *
 * @param dest Destination buffer
 * @param val Value to write
 * @param n Bytes to write
 * @return Pointer to next byte
 */
uint8_t* set_bytes(uint8_t* dest, uint8_t val, size_t n);

uint32_t get_packet_length(const uint8_t* buffer);

bool command_will_respond(uint32_t cmd);

/**
 * Check if GWBUF is a COM_QUERY packet.
 */
bool is_com_query(const GWBUF& buf);

/**
 * Check if GWBUF is a COM_STMT_PREPARE packet.
 */
bool is_com_prepare(const GWBUF& buf);

/**
 * Check if GWBUF is a COM_QUERY or COM_STMT_PREPARE packet.
 */
bool is_com_query_or_prepare(const GWBUF& buf);

GWBUF create_ok_packet(uint8_t sequence, uint64_t affected_rows,
                       const std::map<std::string, std::string>& variables = {});

GWBUF create_query(std::string_view query);

GWBUF get_complete_packets(GWBUF& buffer);

GWBUF get_next_MySQL_packet(GWBUF& buffer);

GWBUF create_ok_packet();

std::string_view get_sql(const GWBUF& packet);

/**
 * Given a buffer containing a MySQL statement, this function will return
 * a pointer to the first character that is not whitespace. In this context,
 * comments are also counted as whitespace. For instance:
 *
 *    "SELECT"                    => "SELECT"
 *    "  SELECT                   => "SELECT"
 *    " / * A comment * / SELECT" => "SELECT"
 *    "-- comment\nSELECT"        => "SELECT"
 *
 *  @param sql  Pointer to buffer containing a MySQL statement
 *  @param len  Length of sql.
 *
 *  @return The first non whitespace (including comments) character. If the
 *          entire buffer is only whitespace, the returned pointer will point
 *          to the character following the buffer (i.e. sql + len).
 */
const char* bypass_whitespace(std::string_view sql);

inline const char* bypass_whitespace(const char* sql, size_t len)
{
    return bypass_whitespace(std::string_view {sql, len});
}

/**
 * Trim whitespace and remove quote characters surrounding a string.
 *
 * @note The string is modified in place.
 *
 *   'abcd' => abcd
 *   "abcd" => abcd
 *   `abcd` => abcd
 *
 * @param s  The string to be trimmed.
 *
 * @return True, if the string could be trimmed, false otherwise. False
 *         indicates that the string was not properly quoted; e.g. had
 *         a starting but not ending quote.
 */
bool trim_quotes(char* s);


/**
 * Create a MySQL ERR packet.
 *
 * @param sequence Packet sequence number
 * @param err_num  MySQL error number
 * @param statemsg MySQL State Message
 * @param msg      Human-readable error message
 *
 * @return The buffer
 */
GWBUF create_error_packet(uint8_t sequence, uint16_t err_num, std::string_view sqlstate,
                          std::string_view msg);

/**
 * Extract error messages from buffers
 *
 * @param buffer Buffer containing an error
 *
 * @return String representation of the error
 */
std::string extract_error(const GWBUF& buffer);
}

/** MySQL protocol constants */
enum gw_mysql_capabilities_t
{
    GW_MYSQL_CAPABILITIES_NONE = 0,
    /** This is sent by pre-10.2 clients */
    GW_MYSQL_CAPABILITIES_CLIENT_MYSQL           = (1 << 0),
    GW_MYSQL_CAPABILITIES_FOUND_ROWS             = (1 << 1),
    GW_MYSQL_CAPABILITIES_LONG_FLAG              = (1 << 2),
    GW_MYSQL_CAPABILITIES_CONNECT_WITH_DB        = (1 << 3),
    GW_MYSQL_CAPABILITIES_NO_SCHEMA              = (1 << 4),
    GW_MYSQL_CAPABILITIES_COMPRESS               = (1 << 5),
    GW_MYSQL_CAPABILITIES_ODBC                   = (1 << 6),
    GW_MYSQL_CAPABILITIES_LOCAL_FILES            = (1 << 7),
    GW_MYSQL_CAPABILITIES_IGNORE_SPACE           = (1 << 8),
    GW_MYSQL_CAPABILITIES_PROTOCOL_41            = (1 << 9),
    GW_MYSQL_CAPABILITIES_INTERACTIVE            = (1 << 10),
    GW_MYSQL_CAPABILITIES_SSL                    = (1 << 11),
    GW_MYSQL_CAPABILITIES_IGNORE_SIGPIPE         = (1 << 12),
    GW_MYSQL_CAPABILITIES_TRANSACTIONS           = (1 << 13),
    GW_MYSQL_CAPABILITIES_RESERVED               = (1 << 14),
    GW_MYSQL_CAPABILITIES_SECURE_CONNECTION      = (1 << 15),
    GW_MYSQL_CAPABILITIES_MULTI_STATEMENTS       = (1 << 16),
    GW_MYSQL_CAPABILITIES_MULTI_RESULTS          = (1 << 17),
    GW_MYSQL_CAPABILITIES_PS_MULTI_RESULTS       = (1 << 18),
    GW_MYSQL_CAPABILITIES_PLUGIN_AUTH            = (1 << 19),
    GW_MYSQL_CAPABILITIES_CONNECT_ATTRS          = (1 << 20),
    GW_MYSQL_CAPABILITIES_AUTH_LENENC_DATA       = (1 << 21),
    GW_MYSQL_CAPABILITIES_EXPIRE_PASSWORD        = (1 << 22),
    GW_MYSQL_CAPABILITIES_SESSION_TRACK          = (1 << 23),
    GW_MYSQL_CAPABILITIES_DEPRECATE_EOF          = (1 << 24),
    GW_MYSQL_CAPABILITIES_SSL_VERIFY_SERVER_CERT = (1 << 30),
    GW_MYSQL_CAPABILITIES_REMEMBER_OPTIONS       = (1 << 31),
    GW_MYSQL_CAPABILITIES_SERVER                 = (
        GW_MYSQL_CAPABILITIES_CLIENT_MYSQL
        | GW_MYSQL_CAPABILITIES_FOUND_ROWS
        | GW_MYSQL_CAPABILITIES_LONG_FLAG
        | GW_MYSQL_CAPABILITIES_CONNECT_WITH_DB
        | GW_MYSQL_CAPABILITIES_NO_SCHEMA
        | GW_MYSQL_CAPABILITIES_ODBC
        | GW_MYSQL_CAPABILITIES_LOCAL_FILES
        | GW_MYSQL_CAPABILITIES_IGNORE_SPACE
        | GW_MYSQL_CAPABILITIES_PROTOCOL_41
        | GW_MYSQL_CAPABILITIES_INTERACTIVE
        | GW_MYSQL_CAPABILITIES_IGNORE_SIGPIPE
        | GW_MYSQL_CAPABILITIES_TRANSACTIONS
        | GW_MYSQL_CAPABILITIES_RESERVED
        | GW_MYSQL_CAPABILITIES_SECURE_CONNECTION
        | GW_MYSQL_CAPABILITIES_MULTI_STATEMENTS
        | GW_MYSQL_CAPABILITIES_MULTI_RESULTS
        | GW_MYSQL_CAPABILITIES_PS_MULTI_RESULTS
        | GW_MYSQL_CAPABILITIES_PLUGIN_AUTH
        | GW_MYSQL_CAPABILITIES_CONNECT_ATTRS
        | GW_MYSQL_CAPABILITIES_AUTH_LENENC_DATA
        | GW_MYSQL_CAPABILITIES_SESSION_TRACK
        | GW_MYSQL_CAPABILITIES_DEPRECATE_EOF),
};

/**
 * Capabilities supported by MariaDB 10.2 and later, stored in the last 4 bytes
 * of the 10 byte filler of the initial handshake packet.
 *
 * The actual capability bytes use by the server are left shifted by an extra 32
 * bits to get one 64 bit capability that combines the old and new capabilities.
 * Since we only use these in the non-shifted form, the definitions declared here
 * are right shifted by 32 bytes and can be directly copied into the extra capabilities.
 */
#define MXS_MARIA_CAP_PROGRESS             (1UL << 0)
#define MXS_MARIA_CAP_COM_MULTI            (1UL << 1)
#define MXS_MARIA_CAP_STMT_BULK_OPERATIONS (1UL << 2)
#define MXS_MARIA_CAP_EXTENDED_TYPES       (1UL << 3)   // Added in 10.5
#define MXS_MARIA_CAP_CACHE_METADATA       (1UL << 4)   // Added in 10.6

// Default extended flags that MaxScale supports
constexpr const uint32_t MXS_EXTRA_CAPABILITIES_SERVER =
    MXS_MARIA_CAP_STMT_BULK_OPERATIONS
    | MXS_MARIA_CAP_CACHE_METADATA;

// Same as above, for uint64.
constexpr const uint64_t MXS_EXTRA_CAPS_SERVER64 = ((uint64_t)MXS_EXTRA_CAPABILITIES_SERVER << 32);

enum mxs_mysql_cmd_t
{
    MXS_COM_SLEEP = 0,
    MXS_COM_QUIT,
    MXS_COM_INIT_DB,
    MXS_COM_QUERY,
    MXS_COM_FIELD_LIST,
    MXS_COM_CREATE_DB,
    MXS_COM_DROP_DB,
    MXS_COM_REFRESH,
    MXS_COM_SHUTDOWN,
    MXS_COM_STATISTICS,
    MXS_COM_PROCESS_INFO,
    MXS_COM_CONNECT,
    MXS_COM_PROCESS_KILL,
    MXS_COM_DEBUG,
    MXS_COM_PING,
    MXS_COM_TIME = 15,
    MXS_COM_DELAYED_INSERT,
    MXS_COM_CHANGE_USER,
    MXS_COM_BINLOG_DUMP,
    MXS_COM_TABLE_DUMP,
    MXS_COM_CONNECT_OUT = 20,
    MXS_COM_REGISTER_SLAVE,
    MXS_COM_STMT_PREPARE        = 22,
    MXS_COM_STMT_EXECUTE        = 23,
    MXS_COM_STMT_SEND_LONG_DATA = 24,
    MXS_COM_STMT_CLOSE          = 25,
    MXS_COM_STMT_RESET          = 26,
    MXS_COM_SET_OPTION          = 27,
    MXS_COM_STMT_FETCH          = 28,
    MXS_COM_DAEMON              = 29,
    MXS_COM_UNSUPPORTED         = 30,
    MXS_COM_RESET_CONNECTION    = 31,
    MXS_COM_XPAND_REPL          = 0x42,
    MXS_COM_STMT_BULK_EXECUTE   = 0xfa,
    MXS_COM_MULTI               = 0xfe,
    MXS_COM_END,
    MXS_COM_UNDEFINED = -1
};

namespace mariadb
{

const char* cmd_to_string(int cmd);

}

/**
 * A GWBUF property with this name will contain the latest GTID in string form.
 * This information is only available in OK packets.
 */
static const char* const MXS_LAST_GTID = "last_gtid";

static inline mxs_mysql_cmd_t MYSQL_GET_COMMAND(const uint8_t* header)
{
    return (mxs_mysql_cmd_t)header[4];
}

static inline uint8_t MYSQL_GET_PACKET_NO(const uint8_t* header)
{
    return header[3];
}

static inline uint32_t MYSQL_GET_PAYLOAD_LEN(const uint8_t* header)
{
    return mariadb::get_byte3(header);
}

static inline uint32_t MYSQL_GET_PACKET_LEN(const GWBUF* buffer)
{
    mxb_assert(buffer);
    return MYSQL_GET_PAYLOAD_LEN(GWBUF_DATA(buffer)) + MYSQL_HEADER_LEN;
}

GWBUF mysql_create_com_quit();
GWBUF mysql_create_custom_error(int sequence, int affected_rows, uint16_t errnum, const char* errmsg);

/**
 * @brief Check if the given command byte is valid
 *
 * @param command Command to inspect
 *
 * @return True if the command is valid
 */
bool mxs_mysql_is_valid_command(uint8_t command);

/**
 * @brief Check if the buffer contains an OK packet
 *
 * @param buffer Buffer containing a complete MySQL packet
 * @return True if the buffer contains an OK packet
 */
bool mxs_mysql_is_ok_packet(const GWBUF& buffer);

/**
 * @brief Check if the buffer contains an ERR packet
 *
 * @param buffer Buffer containing a complete MySQL packet
 * @return True if the buffer contains an ERR packet
 */
bool mxs_mysql_is_err_packet(const GWBUF& buffer);

/**
 * Extract the error code from an ERR packet
 *
 * @param buffer Buffer containing the ERR packet
 *
 * @return The error code or 0 if the buffer is not an ERR packet
 */
uint16_t mxs_mysql_get_mysql_errno(const GWBUF& buffer);

/**
 * Is this a binary protocol command
 *
 * @param cmd Command to check
 *
 * @return True if the command is a binary protocol command
 */
bool mxs_mysql_is_ps_command(uint8_t cmd);

/**
 * @brief Get the command byte
 *
 * @param buffer Buffer containing a complete MySQL packet
 *
 * @return The command byte, or MXS_COM_UNDEFINED if packet does not have a payload.
 */
uint8_t mxs_mysql_get_command(const GWBUF& buffer);

/**
 * @brief Extract the ID from a COM_STMT command
 *
 * All the COM_STMT type commands store the statement ID in the same place.
 *
 * @param buffer Buffer containing one of the COM_STMT commands (not COM_STMT_PREPARE)
 *
 * @return The statement ID
 */
uint32_t mxs_mysql_extract_ps_id(const GWBUF& buffer);

/**
 * @brief Determine if a packet contains a one way message
 *
 * @param cmd Command to inspect
 *
 * @return True if a response is expected from the server
 */
bool mxs_mysql_command_will_respond(uint8_t cmd);

/**
 * Does this command start a binlog dump
 *
 * @param cmd Command to inspect
 *
 * @return True if this command will start a binlog dump, either a normal or an XPand one.
 */
inline constexpr bool mxs_mysql_is_binlog_dump(uint8_t cmd)
{
    return cmd == MXS_COM_BINLOG_DUMP || cmd == MXS_COM_XPAND_REPL;
}

/**
 * Calculates mysql_native_password auth token from a scramble and a password hash
 *
 * The algorithm used is: `SHA1(scramble + SHA1(SHA1(password))) ^ SHA1(password)`
 *
 * @param scramble The 20 byte scramble sent by the server
 * @param pw_sha1  The SHA1(password) sent by the client. Must be 20 bytes long.
 * @param output   Pointer where the resulting 20 byte hash is stored
 * @return Pointer after written data
 */
uint8_t* mxs_mysql_calculate_hash(const uint8_t* scramble, const std::vector<uint8_t>& pw_sha1,
                                  uint8_t* output);

/** IMPL **/
namespace mariadb
{

inline size_t set_byte1(uint8_t* buffer, uint8_t val)
{
    *buffer = val;
    return 1;
}

inline size_t set_byte2(uint8_t* buffer, uint16_t val)
{
    const size_t N = 2;
    uint16_t le16 = htole16(val);
    memcpy(buffer, &le16, N);
    return N;
}

inline size_t set_byte3(uint8_t* buffer, uint32_t val)
{
    const size_t N = 3;
    set_byte2(buffer, val);
    buffer[2] = (val >> 16);
    return N;
}

inline size_t set_byte4(uint8_t* buffer, uint32_t val)
{
    const size_t N = 4;
    uint32_t le32 = htole32(val);
    memcpy(buffer, &le32, N);
    return N;
}

inline size_t set_byte8(uint8_t* buffer, uint64_t val)
{
    const size_t N = 8;
    uint64_t le64 = htole64(val);
    memcpy(buffer, &le64, N);
    return N;
}

inline uint16_t get_byte2(const uint8_t* buffer)
{
    uint16_t le16;
    memcpy(&le16, buffer, 2);
    return le16toh(le16);
}

inline uint32_t get_byte3(const uint8_t* buffer)
{
    uint32_t low = get_byte2(buffer);
    uint32_t high = buffer[2];
    return low | (high << 16);
}

inline uint32_t get_byte4(const uint8_t* buffer)
{
    uint32_t le32;
    memcpy(&le32, buffer, 4);
    return le32toh(le32);
}

inline uint64_t get_byte8(const uint8_t* buffer)
{
    uint64_t le64;
    memcpy(&le64, buffer, 8);
    return le64toh(le64);
}
}
