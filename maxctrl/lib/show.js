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

const {
  maxctrl,
  helpMsg,
  fieldDescriptions,
  getJson,
  formatResource,
  getResource,
  getCollectionAsResource,
  rDnsOption,
  getTransposedCollection,
  getSubCollection,
  getCollection,
  rawCollectionAsTable,
  dateToLocaleString,
} = require("./common.js");

const server_fields = [
  {
    name: "Server",
    path: "id",
    description: "Server name",
  },
  {
    name: "Source",
    path: "attributes.source.file",
    description: "File where the object is stored in",
  },
  {
    name: "Address",
    path: "attributes.parameters.address",
    description: "Address where the server listens",
  },
  {
    name: "Port",
    path: "attributes.parameters.port",
    description: "The port on which the server listens",
  },
  {
    name: "State",
    path: "attributes.state",
    description: "Server state",
  },
  {
    name: "Version",
    path: "attributes.version_string",
    description: "Server version",
  },
  {
    name: "Uptime",
    path: "attributes.uptime",
    description: "Server uptime in seconds",
  },
  {
    name: "Last Event",
    path: "attributes.last_event",
    description: "The type of the latest event",
  },
  {
    name: "Triggered At",
    path: "attributes.triggered_at",
    description: "Time when the latest event was triggered at",
  },
  {
    name: "Services",
    path: "relationships.services.data[].id",
    description: "Services that use this server",
  },
  {
    name: "Monitors",
    path: "relationships.monitors.data[].id",
    description: "Monitors that monitor this server",
  },
  {
    name: "Master ID",
    path: "attributes.master_id",
    description: "The server ID of the master",
  },
  {
    name: "Node ID",
    path: "attributes.node_id",
    description: "The node ID of this server",
  },
  {
    name: "Slave Server IDs",
    path: "attributes.slaves",
    description: "List of slave server IDs",
  },
  {
    name: "Current Connections",
    path: "attributes.statistics.connections",
    description: "Current connection count",
  },
  {
    name: "Total Connections",
    path: "attributes.statistics.total_connections",
    description: "Total cumulative connection count",
  },
  {
    name: "Max Connections",
    path: "attributes.statistics.max_connections",
    description: "Maximum number of concurrent connections ever seen",
  },
  {
    name: "Statistics",
    path: "attributes.statistics",
    description: "Server statistics",
  },
  {
    name: "Parameters",
    path: "attributes.parameters",
    description: "Server parameters",
  },
];

const service_fields = [
  {
    name: "Service",
    path: "id",
    description: "Service name",
  },
  {
    name: "Source",
    path: "attributes.source.file",
    description: "File where the object is stored in",
  },
  {
    name: "Router",
    path: "attributes.router",
    description: "Router that the service uses",
  },
  {
    name: "State",
    path: "attributes.state",
    description: "Service state",
  },
  {
    name: "Started At",
    path: "attributes.started",
    description: "When the service was started",
    formatter: dateToLocaleString,
  },
  {
    name: "Users Loaded At",
    path: "attributes.users_last_update",
    description: "When the users for the service were loaded",
    formatter: dateToLocaleString,
  },
  {
    name: "Current Connections",
    path: "attributes.statistics.connections",
    description: "Current connection count",
  },
  {
    name: "Total Connections",
    path: "attributes.statistics.total_connections",
    description: "Total connection count",
  },
  {
    name: "Max Connections",
    path: "attributes.statistics.max_connections",
    description: "Historical maximum connection count",
  },
  {
    name: "Cluster",
    path: "relationships.monitors.data[].id",
    description: "The cluster that the service uses",
  },
  {
    name: "Servers",
    path: "relationships.servers.data[].id",
    description: "Servers that the service uses",
  },
  {
    name: "Services",
    path: "relationships.services.data[].id",
    description: "Services that the service uses",
  },
  {
    name: "Filters",
    path: "relationships.filters.data[].id",
    description: "Filters that the service uses",
  },
  {
    name: "Parameters",
    path: "attributes.parameters",
    description: "Service parameter",
  },
  {
    name: "Router Diagnostics",
    path: "attributes.router_diagnostics",
    description: "Diagnostics provided by the router module",
  },
];

