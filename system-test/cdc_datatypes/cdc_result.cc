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

#include "cdc_result.h"
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <string>
#include <string.h>

using std::cout;
using std::endl;

TestInput::TestInput(const std::string& value, const std::string& type, const std::string& name)
    : m_value(value)
    , m_type(type)
    , m_name(name)
{
    if (m_value[0] == '"' || m_value[0] == '\'')
    {
        /** Remove quotes from the value */
        m_value = m_value.substr(1, m_value.length() - 2);
    }
}

TestOutput::TestOutput(const std::string& input, const std::string& name)
{
    json_error_t err;
    json_t* js = json_loads(input.c_str(), JSON_ALLOW_NUL, &err);

    if (js)
    {
        json_t* value = json_object_get(js, name.c_str());

        if (value)
        {
            std::stringstream ss;

            if (json_is_string(value))
            {
                if (strlen(json_string_value(value)) == 0)
                {
                    ss << "NULL";
                }
                else
                {
                    ss << json_string_value(value);
                }
            }
            else if (json_is_integer(value))
            {
                ss << json_integer_value(value);
            }
            else if (json_is_null(value))
            {
                ss << "NULL";
            }
            else if (json_is_real(value))
            {
                ss << json_real_value(value);
            }
            else
            {
                cout << "Value '" << name << "' is not a primitive type: " << input << endl;
            }

            m_value = ss.str();
        }
        else
        {
            cout << "Value '" << name << "' not found" << endl;
        }

        json_decref(js);
    }
    else
    {
        cout << "Failed to parse JSON: " << err.text << endl;
    }
}

const std::string& TestOutput::getValue() const
{
    return m_value;
}
