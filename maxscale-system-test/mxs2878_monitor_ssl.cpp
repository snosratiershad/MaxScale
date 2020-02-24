/**
 * Covers the following bugs:
 * MXS-2878: Monitor connections do not insist on SSL being used
 * MXS-2896: Server wrongly in Running state after failure to connect
 */

#include "testconnections.h"
#include <sstream>

std::string join(StringSet st)
{
    std::ostringstream ss;

    for (const auto& a : st)
    {
        ss << a << " ";
    }

    return ss.str();
}

int main(int argc, char** argv)
{
    TestConnections test(argc, argv);

    for (auto srv : {"server1", "server2", "server3", "server4"})
    {
        StringSet expected = {"Down"};
        auto status = test.maxscales->get_server_status(srv);
        test.expect(status == expected,
                    "Expected '%s' but got '%s'", join(expected).c_str(), join(status).c_str());
    }

    return test.global_result;
}