const monitor_fields = [
  {
    name: "Monitor",
    path: "id",
    description: "Monitor name",
  },
  {
    name: "Source",
    path: "attributes.source.file",
    description: "File where the object is stored in",
  },
  {
    name: "Module",
    path: "attributes.module",
    description: "Monitor module",
  },
  {
    name: "State",
    path: "attributes.state",
    description: "Monitor state",
  },
  {
    name: "Servers",
    path: "relationships.servers.data[].id",
    description: "The servers that this monitor monitors",
  },
  {
    name: "Parameters",
    path: "attributes.parameters",
    description: "Monitor parameters",
  },
  {
    name: "Monitor Diagnostics",
    path: "attributes.monitor_diagnostics",
    description: "Diagnostics provided by the monitor module",
  },
];

const session_fields = [
  {
    name: "Id",
    path: "id",
    description: "Session ID",
  },
  {
    name: "Service",
    path: "relationships.services.data[].id",
    description: "The service where the session connected",
  },
  {
    name: "State",
    path: "attributes.state",
    description: "Session state",
  },
  {
    name: "User",
    path: "attributes.user",
    description: "Username",
  },
  {
    name: "Host",
    path: "attributes.remote",
    description: "Client host address",
  },
  {
    name: "Port",
    path: "attributes.port",
    description: "Client network port",
  },
  {
    name: "Database",
    path: "attributes.database",
    description: "Current default database of the connection",
  },
  {
    name: "Connected",
    path: "attributes.connected",
    description: "Time when the session started",
    formatter: dateToLocaleString,
  },
  {
    name: "Idle",
    path: "attributes.idle",
    description: "How long the session has been idle, in seconds",
  },
  {
    name: "Parameters",
    path: "attributes.parameters",
    description: "Session parameters",
  },
  {
    name: "Client TLS Cipher",
    path: "attributes.client.cipher",
    description: "Client TLS cipher",
  },
  {
    name: "Connection attributes",
    path: "attributes.client.connection_attributes",
    description: "Client connection attributes",
  },
  {
    name: "Connections",
    path: "attributes.connections[].server",
    description: "Ordered list of backend connections",
  },
  {
    name: "Connection IDs",
    path: "attributes.connections[].connection_id",
    description: "Thread IDs for the backend connections",
  },
  {
    name: "Queries",
    path: "attributes.queries[].statement",
    description: "Query history",
  },
  {
    name: "Log",
    path: "attributes.log",
    description: "Per-session log messages",
  },
  {
    name: "Memory",
    path: "attributes.memory",
    description: "Memory usage (not exhaustive)"
  },
  {
    name: "I/O Activity",
    path: "attributes.io_activity",
    description: "Number indicating I/O activity"
  }
];

const filter_fields = [
  {
    name: "Filter",
    path: "id",
    description: "Filter name",
  },
  {
    name: "Source",
    path: "attributes.source.file",
    description: "File where the object is stored in",
  },
  {
    name: "Module",
    path: "attributes.module",
    description: "The module that the filter uses",
  },
  {
    name: "Services",
    path: "relationships.services.data[].id",
    description: "Services that use the filter",
  },
  {
    name: "Parameters",
    path: "attributes.parameters",
    description: "Filter parameters",
  },
  {
    name: "Diagnostics",
    path: "attributes.filter_diagnostics",
    description: "Filter diagnostics",
  },
];

const listener_fields = [
  {
    name: "Name",
    path: "id",
    description: "Listener name",
  },
  {
    name: "Source",
    path: "attributes.source.file",
    description: "File where the object is stored in",
  },
  {
    name: "Service",
    path: "relationships.services.data[].id",
    description: "Services that the listener points to",
  },
  {
    name: "Parameters",
    path: "attributes.parameters",
    description: "Listener parameters",
  },
];

const module_fields = [
  {
    name: "Module",
    path: "id",
    description: "Module name",
  },
  {
    name: "Type",
    path: "attributes.module_type",
    description: "Module type",
  },
  {
    name: "Version",
    path: "attributes.version",
    description: "Module version",
  },
  {
    name: "Maturity",
    path: "attributes.maturity",
    description: "Module maturity",
  },
  {
    name: "Description",
    path: "attributes.description",
    description: "Short description about the module",
  },
  {
    name: "Parameters",
    path: "attributes.parameters",
    description: "All the parameters that the module accepts",
  },
  {
    name: "Commands",
    path: "attributes.commands",
    description: "Commands that the module provides",
  },
];

