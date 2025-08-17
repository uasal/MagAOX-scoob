/** \file stdFileName.hpp
 * \brief The stdFileName class for managing standard file names
 * \ingroup file_files
 *
 */

#ifndef file_stdFileName_hpp
#define file_stdFileName_hpp

#include <filesystem>
#include <chrono>
#include <format>

#include <flatlogs/flatlogs.hpp>

#include "../common/exceptions.hpp"

#include "stdSubDir.hpp"
#include "fileTimes.hpp"

namespace MagAOX
{
namespace file
{

/// Organize and analyze the name of a standard file name
template<typename verboseT=XWC_DEFAULT_VERBOSITY>
class stdFileName
{

  protected:
    std::string m_fullName; ///< The full name of the file, including path
    std::string m_baseName; ///< The base name of the file, not including path

    std::string m_extension; ///< The extension of the file

    std::string m_appName; ///< The name of the application which wrote the file

    stdSubDir<verboseT> m_subDir; ///< The subdirectory of the file

    int      m_year{ 0 };   ///< The year of the timestamp
    unsigned m_month{ 0 };  ///< The month of the timestamp
    unsigned m_day{ 0 };    ///< The day of the timestamp
    int      m_hour{ 0 };   ///< The hour of the timestamp
    int      m_minute{ 0 }; ///< The minute of the timestamp
    int      m_second{ 0 }; ///< The second of the timestamp
    int      m_nsec{ 0 };   ///< The nanosecond of the timestamp

    flatlogs::timespecX m_timestamp{ 0, 0 }; ///< The timestamp

    bool m_valid{ false }; ///< Whether or not the filename parsed correctly and the components are valid

  public:
    /// Default c'tor
    stdFileName();

    /// Construct from a full name
    /** This calls parseName, which parses the input and
     * populates all fields.
     *
     * On success, sets `m_valid=true`
     *
     * On error this throws and exception and will leave `m_valid=false`
     *
     * \throws nested MagAOX::xwcException on an exception from parseName.
     *
     * \b Tests:
     * -  Using stdFileName \ref libXWC_logger_file_stdFileName_using "[test doc]"
     */
    explicit stdFileName( const std::string &fullName /**< [in] The new full name of the file (including the path)*/ );

    /// Assignment operator from string
    /** Sets the full name, which is the only way to set any of the values.  This parses the input and
     * populates all fields.
     *
     * On success, sets `m_valid=true`
     *
     * On error, sets `m_valid=false`
     *
     * \returns a reference the `this`
     *
     * \throws nested MagAOX::xwcException on an exception from fullName.
     *
     * \b Tests:
     * -  Using stdFileName \ref libXWC_logger_file_stdFileName_using "[test doc]"
     */
    stdFileName &
    operator=( const std::string &fullname /**< [in] The new full name of the file (including the path)*/ );

    /// Sets the full name
    /** Setting the full name is the only way to set any of the values.  This parses the input and
     * populates all fields.
     *
     * \throws nested MagAOX::xwcException on an exception from string assignment or parseName.
     *
     * \b Tests:
     * -  Using stdFileName \ref libXWC_logger_file_stdFileName_using "[test doc]"
     */
    void fullName( const std::string &fullName /**< [in] The new full name of the file (including the path)*/ );

    /// Get the current value of m_fullName
    /**
     * \returns the current value of m_fullName
     *
     * \b Tests:
     * -  Using stdFileName \ref libXWC_logger_file_stdFileName_using "[test doc]"
     */
    const std::string &fullName() const;

    /// Get the current value of m_baseName
    /**
     * \returns the current value of m_baseName
     *
     * \b Tests:
     * -  Using stdFileName \ref libXWC_logger_file_stdFileName_using "[test doc]"
     */
    const std::string &baseName() const;

    /// Get the current value of
    /**
     * \returns the current value of
     *
     * \b Tests:
     * -  Using stdFileName \ref libXWC_logger_file_stdFileName_using "[test doc]"
     */
    const std::string &extension() const;

    /// Get the current value of m_appName
    /**
     * \returns the current value of m_appName
     *
     * \b Tests:
     * -  Using stdFileName \ref libXWC_logger_file_stdFileName_using "[test doc]"
     */
    const std::string &appName() const;

    /// Get the current value of m_subDir
    /**
     * \returns the current value of m_subDir
     *
     * \b Tests:
     * -  Using stdFileName \ref libXWC_logger_file_stdFileName_using "[test doc]"
     */
    const stdSubDir<verboseT> &subDir() const;

    /// Get the current value of m_year
    /**
     * \returns the current value of m_year
     *
     * \b Tests:
     * -  Using stdFileName \ref libXWC_logger_file_stdFileName_using "[test doc]"
     */
    int year() const;

    /// Get the current value of m_month
    /**
     * \returns the current value of m_month
     *
     * \b Tests:
     * -  Using stdFileName \ref libXWC_logger_file_stdFileName_using "[test doc]"
     */
    unsigned month() const;

