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
 * @file mxs922_bad_server.cpp MXS-922: Server removal test
 *
 */

#include <maxtest/testconnections.hh>

#define MONITOR_NAME "mysql-monitor"
#define SERVICE_NAME "rwsplit-service"

void add_servers(TestConnections* test)
{
    test->tprintf("Adding the servers");
    test->reset_timeout();
    test->check_maxctrl("link monitor " MONITOR_NAME " server1 server2 server3 server4 ");
    test->check_maxctrl("link service " SERVICE_NAME " server1 server2 server3 server4 ");
}

void remove_servers(TestConnections* test)
{
    test->tprintf("Remove the servers");
    test->reset_timeout();
    test->check_maxctrl("unlink monitor " MONITOR_NAME " server1 server2 server3 server4 ");
    test->check_maxctrl("unlink service " SERVICE_NAME " server1 server2 server3 server4 ");
}

void destroy_servers(TestConnections* test)
{
    test->tprintf("Destroy the servers");
    test->reset_timeout();

    for (int i = 0; i < 4; i++)
    {
        test->check_maxctrl("destroy server server" + std::to_string(i + 1));
    }
}

void do_query(TestConnections* test, bool should_fail)
{
    test->tprintf("Trying to query, expecting %s", should_fail ? "failure" : "success");
    test->reset_timeout();

    test->maxscale->connect_maxscale();

    bool failed = execute_query(test->maxscale->conn_rwsplit, "select @@server_id") == 0;

    const char* msg = should_fail ?
        "Query was successful when failure was expected." :
        "Query failed when success was expected.";

    test->add_result(failed == should_fail, "%s", msg);
    test->maxscale->close_maxscale_connections();

}

int main(int argc, char* argv[])
{
    TestConnections* test = new TestConnections(argc, argv);

    test->tprintf("Creating servers with bad addresses");

    for (int i = 0; i < 4; i++)
    {
        test->check_maxctrl("create server server" + std::to_string(i + 1)
                            + " 127.0.0.1 999" + std::to_string(i + 1));
    }

    /** Add the servers to the monitor and service */
    add_servers(test);

    do_query(test, true);

    /** Remove and destroy servers from monitor and service */
    remove_servers(test);
    destroy_servers(test);

    test->tprintf("Create the servers with correct parameters");
    for (int i = 0; i < 4; i++)
    {
        std::stringstream ss;
        ss << "create server server" << i + 1 << " "
           << test->repl->ip_private(i) << " " << test->repl->port(i);
        test->check_maxctrl(ss.str());
    }

    /**  Add the servers again */
    add_servers(test);

    test->tprintf("Wait for the monitor to see the new servers");
    test->maxscale->wait_for_monitor();

    test->tprintf("Reload users");
    test->check_maxctrl("reload service rwsplit-service");

    do_query(test, false);

    /** Remove everything */
    remove_servers(test);
    destroy_servers(test);

    do_query(test, true);

    test->maxscale->expect_running_status(true);
    int rval = test->global_result;
    delete test;
    return rval;
}