const thread_fields = [
  {
    name: "Id",
    path: "id",
    description: "Thread ID",
  },
  {
    name: "State",
    path: "attributes.stats.state",
    description: "The state of the thread",
    summary: "N/A"
  },
  {
    name: "Listening",
    path: "attributes.stats.listening",
    description: "Whether the thread is listening for new connections",
    summary: "N/A"
  },
  {
    name: "Accepts",
    path: "attributes.stats.accepts",
    description: "Number of TCP accepts done by this thread",
  },
  {
    name: "Reads",
    path: "attributes.stats.reads",
    description: "Number of EPOLLIN events",
  },
  {
    name: "Writes",
    path: "attributes.stats.writes",
    description: "Number of EPOLLOUT events",
  },
  {
    name: "Hangups",
    path: "attributes.stats.hangups",
    description: "Number of EPOLLHUP and EPOLLRDUP events",
  },
  {
    name: "Errors",
    path: "attributes.stats.errors",
    description: "Number of EPOLLERR events",
  },
  {
    name: "Avg event queue length",
    path: "attributes.stats.avg_event_queue_length",
    description: "Average number of events returned by one epoll_wait call",
    summary: "avg",
  },
  {
    name: "Max event queue length",
    path: "attributes.stats.max_event_queue_length",
    description: "Maximum number of events returned by one epoll_wait call",
    summary: "max",
  },
  {
    name: "Max exec time",
    path: "attributes.stats.max_exec_time",
    description: "The longest time spent processing events returned by a epoll_wait call",
    summary: "max",
  },
  {
    name: "Max queue time",
    path: "attributes.stats.max_queue_time",
    description: "The longest time an event had to wait before it was processed",
    summary: "max",
  },
  {
    name: "Current FDs",
    path: "attributes.stats.current_descriptors",
    description: "Current number of managed file descriptors",
  },
  {
    name: "Total FDs",
    path: "attributes.stats.total_descriptors",
    description: "Total number of managed file descriptors",
  },
  {
    name: "Load (1s)",
    path: "attributes.stats.load.last_second",
    description: "Load percentage over the last second",
    summary: "avg",
  },
  {
    name: "Load (1m)",
    path: "attributes.stats.load.last_minute",
    description: "Load percentage over the last minute",
    summary: "avg",
  },
  {
    name: "Load (1h)",
    path: "attributes.stats.load.last_hour",
    description: "Load percentage over the last hour",
    summary: "avg",
  },
  {
    name: "QC cache size",
    path: "attributes.stats.query_classifier_cache.size",
    description: "Query classifier size",
  },
  {
    name: "QC cache inserts",
    path: "attributes.stats.query_classifier_cache.inserts",
    description: "Number of times a new query was added into the query classification cache",
  },
  {
    name: "QC cache hits",
    path: "attributes.stats.query_classifier_cache.hits",
    description: "How many times a query classification was found in the query classification cache",
  },
  {
    name: "QC cache misses",
    path: "attributes.stats.query_classifier_cache.misses",
    description: "How many times a query classification was not found in the query classification cache",
  },
  {
    name: "QC cache evictions",
    path: "attributes.stats.query_classifier_cache.evictions",
    description:
      "How many times a query classification result was evicted from the query classification cache",
  },
  {
    name: "Sessions",
    path: "attributes.stats.sessions",
    description: "The current number of sessions",
  },
  {
    name: "Zombies",
    path: "attributes.stats.zombies",
    description: "The current number of zombie connections, waiting to be discarded",
  },
  {
    name: "Memory",
    path: "attributes.stats.memory.total",
    description: "The current (partial) memory usage",
  },
];

const show_maxscale_fields = [
  {
    name: "Version",
    path: "attributes.version",
    description: "MaxScale version",
  },
  {
    name: "Commit",
    path: "attributes.commit",
    description: "MaxScale commit ID",
  },
  {
    name: "Started At",
    path: "attributes.started_at",
    description: "Time when MaxScale was started",
  },
  {
    name: "Activated At",
    path: "attributes.activated_at",
    description: "Time when MaxScale left passive mode",
  },
  {
    name: "Uptime",
    path: "attributes.uptime",
    description: "Time MaxScale has been running",
  },
  {
    name: "Config Sync",
    path: "attributes.config_sync",
    description: "MaxScale configuration synchronization",
  },
  {
    name: "Parameters",
    path: "attributes.parameters",
    description: "Global MaxScale parameters",
  },
  {
    name: "System",
    path: "attributes.system",
    description: "System Information",
  }
];

