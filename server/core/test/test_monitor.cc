/*
 * Copyright (c) 2016 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2026-07-11
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

#include <tuple>
#include <iostream>
#include <vector>
#include <cstdint>
#include <string>

#include <maxscale/monitor.hh>

#define SERVER_DOWN 0

#include "test_monitor_cases.hh"

// Set this to 1 to print the source code for test_monitor_cases.hh into stdout
#define RECORD_TEST 0

std::vector<uint64_t> states = {
    SERVER_RUNNING, SERVER_MAINT, SERVER_MASTER, SERVER_SLAVE, SERVER_JOINED, SERVER_RELAY, SERVER_BLR
};

std::string state_to_str(uint64_t state)
{
    std::vector<std::string> names;

    std::map<uint64_t, std::string> state_names = {
        {SERVER_RUNNING, "SERVER_RUNNING"},
        {SERVER_MAINT,   "SERVER_MAINT"  },
        {SERVER_MASTER,  "SERVER_MASTER" },
        {SERVER_SLAVE,   "SERVER_SLAVE"  },
        {SERVER_JOINED,  "SERVER_JOINED" },
        {SERVER_RELAY,   "SERVER_RELAY"  },
        {SERVER_BLR,     "SERVER_BLR"    }
    };

    if (state == 0)
    {
        names.push_back("SERVER_DOWN");
    }
    else
    {
        for (auto s : states)
        {
            if (state & s)
            {
                names.push_back(state_names[s]);
            }
        }
    }

    return mxb::join(names, "|");
}

std::string event_to_str(mxs_monitor_event_t event)
{
    std::map<mxs_monitor_event_t, std::string> event_names {
        {UNDEFINED_EVENT, "UNDEFINED_EVENT"},
        {MASTER_DOWN_EVENT, "MASTER_DOWN_EVENT"},
        {MASTER_UP_EVENT, "MASTER_UP_EVENT"},
        {SLAVE_DOWN_EVENT, "SLAVE_DOWN_EVENT"},
        {SLAVE_UP_EVENT, "SLAVE_UP_EVENT"},
        {SERVER_DOWN_EVENT, "SERVER_DOWN_EVENT"},
        {SERVER_UP_EVENT, "SERVER_UP_EVENT"},
        {SYNCED_DOWN_EVENT, "SYNCED_DOWN_EVENT"},
        {SYNCED_UP_EVENT, "SYNCED_UP_EVENT"},
        {DONOR_DOWN_EVENT, "DONOR_DOWN_EVENT"},
        {DONOR_UP_EVENT, "DONOR_UP_EVENT"},
        {LOST_MASTER_EVENT, "LOST_MASTER_EVENT"},
        {LOST_SLAVE_EVENT, "LOST_SLAVE_EVENT"},
        {LOST_SYNCED_EVENT, "LOST_SYNCED_EVENT"},
        {LOST_DONOR_EVENT, "LOST_DONOR_EVENT"},
        {NEW_MASTER_EVENT, "NEW_MASTER_EVENT"},
        {NEW_SLAVE_EVENT, "NEW_SLAVE_EVENT"},
        {NEW_SYNCED_EVENT, "NEW_SYNCED_EVENT"},
        {NEW_DONOR_EVENT, "NEW_DONOR_EVENT"},
        {RELAY_UP_EVENT, "RELAY_UP_EVENT"},
        {RELAY_DOWN_EVENT, "RELAY_DOWN_EVENT"},
        {LOST_RELAY_EVENT, "LOST_RELAY_EVENT"},
        {NEW_RELAY_EVENT, "NEW_RELAY_EVENT"},
        {BLR_UP_EVENT, "BLR_UP_EVENT"},
        {BLR_DOWN_EVENT, "BLR_DOWN_EVENT"},
        {LOST_BLR_EVENT, "LOST_BLR_EVENT"},
        {NEW_BLR_EVENT, "NEW_BLR_EVENT"}
    };

    return event_names[event];
}

uint64_t get_state(int num)
{
    uint64_t rval = 0;
    size_t offset = 0;

    while (num && offset < states.size())
    {
        if (num & 0x1)
        {
            rval |= states[offset];
        }

        ++offset;
        num >>= 1;
    }

    return rval;
}

void generate_cases()
{
    std::cout <<
        R"(
#include <vector>
#include <tuple>

#include <maxscale/monitor.hh>

//
// Do not edit this file manually, just format it with the code formatter.
//

std::vector<std::tuple<uint64_t, uint64_t, mxs_monitor_event_t>> test_monitor_test_cases = {
)";

    for (int i = 0; i < 1 << states.size(); i++)
    {
        for (int j = 0; j < 1 << states.size(); j++)
        {
            uint64_t before = get_state(i);
            uint64_t after = get_state(j);

            const auto check = [](uint64_t before, uint64_t after, uint64_t mask){
                    return (before & mask) == mask || (after & mask) == mask;
                };

            // Check for nonsensical states
            if (check(before, after, SERVER_MASTER | SERVER_SLAVE)
                || check(before, after, SERVER_MASTER | SERVER_BLR)
                || check(before, after, SERVER_RELAY | SERVER_BLR)
                || check(before, after, SERVER_JOINED | SERVER_BLR)
                || check(before, after, SERVER_JOINED | SERVER_RELAY)
                || check(before, after, SERVER_SLAVE | SERVER_BLR)
                // Skip states that are essentially "Down" but have some other bits set.
                || ((before & SERVER_RUNNING) == 0 && before)
                || ((after & SERVER_RUNNING) == 0 && after))
            {
                continue;
            }

            if (mxs::MonitorServer::status_changed(before, after))
            {
                auto res = mxs::MonitorServer::event_type(before, after);

                if (res != UNDEFINED_EVENT)
                {
                    std::cout << "{\n"
                              << state_to_str(before) << ",\n"
                              << state_to_str(after) << ",\n"
                              << event_to_str(res) << "\n"
                              << "},\n";
                }
            }
        }
    }

    std::cout << R"(
};
)";
}

int main(int argc, char** argv)
{
    if (RECORD_TEST)
    {
        generate_cases();
        return 0;
    }

    int error = 0;

    for (auto test : test_monitor_test_cases)
    {
        uint64_t before = std::get<0>(test);
        uint64_t after = std::get<1>(test);
        mxs_monitor_event_t expected = std::get<2>(test);
        mxs_monitor_event_t res = mxs::MonitorServer::event_type(before, after);

        if (res != expected)
        {
            error = 1;

            std::cout << "[" << mxs::Target::status_to_string(before, 0) << "] -> "
                      << "[" << mxs::Target::status_to_string(after, 0) << "]"
                      << " ERROR: Expected " << mxs::Monitor::get_event_name(expected)
                      << ", got " << mxs::Monitor::get_event_name(res) << "\n";
        }
    }

    return error;
}
