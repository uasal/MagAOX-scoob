/** \file logFileName.cpp
 * \brief Declares and defines the logFileName class
 * \author Jared R. Males (jaredmales@gmail.com)
 *
 * \ingroup logger_files
 *
 */

#include <filesystem>

#include "logFileName.hpp"
#include "../sys/fileTimes.hpp"

namespace MagAOX
{
namespace logger
{

logFileName::logFileName()
{
    return;
}

logFileName::logFileName( const std::string &fn ) : m_fullName{ fn }
{
    parseName();
}

int logFileName::fullName( const std::string &fn )
{
    m_fullName = fn;
    return parseName();
}

logFileName &logFileName::operator=( const std::string &fn )
{
    fullName( fn );

    return *this;
}

std::string logFileName::fullName() const
{
    return m_fullName;
}

std::string logFileName::baseName() const
{
    return m_baseName;
}

std::string logFileName::appName() const
{
    return m_appName;
}

std::string logFileName::subDir() const
{
    return m_subDir;
}

int logFileName::year() const
{
    return m_year;
}

int logFileName::month() const
{
    return m_month;
}

int logFileName::day() const
{
    return m_day;
}

int logFileName::hour() const
{
    return m_hour;
}

int logFileName::minute() const
{
    return m_minute;
}

int logFileName::second() const
{
    return m_second;
}

int logFileName::nsec() const
{
    return m_nsec;
}

flatlogs::timespecX logFileName::timestamp() const
{
    return m_timestamp;
}

std::string logFileName::extension() const
{
    return m_extension;
}

bool logFileName::valid() const
{
    return m_valid;
}

int logFileName::parseName()
{
    try
    {
        std::filesystem::path p( m_fullName );

        m_baseName  = p.filename();
        m_extension = p.extension();
    }
    catch( const std::exception &e )
    {
        std::cerr << "logFileName: filesystem exception: " << e.what() << '\n';
        std::cerr << __FILE__ << ' ' << __LINE__ << '\n';
        m_valid = false;
        return -1;
    }

    if( m_extension == "" )
    {
        std::cerr << "No extension found in: " << m_fullName << "\n";
        std::cerr << __FILE__ << ' ' << __LINE__ << '\n';
        m_valid = false;
        return -1;
    }

    std::string YYYY, MM, DD, hh, mm, ss, nn;

    if( sys::parseFilePath( m_appName, YYYY, MM, DD, hh, mm, ss, nn, m_baseName ) < 0 )
    {
        std::cerr << "Error parsing filename: " << m_fullName << "\n";
        std::cerr << __FILE__ << ' ' << __LINE__ << '\n';
        m_valid = false;
        return -1;
    }

    m_subDir = YYYY + '_' + MM + '_' + DD;

    m_year   = std::stoi( YYYY );
    m_month  = std::stoi( MM );
    m_day    = std::stoi( DD );
    m_hour   = std::stoi( hh );
    m_minute = std::stoi( mm );
    m_second = std::stoi( ss );
    m_nsec   = std::stoi( nn );

    tm tmst;
    tmst.tm_year = m_year - 1900;
    tmst.tm_mon  = m_month - 1;
    tmst.tm_mday = m_day;
    tmst.tm_hour = m_hour;
    tmst.tm_min  = m_minute;
    tmst.tm_sec  = m_second;

    m_timestamp.time_s  = timegm( &tmst );
    m_timestamp.time_ns = m_nsec;

    m_valid = true;

    return 0;
}

} // namespace logger
} // namespace MagAOX
