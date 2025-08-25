/** \file logFileRaw.hpp
 * \brief Manage a raw log file.
 * \ingroup logger_files
 */

#ifndef logger_logFileRaw_hpp
#define logger_logFileRaw_hpp

#include <iostream>
#include <string>
#include <filesystem>
//#include <cstring>

#include <mx/ioutils/stringUtils.hpp>

#include <flatlogs/flatlogs.hpp>

#include "../file/fileTimes.hpp"


namespace MagAOX
{
namespace logger
{

/// A class to manage raw binary log files
/** Manages a binary file containing MagAO-X logs.
 *
 * The log entries are written as a binary stream of a configurable
 * maximum size.  If this size will be exceed by the next entry, then a new file is created.
 *
 * Filenames have a standard form of: `[path]/[name]/[name]_YYYYMMDDHHMMSSNNNNNNNNN.[ext]` where fields in [] are
 * configurable.
 *
 * The timestamp in the file name is from the first entry of the file.
 *
 */
template<class verboseT=XWC_DEFAULT_VERBOSITY>
class logFileRaw
{

  protected:
    /** \name Configurable Parameters
     *@{
     */
    std::string m_logPath{ "." };                  ///< The base path for the log files.
    std::string m_logName{ "xlog" };               ///< The base name for the log files.
    std::string m_logExt{ MAGAOX_default_logExt }; ///< The extension for the log files.

    size_t m_maxLogSize{ MAGAOX_default_max_logSize }; ///< The maximum file size in bytes. Default is 10 MB.
    ///@}

    /** \name Internal State
     *@{
     */

    FILE *m_fout{ 0 }; ///< The file pointer

    size_t m_currFileSize{ 0 }; ///< The current file size.

    ///@}

  public:
    /// Default constructor
    /** Currently does nothing.
     */
    logFileRaw();

    /// Destructor
    /** Closes the file if open
     */
    ~logFileRaw();

    /// Set the path.
    /**
     *
     * \returns 0 on success
     * \returns -1 on error
     */
    mx::error_t logPath( const std::string &newPath /**< [in] the new value of _path */ );

    /// Get the path.
    /**
     * \returns the current value of m_logPath.
     */
    std::string logPath();

    /// Set the log name
    /**
     *
     * \returns 0 on success
     * \returns -1 on error
     */
    mx::error_t logName( const std::string &newName /**< [in] the new value of m_logName */ );

    /// Get the name
    /**
     * \returns the current value of _name.
     */
    std::string logName();

    /// Set the log extension
    /**
     *
     * \returns 0 on success
     * \returns -1 on error
     */
    mx::error_t logExt( const std::string &newExt /**< [in] the new value of m_logExt */ );

    /// Get the log extension
    /**
     * \returns the current value of m_logExt.
     */
    std::string logExt();

    /// Set the maximum file size
    /**
     *
     * \returns 0 on success
     * \returns -1 on error
     */
    mx::error_t maxLogSize( size_t newMaxFileSize /**< [in] the new value of _maxLogSize */ );

    /// Get the maximum file size
    /**
     * \returns the current value of m_maxLogSize
     */
    size_t maxLogSize();

    /// Write a log entry to the file
    /** Checks if this write will exceed m_maxLogSize, and if so opens a new file.
     * The new file will have the timestamp of this log entry.
     *
     * \returns 0 on success
     * \returns -1 on error
     */
    mx::error_t writeLog( flatlogs::bufferPtrT &data /**< [in] the log entry to write to disk */ );

    /// Flush the stream
    /** Calls `fflush`. See issue #192
     *
     * \returns 0 on success
     * \returns -1 on error
     */
    mx::error_t flush();

    /// Close the file pointer
    /** Sets \ref m_fout to nullptr after calling fclose regardless of error.
     *
     * \returns 0 on success
     * \returns -1 on error
     */
    mx::error_t close();

