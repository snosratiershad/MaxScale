/*
 * Copyright (c) 2019 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */
#pragma once

#include <maxscale/monitor.hh>

class ResultSet;

/**
 * This class contains internal monitor management functions that should not be exposed in the public
 * monitor class. It's a friend of MXS_MONITOR.
 */
class MonitorManager
{
public:

    /**
     * Creates a new monitor. Loads the module, calls constructor and configure, and adds monitor to the
     * global list.
     *
     * @param name          The configuration name of the monitor
     * @param module        The module name to load
     * @return              The newly created monitor, or NULL on error
     */
    static mxs::Monitor* create_monitor(const std::string& name, const std::string& module,
                                        MXS_CONFIG_PARAMETER* params);

    /**
     * Mark monitor as deactivated. A deactivated monitor appears not to exist, as if it had been
     * destroyed. Any servers the monitor had are removed. The monitor should not be serialized after
     * this function.
     *
     * @param monitor Monitor to deactivate
     */
    static void deactivate_monitor(mxs::Monitor* monitor);

    /**
     * @brief Destroys all monitors. At this point all monitors should
     *        have been stopped.
     *
     * @attn Must only be called in single-thread context at system shutdown.
     */
    static void destroy_all_monitors();

    static void start_monitor(mxs::Monitor* monitor);

    /**
     * Stop a given monitor
     *
     * @param monitor The monitor to stop
     */
    static void stop_monitor(mxs::Monitor* monitor);

    static void stop_all_monitors();
    static void start_all_monitors();

    static mxs::Monitor* find_monitor(const char* name);

    /**
     * Populate services with the servers of the monitors. Should be called at the end of configuration file
     * processing to ensure that services are notified of the servers a monitor has. During runtime, the
     * normal add/remove server functions do the notifying. TODO: If a service is created at runtime, is
     * it properly populated?
     */
    static void populate_services();

    /**
     * Get links to monitors that relate to a server.
     *
     * @param server Server to inspect
     * @param host   Hostname of this server
     * @return Array of monitor links or NULL if no relations exist
     */
    static json_t* monitor_relations_to_server(const SERVER* server, const char* host);

    /**
     * Convert all monitors to JSON.
     *
     * @param host    Hostname of this server
     * @return JSON array containing all monitors
     */
    static json_t* monitor_list_to_json(const char* host);

    /**
     * Check if a server is being monitored and return the monitor.
     * @param server Server that is queried
     * @return The monitor watching this server, or NULL if not monitored
     */
    static mxs::Monitor* server_is_monitored(const SERVER* server);

    static void show_all_monitors(DCB* dcb);
    static void monitor_show(DCB* dcb, mxs::Monitor* monitor);

    static void monitor_list(DCB*);

    static std::unique_ptr<ResultSet> monitor_get_list();

    /**
     * @brief Serialize a monitor to a file
     *
     * This converts the static configuration of the monitor into an INI format file.
     *
     * @param monitor Monitor to serialize
     * @return True if serialization was successful
     */
    static bool monitor_serialize(const mxs::Monitor* monitor);

    /**
     * Attempt to reconfigure a monitor
     *
     * If the configuration fails, the old parameters are restored.
     *
     * @param monitor    Monitor to reconfigure
     * @param parameters New parameters to apply
     *
     * @return True if reconfiguration was successful
     */
    static bool reconfigure_monitor(mxs::Monitor* monitor, const MXS_CONFIG_PARAMETER& parameters);

    /**
     * @brief Convert monitor to JSON
     *
     * @param monitor Monitor to convert
     * @param host    Hostname of this server
     *
     * @return JSON representation of the monitor
     */
    static json_t* monitor_to_json(const mxs::Monitor* monitor, const char* host);

    static bool create_monitor_config(const mxs::Monitor* monitor, const char* filename);

    /**
     * Waits until all running monitors have advanced one tick.
     */
    static void debug_wait_one_tick();
};

// RAII helper class for temprarily stopping monitors
class MonitorStop
{
public:
    MonitorStop(mxs::Monitor* monitor)
        : m_monitor(monitor->state() == MONITOR_STATE_RUNNING ? monitor : nullptr)
    {
        if (m_monitor)
        {
            MonitorManager::stop_monitor(m_monitor);
        }
    }

    ~MonitorStop()
    {
        if (m_monitor)
        {
            MonitorManager::start_monitor(m_monitor);
        }
    }

private:
    mxs::Monitor* m_monitor;
};
