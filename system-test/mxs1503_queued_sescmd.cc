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
 * MXS-1503: Test master reconnection with session command history
 *
 * https://jira.mariadb.org/browse/MXS-1503
 */
#include <maxtest/testconnections.hh>
#include <vector>
#include <iostream>
#include <functional>

using std::cout;
using std::endl;

int main(int argc, char** argv)
{
    TestConnections test(argc, argv);

    auto query = [&test](std::string q) {
            return execute_query_silent(test.maxscale->conn_rwsplit, q.c_str());
        };

    auto check_result = [&test](std::string name, std::string res) {
            std::string sql = "SELECT " + name;
            char value[1024];
            return find_field(test.maxscale->conn_rwsplit, sql.c_str(), name.c_str(), value) == 0
                   && res == value;
        };

    test.maxscale->connect();
    test.expect(query("DROP TABLE IF EXISTS test.t1;") == 0, "DROP TABLE should work.");
    test.expect(query("CREATE TABLE test.t1 (id INT);") == 0, "CREATE TABLE should work.");

    // Execute session commands so that the history is not empty
    cout << "Setting user variables" << endl;
    test.expect(query("SET @a = 1") == 0, "First session command should work.");
    test.expect(query("USE test") == 0, "Second session command should work.");
    test.expect(query("SET @b = 2") == 0, "Third session command should work.");

    // Block the master to trigger reconnection
    cout << "Blocking master" << endl;
    test.repl->block_node(0);
    test.maxscale->wait_for_monitor();
    cout << "Unblocking master" << endl;
    test.repl->unblock_node(0);
    test.maxscale->wait_for_monitor();

    // Check that inserts work
    cout << "Selecting user variables" << endl;
    test.reset_timeout();
    test.expect(query("INSERT INTO test.t1 VALUES (1)") == 0, "Write should work after unblocking master");
    test.expect(check_result("@a", "1"), "@a should be 1");
    test.expect(check_result("@b", "2"), "@b should be 2");
    query("DROP TABLE test.t1");

    return test.global_result;
}