  protected:
    /// Create a new file
    /** Closes the current file if open.  Then creates a new file with a name of the form
     * [path]/[name]/YYYY_MM_DD/[name]_YYYYMMDDHHMMSSNNNNNNNNN.[ext]
     *
     *
     * \returns 0 on success
     * \returns -1 on error
     */
    mx::error_t createFile( flatlogs::timespecX &ts /**< [in] A MagAOX timespec, used to set the timestamp */ );
};

template <class verboseT>
logFileRaw<verboseT>::logFileRaw()
{
}

template <class verboseT>
logFileRaw<verboseT>::~logFileRaw()
{
    close();
}

template <class verboseT>
mx::error_t logFileRaw<verboseT>::logPath( const std::string &newPath )
{
    try
    {
        m_logPath = newPath;
    }
    catch( const std::bad_alloc &e )
    {
        std::throw_with_nested( xwcException( "string assignment" ) );
    }
    catch( const std::exception &e )
    {
        return mx::error_report( mx::error_t::std_exception, std::string( "string assignment: " ) + e.what() );
    }

    return mx::error_t::noerror;
}

template <class verboseT>
std::string logFileRaw<verboseT>::logPath()
{
    return m_logPath;
}

template <class verboseT>
mx::error_t logFileRaw<verboseT>::logName( const std::string &newName )
{
    try
    {
        m_logName = newName;
    }
    catch( const std::bad_alloc &e )
    {
        std::throw_with_nested( xwcException( "string assignment" ) );
    }
    catch( const std::exception &e )
    {
        return mx::error_report( mx::error_t::std_exception, std::string( "string assignment: " ) + e.what() );
    }

    return mx::error_t::noerror;
}

template <class verboseT>
std::string logFileRaw<verboseT>::logName()
{
    return m_logName;
}

template <class verboseT>
mx::error_t logFileRaw<verboseT>::logExt( const std::string &newExt )
{
    try
    {
        m_logExt = newExt;
    }
    catch( const std::bad_alloc &e )
    {
        std::throw_with_nested( xwcException( "string assignment" ) );
    }
    catch( const std::exception &e )
    {
        return mx::error_report( mx::error_t::std_exception, std::string( "string assignment: " ) + e.what() );
    }

    return mx::error_t::noerror;
}

template <class verboseT>
std::string logFileRaw<verboseT>::logExt()
{
    return m_logExt;
}

template <class verboseT>
mx::error_t logFileRaw<verboseT>::maxLogSize( size_t newMaxFileSize )
{
    try
    {
        m_maxLogSize = newMaxFileSize;
    }
    catch( const std::bad_alloc &e )
    {
        std::throw_with_nested( xwcException( "string assignment" ) );
    }
    catch( const std::exception &e )
    {
        return mx::error_report( mx::error_t::std_exception, std::string( "string assignment: " ) + e.what() );
    }

    return mx::error_t::noerror;
}

template <class verboseT>
size_t logFileRaw<verboseT>::maxLogSize()
{
    return m_maxLogSize;
}

template <class verboseT>
mx::error_t logFileRaw<verboseT>::writeLog( flatlogs::bufferPtrT &data )
{
    size_t N = flatlogs::logHeader::totalSize( data );

    // Check if we need a new file
    if( m_currFileSize + N > m_maxLogSize || m_fout == 0 )
    {
        flatlogs::timespecX ts = flatlogs::logHeader::timespec( data );

        mx_error_check( createFile( ts ) );
    }

    size_t nwr = fwrite( data.get(), sizeof( char ), N, m_fout );

    if( nwr != N * sizeof( char ) )
    {
        return mx::error_report<verboseT>( mx::errno2error_t( errno ), "Error from fwrite" );
    }

    m_currFileSize += N;

    return mx::error_t::noerror;
}

template <class verboseT>
mx::error_t logFileRaw<verboseT>::flush()
{
    ///\todo this probably should be fsync, with appropriate error handling (see fsyncgate) [issue #192]

    if( m_fout )
    {
        if( fflush( m_fout ) != 0 )
        {
            return mx::error_report<verboseT>( mx::errno2error_t( errno ), "Error from fflush" );
        }
    }
    return mx::error_t::noerror;
}

template <class verboseT>
mx::error_t logFileRaw<verboseT>::close()
{
    if( m_fout )
    {
        errno = 0;

        if( fclose( m_fout ) != 0 )
        {
            m_fout = nullptr;

            return mx::error_report<verboseT>( mx::errno2error_t( errno ), "Error from fflush" );
        }

        m_fout = nullptr;
    }

    return mx::error_t::noerror;
}

template <class verboseT>
mx::error_t logFileRaw<verboseT>::createFile( flatlogs::timespecX &ts )
{
    std::string fileName;
    std::string relPath;

    try
    {
        file::fileTimeRelPath( fileName, relPath, m_logName, m_logExt, ts.time_s, ts.time_ns );
    }
    catch( ... )
    {
        std::throw_with_nested( xwcException( "logFileRaw<verboseT>::createFile: Error from fileTimePath" ) );
    }

    std::string fullPath = m_logPath + '/' + relPath + '/';

    // Create directory
    try
    {
        std::filesystem::create_directories( fullPath ); // this does nothing if fname already exists
    }
    catch( ... )
    {
        std::throw_with_nested(
            xwcException( "logFileRaw<verboseT>::createFile: filesystem_error from std::create_directories" ) );
    }

    fullPath += fileName;

    // Check if file exists
    try
    {
        // This will be true only if another instance with same name (essentially impossible) tries
        // at same ns, which is pathological
        if( std::filesystem::exists( fullPath ) )
        {
            return mx::error_report<verboseT>( mx::error_t::eexist, "file " + fullPath + " exists" );
        }
    }
    catch( const std::filesystem::filesystem_error &e )
    {
        return mx::error_report<verboseT>( mx::error_t::std_filesystem_error,
                                           std::string( "from std::filesystem::exists " ) + e.what() );
    }
    catch( const std::exception &e )
    {
        return mx::error_report<verboseT>( mx::error_t::std_exception,
                                           std::string( "from std::filesystem::exists " ) + e.what() );
    }

    // Close current file if it's open
    if( close() != mx::error_t::noerror )
    {
        std::cerr << "logFileRaw<verboseT>::createFile: Error from close, attempting to continue. At: ";
        std::cerr << __FILE__ << " " << __LINE__ << '\n';
    }

    errno = 0;

    m_fout = fopen( fullPath.c_str(), "wb" );

    if( m_fout == 0 )
    {
        return mx::error_report<verboseT>( mx::errno2error_t( errno ), "Error from fopen on " + fullPath );
    }

    // Reset counters.
    m_currFileSize = 0;

    return mx::error_t::noerror;
}

extern template class logFileRaw<XWC_DEFAULT_VERBOSITY>;

} // namespace logger
} // namespace MagAOX

#endif // logger_logFileRaw_hpp
