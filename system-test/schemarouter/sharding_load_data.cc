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
 * MXS-1160: LOAD DATA LOCAL INFILE with schemarouter
 */

#include <maxtest/testconnections.hh>

#include <fstream>
#include <sstream>
#include <string>

int main(int argc, char** argv)
{
    // Create a CSV file
    unlink("data.csv");
    std::ofstream output("data.csv");
    std::stringstream ss;

    for (int i = 0; i < 100; i++)
    {
        ss << i << "\n";
    }

    output << ss.str();
    output.close();

    TestConnections test(argc, argv);
    test.repl->execute_query_all_nodes("DROP DATABASE db1");
    test.repl->connect();
    execute_query(test.repl->nodes[0], "CREATE DATABASE db1");
    execute_query(test.repl->nodes[0], "CREATE TABLE db1.t1(id INT)");
    test.maxscale->connect_maxscale();

    test.tprintf("Loading local data file");

    test.try_query(test.maxscale->conn_rwsplit, "LOAD DATA LOCAL INFILE 'data.csv' INTO TABLE db1.t1");

    test.tprintf("Verifying that data was loaded");

    long total = execute_query_count_rows(test.maxscale->conn_rwsplit, "SELECT * FROM db1.t1");
    test.add_result(total != 100, "Expected 100 rows, got %ld", total);

    test.maxscale->close_maxscale_connections();

    test.repl->execute_query_all_nodes("DROP DATABASE db1");

    // Remove the test data
    unlink("data.csv");

    return test.global_result;
}
