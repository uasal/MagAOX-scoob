/** \file fileTimes.hpp
 * \brief Utilities for working with file timestamps
 * \ingroup file_files
 */

#ifndef file_fileTimes_hpp
#define file_fileTimes_hpp

#include <time.h>

#include <iostream>
#include <format>

#include "../common/exceptions.hpp"

namespace MagAOX
{
namespace file
{

#ifdef XWCTEST_NAMESPACE
namespace XWCTEST_NAMESPACE
{
#endif

namespace internal
{

inline void initbdtime( tm &bt )
{
    bt.tm_sec    = 0;
    bt.tm_min    = 0;
    bt.tm_hour   = 0;
    bt.tm_mday   = 0;
    bt.tm_mon    = 0;
    bt.tm_year   = 0;
    bt.tm_wday   = 0;
    bt.tm_yday   = 0;
    bt.tm_isdst  = 0;
    bt.tm_gmtoff = 0;
    bt.tm_zone   = 0;
}

} // namespace internal

/// Get the filename timestamp from the breakdown for a time.
/** Fills in the \p tstamp string with the timestamp encoded as
 * \verbatim
 *      YYYYMMDDHHMMSSNNNNNNNNN
 * \endverbatim
 *
 * \returns 0 on success
 * \returns -2 if snprintf returns an error
 * \returns -3 if snprintf does not write enough characters
 *
 * \b Tests
 * - Getting timestamp string and broken-down time for a given time \ref
 * tests_libMagAOX_file_fileTimes_timestamp_bdtime "[test doc]"
 *     - Getting timestamp and broken-down time with errors \ref
 * tests_libMagAOX_file_fileTimes_parse_filenames_timestamp_bdtime_errors "[test doc]"
 *     - Getting timestamp string only for a given time \ref tests_libMagAOX_file_fileTimes_timestamp_only "[test doc]"
 *     - Getting filename and relative path for a given time \ref tests_libMagAOX_file_fileTimes_filename_relpath_time
 * "[test doc]"
 *     - Getting filename and relative path with errors \ref
 * tests_libMagAOX_file_fileTimes_parse_filename_relpath_only_errors "[test doc]"
 */
inline void timestamp( std::string &tstamp, /**< [out] the timestamp string*/
                       const tm    &uttime, /**< [in] the broken down time*/
                       long         ts_nsec /**< [in] the nanosecond*/
)
{

    try
    {
        tstamp = std::format( "{:04}{:02}{:02}{:02}{:02}{:02}{:09}",
                              uttime.tm_year + 1900,
                              uttime.tm_mon + 1,
                              uttime.tm_mday,
                              uttime.tm_hour,
                              uttime.tm_min,
                              uttime.tm_sec,
                              ts_nsec );
    }
    catch( ... )
    {
        std::throw_with_nested( xwcException( "error from std::format", -12 ) );
    }
}

/// Get the filename timestamp and the breakdown for a time.
/** Fills in the \p tstamp string with the timestamp encoded as
 * \verbatim
 *      YYYYMMDDHHMMSSNNNNNNNNN
 * \endverbatim
 * and the broken down `tm` structure \p uttime
 *
 * \returns 0 on success
 * \returns -1 if gmtime_r returns an error
 * \returns -2 if snprintf returns an error
 * \returns -3 if snprintf does not write enough characters
 *
 * \b Tests
 *     - Getting timestamp string and broken-down time for a given time \ref
 * tests_libMagAOX_file_fileTimes_timestamp_bdtime "[test doc]"
 *     - Getting timestamp and broken-down time with errors \ref
 * tests_libMagAOX_file_fileTimes_parse_filenames_timestamp_bdtime_errors "[test doc]"
 *     - Getting timestamp string only for a given time \ref tests_libMagAOX_file_fileTimes_timestamp_only "[test doc]"
 *     - Getting filename and relative path for a given time \ref tests_libMagAOX_file_fileTimes_filename_relpath_time
 * "[test doc]"
 *     - Getting filename and relative path with errors \ref
 * tests_libMagAOX_file_fileTimes_parse_filename_relpath_only_errors "[test doc]"
 */
inline void timestamp( std::string &tstamp, /**< [out] the timestamp string*/
                       tm          &uttime, /**< [out] the broken down time*/
                       time_t       ts_sec, /**< [in] the unix time second*/
                       long         ts_nsec /**< [in] the nanosecond*/
)
{
    if( gmtime_r( &ts_sec, &uttime ) == 0 )
    {
        throw xwcException( "error getting UT time (gmtime_r returned 0)", -1 );
    }

    try
    {
        timestamp( tstamp, uttime, ts_nsec );
    }
    catch( ... )
    {
        std::throw_with_nested( xwcException( "error getting timestamp from broken down time", -5 ) );
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
 * \returns 0 on success
 * \returns -1 if gmtime_r returns an error
 * \returns -2 if snprintf returns an error
 * \returns -3 if snprintf does not write enough characters
 *
 * \b Tests
 *     - Getting timestamp string only for a given time \ref tests_libMagAOX_file_fileTimes_timestamp_only "[test doc]"
 *     - Getting timestamp only with errors \ref tests_libMagAOX_file_fileTimes_parse_filenames_timestamp_only_errors
 * "[test doc]"
 */
inline void timestamp( std::string &tstamp, /**< [out] the timestamp string*/
                       time_t       ts_sec, /**< [in] the unix time second*/
                       long         ts_nsec /**< [in] the nanosecond*/
)
{
    tm uttime;
    internal::initbdtime( uttime );

    try
    {
        timestamp( tstamp, uttime, ts_sec, ts_nsec );
    }
    catch( ... )
    {
        std::throw_with_nested( xwcException( "error getting timestamp", -6 ) );
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
inline void fileTimeRelPath( std::string &tstamp,  /**< [out] */
                             std::string &relPath, /**< [out] */
                             time_t       ts_sec,  /**< [in] the unix time second*/
                             long         ts_nsec  /**< [in] the nanosecond*/
)
{
    tm uttime;
    internal::initbdtime( uttime );

    try
    {
        timestamp( tstamp, uttime, ts_sec, ts_nsec );
    }
    catch( ... )
    {
        std::throw_with_nested( xwcException( "Error from timestamp", -7 ) );
    }

    try
    {
        relPath = std::format( "{:04}_{:02}_{:02}", uttime.tm_year + 1900, uttime.tm_mon + 1, uttime.tm_mday );
    }
    catch( ... )
    {
        std::throw_with_nested( xwcException( "error formatting timestamp string", -8 ) );
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
 *     - Getting filename and relative path for a given time \ref tests_libMagAOX_file_fileTimes_filename_relpath_time
 * "[test doc]"
 *     - Getting filename and relative path with errors \ref
 * tests_libMagAOX_file_fileTimes_parse_filename_relpath_only_errors "[test doc]"
 *     - Getting filename and relative path for a given time with errors \ref
 * tests_libMagAOX_file_fileTimes_parse_filenames_relpath_errors "[test doc]"../libMagAOX/logger/tests/logMap_test
 */
inline void fileTimeRelPath( std::string       &fileName, /**< [out] the resulting file name*/
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
        fileTimeRelPath( tstamp, tmprelpath, ts_sec, ts_nsec );
    }
    catch( ... )
    {
        std::throw_with_nested( xwcException( "Error from fileTimeRelPath", -17 ) );
    }

    try
    {
        relPath  = devName + '/' + tmprelpath;
        fileName = devName + '_' + tstamp + '.' + ext;
    }
    catch( ... )
    {
        std::throw_with_nested( xwcException( "std::string error assembling paths", -18 ) );
    }
}

/// Parse a standard XWCTk timestamp string
/** Extracts the date components.
 *
 * The input muse be exactly 23 characters long.
 *
 * No validity checks are done on the components.
 *
 * \returns 0 on success
 * \returns -1 on error
 *
 * \b Tests
 *     - Parsing filenames, paths and timestamps \ref tests_libMagAOX_file_fileTimes_parse_filenames_timestamps "[test
 * doc]"
 */
inline void parseTimestamp( std::string       &YYYY,  /**< [out] the 4 digit year*/
                            std::string       &MM,    /**< [out] the 2 digit month*/
                            std::string       &DD,    /**< [out] the 2 digit day*/
                            std::string       &hh,    /**< [out] the 2 digit hour*/
                            std::string       &mm,    /**< [out] the 2 digit minute*/
                            std::string       &ss,    /**< [out] the 2 digit second*/
                            std::string       &nn,    /**< [out] the 9 digit nanosecond*/
                            const std::string &tstamp /**< [in] the 23-digit timestamp */
)
{
    if( tstamp.length() != 23 )
    {
        throw xwcException( "timestamp does not have 23 characters", -19 );
    }

    try
    {
        YYYY = tstamp.substr( 0, 4 );
        MM   = tstamp.substr( 4, 2 );
        DD   = tstamp.substr( 6, 2 );
        hh   = tstamp.substr( 8, 2 );
        mm   = tstamp.substr( 10, 2 );
        ss   = tstamp.substr( 12, 2 );
        nn   = tstamp.substr( 14, 9 );
    }
    catch( ... )
    {
        std::throw_with_nested( xwcException( "error from std::string::substr for " + tstamp ) );
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
 *     - Parsing filenames, paths and timestamps \ref tests_libMagAOX_file_fileTimes_parse_filenames_timestamps "[test
 * doc]"
 */
inline void parseFilePath( std::string       &devName, /**< [out] the device name */
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
    size_t est = fname.rfind( '.' ); // throws nothing
    if( est == std::string::npos )
    {
        est = fname.size(); // no extension
    }

    size_t dst;
    size_t dend;

    try
    {
#ifdef XWCTEST_FILETIMES_ERR20
        throw std::runtime_error( "XWCTEST_FILETIMES_ERR20" );
#endif

        dend = fname.rfind( '_', est );
    }
    catch( ... )
    {
        std::throw_with_nested( xwcException( "error from std::string::rfind", -20 ) ); /*tested*/
    }

    if( dend == std::string::npos ) // no device name, just a timestamp
    {
        try
        {
#ifdef XWCTEST_FILETIMES_ERR21
            throw std::runtime_error( "XWCTEST_FILETIMES_ERR21" );
#endif
                dst = fname.rfind( '/', est );
        }
        catch( ... )
        {
            std::throw_with_nested( xwcException( "error from std::string::rfind", -21 ) ); /*tested*/
        }
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
        try
        {
#ifdef XWCTEST_FILETIMES_ERR22
            throw std::runtime_error( "XWCTEST_FILETIMES_ERR22" );
#endif
                dst = fname.rfind( '/', dend );
        }
        catch( ... )
        {
            std::throw_with_nested( xwcException( "error from std::string::rfind", -22 ) );/*tested*/
        }
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
            try
            {
#ifdef XWCTEST_FILETIMES_ERR23
                throw std::runtime_error( "XWCTEST_FILETIMES_ERR23" );
#endif
                    devName = fname.substr( dst, dend - dst );
            }
            catch( ... )
            {
                std::throw_with_nested( xwcException( "error from std::string::substr", -23 ) );/*tested*/
            }
        }

        dst  = dend + 1; // now move to beginning of timestamp
        dend = est;      // and one-past-the-end of the timestamp
    }

    // Here dst...dend should be just the timestamp
    if( dend - dst != 23 )
    {
        throw xwcException( "MagAOX::file::parseFilePath: timestamp does not have 23 characters", -24 ); /*tested*/
    }

    try
    {
#ifdef XWCTEST_FILETIMES_ERR25
        ++dst; // this will generate a too-short exception
#endif

        parseTimestamp( YYYY, MM, DD, hh, mm, ss, nn, fname.substr( dst, dend - dst ) );
    }
    catch( ... )
    {
        std::throw_with_nested( xwcException( "error parsing timestamp", -25 ) ); /*tested*/
    }
}

#ifdef XWCTEST_NAMESPACE
}
#endif

} // namespace file
} // namespace MagAOX

#endif // file_fileTimes_hpp
