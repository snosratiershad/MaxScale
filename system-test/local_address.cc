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

#include <maxtest/testconnections.hh>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <vector>
#include <string>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

using namespace std;

namespace
{

template<class T>
void to_collection(string s, const string& delimiter, T* pT)
{
    size_t pos;

    while ((pos = s.find(delimiter)) != std::string::npos)
    {
        pT->push_back(s.substr(0, pos));
        s.erase(0, pos + delimiter.length());
    }

    if (s.length() != 0)
    {
        pT->push_back(s);
    }
}

string& ltrim(std::string& s)
{
    s.erase(s.begin(),
            std::find_if(s.begin(),
                         s.end(),
                         std::not1(std::ptr_fun<int, int>(std::isspace))));
    return s;
}

string& rtrim(std::string& s)
{
    s.erase(std::find_if(s.rbegin(),
                         s.rend(),
                         std::not1(std::ptr_fun<int, int>(std::isspace))).base(),
            s.end());
    return s;
}

string& trim(std::string& s)
{
    return ltrim(rtrim(s));
}

string extract_ip(string s)
{
    // 's' looks something like: "    inet 127.0.0.1/...";
    s = s.substr(9);                        // => "127.0.0.1/...";
    s = s.substr(0, s.find_first_of('/'));  // => "127.0.0.1"
    return s;
}

void get_maxscale_ips(TestConnections& test, vector<string>* pIps)
{
    static const char COMMAND[] = "export PATH=$PATH:/sbin:/usr/sbin; ip addr|fgrep inet|fgrep -v ::";
    auto res = test.maxscale->ssh_output(COMMAND, false);
    to_collection(res.output, "\n", pIps);
    transform(pIps->begin(), pIps->end(), pIps->begin(), extract_ip);

    // Remove 127.0.0.1 if it is present.
    auto i = find(pIps->begin(), pIps->end(), "127.0.0.1");

    if (i != pIps->end())
    {
        pIps->erase(i);
    }
}
}

