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
 * @file cdc_client.cpp Test of CDC protocol (avro listener)
 * - configure binlog router setup, avro router, avro listener
 * - connect to avro listener
 * - start INSERT load thread
 * - read data from avro listener, comapre it with inserted data
 */

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

#include <maxtest/cdc_tools.hh>
#include <maxtest/sql_t1.hh>
#include <maxtest/testconnections.hh>

using namespace maxtest;
char reg_str[] = "REGISTER UUID=XXX-YYY_YYY, TYPE=JSON";
char req_str[] = "REQUEST-DATA test.t1";
std::atomic<int> insert_val {0};
bool exit_flag = false;

void* query_thread(void* ptr);

/**
 * @brief cdc_com Connects to avro listenet by CDC protocal, read data, compare data with inserted data
 * @param Test TestConnections object
 * @return true if test PASSED
 */
bool cdc_com(TestConnections* Test)
{
    int max_inserted_val = Test->smoke ? 25 : 100;
    int sock = create_tcp_socket();
    char* ip = get_ip(Test->maxscale->ip4());

    if (ip == NULL)
    {
        Test->tprintf("Can't get IP");
        return false;
    }

    struct sockaddr_in* remote = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in*));
    remote->sin_family = AF_INET;
    int tmpres = inet_pton(AF_INET, ip, (void*)(&(remote->sin_addr.s_addr)));

    if (tmpres < 0)
    {
        Test->tprintf("Can't set remote->sin_addr.s_addr");
        return false;
    }
    else if (tmpres == 0)
    {
        Test->tprintf("%s is not a valid IP address", ip);
        return false;
    }

    remote->sin_port = htons(4001);

    if (connect(sock, (struct sockaddr*)remote, sizeof(struct sockaddr)) < 0)
    {
        Test->tprintf("Could not connect");
        return false;
    }

    char* get = cdc_auth_srt((char*) "skysql", (char*) "skysql");
    Test->tprintf("Auth string: %s", get);

    // Send the query to the server
    int rv = send_so(sock, get);
    free(get);
    if (rv != 0)
    {
        Test->tprintf("Cat't send data to scoket");
        return false;
    }

    char buf1[1024];
    recv(sock, buf1, 1024, 0);

    // Send the query to the server
    if (send_so(sock, reg_str) != 0)
    {
        Test->tprintf("Cat't send data to scoket");
        return false;
    }

    recv(sock, buf1, 1024, 0);

    // Send the query to the server
    if (send_so(sock, req_str) != 0)
    {
        Test->tprintf("Cat't send data to scoket");
        return false;
    }

    int epfd = epoll_create(1);
    static struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLPRI | EPOLLERR | EPOLLHUP;
    ev.data.fd = sock;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sock, &ev) < 0)
    {
        Test->tprintf("Error in epoll_ctl! errno = %d, %s", errno, strerror(errno));
        return false;
    }

    epoll_event events[2];
    setnonblocking(sock);

    int inserted_val = 0;
    int ignore_first = 2;

    while (inserted_val < max_inserted_val)
    {
        Test->reset_timeout();
        // wait for something to do...
        Test->tprintf("epoll_wait");
        int nfds = epoll_wait(epfd, &events[0], 1, -1);
        if (nfds < 0)
        {
            Test->tprintf("Error in epoll_wait! errno = %d, %s", errno, strerror(errno));
            return false;
        }

        if (nfds > 0)
        {
            char* data = read_sc(sock);
            std::istringstream iss(data);
            free(data);

            for (std::string json; std::getline(iss, json);)
            {
                Test->tprintf("%s", json.c_str());

                if (ignore_first > 0)
                {
                    ignore_first--;     // ignoring first reads
                    if (ignore_first == 0)
                    {
                        // first reads done, starting inserting
                        insert_val = 10;
                        inserted_val = insert_val;
                    }
                }
                else
                {
                    // trying to check JSON
                    long long int x1;
                    long long int fl;
                    get_x_fl_from_json(json.c_str(), &x1, &fl);
                    Test->tprintf("data received, x1=%lld fl=%lld", x1, fl);

                    if (x1 != inserted_val || fl != inserted_val + 100)
                    {
                        Test->tprintf("wrong values in JSON");
                    }

                    inserted_val++;
                    insert_val = inserted_val;
                }
            }
        }
        else
        {
            Test->tprintf("waiting");
        }
    }

    free(remote);
    free(ip);
    close(sock);

    return true;
}

static TestConnections* Test;

int main(int argc, char* argv[])
{
    TestConnections::skip_maxscale_start(true);
    Test = new TestConnections(argc, argv);

    Test->reset_timeout();
    Test->repl->connect();
    Test->try_query(Test->repl->nodes[0], "RESET MASTER");
    create_t1(Test->repl->nodes[0]);
    execute_query(Test->repl->nodes[0], (char*) "INSERT INTO t1 VALUES (111, 222)");
    Test->repl->close_connections();

    Test->tprintf("Waiting for binlogs to be processed...");
    Test->maxscale->start();
    sleep(10);

    Test->reset_timeout();

    pthread_t thread;
    pthread_create(&thread, NULL, query_thread, NULL);

    Test->add_result(!cdc_com(Test), "Failed to execute test");

    exit_flag = true;

    pthread_join(thread, NULL);

    int rval = Test->global_result;
    Test->revert_replicate_from_master();
    delete Test;
    return rval;
}

void* query_thread(void* ptr)
{

    Test->repl->connect();

    while (!exit_flag)
    {
        if (insert_val != 0)
        {
            char str[256];
            sprintf(str, "INSERT INTO t1 VALUES (%d, %d)", insert_val.load(), insert_val.load() + 100);
            insert_val = 0;
            execute_query(Test->repl->nodes[0], "%s", str);
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    Test->repl->close_connections();

    return NULL;
}
