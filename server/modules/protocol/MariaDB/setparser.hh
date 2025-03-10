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

#include <maxscale/ccdefs.hh>
#include <vector>
#include <maxscale/protocol/mariadb/customparser.hh>
#include <maxscale/protocol/mariadb/mysql.hh>


class SetParser : public maxscale::CustomParser
{
public:
    enum status_t
    {
        ERROR,          // Some fatal error occurred; mem alloc failed, parsing failed, etc.
        IS_SET_SQL_MODE,// The COM_QUERY is "set [GLOBAL|SESSION] sql_mode=..."
        IS_SET_MAXSCALE,// The COM_QUERY is "set @MAXSCALE..."
        NOT_RELEVANT    // Neither of the above.
    };

    enum
    {
        UNUSED_FIRST = 0xFF,
        TK_GLOBAL,
        TK_GLOBAL_VAR,
        TK_SESSION,
        TK_SESSION_VAR,
        TK_SET,
        TK_SQL_MODE,
        TK_MAXSCALE_VAR
    };

    SetParser()
    {
    }

    class Result
    {
    public:
        typedef std::pair<const char*, const char*> Item;
        typedef std::vector<Item>                   Items;

        Result()
        {
        }

        const Items& variables() const
        {
            return m_variables;
        }
        const Items& values() const
        {
            return m_values;
        }

        void add_variable(const char* begin, const char* end)
        {
            m_variables.push_back(Item(begin, end));
        }

        void add_value(const char* begin, const char* end)
        {
            m_values.push_back(Item(begin, end));
        }

    private:
        std::vector<Item> m_variables;
        std::vector<Item> m_values;
    };

    /**
     * Return whether the statement is a "SET SQL_MODE=" statement and if so,
     * whether the state is ORACLE, DEFAULT or something else.
     *
     * @param ppBuffer   Address of pointer to buffer containing statement.
     *                   The GWBUF must contain a complete statement, but the
     *                   buffer need not be contiguous.
     * @param pSql_mode  Pointer to variable receiving the sql_mode state, if
     *                   the statement is a "SET SQL_MODE=" statement.
     *
     * @return ERROR            if a fatal error occurred during parsing
     *         IS_SET_SQL_MODE  if the statement is a "SET SQL_MODE=" statement
     *         NOT_SET_SQL_MODE if the statement is not a "SET SQL_MODE="
     *                          statement
     */
    status_t check(std::string_view sql, Result* pResult)
    {
        status_t rv = NOT_RELEVANT;

        // sizeof(command_byte) + MIN(strlen("SET maxscale"), strlen("SET sql_mode=ORACLE"))
        if (sql.length() >= 13)
        {
            const char* pStmt = sql.data();

            if (is_alpha(*pStmt))
            {
                // First character is alphabetic, we can check whether it is "SET".
                if (is_set(pStmt))
                {
                    // It is, so we must parse further.
                    initialize(sql);
                    rv = parse(pResult);
                }
            }
            else
            {
                // If the first character is not an alphabetic character we assume there
                // is a comment.
                initialize(sql);
                bypass_whitespace();

                if (m_pEnd - m_pI > 3 && is_set(m_pI))
                {
                    rv = parse(pResult);
                }
            }
        }

        return rv;
    }

    /**
     * Returns a @c status_t as a string.
     *
     * @param status_t  A result.
     *
     * @return The corresponding string.
     */
    static const char* to_string(status_t result)
    {
        switch (result)
        {
        case ERROR:
            return "ERROR";

        case IS_SET_SQL_MODE:
            return "IS_SET_SQL_MODE";

        case IS_SET_MAXSCALE:
            return "IS_SET_MAXSCALE";

        case NOT_RELEVANT:
            return "NOT_RELEVANT";

        default:
            mxb_assert(!true);
            return "UNKNOWN";
        }
    }

private:
    static bool is_set(const char* pStmt)
    {
        return (pStmt[0] == 's' || pStmt[0] == 'S')
               && (pStmt[1] == 'e' || pStmt[1] == 'E')
               && (pStmt[2] == 't' || pStmt[2] == 'T');
    }

    static bool is_set(const uint8_t* pStmt)
    {
        return is_set(reinterpret_cast<const char*>(pStmt));
    }

