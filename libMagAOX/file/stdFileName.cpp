/** \file stdFileName.cpp
 * \brief The stdFileName class for managing standard file names
 * \ingroup file_files
 *
 */

#include <filesystem>
#include <chrono>
#include <format>

#include "stdFileName.hpp"
#include "fileTimes.hpp"

namespace MagAOX
{
namespace file
{

stdFileName::stdFileName()
{
    return;
}

stdFileName::stdFileName( const std::string &fn ) : m_fullName{ fn }
{
    try
    {
        parseName();
    }
    catch( ... )
    {
        std::throw_with_nested( xwcException( "error parsing filename: " + m_fullName ) );
    }
}

stdFileName &stdFileName::operator=( const std::string &fn )
{
    try
    {
        fullName( fn );
    }
    catch( ... )
    {
        std::throw_with_nested( xwcException( "error parsing name in std::fileName:operator=" ) );
    }

    return *this;
}

void stdFileName::fullName( const std::string &fn )
{
    try
    {
        m_fullName = fn;
    }
    catch( ... )
    {
        std::throw_with_nested( xwcException( "error from std::string assignment" ) );
    }

    try
    {
        parseName();
    }
    catch( ... )
    {
        std::throw_with_nested( xwcException( "error from parseName" ) );
    }
}

const std::string &stdFileName::fullName() const
{
    return m_fullName;
}

const std::string &stdFileName::baseName() const
{
    return m_baseName;
}

const std::string &stdFileName::extension() const
{
    return m_extension;
}

const std::string &stdFileName::appName() const
{
    return m_appName;
}

const stdSubDir &stdFileName::subDir() const
{
    return m_subDir;
}

int stdFileName::year() const
{
    return m_year;
}

unsigned stdFileName::month() const
{
    return m_month;
}

unsigned stdFileName::day() const
{
    return m_day;
}

int stdFileName::hour() const
{
    return m_hour;
}

int stdFileName::minute() const
{
    return m_minute;
}

int stdFileName::second() const
{
    return m_second;
}

int stdFileName::nsec() const
{
    return m_nsec;
}

flatlogs::timespecX stdFileName::timestamp() const
{
    return m_timestamp;
}

bool stdFileName::valid() const
{
    return m_valid;
}

void stdFileName::parseName()
{
    try
    {
        std::filesystem::path p( m_fullName );

        m_baseName  = p.filename();
        m_extension = p.extension();
    }
    catch( ... )
    {
        m_valid = false;

        std::throw_with_nested( xwcException( "error extracting basename and extension" ) );
    }

    if( m_extension == "" )
    {
        m_valid = false;
        throw xwcException( "No extension found in: " + m_fullName );
    }

    std::string YYYY, MM, DD, hh, mm, ss, nn;

    try
    {
        parseFilePath( m_appName, YYYY, MM, DD, hh, mm, ss, nn, m_baseName );
    }
    catch( ... )
    {
        m_valid = false;

        std::throw_with_nested( xwcException( "Error parsing filename" ) );
    }

    try
    {
        m_year   = std::stoi( YYYY );
        m_month  = std::stoi( MM );
        m_day    = std::stoi( DD );
        m_hour   = std::stoi( hh );
        m_minute = std::stoi( mm );
        m_second = std::stoi( ss );
        m_nsec   = std::stoi( nn );
    }
    catch( ... )
    {
        std::throw_with_nested( xwcException( "error from std:stoi" ) );
    }

    try
    {
        m_subDir.ymd( m_year, m_month, m_day );
    }
    catch( ... )
    {
        std::throw_with_nested( xwcException( "error from std::subDir::ymd" ) );
    }

    tm tmst;
    tmst.tm_year = m_year - 1900;
    tmst.tm_mon  = m_month - 1;
    tmst.tm_mday = m_day;
    tmst.tm_hour = m_hour;
    tmst.tm_min  = m_minute;
    tmst.tm_sec  = m_second;

    time_t tgm = timegm( &tmst );

    if( tgm == static_cast<time_t>( -1 ) )
    {
        throw xwcException( "error from timegm: " + std::string( strerror( errno ) ) );
    }

    m_timestamp.time_s  = tgm;
    m_timestamp.time_ns = m_nsec;

    m_valid = true;
}

} // namespace file
} // namespace MagAOX