const show_logging_fields = [
  {
    name: "Current Log File",
    path: "attributes.log_file",
    description: "The current log file MaxScale is logging into",
  },
  {
    name: "Enabled Log Levels",
    path: "attributes.log_priorities",
    description: "List of log levels enabled in MaxScale",
  },
  {
    name: "Parameters",
    path: "attributes.parameters",
    description: "Logging parameters",
  },
];

const show_commands_fields = [
  {
    name: "Command",
    path: "id",
    description: "Command name",
  },
  {
    name: "Parameters",
    path: "attributes.parameters[].type",
    description: "Parameters the command supports",
  },
  {
    name: "Descriptions",
    path: "attributes.parameters[].description",
    description: "Parameter descriptions",
  },
];

const qc_cache_fields = [
  {
    name: "Statement",
    path: "id",
    description: "The canonical form of the SQL statement",
  },
  {
    name: "Hits",
    path: "attributes.hits",
    description: "Number of times cache entry has been used",
  },
  {
    name: "Type",
    path: "attributes.classification.type_mask",
    description: "Query type",
  },
  {
    name: "Size",
    path: "attributes.classification.size",
    description: "Size of cache entry"
  }
];

// Note: These are only used as the headings for the table and to generate the field descriptions. This is why the "path" field is not defined.
const show_dbusers_fields = [
  {
    name: "User",
    description: "The user name of the account",
  },
  {
    name: "Host",
    description: "The host of the account",
  },
  {
    name: "Plugin",
    description: "Authentication plugin",
  },
  {
    name: "TLS",
    description: "Whether TLS is required from this user",
  },
  {
    name: "Super",
    description: "Does the user have a SUPER grant",
  },
  {
    name: "Global",
    description: "Does the user have global database access",
  },
  {
    name: "Proxy",
    description: "Whether this is a proxy user",
  },
  {
    name: "Role",
    description: "The default role for this user",
  },
];

