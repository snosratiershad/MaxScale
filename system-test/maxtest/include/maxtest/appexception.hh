/*
 * Copyright (c) 2018 MariaDB Corporation Ab
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

#pragma once

#include <stdexcept>
#include <sstream>

namespace base
{

// TODO Add back trace.
class AppException : public std::runtime_error
{
public:
    AppException(const std::string& msg,
                 const std::string& file,
                 int line)
        : std::runtime_error(msg)
        , m_file(file)
    {
    }
private:
    std::string m_file;
};
}   // base

#define DEFINE_EXCEPTION(Type) \
    struct Type : public base::AppException { \
        Type(const std::string& msg, \
             const char* file, \
             int line)   \
            : AppException(msg, file, line) {} }

#define THROW(Type, msg_str) \
    do { \
        std::ostringstream exception_msg_ss; \
        exception_msg_ss << __FILE__ << ':' << __LINE__ << '\n' << msg_str; \
        throw Type(exception_msg_ss.str(), __FILE__, __LINE__);} while (false)
