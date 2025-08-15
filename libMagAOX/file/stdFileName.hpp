/** \file stdFileName.hpp
 * \brief The stdFileName class for managing standard file names
 * \ingroup file_files
 *
 */

#ifndef file_stdFileName_hpp
#define file_stdFileName_hpp

#include <flatlogs/flatlogs.hpp>

#include "../common/exceptions.hpp"

#include "stdSubDir.hpp"

namespace MagAOX
{
namespace file
{

/// Organize and analyze the name of a standard file name
class stdFileName
{

  protected:
    std::string m_fullName; ///< The full name of the file, including path
    std::string m_baseName; ///< The base name of the file, not including path

    std::string m_extension; ///< The extension of the file

    std::string m_appName; ///< The name of the application which wrote the file

    stdSubDir m_subDir; ///< The subdirectory of the file

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
    const stdSubDir &subDir() const;

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
     * \throws nested xwcException on errors from stdSubDir::ymd
     * \throws xwcException on error from timegm
     *
     * \b Tests:
     * -  Using stdFileName \ref libXWC_logger_file_stdFileName_using "[test doc]"
     */
    void parseName();
};

/// Sort predicate for stdFileNames
/** Sorting is on 'fullName()'
 */
struct compStdFileName
{
    /// Comparison operator.
    /** \returns true if a < b
     * \returns false otherwise
     */
    bool operator()( const stdFileName &a, const stdFileName &b ) const
    {
        return ( a.baseName() < b.baseName() );
    }
};

} // namespace file
} // namespace MagAOX

#endif // file_stdFileName_hpp
