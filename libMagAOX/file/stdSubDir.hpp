/** \file stdSubDir.hpp
 * \brief The stdSubDir class for managing subdirectories
 *
 * \ingroup file_files
 *
 */

#ifndef file_stdSubDir_hpp
#define file_stdSubDir_hpp

#include <string>
#include <chrono>

namespace MagAOX
{
namespace file
{

/// Manage a standard subdirectory
/** MagAO-X data storage subdirectories have the format YYYY_MM_DD
 *  This class provides parsing and date arithmetic.
 */
class stdSubDir
{

  protected:
    std::chrono::sys_days m_sysday;

    mutable bool m_subDirMade{ false }; ///< Whether or not the subDir string has been constructed

    mutable std::string m_subDir;

    bool m_valid{ false }; ///< Whether or not the components are valid

  public:
    /// Default c'tor
    stdSubDir();

    /// Construct from std::sys_days
    /**
     * On success, sets `m_valid=true`
     *
     * On error, sets `m_valid=false`
     */
    stdSubDir( const std::chrono::sys_days &sysday /**< [in] The new year*/ );

    /// Construct from components
    /**
     * On success, sets `m_valid=true`
     *
     * On error, sets `m_valid=false`
     */
    stdSubDir( int      year,  /**< [in] The new year*/
               unsigned month, /**< [in] The new month*/
               unsigned day    /**< [in] The new day*/
    );

    /// Construct from a string
    /**
     * On success, sets `m_valid=true`
     *
     * On error, sets `m_valid=false`
     */
    stdSubDir( const std::string &subdir /**< [in] The string of format YYYY_MM_DD*/ );

    /// Setup from components
    /**
     * On success, sets `m_valid=true`
     *
     * On error, sets `m_valid=false`
     */
    void ymd( int      year,  /**< [in] The new year*/
              unsigned month, /**< [in] The new month*/
              unsigned day    /**< [in] The new day*/
    );

    /// Set the subdirectory
    /**
     * Parses the string and sets the time point
     */
    void subDir( const std::string &subdir );

    /// Get the current value of m_subDir
    /**
     * \returns the current value of m_subDir
     */
    std::string subDir() const;

    /// Get the current value of m_year
    /**
     * \returns the current value of m_year
     */
    int year() const;

    /// Get the current value of m_month
    /**
     * \returns the current value of m_month
     */
    unsigned month() const;

    /// Get the current value of m_day
    /**
     * \returns the current value of m_day
     */
    unsigned day() const;

    /// Get the current value of m_valid
    /**
     * \returns the current value of m_valid
     */
    bool valid() const;

    // Manipulations

    /// Get the previous day's subdirectory
    stdSubDir previousSubdir();

    /// Get the following day's subdirectory
    stdSubDir followingSubdir();

    /// Add a day to this subdirectory
    void addDay();

    /// Subtract a day from this subdirectory
    void subDay();
};

} // namespace file
} // namespace MagAOX

#endif // file_stdSubDir_hpp
