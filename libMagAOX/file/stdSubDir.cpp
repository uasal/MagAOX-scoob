/** \file stdSubDir.cpp
 * \brief The stdSubDir class for managing subdirectories
 *
 * \ingroup file_files
 *
 */

#include "stdSubDir.hpp"

namespace MagAOX
{
namespace file
{

stdSubDir::stdSubDir()
{
    return;
}

stdSubDir::stdSubDir( const std::chrono::sys_days &sysday )
{
    m_sysday = sysday;

    m_valid = true;
}
stdSubDir::stdSubDir( int year, unsigned month, unsigned day )
{
    ymd( year, month, day );
}

stdSubDir::stdSubDir( const std::string &subdir )
{
    subDir( subdir );
}

void stdSubDir::ymd( int year, unsigned month, unsigned day )
{
    std::chrono::year_month_day ymd{ std::chrono::year( year ), std::chrono::month( month ), std::chrono::day( day ) };
    m_sysday = ymd;

    m_subDirMade = false;
    m_valid      = true;
}

void stdSubDir::subDir( const std::string &subdir )
{
    if( subdir.length() != 10 )
    {
        throw std::invalid_argument(
            std::format( "subdir {} is not 10 chars long {} {}", subdir, __FILE__, __LINE__ ) );
    }

    for( size_t n : { 4, 7 } )
    {
        if( subdir[n] != '_' )
        {
            throw std::invalid_argument(
                std::format( "subdir {} is missing _  at {}. {} {}", subdir, n, __FILE__, __LINE__ ) );
        }
    }

    for( size_t n : { 0, 1, 2, 3, 5, 6, 8, 9 } )
    {
        if( !isdigit(subdir[n]) )
        {
            throw std::invalid_argument(
                std::format( "subdir {} has non-digit at {}. {} {}", subdir, n, __FILE__, __LINE__ ) );
        }
    }

    int year = std::stoi( subdir.substr( 0, 4 ) );

    unsigned month = std::stoul( subdir.substr( 5, 2 ) );

    unsigned day = std::stoul( subdir.substr( 8, 2 ) );

    std::chrono::year_month_day ymd{ std::chrono::year( year ), std::chrono::month( month ), std::chrono::day( day ) };
    m_sysday = ymd;

    m_subDir     = subdir;
    m_subDirMade = true;

    m_valid = true;
}

std::string stdSubDir::subDir() const
{
    if( !m_valid )
    {
        std::string what = "attempt to access subdir while invalid at ";
        what += __FILE__ + ' ' + std::to_string( __LINE__ );

        throw std::runtime_error( what );
    }

    if( !m_subDirMade )
    {
        m_subDir     = std::format( "{:%Y_%m_%d}", m_sysday );
        m_subDirMade = true;
    }

    return m_subDir;
}

int stdSubDir::year() const
{
    std::chrono::year_month_day ymd{ m_sysday };
    return static_cast<int>( ymd.year() );
}

unsigned stdSubDir::month() const
{
    std::chrono::year_month_day ymd{ m_sysday };
    return static_cast<unsigned>( ymd.month() );
}

unsigned stdSubDir::day() const
{
    std::chrono::year_month_day ymd{ m_sysday };
    return static_cast<unsigned>( ymd.day() );
}

bool stdSubDir::valid() const
{
    return m_valid;
}

stdSubDir stdSubDir::previousSubdir()
{
    std::chrono::sys_days symd = m_sysday;
    --symd;

    return stdSubDir( symd );
}

stdSubDir stdSubDir::followingSubdir()
{
    std::chrono::sys_days symd = m_sysday;
    ++symd;

    return stdSubDir( symd );
}

void stdSubDir::addDay()
{
    ++m_sysday;

    m_subDir = std::format( "{:%Y_%m_%d}", m_sysday );
}

void stdSubDir::subDay()
{
    --m_sysday;

    m_subDir = std::format( "{:%Y_%m_%d}", m_sysday );
}

} // namespace file
} // namespace MagAOX