    static bool is_error(status_t rv)
    {
        return rv == ERROR;
    }

    void initialize(std::string_view sql)
    {
        status_t rv = ERROR;

        m_pSql = sql.data();
        m_pI = m_pSql;
        m_pEnd = m_pI + sql.length();
    }

    bool consume_id()
    {
        // Consumes "([a-zA-Z]([\.a-zA-Z0-9_])+)*"

        bool rv = false;

        if (is_alpha(*m_pI))
        {
            rv = true;

            ++m_pI;

            while ((m_pI < m_pEnd)
                   && (is_alpha(*m_pI) || is_number(*m_pI) || (*m_pI == '.') || (*m_pI == '_')))
            {
                ++m_pI;
            }
        }

        return rv;
    }

    void consume_value(const char** ppBegin, const char** ppEnd)
    {
        // Consumes everything until a ',' outside of a SQL string, or eol is
        // encountered.
        bool rv = false;
        bool consumed = false;
        const char* pBegin = m_pI;
        const char* pEnd = m_pI;

        while ((m_pI < m_pEnd) && (*m_pI != ',') && (*m_pI != ';'))
        {
            switch (*m_pI)
            {
            case '\'':
            case '"':
            case '`':
                {
                    char quote = *m_pI;
                    pBegin = m_pI;
                    ++m_pI;
                    bool done = false;

                    while (m_pI < m_pEnd && !done)
                    {
                        if (*m_pI == '\\')
                        {
                            ++m_pI;             // Backslash escape, skip the next character
                        }
                        else if (*m_pI == quote)
                        {
                            if (m_pI + 1 < m_pEnd && m_pI[1] == quote)
                            {
                                ++m_pI;         // Doubled quote character, skip both characters
                            }
                            else
                            {
                                // Found matching quote, stop parsing
                                break;
                            }
                        }

                        ++m_pI;
                    }


                    if (m_pI < m_pEnd)
                    {
                        mxb_assert(*m_pI == quote);
                        ++m_pI;
                        pEnd = m_pI;
                    }
                }
                break;

            default:
                ++m_pI;
                pEnd = m_pI;
            }

            bypass_whitespace();
        }

        *ppBegin = pBegin;
        *ppEnd = pEnd;
    }

    status_t parse(Result* pResult)
    {
        status_t rv = NOT_RELEVANT;
        token_t token = next_token();

        switch (token)
        {
        case TK_SET:
            rv = parse_set(pResult);
            break;

        case PARSER_EXHAUSTED:
            log_exhausted();
            break;

        case PARSER_UNKNOWN_TOKEN:
        default:
            log_unexpected();
            break;
        }

        return rv;
    }

    status_t parse_set(Result* pResult)
    {
        status_t rv = NOT_RELEVANT;

        char c;

        do
        {
            bypass_whitespace();

            const char* pVariable_begin = m_pI;

            token_t token = next_token();

            switch (token)
            {
            case TK_GLOBAL:
                rv = parse_set(pResult);
                break;

            case TK_SESSION:
                rv = parse_set(pResult);
                break;

            case TK_GLOBAL_VAR:
            case TK_SESSION_VAR:
                if (next_token() == '.')
                {
                    rv = parse_set(pResult);
                }
                else
                {
                    rv = ERROR;
                }
                break;

            case TK_SQL_MODE:
                {
                    const char* pVariable_end = m_pI;

                    if (next_token() == '=')
                    {
                        pResult->add_variable(pVariable_begin, pVariable_end);

                        bypass_whitespace();

                        const char* pValue_begin;
                        const char* pValue_end;

                        consume_value(&pValue_begin, &pValue_end);

                        pResult->add_value(pValue_begin, pValue_end);

                        rv = IS_SET_SQL_MODE;
                    }
                    else
                    {
                        rv = ERROR;
                    }
                }
                break;

            case TK_MAXSCALE_VAR:
                {
                    if (*m_pI == '.')
                    {
                        ++m_pI;
                        consume_id();
                        const char* pVariable_end = m_pI;

                        if (next_token() == '=')
                        {
                            pResult->add_variable(pVariable_begin, pVariable_end);

                            bypass_whitespace();

                            const char* pValue_begin;
                            const char* pValue_end;

                            consume_value(&pValue_begin, &pValue_end);

                            pResult->add_value(pValue_begin, pValue_end);

                            rv = IS_SET_MAXSCALE;
                        }
                        else
                        {
                            rv = ERROR;
                        }
                    }
                    else
                    {
                        rv = ERROR;
                    }
                }
                break;

            case PARSER_EXHAUSTED:
                log_exhausted();
                rv = ERROR;
                break;

            case PARSER_UNKNOWN_TOKEN:
                // Might be something like "SET A=B, C=D, SQL_MODE=ORACLE", so we first consume
                // the identifier and if it is followed by a "=" we consume the value.
                {
                    char ch;
                    if (consume_id())
                    {
                        bypass_whitespace();

                        if (peek_current_char(&ch) && (ch == '='))
                        {
                            ++m_pI;
                            const char* ignored1;
                            const char* ignored2;
                            consume_value(&ignored1, &ignored2);
                        }
                    }
                    else
                    {
                        log_unexpected();
                        rv = ERROR;
                    }
                }
                break;

            default:
                log_unexpected();
                rv = ERROR;
                break;
            }

            c = 0;

            if (rv != ERROR)
            {
                bypass_whitespace();

                if (peek_current_char(&c))
                {
                    if (c == ',')
                    {
                        ++m_pI;
                    }
                    else
                    {
                        c = 0;
                    }
                }
                else
                {
                    c = 0;
                }
            }
        }
        while ((rv != ERROR) && (c == ','));

        return rv;
    }

