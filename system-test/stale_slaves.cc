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
 * @file stale_slaves.cpp Testing slaves who have lost their master and how MaxScale works with them
 *
 * When the master server is blocked and slaves lose their master, they should
 * still be available for read queries. Once the master comes back, all slaves
 * should get slave status if replication is running.
 */

#include <maxtest/testconnections.hh>

#include <algorithm>
#include <string>
#include <vector>

using namespace std;

int main(int argc, char** argv)
{
    TestConnections test(argc, argv);
    vector<string> ids;

    test.repl->connect();
    for (int i = 0; i < test.repl->N; i++)
    {
        ids.push_back(test.repl->get_server_id_str(i));
    }

    auto get_id = [&]() {
            Connection c = test.maxscale->readconn_slave();
            test.expect(c.connect(), "Connection should be OK: %s", c.error());
            string res = c.field("SELECT @@server_id");
            test.expect(!res.empty(), "Field should not be empty: %s", c.error());
            return res;
        };

    auto in_use = [&](string id) {
            for (int i = 0; i < 2 * test.repl->N; i++)
            {
                if (get_id() == id)
                {
                    return true;
                }
            }

            return false;
        };

    test.tprintf("Blocking the master and doing a read query");
    test.repl->block_node(0);
    test.maxscale->wait_for_monitor(2);

    string first = get_id();
    auto it = find(begin(ids), end(ids), first);
    test.expect(it != end(ids), "ID should be found");
    int node = distance(begin(ids), it);

    test.tprintf("Blocking the slave that replied to us");
    test.repl->block_node(node);
    test.maxscale->wait_for_monitor(2);
    test.expect(!in_use(first), "The first slave should not be in use");

    test.tprintf("Unblocking all nodes");
    test.repl->unblock_all_nodes();
    test.maxscale->wait_for_monitor(2);
    test.expect(in_use(first), "The first slave should be in use");

    test.tprintf("Stopping replication on first slave");
    execute_query(test.repl->nodes[node], "STOP SLAVE");
    test.maxscale->wait_for_monitor(2);
    test.expect(!in_use(first), "The first slave should not be in use");

    test.tprintf("Starting replication on first slave");
    execute_query(test.repl->nodes[node], "START SLAVE");
    test.maxscale->wait_for_monitor(2);
    test.expect(in_use(first), "The first slave should be in use");
    test.repl->disconnect();

    return test.global_result;
}
