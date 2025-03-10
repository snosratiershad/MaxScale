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

#include <maxbase/ccdefs.hh>

#include <string>
#include <mutex>
#include <memory>

#include <unistd.h>

namespace maxbase
{

// Minimal logger interface
class Logger
{
public:
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    virtual ~Logger()
    {
    }

    /**
     * Set the identification, used in log header and footer.
     * If not specified, the program name will be used.
     *
     * @param ident  The identifying string.
     *
     * @attention The identification should be specified before the
     *            log is initialized.
     */
    static void set_ident(const std::string& ident);

    /**
     * Write a message to the log
     *
     * @param msg Message to write
     * @param len Length of message
     *
     * @return True on success
     */
    virtual bool write(const char* msg, int len) = 0;

    /**
     * Rotate the logfile
     *
     * @return True if the log was rotated
     */
    virtual bool rotate() = 0;

    /**
     * Get the name of the log file
     *
     * @return The name of the log file
     */
    const char* filename() const
    {
        return m_filename.c_str();
    }

protected:
    Logger(const std::string& filename)
        : m_filename(filename)
    {
    }

    std::string m_filename;
};

class FileLogger : public Logger
{
public:
    FileLogger(const FileLogger&) = delete;
    FileLogger& operator=(const FileLogger&) = delete;

    /**
     * Create a new logger that writes to a file
     *
     * @param logdir Log file to open
     *
     * @return New logger instance or an empty unique_ptr on error
     */
    static std::unique_ptr<Logger> create(const std::string& filename);

    /**
     * Close the log
     *
     * A footer is written to the log and the file is closed.
     */
    ~FileLogger();

    /**
     * Write a message to the log
     *
     * @param msg Message to write
     * @param len Length of message
     *
     * @return True on success
     */
    bool write(const char* msg, int len) override;

    /**
     * Rotate the logfile by reopening it
     *
     * @return True if the log was rotated. False if the opening of the new file
     *         descriptor failed in which case the old file descriptor will be used.
     */
    bool rotate() override;

private:
    int        m_fd;
    std::mutex m_lock;

    FileLogger(int fd, const std::string& filename);
    bool write_header();
    bool write_footer(const char* suffix);
    void close(const char* msg);
};

class FDLogger : public Logger
{
public:
    FDLogger(const FDLogger&) = delete;
    FDLogger& operator=(const FDLogger&) = delete;

    /**
     * Create a new logger that writes to stdout
     *
     * @param logdir Log file to open, has no functional effect on this logger
     * @param fd     The file descriptor to write to, e.g. STDOUT_FILENO.
     *
     * @return New logger instance or an empty unique_ptr on error
     */
    static std::unique_ptr<Logger> create(const std::string& filename, int fd)
    {
        return std::unique_ptr<Logger>(new FDLogger(filename, fd));
    }

    /**
     * Write a message to stdout
     *
     * @param msg Message to write
     * @param len Length of message
     *
     * @return True on success
     */
    bool write(const char* msg, int len) override
    {
        return ::write(m_fd, msg, len) != -1;
    }

    /**
     * Rotate the "logfile"
     *
     * @return Always true
     */
    bool rotate() override
    {
        return true;
    }

private:
    FDLogger(const std::string& filename, int fd)
        : Logger(filename)
        , m_fd(fd)
    {
    }

    int m_fd;
};
}
