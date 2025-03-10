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
 * MXS-1507: Transaction replay stress test
 *
 * https://jira.mariadb.org/browse/MXS-1507
 */
#include <maxtest/testconnections.hh>
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>

using namespace std;

atomic<bool> running {true};

void client_thr(TestConnections* test, int id)
{
    MYSQL* conn = test->maxscale->open_rwsplit_connection();

    while (running && test->global_result == 0)
    {
        test->try_query(conn, "START TRANSACTION");
        test->try_query(conn, "INSERT INTO test.t1 (a) VALUES (%d)", id);
        int last_id = mysql_insert_id(conn);
        test->try_query(conn, "UPDATE test.t1 SET a = -1 WHERE id = %d", last_id);
        test->try_query(conn, "COMMIT");
        test->try_query(conn, "DELETE FROM test.t1 WHERE id = %d", last_id);
        sleep(1);
    }

    mysql_close(conn);
}

int main(int argc, char** argv)
{
    TestConnections test(argc, argv);

    test.repl->connect();

    cout << "Creating table" << endl;
    test.try_query(test.repl->nodes[0],
                   "CREATE OR REPLACE TABLE "
                   "test.t1 (id int, a int)");

    cout << "Syncing slaves" << endl;
    test.repl->sync_slaves();


    cout << "Starting threads" << endl;
    const int N_THREADS = 1;
    vector<thread> threads;

    for (int i = 0; i < N_THREADS; i++)
    {
        threads.emplace_back(client_thr, &test, i);
    }

    for (int i = 0; i < 5; i++)
    {
        sleep(10);
        test.repl->block_node(0);
        sleep(10);
        test.repl->unblock_node(0);
    }

    cout << "Stopping threads" << endl;
    running = false;

    // Should be plenty of time for the threads to stop
    test.reset_timeout();

    for (auto& a : threads)
    {
        a.join();
    }

    test.repl->connect();
    execute_query_silent(test.repl->nodes[0], "DROP TABLE test.t1");
    execute_query_silent(test.repl->nodes[0], "DROP USER 'testuser'@'%'");
    test.repl->disconnect();

    return test.global_result;
}