    /// Get the current value of m_day
    /**
     * \returns the current value of m_day
     *
     * \b Tests:
     * -  Using stdFileName \ref libXWC_logger_file_stdFileName_using "[test doc]"
     */

    unsigned day() const;

    /// Get the current value of m_hour
    /**
     * \returns the current value of m_hour
     *
     * \b Tests:
     * -  Using stdFileName \ref libXWC_logger_file_stdFileName_using "[test doc]"
     */
    int hour() const;

    /// Get the current value of m_minute
    /**
     * \returns the current value of m_minute
     *
     * \b Tests:
     * -  Using stdFileName \ref libXWC_logger_file_stdFileName_using "[test doc]"
     */
    int minute() const;

    /// Get the current value of m_second
    /**
     * \returns the current value of m_second
     *
     * \b Tests:
     * -  Using stdFileName \ref libXWC_logger_file_stdFileName_using "[test doc]"
     */
    int second() const;

    /// Get the current value of m_nsec
    /**
     * \returns the current value of m_nsec
     *
     * \b Tests:
     * -  Using stdFileName \ref libXWC_logger_file_stdFileName_using "[test doc]"
     */
    int nsec() const;

    /// Get the current value of m_valid
    /**
     * \returns the current value of m_valid
     *
     * \b Tests:
     * -  Using stdFileName \ref libXWC_logger_file_stdFileName_using "[test doc]"
     */
    flatlogs::timespecX timestamp() const;

    /// Get the current value of
    /**
     * \returns the current value of
     *
     * \b Tests:
     * -  Using stdFileName \ref libXWC_logger_file_stdFileName_using "[test doc]"
     */
    bool valid() const;

  protected:
    /// Parses the `m_fullName` and populates all fields.
    /** On success m_valid will be true.
     *
     * \throws nested xwcException on errors from std::filesystem
     * \throws xwcException if no extension found
     * \throws nested xwcException on errors from parseFilePath
     * \throws nested xwcException on errors from std::stoi
     * \throws nested xwcException on errors from stdSubDir<verboseT>::ymd
     * \throws xwcException on error from timegm
     *
     * \b Tests:
     * -  Using stdFileName \ref libXWC_logger_file_stdFileName_using "[test doc]"
     */
    void parseName();
};

template<class verboseT>
stdFileName<verboseT>::stdFileName()
{
    return;
}

template<class verboseT>
stdFileName<verboseT>::stdFileName( const std::string &fn ) : m_fullName{ fn }
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

template<class verboseT>
stdFileName<verboseT> &stdFileName<verboseT>::operator=( const std::string &fn )
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

template<class verboseT>
void stdFileName<verboseT>::fullName( const std::string &fn )
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

template<class verboseT>
const std::string &stdFileName<verboseT>::fullName() const
{
    return m_fullName;
}

template<class verboseT>
const std::string &stdFileName<verboseT>::baseName() const
{
    return m_baseName;
}

template<class verboseT>
const std::string &stdFileName<verboseT>::extension() const
{
    return m_extension;
}

template<class verboseT>
const std::string &stdFileName<verboseT>::appName() const
{
    return m_appName;
}

template<class verboseT>
const stdSubDir<verboseT> &stdFileName<verboseT>::subDir() const
{
    return m_subDir;
}

template<class verboseT>
int stdFileName<verboseT>::year() const
{
    return m_year;
}

template<class verboseT>
unsigned stdFileName<verboseT>::month() const
{
    return m_month;
}

template<class verboseT>
unsigned stdFileName<verboseT>::day() const
{
    return m_day;
}

template<class verboseT>
int stdFileName<verboseT>::hour() const
{
    return m_hour;
}

template<class verboseT>
int stdFileName<verboseT>::minute() const
{
    return m_minute;
}

template<class verboseT>
int stdFileName<verboseT>::second() const
{
    return m_second;
}

template<class verboseT>
int stdFileName<verboseT>::nsec() const
{
    return m_nsec;
}

template<class verboseT>
flatlogs::timespecX stdFileName<verboseT>::timestamp() const
{
    return m_timestamp;
}

template<class verboseT>
bool stdFileName<verboseT>::valid() const
{
    return m_valid;
}

template<class verboseT>
void stdFileName<verboseT>::parseName()
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

extern template class stdFileName<XWC_DEFAULT_VERBOSITY>;

/// Sort predicate for stdFileNames
/** Sorting is on 'fullName()'
 */
template<class stdFileNameT>
struct compStdFileName
{
    /// Comparison operator.
    /** \returns true if a < b
     * \returns false otherwise
     */
    bool operator()( const stdFileNameT &a, const stdFileNameT &b ) const
    {
        return ( a.baseName() < b.baseName() );
    }
};


} // namespace file
} // namespace MagAOX

#endif // file_stdFileName_hpp
