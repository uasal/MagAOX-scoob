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
    parseName();
}

int stdFileName::fullName( const std::string &fn )
{
    m_fullName = fn;
    return parseName();
}

stdFileName &stdFileName::operator=( const std::string &fn )
{
    fullName( fn );

    return *this;
}

std::string stdFileName::fullName() const
{
    return m_fullName;
}

std::string stdFileName::baseName() const
{
    return m_baseName;
}

std::string stdFileName::extension() const
{
    return m_extension;
}

std::string stdFileName::appName() const
{
    return m_appName;
}

std::string stdFileName::subDir() const
{
    return m_subDir;
}

int stdFileName::year() const
{
    return m_year;
}

int stdFileName::month() const
{
    return m_month;
}

int stdFileName::day() const
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

std::string stdFileName::previousSubdir()
{
    //Normalize the decrement using year_month_day to/from sys_days
    std::chrono::year_month_day ymd{std::chrono::year(m_year), std::chrono::month(m_month), std::chrono::day(m_day)};
    std::chrono::sys_days symd = ymd;
    --symd;

    return std::format("{:%Y_%m_%d}", symd);
}

std::string stdFileName::followingSubdir()
{
    //Normalize the decrement using year_month_day to/from sys_days
    std::chrono::year_month_day ymd{std::chrono::year(m_year), std::chrono::month(m_month), std::chrono::day(m_day)};
    std::chrono::sys_days symd = ymd;
    ++symd;

    return std::format("{:%Y_%m_%d}", symd);
}


int stdFileName::parseName()
{
    try
    {
        std::filesystem::path p( m_fullName );

        m_baseName  = p.filename();
        m_extension = p.extension();
    }
    catch( const std::exception &e )
    {
        std::cerr << "stdFileName: filesystem exception: " << e.what() << '\n';
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

    if( parseFilePath( m_appName, YYYY, MM, DD, hh, mm, ss, nn, m_baseName ) < 0 )
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

} // namespace file
} // namespace MagAOX
