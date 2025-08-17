/** \file fileTimes.hpp
 * \brief Utilities for working with file timestamps
 * \ingroup file_files
 */

#ifndef file_fileTimes_hpp
#define file_fileTimes_hpp

#include <time.h>

#include <iostream>
#include <format>

#include <mx/mxError.hpp>

#include "../common/exceptions.hpp"

namespace MagAOX
{
namespace file
{

#ifdef XWCTEST_NAMESPACE
namespace XWCTEST_NAMESPACE
{
#endif

/// Get the filename timestamp from the breakdown for a time.
/** Fills in the \p tstamp string with the timestamp encoded as
 * \verbatim
 *      YYYYMMDDHHMMSSNNNNNNNNN
 * \endverbatim
 *
 * \returns mx::error_t::noerror on success
 * \returns mx::error_t::format_error if std::format throws and exception
 * \returns mx::error_t::std_exception if any other exception other than bad_alloc is caught
 *
 * \returns throws nested xwcException if std::bad_alloc is caught from std::format
 *
 * \b Tests
 *
 */
template <class verboseT = mx::verbose::vvv>
mx::error_t timestamp( std::string &tstamp, /**< [out] the timestamp string*/
                       const tm    &uttime, /**< [in] the broken down time*/
                       long         ts_nsec /**< [in] the nanosecond*/
)
{
    try
    {

        // clang-format off
        #ifdef XWCTEST_TIMESTAMP_THROW_BAD_ALLOC
        throw std::bad_alloc();
        #endif
        // clang-format on

        // clang-format off
        #ifdef XWCTEST_TIMESTAMP_THROW_FORMAT_ERROR
        throw std::format_error("testing format_error");
        #endif
        // clang-format on

        // clang-format off
        #ifdef XWCTEST_TIMESTAMP_THROW_EXCEPTION
        throw std::exception();
        #endif
        // clang-format on

        tstamp = std::format( "{:04}{:02}{:02}{:02}{:02}{:02}{:09}",
                              uttime.tm_year + 1900,
                              uttime.tm_mon + 1,
                              uttime.tm_mday,
                              uttime.tm_hour,
                              uttime.tm_min,
                              uttime.tm_sec,
                              ts_nsec );
    }
    catch( const std::bad_alloc &e )
    {
        std::throw_with_nested( xwcException( "bad_alloc from std::format", -12 ) );
    }
    catch( const std::format_error &e )
    {
        return mx::error_report<verboseT>( mx::error_t::std_format_error,
                                           std::string( "from std::format: " ) + e.what() );
    }
    catch( const std::exception &e )
    {
        return mx::error_report<verboseT>( mx::error_t::std_exception, std::string( "from std::format: " ) + e.what() );
    }

    return mx::error_t::noerror;
}

/// Get the filename timestamp and the breakdown for a time.
/** Fills in the \p tstamp string with the timestamp encoded as
 * \verbatim
 *      YYYYMMDDHHMMSSNNNNNNNNN
 * \endverbatim
 * and the broken down `tm` structure \p uttime
 *
 * \returns mx::error_t::noerror on success
 * \returns mx::error_t::eoverflow if year is too big for gmtime_r
 * \returns mx::error_t::error if gmtime_r returns an error without setting errno
 * \returns mx::error_t::format_error if std::format throws and exception
 * \returns mx::error_t::std_exception if any other exception other than bad_alloc is caught in \ref
 * timestamp(std::string &, const tm&, long) "timestamp"
 *
 * \returns throws nested xwcException if an exception is caught from \ref timestamp(std::string &, const tm&, long)
 * "timestamp" , which means std::bad_alloc was thrown
 *
 */
template <typename verboseT = mx::verbose::vvv>
mx::error_t timestamp( std::string &tstamp, /**< [out] the timestamp string*/
                       tm          &uttime, /**< [out] the broken down time*/
                       time_t       ts_sec, /**< [in] the unix time second*/
                       long         ts_nsec /**< [in] the nanosecond*/
)
{
    memset( &uttime, 0, sizeof( uttime ) );

    errno = 0;
    if( gmtime_r( &ts_sec, &uttime ) == 0 )
    {
        if( errno != 0 )
        {
            return mx::error_report<verboseT>( mx::errno2error_t( errno ),
                                               "error getting UT time (gmtime_r returned 0)" );
        }
        else
        {
            return mx::error_report<verboseT>( mx::error_t::error, "error getting UT time (gmtime_r returned 0)" );
        }
    }

    try
    {
        mx_error_return( timestamp<verboseT>( tstamp, uttime, ts_nsec ) );
    }
    catch( const std::exception &e )
    {
        std::throw_with_nested( xwcException( "exception from timestamp", -5 ) );
    }
}

/// Get the filename timestamp for a time.
/** Fills in the \p tstamp string with the timestamp encoded as
 * \verbatim
 * YYYYMMDDHHMMSSNNNNNNNNN
 * \endverbatim
 *
 * \overload
 *
 * \returns mx::error_t::noerror on success
 * \returns mx::error_t::eoverflow if year is too big for gmtime_r
 * \returns mx::error_t::error if gmtime_r returns an error without setting errno
 * \returns mx::error_t::std_exception if an exception other than bad_alloc is caught in \ref timestamp(std::string &,
 * const tm&, long) "timestamp"
 *
 * \returns throws nested xwcException if an exception is caught from \ref timestamp(std::string &, const tm&, long)
 * "timestamp" , which means std::bad_alloc was thrown
 *
 * \b Tests
 */
template <class verboseT = mx::verbose::vvv>
mx::error_t timestamp( std::string &tstamp, /**< [out] the timestamp string*/
                       time_t       ts_sec, /**< [in] the unix time second*/
                       long         ts_nsec /**< [in] the nanosecond*/
)
{
    tm uttime;
    memset( &uttime, 0, sizeof( uttime ) );

    try
    {
        mx_error_return( timestamp<verboseT>( tstamp, uttime, ts_sec, ts_nsec ) );
    }
    catch( const std::exception &e )
    {
        std::throw_with_nested( xwcException( "exception from timestamp", -5 ) );
    }
}

/// Get the timestamp and the relative path based on a time
/**  Fills in the \p tstamp string with the timestamp encoded as
 * \verbatim
 *      YYYYMMDDHHMMSSNNNNNNNNN
 * \endverbatim
 * and the \p relPath string with the format
 * \verbatim
 *      yyyy_mm_dd
 * \endverbatim
 *
 * \returns 0 on success
 * \returns -1 if gmtime_r returns an error (from \ref timestamp)
 * \returns -2 if snprintf returns an error writing tstamp (from \ref timestamp)
 * \returns -3 if snprintf does not write enough characters to tstamp (from \ref timestamp)
 * \returns -4 if snprintf returns an error writing relPath
 * \returns -5 if snprintf does not write enough characters to relPath
 *
 * \b Tests
 *     - Getting filename and relative path for a given time \ref tests_libMagAOX_file_fileTimes_filename_relpath_time
 * "[test doc]"
 */
template <class verboseT = mx::verbose::vvv>
mx::error_t fileTimeRelPath( std::string &tstamp,  /**< [out] */
                             std::string &relPath, /**< [out] */
                             time_t       ts_sec,  /**< [in] the unix time second*/
                             long         ts_nsec  /**< [in] the nanosecond*/
)
{
    tm uttime;
    memset( &uttime, 0, sizeof( uttime ) );

    try
    {
        // clang-format off
        #ifdef XWCTEST_FILETIMERELPATH_THROW_BAD_ALLOC
        throw std::bad_alloc();
        #endif
        // clang-format on

        // clang-format off
        #ifdef XWCTEST_FILETIMERELPATH_THROW_FORMAT_ERROR
        throw std::format_error("testing format_error");
        #endif
        // clang-format on

        // clang-format off
        #ifdef XWCTEST_FILETIMERELPATH_THROW_EXCEPTION
        throw std::exception();
        #endif
        // clang-format on

        mx_error_check( timestamp( tstamp, uttime, ts_sec, ts_nsec ) );

        relPath = std::format( "{:04}_{:02}_{:02}", uttime.tm_year + 1900, uttime.tm_mon + 1, uttime.tm_mday );

        return mx::error_t::noerror;
    }
    catch( const xwcException &e ) // This is from previous bad_alloc
    {
        std::throw_with_nested( xwcException( "getting relPath", -5 ) );
    }
    catch( const std::bad_alloc &e )
    {
        std::throw_with_nested( xwcException( "getting relPath", -6 ) );
    }
    catch( const std::format_error &e )
    {
        return mx::error_report<verboseT>( mx::error_t::std_format_error,
                                           std::string( "from std::format: " ) + e.what() );
    }
    catch( const std::exception &e )
    {
        return mx::error_report<verboseT>( mx::error_t::std_exception, e.what() );
    }
}

/// Construct the filename and full relative path based on a time and a device name and extension
/**  Fills in the fileName string with the timestamp encoded as
 * \verbatim
 *      devName_YYYYMMDDHHMMSSNNNNNNNNN.ext
 * \endverbatim
 * and the \p relPath string with the format
 * \verbatim
 *      devName/YYYY_MM_DD
 * \endverbatim
 *
 * \overload
 *
 * \returns 0 on success
 * \returns -1 if gmtime_r returns an error (from \ref timestamp)
 * \returns -2 if snprintf returns an error writing tstamp (from \ref timestamp)
 * \returns -3 if snprintf does not write enough characters to tstamp (from \ref timestamp)
 * \returns -4 if snprintf returns an error writing relPath
 * \returns -5 if snprintf does not write enough characters to relPath
 *
 * \b Tests
 *
 */
template <class verboseT = mx::verbose::vvv>
mx::error_t fileTimeRelPath( std::string       &fileName, /**< [out] the resulting file name*/
                             std::string       &relPath,  /**< [out] the resulting relative path*/
                             const std::string &devName,  /**< [in] the device name part of the path.  No '/'. */
                             const std::string &ext,      /**< [in] the extension part of the filename. No `.`. */
                             time_t             ts_sec,   /**< [in] the unix time second*/
                             long               ts_nsec   /**< [in] the nanosecond*/
)
{
    std::string tstamp, tmprelpath;

    try
    {
        mx_error_check( fileTimeRelPath<verboseT>( tstamp, tmprelpath, ts_sec, ts_nsec ) );
    }
    catch( const xwcException &e ) // This is from previous bad_alloc
    {
        std::throw_with_nested( xwcException( "from fileTimeRelPath", -5 ) );
    }

    try
    {
        // clang-format off
        #ifdef XWCTEST_FILETIMERELPATHSTRING_THROW_BAD_ALLOC
        throw std::bad_alloc();
        #endif
        // clang-format on

        // clang-format off
        #ifdef XWCTEST_FILETIMERELPATHSTRING_THROW_EXCEPTION
        throw std::exception();
        #endif
        // clang-format on

        relPath  = devName + '/' + tmprelpath;
        fileName = devName + '_' + tstamp + '.' + ext;

        return mx::error_t::noerror;
    }
    catch( const std::bad_alloc &e )
    {
        std::throw_with_nested( xwcException( "std::string error assembling paths", -6 ) );
    }
    catch( const std::exception &e )
    {
        return mx::error_report<verboseT>( mx::error_t::std_exception,
                                           std::string( "std::string error assembling paths" ) + e.what() );
    }
}

/// Parse a standard XWCTk timestamp string
/** Extracts the date components.
 *
 * The input must be exactly 23 characters long.
 *
 * No validity checks are done on the components.
 *
 * \returns mx::error_t::noerror on success
 * \returns mx::error_t::invalidarg if timestamp is not 23 characters long
 * \returns mx::error_t::std_out_of_range if std::out_of_range is thrown by std::substr
 * \returns mx::error_t::exception if any other exception other than bad_alloc is thrown by std::string::substr
 *
 * \throws nested mxException if std::bad_alloc is thrown
 *
 * \b Tests

 */
template <class verboseT = mx::verbose::vvv>
mx::error_t parseTimestamp( std::string       &YYYY,  /**< [out] the 4 digit year*/
                            std::string       &MM,    /**< [out] the 2 digit month*/
                            std::string       &DD,    /**< [out] the 2 digit day*/
                            std::string       &hh,    /**< [out] the 2 digit hour*/
                            std::string       &mm,    /**< [out] the 2 digit minute*/
                            std::string       &ss,    /**< [out] the 2 digit second*/
                            std::string       &nn,    /**< [out] the 9 digit nanosecond*/
                            const std::string &tstamp /**< [in] the 23-digit timestamp */
)
{
    try
    {
        // clang-format off
        #ifdef XWCTEST_PARSETIMESTAMP_THROW_BAD_ALLOC
        throw std::bad_alloc();
        #endif
        // clang-format on

        // clang-format off
        #ifdef XWCTEST_PARSETIMESTAMP_THROW_OUT_OF_RANGE
        throw std::out_of_range("testing out of range");
        #endif
        // clang-format on

        // clang-format off
        #ifdef XWCTEST_PARSETIMESTAMP_THROW_EXCEPTION
        throw std::exception();
        #endif
        // clang-format on

        if( tstamp.length() != 23 )
        {
            return mx::error_report<verboseT>( mx::error_t::invalidarg, "timestamp does not have 23 characters" );
        }

        YYYY = tstamp.substr( 0, 4 );
        MM   = tstamp.substr( 4, 2 );
        DD   = tstamp.substr( 6, 2 );
        hh   = tstamp.substr( 8, 2 );
        mm   = tstamp.substr( 10, 2 );
        ss   = tstamp.substr( 12, 2 );
        nn   = tstamp.substr( 14, 9 );

        return mx::error_t::noerror;
    }
    catch( const std::bad_alloc &e )
    {
        std::throw_with_nested( xwcException( "parsing timestamp", -6 ) );
    }
    catch( const std::out_of_range &e )
    {
        return mx::error_report<verboseT>( mx::error_t::std_out_of_range,
                                           std::string( "parsing timestamp" ) + e.what() );
    }
    catch( const std::exception &e )
    {
        return mx::error_report<verboseT>( mx::error_t::std_exception, std::string( "parsing timestamp" ) + e.what() );
    }
}

/// Parse a standard XWCTk timestamp filepath
/** Extracts the device name and the date components.
 * The only restriction on the input \fname is that it be at least 23 characters long.
 * In this case it contains only the timestamp.
 *
 * No validity checks are done on the components (i.e. no check that the timestamp
 * is all numeric, no check on device name format).
 *
 * Examples of valid inputs are:
 * - `device_20241121063300000000000.txt`
 * - `/path/to/device_20241121063300000000000.txt`
 * - `20241121063300000000000`
 *
 * \returns 0 on success
 * \returns -1 on error
 *
 * \b Tests

 */
template <class verboseT = mx::verbose::vvv>
mx::error_t parseFilePath( std::string       &devName, /**< [out] the device name */
                           std::string       &YYYY,    /**< [out] the 4 digit year*/
                           std::string       &MM,      /**< [out] the 2 digit month*/
                           std::string       &DD,      /**< [out] the 2 digit day*/
                           std::string       &hh,      /**< [out] the 2 digit hour*/
                           std::string       &mm,      /**< [out] the 2 digit minute*/
                           std::string       &ss,      /**< [out] the 2 digit second*/
                           std::string       &nn,      /**< [out] the 9 digit nanosecond*/
                           const std::string &fname    /**< [in] the filename, which can include a path */
)
{
    try
    {
        // clang-format off
        #ifdef XWCTEST_PARSEFILEPATH_THROW_BAD_ALLOC
        throw std::bad_alloc();
        #endif
        // clang-format on

        // clang-format off
        #ifdef XWCTEST_PARSEFILEPATH_THROW_EXCEPTION
        throw std::exception();
        #endif
        // clang-format on

        size_t est = fname.rfind( '.' ); // rfind does not throw
        if( est == std::string::npos )
        {
            est = fname.size(); // no extension
        }

        size_t dst;
        size_t dend;

        dend = fname.rfind( '_', est ); // rfind does not throw

        if( dend == std::string::npos ) // no device name, just a timestamp
        {

            dst = fname.rfind( '/', est );

            if( dst == std::string::npos ) // no path
            {
                dst = 0;
            }
            else
            {
                ++dst; // move past the '/'
            }

            dend = est;

            devName = "";
        }
        else
        {

            dst = fname.rfind( '/', dend );

            if( dst == std::string::npos ) // no path
            {
                dst = 0;
            }
            else
            {
                ++dst; // move past the '/'
            }

            if( dst >= dend ) // This is '/_YYYY....'
            {
                devName = ""; // no device
            }
            else // finally, we have a device name
            {

                // clang-format off
                #ifdef XWCTEST_PARSEFILEPATH_THROW_OUT_OF_RANGE
                throw std::out_of_range("testing out of range");
                #endif
                // clang-format on

                devName = fname.substr( dst, dend - dst );
            }

            dst  = dend + 1; // now move to beginning of timestamp
            dend = est;      // and one-past-the-end of the timestamp
        }

        // Here dst...dend should be just the timestamp
        if( dend - dst != 23 )
        {
            return mx::error_report<verboseT>( mx::error_t::invalidarg, "timestamp does not have 23 characters" );
        }

        // clang-format off
        #ifdef XWCTEST_PARSEFILEPATH_TOO_SHORT
        ++dst; // this will generate a too-short error at this point (invalidarg)
        #endif
        // clang-format on

        mx_error_return( parseTimestamp( YYYY, MM, DD, hh, mm, ss, nn, fname.substr( dst, dend - dst ) ) );
    }
    catch( const xwcException &e ) // from a previous bad_alloc
    {
        std::throw_with_nested( xwcException( "parsing filepath", -6 ) );
    }
    catch( const std::bad_alloc &e )
    {
        std::throw_with_nested( xwcException( "parsing filepath", -6 ) );
    }
    catch( const std::out_of_range &e )
    {
        return mx::error_report<verboseT>( mx::error_t::std_out_of_range, e.what() );
    }
    catch( const std::exception &e )
    {
        return mx::error_report<verboseT>( mx::error_t::std_exception, e.what() );
    }
}

#ifdef XWCTEST_NAMESPACE
}
#endif

} // namespace file
} // namespace MagAOX

#endif // file_fileTimes_hpp
