/** \file stdSubDir.cpp
 * \brief The stdSubDir class for managing subdirectories
 *
 * \ingroup file_files
 *
 */

#include "stdSubDir.hpp"
#include "../common/exceptions.hpp"

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
    path( subdir );
}

void stdSubDir::ymd( int year, unsigned month, unsigned day )
{
    std::chrono::year_month_day ymd{ std::chrono::year( year ), std::chrono::month( month ), std::chrono::day( day ) };
    m_sysday = ymd;

    m_pathMade = false;
    m_valid    = true;
}

void stdSubDir::path( const std::string &subdir )
{
    if( subdir.length() != 10 )
    {
        throw xwcException( "subdir " + subdir + " is not 10 chars long " );
    }

    for( size_t n : { 4, 7 } )
    {
        if( subdir[n] != '_' )
        {
            throw xwcException( "subdir " + subdir + " is missing _ " );
        }
    }

    for( size_t n : { 0, 1, 2, 3, 5, 6, 8, 9 } )
    {
        if( !isdigit( subdir[n] ) )
        {
            throw xwcException( "subdir " + subdir + "has non-digit at " + std::to_string( n ) );
        }
    }

    int year;
    try
    {
        year = std::stoi( subdir.substr( 0, 4 ) );
    }
    catch( ... )
    {
        std::throw_with_nested( xwcException( "error extracting year" ) );
    }

    unsigned month;
    try
    {
        month = std::stoul( subdir.substr( 5, 2 ) );
    }
    catch( ... )
    {
        std::throw_with_nested( xwcException( "error extracting month" ) );
    }

    unsigned day;
    try
    {
        day = std::stoul( subdir.substr( 8, 2 ) );
    }
    catch( ... )
    {
        std::throw_with_nested( xwcException( "error extracting day" ) );
    }

    std::chrono::year_month_day ymd{ std::chrono::year( year ), std::chrono::month( month ), std::chrono::day( day ) };

    try
    {
        m_sysday = ymd;
    }
    catch( ... )
    {
        std::throw_with_nested( xwcException( "error creating sys_day from ymd" ) );
    }

    m_path     = subdir;
    m_pathMade = true;

    m_valid = true;
}

std::string stdSubDir::path() const
{
    if( !m_valid )
    {
        throw xwcException( "attempt to access path while invalid" );
    }

    if( !m_pathMade )
    {
        try
        {
            m_path = std::format( "{:%Y_%m_%d}", m_sysday );
        }
        catch( ... )
        {
            std::throw_with_nested( xwcException( "error from std::format" ) );
        }

        m_pathMade = true;
    }

    return m_path;
}

int stdSubDir::year() const
{
    if( !m_valid )
    {
        throw xwcException( "attempt to access year while invalid" );
    }

    std::chrono::year_month_day ymd{ m_sysday };
    return static_cast<int>( ymd.year() );
}

unsigned stdSubDir::month() const
{
    if( !m_valid )
    {
        throw xwcException( "attempt to access month while invalid" );
    }

    std::chrono::year_month_day ymd{ m_sysday };
    return static_cast<unsigned>( ymd.month() );
}

unsigned stdSubDir::day() const
{
    if( !m_valid )
    {
        throw xwcException( "attempt to access day while invalid" );
    }

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

    try
    {
        --symd;
    }
    catch( ... )
    {
        std::throw_with_nested( xwcException( "error decrementing date" ) );
    }

    return stdSubDir( symd );
}

stdSubDir stdSubDir::followingSubdir()
{
    std::chrono::sys_days symd = m_sysday;

    try
    {
        ++symd;
    }
    catch( ... )
    {
        std::throw_with_nested( xwcException( "error incrementing date" ) );
    }

    return stdSubDir( symd );
}

void stdSubDir::addDay()
{
    try
    {
        ++m_sysday;
    }
    catch( ... )
    {
        std::throw_with_nested( xwcException( "error incrementing date" ) );
    }

    m_pathMade = false;
}

void stdSubDir::subDay()
{
    try
    {
        --m_sysday;
    }
    catch( ... )
    {
        std::throw_with_nested( xwcException( "error decrementing date" ) );
    }

    m_pathMade = false;
}

} // namespace file
} // namespace MagAOX