    token_t next_token(token_required_t required = TOKEN_NOT_REQUIRED)
    {
        token_t token = PARSER_UNKNOWN_TOKEN;

        bypass_whitespace();

        if (m_pI == m_pEnd)
        {
            token = PARSER_EXHAUSTED;
        }
        else if (*m_pI == ';')
        {
            ++m_pI;

            while ((m_pI != m_pEnd) && isspace(*m_pI))
            {
                ++m_pI;
            }

            if (m_pI != m_pEnd)
            {
                MXB_WARNING("Non-space data found after semi-colon: '%.*s'.",
                            (int)(m_pEnd - m_pI),
                            m_pI);
            }

            token = PARSER_EXHAUSTED;
        }
        else
        {
            switch (*m_pI)
            {
            case '@':
                if (is_next_alpha('S', 2))
                {
                    token = expect_token(MXS_CP_EXPECT_TOKEN("@@SESSION"), TK_SESSION_VAR);
                }
                else if (is_next_alpha('G', 2))
                {
                    token = expect_token(MXS_CP_EXPECT_TOKEN("@@GLOBAL"), TK_GLOBAL_VAR);
                }
                else if (is_next_alpha('L', 2))
                {
                    token = expect_token(MXS_CP_EXPECT_TOKEN("@@LOCAL"), TK_SESSION_VAR);
                }
                else if (is_next_alpha('M', 1))
                {
                    token = expect_token(MXS_CP_EXPECT_TOKEN("@MAXSCALE"), TK_MAXSCALE_VAR);
                }
                break;

            case '.':
            case '\'':
            case '"':
            case '`':
            case ',':
            case '=':
                token = *m_pI;
                ++m_pI;
                break;

            case 'g':
            case 'G':
                token = expect_token(MXS_CP_EXPECT_TOKEN("GLOBAL"), TK_GLOBAL);
                break;

            case 'l':
            case 'L':
                token = expect_token(MXS_CP_EXPECT_TOKEN("LOCAL"), TK_SESSION);
                break;

            case 's':
            case 'S':
                if (is_next_alpha('E'))
                {
                    if (is_next_alpha('S', 2))
                    {
                        token = expect_token(MXS_CP_EXPECT_TOKEN("SESSION"), TK_SESSION);
                    }
                    else
                    {
                        token = expect_token(MXS_CP_EXPECT_TOKEN("SET"), TK_SET);
                    }
                }
                else if (is_next_alpha('Q'))
                {
                    token = expect_token(MXS_CP_EXPECT_TOKEN("SQL_MODE"), TK_SQL_MODE);
                }
                break;

            default:
                ;
            }
        }

        if ((token == PARSER_EXHAUSTED) && (required == TOKEN_REQUIRED))
        {
            log_exhausted();
        }

        return token;
    }
};