exports.command = "show <command>";
exports.desc = "Show objects";
exports.handler = function () {};
exports.builder = function (yargs) {
  yargs
    .command(
      "server <server>",
      "Show server",
      function (yargs) {
        return yargs
          .epilog(
            "Show detailed information about a server. The `Parameters` " +
              "field contains the currently configured parameters for this " +
              "server. See `--help alter server` for more details about altering " +
              "server parameters." +
              fieldDescriptions(server_fields)
          )
          .wrap(null)
          .usage("Usage: show server <server>");
      },
      function (argv) {
        maxctrl(argv, function (host) {
          return getJson(host, "servers/" + argv.server).then((res) => {
            if (res.data.attributes.state_details) {
              res.data.attributes.state += ", " + res.data.attributes.state_details;
            }

            return formatResource(server_fields, res.data);
          });
        });
      }
    )
    .command(
      "servers",
      "Show all servers",
      function (yargs) {
        return yargs
          .epilog("Show detailed information about all servers." + fieldDescriptions(server_fields))
          .wrap(null)
          .usage("Usage: show servers");
      },
      function (argv) {
        maxctrl(argv, function (host) {
          return getJson(host, "servers").then((res) => {
            for (let s of res.data) {
              if (s.attributes.state_details) {
                s.attributes.state += ", " + s.attributes.state_details;
              }
            }
            return res.data.map((i) => formatResource(server_fields, i)).join("\n");
          });
        });
      }
    )
    .command(
      "service <service>",
      "Show service",
      function (yargs) {
        return yargs
          .epilog(
            "Show detailed information about a service. The `Parameters` " +
              "field contains the currently configured parameters for this " +
              "service. See `--help alter service` for more details about altering " +
              "service parameters." +
              fieldDescriptions(service_fields)
          )
          .wrap(null)
          .usage("Usage: show service <service>");
      },
      function (argv) {
        maxctrl(argv, function (host) {
          return getResource(host, "services/" + argv.service, service_fields);
        });
      }
    )
    .command(
      "services",
      "Show all services",
      function (yargs) {
        return yargs
          .epilog("Show detailed information about all services." + fieldDescriptions(service_fields))
          .wrap(null)
          .usage("Usage: show services");
      },
      function (argv) {
        maxctrl(argv, function (host) {
          return getCollectionAsResource(host, "services/", service_fields);
        });
      }
    )
    .command(
      "monitor <monitor>",
      "Show monitor",
      function (yargs) {
        return yargs
          .epilog(
            "Show detailed information about a monitor. The `Parameters` " +
              "field contains the currently configured parameters for this " +
              "monitor. See `--help alter monitor` for more details about altering " +
              "monitor parameters." +
              fieldDescriptions(monitor_fields)
          )
          .wrap(null)
          .usage("Usage: show monitor <monitor>");
      },
      function (argv) {
        maxctrl(argv, function (host) {
          return getResource(host, "monitors/" + argv.monitor, monitor_fields);
        });
      }
    )
    .command(
      "monitors",
      "Show all monitors",
      function (yargs) {
        return yargs
          .epilog("Show detailed information about all monitors." + fieldDescriptions(monitor_fields))
          .wrap(null)
          .usage("Usage: show monitors");
      },
      function (argv) {
        maxctrl(argv, function (host) {
          return getCollectionAsResource(host, "monitors/", monitor_fields);
        });
      }
    )
    .command(
      "session <session>",
      "Show session",
      function (yargs) {
        return yargs
          .epilog(
            "Show detailed information about a single session. " +
              "The list of sessions can be retrieved with the " +
              "`list sessions` command. The <session> is the session " +
              "ID of a particular session.\n\n" +
              "The `Connections` field lists the servers to which " +
              "the session is connected and the `Connection IDs` " +
              "field lists the IDs for those connections." +
              fieldDescriptions(session_fields)
          )
          .wrap(null)
          .usage("Usage: show session <session>")
          .group([rDnsOption.shortname], "Options:")
          .option(rDnsOption.shortname, rDnsOption.definition);
      },
      function (argv) {
        maxctrl(argv, function (host) {
          var resource = "sessions/" + argv.session;
          if (argv[rDnsOption.shortname]) {
            resource += "?" + rDnsOption.optionOn;
          }
          return getResource(host, resource, session_fields);
        });
      }
    )
    .command(
      "sessions",
      "Show all sessions",
      function (yargs) {
        return yargs
          .epilog(
            "Show detailed information about all sessions. " +
              "See `--help show session` for more details." +
              fieldDescriptions(session_fields)
          )
          .wrap(null)
          .usage("Usage: show sessions")
          .group([rDnsOption.shortname], "Options:")
          .option(rDnsOption.shortname, rDnsOption.definition);
      },
      function (argv) {
        maxctrl(argv, function (host) {
          var resource = "sessions/";
          if (argv[rDnsOption.shortname]) {
            resource += "?" + rDnsOption.optionOn;
          }
          return getCollectionAsResource(host, resource, session_fields);
        });
      }
    )
    .command(
      "filter <filter>",
      "Show filter",
      function (yargs) {
        return yargs
          .epilog(
            "The list of services that use this filter is show in the `Services` field." +
              fieldDescriptions(filter_fields)
          )
          .wrap(null)
          .usage("Usage: show filter <filter>");
      },
      function (argv) {
        maxctrl(argv, function (host) {
          return getResource(host, "filters/" + argv.filter, filter_fields);
        });
      }
    )
    .command(
      "filters",
      "Show all filters",
      function (yargs) {
        return yargs
          .epilog("Show detailed information of all filters." + fieldDescriptions(filter_fields))
          .wrap(null)
          .usage("Usage: show filters");
      },
      function (argv) {
        maxctrl(argv, function (host) {
          return getCollectionAsResource(host, "filters/", filter_fields);
        });
      }
    )
    .command(
      "listener <listener>",
      "Show listener",
      function (yargs) {
        return yargs
          .epilog(fieldDescriptions(listener_fields))
          .wrap(null)
          .usage("Usage: show listener <listener>");
      },
      function (argv) {
        maxctrl(argv, function (host) {
          return getResource(host, "listeners/" + argv.listener, listener_fields);
        });
      }
    )
    .command(
      "listeners",
      "Show all listeners",
      function (yargs) {
        return yargs
          .epilog("Show detailed information of all filters." + fieldDescriptions(filter_fields))
          .wrap(null)
          .usage("Usage: show filters");
      },
      function (argv) {
        maxctrl(argv, function (host) {
          return getCollectionAsResource(host, "listeners/", listener_fields);
        });
      }
    )
    .command(
      "module <module>",
      "Show loaded module",
      function (yargs) {
        return yargs
          .epilog(
            "This command shows all available parameters as well as " +
              "detailed version information of a loaded module." +
              fieldDescriptions(module_fields)
          )
          .wrap(null)
          .usage("Usage: show module <module>");
      },
      function (argv) {
        maxctrl(argv, function (host) {
          return getResource(host, "maxscale/modules/" + argv.module, module_fields);
        });
      }
    )
    .command(
      "modules",
      "Show all loaded modules",
      function (yargs) {
        return yargs
          .option("load-all", {
            describe: "Load all included modules before showing them",
            type: "boolean",
          })
          .epilog("Displays detailed information about all modules." + fieldDescriptions(module_fields))
          .wrap(null)
          .usage("Usage: show modules");
      },
      function (argv) {
        maxctrl(argv, function (host) {
          return getCollectionAsResource(host, "maxscale/modules/" + (argv["load-all"] ? "?load=all" : ""), module_fields);
        });
      }
    )
    .command(
      "maxscale",
      "Show MaxScale information",
      function (yargs) {
        return yargs
          .epilog(
            "See `--help alter maxscale` for more details about altering " +
              "MaxScale parameters." +
              fieldDescriptions(show_maxscale_fields)
          )
          .wrap(null)
          .usage("Usage: show maxscale");
      },
      function (argv) {
        maxctrl(argv, function (host) {
          return getResource(host, "maxscale", show_maxscale_fields);
        });
      }
    )
    .command(
      "thread <thread>",
      "Show thread",
      function (yargs) {
        return yargs
          .epilog("Show detailed information about a worker thread." + fieldDescriptions(thread_fields))
          .wrap(null)
          .usage("Usage: show thread <thread>");
      },
      function (argv) {
        maxctrl(argv, function (host) {
          return getResource(host, "maxscale/threads/" + argv.thread, thread_fields);
        });
      }
    )
    .command(
      "threads",
      "Show all threads",
      function (yargs) {
        return yargs
          .epilog("Show detailed information about all worker threads." + fieldDescriptions(thread_fields))
          .wrap(null)
          .usage("Usage: show threads");
      },
      function (argv) {
        maxctrl(argv, function (host) {
          return getTransposedCollection(host, "maxscale/threads", thread_fields);
        });
      }
    )
    .command(
      "logging",
      "Show MaxScale logging information",
      function (yargs) {
        return yargs
          .epilog(
            "See `--help alter logging` for more details about altering " +
              "logging parameters." +
              fieldDescriptions(show_logging_fields)
          )
          .wrap(null)
          .usage("Usage: show logging");
      },
      function (argv) {
        maxctrl(argv, function (host) {
          return getResource(host, "maxscale/logs", show_logging_fields);
        });
      }
    )
    .command(
      "commands <module>",
      "Show module commands of a module",
      function (yargs) {
        return yargs
          .epilog(
            "This command shows the parameters the command expects with " +
              "the parameter descriptions." +
              fieldDescriptions(show_commands_fields)
          )
          .wrap(null)
          .usage("Usage: show commands <module>");
      },
      function (argv) {
        maxctrl(argv, function (host) {
          return getSubCollection(
            host,
            "maxscale/modules/" + argv.module,
            "attributes.commands",
            show_commands_fields
          );
        });
      }
    )
    .command(
      "qc_cache",
      "Show query classifier cache",
      function (yargs) {
        return yargs
          .epilog("Show contents (statement and hits) of query classifier cache.")
          .wrap(null)
          .usage("Usage: show qc_cache")
          .group(["top"], "QC Cache Options")
          .option("top", {
            describe: "Show top N most used cache entries",
            type: "number",
            default: 20,
          });
      },
      function (argv) {
        maxctrl(argv, function (host) {
          return getCollection(host, "maxscale/query_classifier/cache?top=" + argv.top, qc_cache_fields);
        });
      }
    )
    .command(
      "dbusers <service>",
      "Show database users of the service",
      function (yargs) {
        return yargs
          .epilog(
            "Show information about the database users of the service." +
              fieldDescriptions(show_dbusers_fields)
          )
          .wrap(null)
          .usage("Usage: show dbusers <service>");
      },
      function (argv) {
        maxctrl(argv, async function (host) {
          var res = await getJson(host, "services/" + argv.service);

          var table = [];

          if (res.data.attributes.users) {
            for (var user of res.data.attributes.users) {
              table.push([
                user.user,
                user.host,
                user.plugin,
                user.ssl,
                user.super_priv,
                user.global_priv,
                user.proxy_priv,
                user.default_role,
              ]);
            }
          }

          return rawCollectionAsTable(table, show_dbusers_fields);
        });
      }
    )
    .wrap(null)
    .usage("Usage: show <command>")
    .help()
    .wrap(null)
    .demandCommand(1, helpMsg);
};
