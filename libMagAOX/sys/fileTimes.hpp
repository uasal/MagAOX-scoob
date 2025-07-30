/** \file fileTimes.hpp
 * \brief Utilities for working with file timestamps
 * \ingroup sys_files
 */

#ifndef sys_fileTimes_hpp
#define sys_fileTimes_hpp

#include <time.h>

#include <iostream>

namespace MagAOX
{
namespace sys
{

// These are exposed just to enable testing of error handling
// don't change
#ifndef XWC_TIMESTAMP_BUFFER_SIZE
    #define XWC_TIMESTAMP_BUFFER_SIZE ( 48 )
#endif

#ifndef XWC_RELPATH_BUFFER_SIZE
    #define XWC_RELPATH_BUFFER_SIZE ( 16 )
#endif

namespace internal
{
inline
void initbdtime( tm &bt )
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
 *     - Getting timestamp string and broken-down time for a given time \ref tests_libMagAOX_sys_fileTimes_timestamp_bdtime "[test doc]"
 *     - Getting timestamp and broken-down time with errors \ref tests_libMagAOX_sys_fileTimes_parse_filenames_timestamp_bdtime_errors "[test doc]"
 *     - Getting timestamp string only for a given time \ref tests_libMagAOX_sys_fileTimes_timestamp_only "[test doc]"
 *     - Getting filename and relative path for a given time \ref tests_libMagAOX_sys_fileTimes_filename_relpath_time "[test doc]"
 *     - Getting filename and relative path with errors \ref tests_libMagAOX_sys_fileTimes_parse_filename_relpath_only_errors "[test doc]"
 */
inline int timestamp( std::string &tstamp, /**< [out] the timestamp string*/
               tm          &uttime, /**< [out] the broken down time*/
               time_t       ts_sec, /**< [in] the unix time second*/
               long         ts_nsec /**< [in] the nanosecond*/
)
{
    if( gmtime_r( &ts_sec, &uttime ) == 0 )
    {
        std::cerr << "Error getting UT time (gmtime_r returned 0). At: " << __FILE__ << " " << __LINE__ << "\n";
        return -1;
    }

    char buffer[XWC_TIMESTAMP_BUFFER_SIZE];

    int wrt = snprintf( buffer,
                        sizeof( buffer ),
                        "%04i%02i%02i%02i%02i%02i%09li",
                        uttime.tm_year + 1900,
                        uttime.tm_mon + 1,
                        uttime.tm_mday,
                        uttime.tm_hour,
                        uttime.tm_min,
                        uttime.tm_sec,
                        ts_nsec );

    if( wrt < 0 )
    {
        std::cerr << "Error writing formatted timestamp: " << __FILE__ << " " << __LINE__ << "\n";
        return -2;
    }
    else if( !( static_cast<size_t>( wrt ) < sizeof( buffer ) ) )
    {
        std::cerr << "Formatted timestamp buffer not long enough: " << __FILE__ << " " << __LINE__ << "\n";
        return -3;
    }

    tstamp = buffer;

    return 0;
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
 *     - Getting timestamp string only for a given time \ref tests_libMagAOX_sys_fileTimes_timestamp_only "[test doc]"
 *     - Getting timestamp only with errors \ref tests_libMagAOX_sys_fileTimes_parse_filenames_timestamp_only_errors "[test doc]"
 */
inline int timestamp( std::string &tstamp, /**< [out] the timestamp string*/
               time_t       ts_sec, /**< [in] the unix time second*/
               long         ts_nsec /**< [in] the nanosecond*/
)
{
    tm uttime;
    internal::initbdtime( uttime );

    return timestamp( tstamp, uttime, ts_sec, ts_nsec );
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
 *     - Getting filename and relative path for a given time \ref tests_libMagAOX_sys_fileTimes_filename_relpath_time "[test doc]"
 */
inline int fileTimeRelPath( std::string &tstamp,  /**< [out] */
                     std::string &relPath, /**< [out] */
                     time_t       ts_sec,  /**< [in] the unix time second*/
                     long         ts_nsec  /**< [in] the nanosecond*/
)
{
    tm uttime;
    internal::initbdtime( uttime );

    int rv = timestamp( tstamp, uttime, ts_sec, ts_nsec );

    if( rv < 0 )
    {
        std::cerr << "Error from timestamp: " << __FILE__ << " " << __LINE__ << "\n";
        return rv;
    }

    char buffer[XWC_RELPATH_BUFFER_SIZE];

    int wrt = snprintf(
        buffer, sizeof( buffer ), "%04i_%02i_%02i", uttime.tm_year + 1900, uttime.tm_mon + 1, uttime.tm_mday );

    if( wrt < 0 )
    {
        std::cerr << "Error writing formatted relPath: " << __FILE__ << " " << __LINE__ << "\n";
        return -4;
    }
    else if( !( static_cast<size_t>( wrt ) < sizeof( buffer ) ) )
    {
        std::cerr << "Formatted relPath buffer not long enough: " << __FILE__ << " " << __LINE__ << "\n";
        return -5;
    }

    relPath = buffer;

    return 0;
}

/// Construct the filename and full relative path based on a time and a device name and extension
/**  Fills in the fileName string with the timestamp encoded as
 * \verbatim
 *      devName_YYYYMMDDHHMMSSNNNNNNNNN.ext
 * \endverbatim
 * and the \p relPath string with the format
 * \verbatim
 *      devName/YYYY/MM/DD
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
 *     - Getting filename and relative path for a given time \ref tests_libMagAOX_sys_fileTimes_filename_relpath_time "[test doc]"
 *     - Getting filename and relative path with errors \ref tests_libMagAOX_sys_fileTimes_parse_filename_relpath_only_errors "[test doc]"
 *     - Getting filename and relative path for a given time with errors \ref tests_libMagAOX_sys_fileTimes_parse_filenames_relpath_errors "[test doc]"
 */
inline int fileTimeRelPath( std::string       &fileName, /**< [out] the resulting file name*/
                     std::string       &relPath,  /**< [out] the resulting relative path*/
                     const std::string &devName,  /**< [in] the device name part of the path.  No '/'. */
                     const std::string &ext,      /**< [in] the extension part of the filename. No `.`. */
                     time_t             ts_sec,   /**< [in] the unix time second*/
                     long               ts_nsec   /**< [in] the nanosecond*/
)
{
    std::string tstamp, tmprelpath;
    int         rv = fileTimeRelPath( tstamp, tmprelpath, ts_sec, ts_nsec );

    if( rv < 0 )
    {
        std::cerr << "Error from fileTimeRelPath: " << __FILE__ << " " << __LINE__ << "\n";
        return rv;
    }

    relPath  = devName + '/' + tmprelpath;
    fileName = devName + '_' + tstamp + '.' + ext;

    return 0;
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
 *     - Parsing filenames, paths and timestamps \ref tests_libMagAOX_sys_fileTimes_parse_filenames_timestamps "[test doc]"
 */
inline int parseTimestamp( std::string       &YYYY,  /**< [out] the 4 digit year*/
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
        std::cerr << "MagAOX::sys::parseTimestamp: timestamp does not have 23 characters.\n";
        std::cerr << __FILE__ << ' ' << __LINE__ << '\n';
        return -1;
    }

    YYYY = tstamp.substr( 0, 4 );
    MM   = tstamp.substr( 4, 2 );
    DD   = tstamp.substr( 6, 2 );
    hh   = tstamp.substr( 8, 2 );
    mm   = tstamp.substr( 10, 2 );
    ss   = tstamp.substr( 12, 2 );
    nn   = tstamp.substr( 14, 9 );

    return 0;
}

/// Parse a standard XWCTk timestamp filepath
/** Extracts the device name and the date components.
 * The only restriction on the input \fname is that be at least 23 characters long.
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
 *     - Parsing filenames, paths and timestamps \ref tests_libMagAOX_sys_fileTimes_parse_filenames_timestamps "[test doc]"
 */
inline int parseFilePath( std::string       &devName, /**< [out] the device name */
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
    size_t est = fname.rfind( '.' );
    if( est == std::string::npos )
    {
        est = fname.size(); // no extension
    }

    size_t dst;
    size_t dend = fname.rfind( '_', est );

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
            devName = fname.substr( dst, dend - dst );
        }

        dst  = dend + 1; // now move to beginning of timestamp
        dend = est;      // and one-past-the-end of the timestamp
    }

    // Here dst...dend should be just the timestamp
    if( dend - dst != 23 )
    {
        std::cerr << "MagAOX::sys::parseFilePath: timestamp does not have 23 characters.\n";
        std::cerr << __FILE__ << ' ' << __LINE__ << '\n';
        return -1;
    }

    return parseTimestamp( YYYY, MM, DD, hh, mm, ss, nn, fname.substr( dst, dend - dst ) );
}

} // namespace sys
} // namespace MagAOX

#endif // sys_fileTimes_hpp
