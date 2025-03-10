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
#include "mirror.hh"
#include "mirrorsession.hh"

#include <maxscale/protocol/mariadb/mysql.hh>

using namespace maxscale;

MirrorSession::MirrorSession(MXS_SESSION* session, Mirror* router, SMyBackends backends)
    : RouterSession(session)
    , m_backends(std::move(backends))
    , m_router(router)
{
    for (const auto& a : m_backends)
    {
        if (a->target() == m_router->get_main())
        {
            m_main = a.get();
        }
    }
}

bool MirrorSession::routeQuery(GWBUF&& packet)
{
    int rc = 0;

    if (m_responses)
    {
        m_queue.push_back(std::move(packet));
        rc = 1;
    }
    else
    {
        m_query = get_sql_string(packet);
        m_command = mxs_mysql_get_command(packet);
        bool expecting_response = protocol_data().will_respond(packet);

        for (const auto& a : m_backends)
        {
            auto type = mxs::Backend::NO_RESPONSE;

            if (expecting_response)
            {
                type = a.get() == m_main ? mxs::Backend::EXPECT_RESPONSE : mxs::Backend::IGNORE_RESPONSE;
            }

            if (a->in_use() && a->write(packet.shallow_clone(), type))
            {
                if (a.get() == m_main)
                {
                    // Routing is successful as long as we can write to the main connection
                    rc = 1;
                }

                if (expecting_response)
                {
                    ++m_responses;
                }
            }
        }
    }

    return rc;
}

void MirrorSession::route_queued_queries()
{
    while (!m_queue.empty() && m_responses == 0)
    {
        MXB_INFO(">>> Routing queued queries");
        auto query = std::move(m_queue.front());
        m_queue.pop_front();

        MXB_AT_DEBUG(std::string query_sql = get_sql_string(query));

        if (!routeQuery(std::move(query)))
        {
            break;
        }

        MXB_INFO("<<< Queued queries routed");

        // Routing of queued queries should never cause the same query to be put back into the queue. The
        // check for m_responses should prevent it.
        mxb_assert(m_queue.empty() || get_sql(m_queue.back()) != query_sql);
    }
}

void MirrorSession::finalize_reply()
{
    // All replies have now arrived. Return the last chunk of the result to the client
    // that we've been storing in the session.
    MXB_INFO("All replies received, routing last chunk to the client.");

    RouterSession::clientReply(std::move(m_last_chunk), m_last_route, m_main->reply());
    m_last_chunk.clear();

    generate_report();
    route_queued_queries();
}

bool MirrorSession::clientReply(GWBUF&& packet, const mxs::ReplyRoute& down, const mxs::Reply& reply)
{
    auto backend = static_cast<MyBackend*>(down.endpoint()->get_userdata());
    backend->process_result(packet, reply);

    if (reply.is_complete())
    {
        backend->ack_write();
        --m_responses;

        MXB_INFO("Reply from '%s' complete%s.", backend->name(), backend == m_main ?
                 ", delaying routing of last chunk until all replies have been received" : "");

        if (backend == m_main)
        {
            m_last_chunk = std::move(packet);
            m_last_route = down;
            packet.clear();
        }

        if (m_responses == 0)
        {
            mxb_assert(!m_last_chunk.empty());
            mxb_assert(!packet || backend != m_main);

            packet.clear();
            finalize_reply();
        }
    }

    bool rc = true;

    if (packet && backend == m_main)
    {
        rc = RouterSession::clientReply(std::move(packet), down, reply);
    }

    return rc;
}

bool MirrorSession::handleError(mxs::ErrorType type,
                                const std::string& message,
                                mxs::Endpoint* pProblem,
                                const mxs::Reply& reply)
{
    Backend* backend = static_cast<Backend*>(pProblem->get_userdata());

    if (backend->is_waiting_result())
    {
        --m_responses;

        if (m_responses == 0 && backend != m_main)
        {
            finalize_reply();
        }
    }

    backend->close();

    // We can continue as long as the main connection isn't dead
    bool ok = m_router->config().on_error.get() == ErrorAction::ERRACT_IGNORE && backend != m_main;
    return ok || mxs::RouterSession::handleError(type, message, pProblem, reply);
}

bool MirrorSession::should_report() const
{
    bool rval = true;

    if (m_router->config().report.get() == ReportAction::REPORT_ON_CONFLICT)
    {
        rval = false;
        std::string checksum;

        for (const auto& a : m_backends)
        {
            if (a->in_use())
            {
                if (checksum.empty())
                {
                    checksum = a->checksum().hex();
                }
                else if (checksum != a->checksum().hex())
                {
                    rval = true;
                }
            }
        }
    }

    return rval;
}

void MirrorSession::generate_report()
{
    if (should_report())
    {
        json_t* obj = json_object();
        json_object_set_new(obj, "query", json_string(m_query.c_str()));
        json_object_set_new(obj, "command", json_string(mariadb::cmd_to_string(m_command)));
        json_object_set_new(obj, "session", json_integer(m_pSession->id()));
        json_object_set_new(obj, "query_id", json_integer(++m_num_queries));

        json_t* arr = json_array();

        for (const auto& a : m_backends)
        {
            if (a->in_use())
            {
                const char* type = a->reply().error() ?
                    "error" : (a->reply().is_resultset() ? "resultset" : "ok");

                json_t* o = json_object();
                json_object_set_new(o, "target", json_string(a->name()));
                json_object_set_new(o, "checksum", json_string(a->checksum().hex().c_str()));
                json_object_set_new(o, "rows", json_integer(a->reply().rows_read()));
                json_object_set_new(o, "warnings", json_integer(a->reply().num_warnings()));
                json_object_set_new(o, "duration", json_integer(a->duration()));
                json_object_set_new(o, "type", json_string(type));

                json_array_append_new(arr, o);
            }
        }

        json_object_set_new(obj, "results", arr);

        m_router->ship(obj);
    }
}