namespace
{

void drop_user(TestConnections& test, const string& user, const string& host)
{
    string stmt("DROP USER IF EXISTS ");

    stmt += "'";
    stmt += user;
    stmt += "'@'";
    stmt += host;
    stmt += "'";
    test.try_query(test.maxscale->conn_rwsplit, "%s", stmt.c_str());
}

void create_user(TestConnections& test, const string& user, const string& password, const string& host)
{
    string stmt("CREATE USER ");

    stmt += "'";
    stmt += user;
    stmt += "'@'";
    stmt += host;
    stmt += "'";
    stmt += " IDENTIFIED BY ";
    stmt += "'";
    stmt += password;
    stmt += "'";
    test.try_query(test.maxscale->conn_rwsplit, "%s", stmt.c_str());
}

void grant_access(TestConnections& test, const string& user, const string& host)
{
    string stmt("GRANT SELECT, INSERT, UPDATE ON *.* TO ");

    stmt += "'";
    stmt += user;
    stmt += "'@'";
    stmt += host;
    stmt += "'";
    test.try_query(test.maxscale->conn_rwsplit, "%s", stmt.c_str());

    test.try_query(test.maxscale->conn_rwsplit, "FLUSH PRIVILEGES");
}

void create_user_and_grants(TestConnections& test,
                            const string& user,
                            const string& password,
                            const string& host)
{
    test.tprintf("Creating user: %s@%s", user.c_str(), host.c_str());

    drop_user(test, user, host);
    create_user(test, user, password, host);
    grant_access(test, user, host);
}

bool select_user(MYSQL* pMysql, string* pUser)
{
    bool rv = false;

    if (mysql_query(pMysql, "SELECT USER()") == 0)
    {
        MYSQL_RES* pRes = mysql_store_result(pMysql);

        if (mysql_num_rows(pRes) == 1)
        {
            MYSQL_ROW row = mysql_fetch_row(pRes);
            *pUser = row[0];
            rv = true;
        }

        mysql_free_result(pRes);

        while (mysql_next_result(pMysql) == 0)
        {
            mysql_free_result(mysql_store_result(pMysql));
        }
    }

    return rv;
}

bool can_connect_to_maxscale(const char* zHost, int port, const char* zUser, const char* zPassword)
{
    bool could_connect = false;

    MYSQL* pMysql = mysql_init(NULL);

    if (pMysql)
    {
        unsigned int timeout = 5;
        mysql_options(pMysql, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
        mysql_options(pMysql, MYSQL_OPT_READ_TIMEOUT, &timeout);
        mysql_options(pMysql, MYSQL_OPT_WRITE_TIMEOUT, &timeout);

        if (mysql_real_connect(pMysql, zHost, zUser, zPassword, NULL, port, NULL, 0))
        {
            string user;
            if (select_user(pMysql, &user))
            {
                could_connect = true;
            }
            else
            {
                cout << "Could not 'SELECT USER()' as '" << zUser << "': " << mysql_error(pMysql) << endl;
            }
        }
        else
        {
            cout << "Could not connect as '" << zUser << "': " << mysql_error(pMysql) << endl;
        }

        mysql_close(pMysql);
    }

    return could_connect;
}

string get_local_ip(TestConnections& test)
{
    auto res = test.maxscale->ssh_output("nslookup maxscale|fgrep Server:|sed s/Server://", false);

    return trim(res.output);
}

string get_gateway_ip(TestConnections& test)
{
    auto res = test.maxscale->ssh_output("echo $SSH_CLIENT", false);
    return mxt::cutoff_string(res.output, ' ');
}

void start_maxscale_with_local_address(TestConnections& test,
                                       const string& replace,
                                       const string& with)
{
    string command("sed -i s/");
    command += replace;
    command += "/";
    command += with;
    command += "/ ";
    command += "/etc/maxscale.cnf";

    test.maxscale->ssh_node(command.c_str(), true);
    test.maxscale->start_and_check_started();
}

void test_connecting(TestConnections& test,
                     const char* zUser,
                     const char* zPassword,
                     const char* zHost,
                     bool should_be_able_to)
{
    bool could_connect = can_connect_to_maxscale(test.maxscale->ip4(),
                                                 test.maxscale->rwsplit_port,
                                                 zUser,
                                                 zPassword);

    if (!could_connect && should_be_able_to)
    {
        test.expect(false, "%s@%s should have been able to connect, but wasn't.", zUser, zHost);
    }
    else if (could_connect && !should_be_able_to)
    {
        test.expect(false, "%s@%s should NOT have been able to connect, but was.", zUser, zHost);
    }
    else
    {
        if (could_connect)
        {
            test.tprintf("%s@%s could connect, as expected.", zUser, zHost);
        }
        else
        {
            test.tprintf("%s@%s could NOT connect, as expected.", zUser, zHost);
        }
    }
}

void run_test(TestConnections& test, const vector<string>& ips)
{
    auto* mxs = test.maxscale;
    test.maxscale->connect();

    string ip1 = ips[0];
    // If we do not have a proper second IP-address, we'll use an arbitrary one.
    string ip2 = (ips.size() > 1) ? ips[1] : string("42.42.42.42");

    string local_ip = get_local_ip(test);
    string gateway_ip = get_gateway_ip(test);

    const char* zUser1 = "alice";
    const char* zUser2 = "bob";
    const char* zPassword1 = "alicepwd";
    const char* zPassword2 = "bobpwd";

    create_user_and_grants(test, zUser1, zPassword1, ip1);
    create_user_and_grants(test, zUser1, zPassword1, local_ip);
    create_user_and_grants(test, zUser1, zPassword1, gateway_ip);
    create_user_and_grants(test, zUser2, zPassword2, ip2);
    create_user_and_grants(test, zUser2, zPassword2, local_ip);
    create_user_and_grants(test, zUser2, zPassword2, gateway_ip);
    test.repl->sync_slaves();

    test.tprintf("\n");
    test.tprintf("Testing default; alice should be able to access, bob not.");

    test_connecting(test, zUser1, zPassword1, ip1.c_str(), true);
    test_connecting(test, zUser2, zPassword2, ip2.c_str(), false);

    test.maxscale->disconnect();
    mxs->stop_and_check_stopped();

    test.tprintf("\n");
    test.tprintf("Testing with local_address=%s; alice should be able to access, bob not.",
                 ip1.c_str());

    string local_address_ip1 = "local_address=" + ip1;
    start_maxscale_with_local_address(test, "###local_address###", local_address_ip1);
    test.maxscale->connect();

    test_connecting(test, zUser1, zPassword1, ip1.c_str(), true);
    test_connecting(test, zUser2, zPassword2, ip2.c_str(), false);

    test.maxscale->disconnect();
    mxs->stop_and_check_stopped();

    if (ips.size() > 1)
    {
#ifdef USABLE_SECOND_IP_ADDRESS_ON_MAXSCALE_NODE_IS_AVAILABLE
        test.tprintf("\n");
        test.tprintf("\nTesting with local_address=%s, bob should be able to access, alice not.",
                     ip2.c_str());

        string local_address_ip2 = "local_address=" + ip2;
        start_maxscale_with_local_address(test, local_address_ip1, local_address_ip2);
        test.connect_maxscale();

        test_connecting(test, zUser1, zPassword1, ip1.c_str(), false);
        test_connecting(test, zUser2, zPassword2, ip2.c_str(), true);

        test.maxscales->disconnect();
        test.stop_maxscale();
#else
        test.tprintf("\n");
        test.tprintf("WARNING: Other IP-address (%s) not tested, as IP-address currently "
                     "not usable on VM.",
                     ip2.c_str());
#endif
    }
    else
    {
        test.tprintf("\n");
        test.tprintf("WARNING: Only one IP-address found on MaxScale node, 'local_address' "
                     "not properly tested.");
    }

    start_maxscale_with_local_address(test, "local_address.*", "");
    test.maxscale->connect();

    drop_user(test, zUser1, ip1);
    drop_user(test, zUser1, local_ip);
    drop_user(test, zUser1, gateway_ip);
    drop_user(test, zUser2, ip2);
    drop_user(test, zUser2, local_ip);
    drop_user(test, zUser2, gateway_ip);
}
}

int main(int argc, char** argv)
{
    TestConnections test(argc, argv);

    vector<string> ips;
    get_maxscale_ips(test, &ips);

    if (ips.size() >= 1)
    {
        run_test(test, ips);
    }
    else
    {
        test.expect(false, "MaxScale node does not have at least one IP-address.");
    }

    return test.global_result;
}
