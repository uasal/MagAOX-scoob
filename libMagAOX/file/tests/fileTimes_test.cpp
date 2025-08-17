/** \file fileTimes_test.hpp
 * \brief Tests for file timestamps
 * \ingroup file_files
 */

#include "../../../tests/catch2/catch.hpp"

#include <iostream>

#include "../fileTimes.hpp"

#undef file_fileTimes_hpp
#define XWCTEST_NAMESPACE XWCTEST_TIMESTAMP_THROW_BAD_ALLOC_ns
#define XWCTEST_TIMESTAMP_THROW_BAD_ALLOC
#include "../fileTimes.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_TIMESTAMP_THROW_BAD_ALLOC

#undef file_fileTimes_hpp
#define XWCTEST_NAMESPACE XWCTEST_TIMESTAMP_THROW_FORMAT_ERROR_ns
#define XWCTEST_TIMESTAMP_THROW_FORMAT_ERROR
#include "../fileTimes.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_TIMESTAMP_THROW_FORMAT_ERROR

#undef file_fileTimes_hpp
#define XWCTEST_NAMESPACE XWCTEST_TIMESTAMP_THROW_EXCEPTION_ns
#define XWCTEST_TIMESTAMP_THROW_EXCEPTION
#include "../fileTimes.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_TIMESTAMP_THROW_EXCEPTION

#undef file_fileTimes_hpp
#define XWCTEST_NAMESPACE XWCTEST_FILETIMERELPATH_THROW_BAD_ALLOC_ns
#define XWCTEST_FILETIMERELPATH_THROW_BAD_ALLOC
#include "../fileTimes.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_FILETIMERELPATH_THROW_BAD_ALLOC

#undef file_fileTimes_hpp
#define XWCTEST_NAMESPACE XWCTEST_FILETIMERELPATH_THROW_FORMAT_ERROR_ns
#define XWCTEST_FILETIMERELPATH_THROW_FORMAT_ERROR
#include "../fileTimes.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_FILETIMERELPATH_THROW_FORMAT_ERROR

#undef file_fileTimes_hpp
#define XWCTEST_NAMESPACE XWCTEST_FILETIMERELPATH_THROW_EXCEPTION_ns
#define XWCTEST_FILETIMERELPATH_THROW_EXCEPTION
#include "../fileTimes.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_FILETIMERELPATH_THROW_EXCEPTION

#undef file_fileTimes_hpp
#define XWCTEST_NAMESPACE XWCTEST_FILETIMERELPATHSTRING_THROW_BAD_ALLOC_ns
#define XWCTEST_FILETIMERELPATHSTRING_THROW_BAD_ALLOC
#include "../fileTimes.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_FILETIMERELPATHSTRING_THROW_BAD_ALLOC

#undef file_fileTimes_hpp
#define XWCTEST_NAMESPACE XWCTEST_FILETIMERELPATHSTRING_THROW_EXCEPTION_ns
#define XWCTEST_FILETIMERELPATHSTRING_THROW_EXCEPTION
#include "../fileTimes.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_FILETIMERELPATHSTRING_THROW_EXCEPTION

#undef file_fileTimes_hpp
#define XWCTEST_NAMESPACE XWCTEST_PARSETIMESTAMP_THROW_BAD_ALLOC_ns
#define XWCTEST_PARSETIMESTAMP_THROW_BAD_ALLOC
#include "../fileTimes.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_PARSETIMESTAMP_THROW_BAD_ALLOC

#undef file_fileTimes_hpp
#define XWCTEST_NAMESPACE XWCTEST_PARSETIMESTAMP_THROW_OUT_OF_RANGE_ns
#define XWCTEST_PARSETIMESTAMP_THROW_OUT_OF_RANGE
#include "../fileTimes.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_PARSETIMESTAMP_THROW_OUT_OF_RANGE

#undef file_fileTimes_hpp
#define XWCTEST_NAMESPACE XWCTEST_PARSETIMESTAMP_THROW_EXCEPTION_ns
#define XWCTEST_PARSETIMESTAMP_THROW_EXCEPTION
#include "../fileTimes.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_PARSETIMESTAMP_THROW_EXCEPTION

#undef file_fileTimes_hpp
#define XWCTEST_NAMESPACE XWCTEST_PARSEFILEPATH_THROW_BAD_ALLOC_ns
#define XWCTEST_PARSEFILEPATH_THROW_BAD_ALLOC
#include "../fileTimes.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_PARSEFILEPATH_THROW_BAD_ALLOC

#undef file_fileTimes_hpp
#define XWCTEST_NAMESPACE XWCTEST_PARSEFILEPATH_THROW_OUT_OF_RANGE_ns
#define XWCTEST_PARSEFILEPATH_THROW_OUT_OF_RANGE
#include "../fileTimes.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_PARSEFILEPATH_THROW_OUT_OF_RANGE

#undef file_fileTimes_hpp
#define XWCTEST_NAMESPACE XWCTEST_PARSEFILEPATH_THROW_EXCEPTION_ns
#define XWCTEST_PARSEFILEPATH_THROW_EXCEPTION
#include "../fileTimes.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_PARSEFILEPATH_THROW_EXCEPTION

/** \test Scenario: Getting timestamp string and broken-down time for a given time
 *
 * \anchor tests_libMagAOX_file_fileTimes_timestamp_bdtime
 */
SCENARIO( "Getting timestamp string and broken-down time for a given time", "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "A time with 0 sec and 0 nsec" )
    {
        time_t        ts_sec  = 1732170780;
        unsigned long ts_nsec = 0;

        std::string tstamp;
        tm          uttime;

        mx::error_t errc = MagAOX::file::timestamp( tstamp, uttime, ts_sec, ts_nsec );

        REQUIRE( errc == mx::error_t::noerror );
        REQUIRE( tstamp == "20241121063300000000000" );
        REQUIRE( uttime.tm_year == 124 );
        REQUIRE( uttime.tm_mon == 10 );
        REQUIRE( uttime.tm_mday == 21 );
        REQUIRE( uttime.tm_hour == 6 );
        REQUIRE( uttime.tm_min == 33 );
        REQUIRE( uttime.tm_sec == 0 );
    }

    GIVEN( "A time with non-0 sec and 0 nsec" )
    {
        time_t        ts_sec  = 1732170781;
        unsigned long ts_nsec = 0;

        std::string tstamp;
        tm          uttime;

        mx::error_t errc = MagAOX::file::timestamp( tstamp, uttime, ts_sec, ts_nsec );

        REQUIRE( errc == mx::error_t::noerror );
        REQUIRE( tstamp == "20241121063301000000000" );
        REQUIRE( uttime.tm_year == 124 );
        REQUIRE( uttime.tm_mon == 10 );
        REQUIRE( uttime.tm_mday == 21 );
        REQUIRE( uttime.tm_hour == 6 );
        REQUIRE( uttime.tm_min == 33 );
        REQUIRE( uttime.tm_sec == 1 );
    }

    GIVEN( "A time with non-0 sec and 9-digit nsec" )
    {
        time_t        ts_sec  = 1732170785;
        unsigned long ts_nsec = 434878292;

        std::string tstamp;
        tm          uttime;

        mx::error_t errc = MagAOX::file::timestamp( tstamp, uttime, ts_sec, ts_nsec );

        REQUIRE( errc == mx::error_t::noerror );
        REQUIRE( tstamp == "20241121063305434878292" );
        REQUIRE( uttime.tm_year == 124 );
        REQUIRE( uttime.tm_mon == 10 );
        REQUIRE( uttime.tm_mday == 21 );
        REQUIRE( uttime.tm_hour == 6 );
        REQUIRE( uttime.tm_min == 33 );
        REQUIRE( uttime.tm_sec == 5 );
    }

    GIVEN( "A time with non-0 sec and 3-digit nsec" )
    {
        time_t        ts_sec  = 1732170785;
        unsigned long ts_nsec = 292;

        std::string tstamp;
        tm          uttime;

        mx::error_t errc = MagAOX::file::timestamp( tstamp, uttime, ts_sec, ts_nsec );

        REQUIRE( errc == mx::error_t::noerror );
        REQUIRE( tstamp == "20241121063305000000292" );
        REQUIRE( uttime.tm_year == 124 );
        REQUIRE( uttime.tm_mon == 10 );
        REQUIRE( uttime.tm_mday == 21 );
        REQUIRE( uttime.tm_hour == 6 );
        REQUIRE( uttime.tm_min == 33 );
        REQUIRE( uttime.tm_sec == 5 );
    }
}

/** \test Scenario: Getting timestamp and broken-down time with errors
 *
 * This is in a separate file due to need to define a buffer size too small to generate errors
 *
 * \anchor tests_libMagAOX_file_fileTimes_timestamp_bdtime_errors
 */
SCENARIO( "Getting timestamp and broken-down time with errors", "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "A year that's too big (gmtime_r error)" )
    {
        time_t        ts_sec  = 1.355388599402496e+17; // huge year
        unsigned long ts_nsec = 0;

        std::string tstamp;
        tm          uttime;

        mx::error_t errc = MagAOX::file::timestamp( tstamp, uttime, ts_sec, ts_nsec );

        REQUIRE( errc == mx::error_t::eoverflow );
    }
}

/** \test Scenario: Getting timestamp only with errors
 *
 * This is in a separate file due to need to define a buffer size too small to generate errors
 *
 * \anchor tests_libMagAOX_file_fileTimes_timestamp_only_errors
 */
SCENARIO( "Getting timestamp only with errors", "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "A year that's too big (gmtime_r error)" )
    {
        time_t        ts_sec  = 1.355388599402496e+17; // huge year
        unsigned long ts_nsec = 0;

        std::string tstamp;

        mx::error_t errc = MagAOX::file::timestamp( tstamp, ts_sec, ts_nsec );

        REQUIRE( errc == mx::error_t::eoverflow );
    }
}

/** \test Scenario: Getting timestamp string and broken-down time causes bad_alloc
 *
 * \anchor tests_libMagAOX_file_fileTimes_timestamp_bdtime_bad_alloc
 */
SCENARIO( "Getting timestamp string and broken-down time causes bad_alloc", "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "A time with non-0 sec and 3-digit nsec" )
    {
        time_t        ts_sec  = 1732170785;
        unsigned long ts_nsec = 292;

        std::string tstamp;
        tm          uttime;

        bool caught = false;

        try
        {
            MagAOX::file::XWCTEST_TIMESTAMP_THROW_BAD_ALLOC_ns::timestamp( tstamp, uttime, ts_sec, ts_nsec );
        }
        catch( const MagAOX::xwcException &e )
        {
            caught = true;
        }

        REQUIRE( caught == true );
    }
}

/** \test Scenario: Getting timestamp only causes bad_alloc
 *
 * \anchor tests_libMagAOX_file_fileTimes_timestamp_only_bad_alloc
 */
SCENARIO( "Getting timestamp only causes bad_alloc", "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "A time with non-0 sec and 3-digit nsec" )
    {
        time_t        ts_sec  = 1732170785;
        unsigned long ts_nsec = 292;

        std::string tstamp;

        bool caught = false;

        try
        {
            MagAOX::file::XWCTEST_TIMESTAMP_THROW_BAD_ALLOC_ns::timestamp( tstamp, ts_sec, ts_nsec );
        }
        catch( const MagAOX::xwcException &e )
        {
            caught = true;
        }

        REQUIRE( caught == true );
    }
}

/** \test Scenario: Getting timestamp string and broken-down time causes filesystem_error
 *
 * \anchor tests_libMagAOX_file_fileTimes_timestamp_bdtime_filesystem_error
 */
SCENARIO( "Getting timestamp string and broken-down time causes filesystem_error", "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "A time with non-0 sec and 3-digit nsec" )
    {
        time_t        ts_sec  = 1732170785;
        unsigned long ts_nsec = 292;

        std::string tstamp;
        tm          uttime;

        mx::error_t errc =
            MagAOX::file::XWCTEST_TIMESTAMP_THROW_FORMAT_ERROR_ns::timestamp( tstamp, uttime, ts_sec, ts_nsec );

        REQUIRE( errc == mx::error_t::std_format_error );
    }
}

/** \test Scenario: Getting timestamp only causes filesystem_error
 *
 * \anchor tests_libMagAOX_file_fileTimes_timestamp_only_filesystem_error
 */
SCENARIO( "Getting timestamp only causes filesystem_error", "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "A time with non-0 sec and 3-digit nsec" )
    {
        time_t        ts_sec  = 1732170785;
        unsigned long ts_nsec = 292;

        std::string tstamp;

        mx::error_t errc = MagAOX::file::XWCTEST_TIMESTAMP_THROW_FORMAT_ERROR_ns::timestamp( tstamp, ts_sec, ts_nsec );

        REQUIRE( errc == mx::error_t::std_format_error );
    }
}

/** \test Scenario: Getting timestamp string and broken-down time causes exception
 *
 * \anchor tests_libMagAOX_file_fileTimes_timestamp_bdtime_exception
 */
SCENARIO( "Getting timestamp string and broken-down time causes exception", "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "A time with non-0 sec and 3-digit nsec" )
    {
        time_t        ts_sec  = 1732170785;
        unsigned long ts_nsec = 292;

        std::string tstamp;
        tm          uttime;

        mx::error_t errc =
            MagAOX::file::XWCTEST_TIMESTAMP_THROW_EXCEPTION_ns::timestamp( tstamp, uttime, ts_sec, ts_nsec );

        REQUIRE( errc == mx::error_t::std_exception );
    }
}

/** \test Scenario: Getting timestamp only causes exception
 *
 * \anchor tests_libMagAOX_file_fileTimes_timestamp_only_exception
 */
SCENARIO( "Getting timestamp only causes exception", "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "A time with non-0 sec and 3-digit nsec" )
    {
        time_t        ts_sec  = 1732170785;
        unsigned long ts_nsec = 292;

        std::string tstamp;

        mx::error_t errc = MagAOX::file::XWCTEST_TIMESTAMP_THROW_EXCEPTION_ns::timestamp( tstamp, ts_sec, ts_nsec );

        REQUIRE( errc == mx::error_t::std_exception );
    }
}

/** \test Scenario: Getting filename and relative path for a given time
 *
 * \anchor tests_libMagAOX_file_fileTimes_filename_relpath_time
 */
SCENARIO( "Getting filename and relative path for a given time", "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "A time with 0 sec and 0 nsec" )
    {
        time_t        ts_sec  = 1732170780;
        unsigned long ts_nsec = 0;

        std::string fileName, relPath;

        mx::error_t errc = MagAOX::file::fileTimeRelPath( fileName, relPath, "tdevice", "txt", ts_sec, ts_nsec );

        REQUIRE( errc == mx::error_t::noerror );
        REQUIRE( fileName == "tdevice_20241121063300000000000.txt" );
        REQUIRE( relPath == "tdevice/2024_11_21" );
    }
}

/** \test Scenario: Getting filename and relative path for a given time causes overflow
 *
 * \anchor tests_libMagAOX_file_fileTimes_filename_relpath_time_overflow
 */
SCENARIO( "Getting filename and relative path for a given time causes overflow", "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "A year that's too big (gmtime_r error)" )
    {
        time_t        ts_sec  = 1.355388599402496e+17; // huge year
        unsigned long ts_nsec = 0;

        std::string fileName, relPath;

        mx::error_t errc = MagAOX::file::fileTimeRelPath( fileName, relPath, "tdevice", "txt", ts_sec, ts_nsec );

        REQUIRE( errc == mx::error_t::eoverflow );
    }
}

/** \test Scenario: Getting filename and relative path for a given time causes bad_alloc in timestamp
 *
 * \anchor tests_libMagAOX_file_fileTimes_filename_relpath_time_timestamp_bad_alloc
 */
SCENARIO( "Getting filename and relative path for a given time causes bad_alloc in timestamp",
          "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "A time with 0 sec and 0 nsec" )
    {
        time_t        ts_sec  = 1732170780;
        unsigned long ts_nsec = 0;

        std::string fileName, relPath;

        bool caught = false;
        try
        {
            MagAOX::file::XWCTEST_TIMESTAMP_THROW_BAD_ALLOC_ns::fileTimeRelPath(
                fileName, relPath, "tdevice", "txt", ts_sec, ts_nsec );
        }
        catch( const MagAOX::xwcException &e )
        {
            caught = true;
        }

        REQUIRE( caught == true );
    }
}

/** \test Scenario: Getting filename and relative path for a given time causes format_error in timestamp
 *
 * \anchor tests_libMagAOX_file_fileTimes_filename_relpath_time_timestamp_format_error
 */
SCENARIO( "Getting filename and relative path for a given time causes format_error in timestamp",
          "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "A time with 0 sec and 0 nsec" )
    {
        time_t        ts_sec  = 1732170780;
        unsigned long ts_nsec = 0;

        std::string fileName, relPath;

        mx::error_t errc = MagAOX::file::XWCTEST_TIMESTAMP_THROW_FORMAT_ERROR_ns::fileTimeRelPath(
            fileName, relPath, "tdevice", "txt", ts_sec, ts_nsec );

        REQUIRE( errc == mx::error_t::std_format_error );
    }
}

/** \test Scenario: Getting filename and relative path for a given time causes exception in timestamp
 *
 * \anchor tests_libMagAOX_file_fileTimes_filename_relpath_time_timestamp_exception
 */
SCENARIO( "Getting filename and relative path for a given time causes exception in timestamp",
          "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "A time with 0 sec and 0 nsec" )
    {
        time_t        ts_sec  = 1732170780;
        unsigned long ts_nsec = 0;

        std::string fileName, relPath;

        mx::error_t errc = MagAOX::file::XWCTEST_TIMESTAMP_THROW_EXCEPTION_ns::fileTimeRelPath(
            fileName, relPath, "tdevice", "txt", ts_sec, ts_nsec );

        REQUIRE( errc == mx::error_t::std_exception );
    }
}

/** \test Scenario: Getting filename and relative path for a given time causes bad_alloc in top relpath
 *
 * \anchor tests_libMagAOX_file_fileTimes_filename_relpath_time_top_rp_bad_alloc
 */
SCENARIO( "Getting filename and relative path for a given time causes bad_alloc in top relpath",
          "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "A time with 0 sec and 0 nsec" )
    {
        time_t        ts_sec  = 1732170780;
        unsigned long ts_nsec = 0;

        std::string fileName, relPath;

        bool caught = false;
        try
        {
            MagAOX::file::XWCTEST_FILETIMERELPATH_THROW_BAD_ALLOC_ns::fileTimeRelPath(
                fileName, relPath, "tdevice", "txt", ts_sec, ts_nsec );
        }
        catch( const MagAOX::xwcException &e )
        {
            caught = true;
        }

        REQUIRE( caught == true );
    }
}

/** \test Scenario: Getting filename and relative path for a given time causes bad_alloc in relpath string
 *
 * \anchor tests_libMagAOX_file_fileTimes_filename_relpath_time_rpstring_bad_alloc
 */
SCENARIO( "Getting filename and relative path for a given time causes bad_alloc in relpath string",
          "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "A time with 0 sec and 0 nsec" )
    {
        time_t        ts_sec  = 1732170780;
        unsigned long ts_nsec = 0;

        std::string fileName, relPath;

        bool caught = false;
        try
        {
            MagAOX::file::XWCTEST_FILETIMERELPATHSTRING_THROW_BAD_ALLOC_ns::fileTimeRelPath(
                fileName, relPath, "tdevice", "txt", ts_sec, ts_nsec );
        }
        catch( const MagAOX::xwcException &e )
        {
            caught = true;
        }

        REQUIRE( caught == true );
    }
}

/** \test Scenario: Getting filename and relative path for a given time causes format_error in top relpath
 *
 * \anchor tests_libMagAOX_file_fileTimes_filename_relpath_time_top_rp_format_error
 */
SCENARIO( "Getting filename and relative path for a given time causes format_error in top relpath",
          "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "A time with 0 sec and 0 nsec" )
    {
        time_t        ts_sec  = 1732170780;
        unsigned long ts_nsec = 0;

        std::string fileName, relPath;

        mx::error_t errc = MagAOX::file::XWCTEST_FILETIMERELPATH_THROW_FORMAT_ERROR_ns::fileTimeRelPath(
            fileName, relPath, "tdevice", "txt", ts_sec, ts_nsec );

        REQUIRE( errc == mx::error_t::std_format_error );
    }
}

/** \test Scenario: Getting filename and relative path for a given time causes exception in top relpath
 *
 * \anchor tests_libMagAOX_file_fileTimes_filename_relpath_time_top_rp_exception
 */
SCENARIO( "Getting filename and relative path for a given time causes exception in top relpath",
          "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "A time with 0 sec and 0 nsec" )
    {
        time_t        ts_sec  = 1732170780;
        unsigned long ts_nsec = 0;

        std::string fileName, relPath;

        mx::error_t errc = MagAOX::file::XWCTEST_FILETIMERELPATH_THROW_EXCEPTION_ns::fileTimeRelPath(
            fileName, relPath, "tdevice", "txt", ts_sec, ts_nsec );

        REQUIRE( errc == mx::error_t::std_exception );
    }
}

/** \test Scenario: Getting filename and relative path for a given time causes exception in relpath string
 *
 * \anchor tests_libMagAOX_file_fileTimes_filename_relpath_time_rpstring_exception
 */
SCENARIO( "Getting filename and relative path for a given time causes exception in relpath string",
          "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "A time with 0 sec and 0 nsec" )
    {
        time_t        ts_sec  = 1732170780;
        unsigned long ts_nsec = 0;

        std::string fileName, relPath;

        mx::error_t errc = MagAOX::file::XWCTEST_FILETIMERELPATHSTRING_THROW_EXCEPTION_ns::fileTimeRelPath(
            fileName, relPath, "tdevice", "txt", ts_sec, ts_nsec );

        REQUIRE( errc == mx::error_t::std_exception );
    }
}

/** \test Scenario: Parsing filenames, paths and timestamps, with no errors
 *
 * \anchor tests_libMagAOX_file_fileTimes_parse_filenames_timestamps
 */
SCENARIO( "Parsing filenames, paths and timestamps, with no errors", "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "A valid MagAO-X filename" )
    {
        std::string devName, YYYY, MM, DD, hh, mm, ss, nn;

        mx::error_t errc =
            MagAOX::file::parseFilePath( devName, YYYY, MM, DD, hh, mm, ss, nn, "device_20241121063300000000000.txt" );

        REQUIRE( errc == mx::error_t::noerror );
        REQUIRE( devName == "device" );
        REQUIRE( YYYY == "2024" );
        REQUIRE( MM == "11" );
        REQUIRE( DD == "21" );
        REQUIRE( hh == "06" );
        REQUIRE( mm == "33" );
        REQUIRE( ss == "00" );
        REQUIRE( nn == "000000000" );
    }

    GIVEN( "A valid MagAO-X filepath" )
    {
        std::string devName, YYYY, MM, DD, hh, mm, ss, nn;

        mx::error_t errc = MagAOX::file::parseFilePath(
            devName, YYYY, MM, DD, hh, mm, ss, nn, "/path/to/device_20241121063300000000000.txt" );

        REQUIRE( errc == mx::error_t::noerror );
        REQUIRE( devName == "device" );
        REQUIRE( YYYY == "2024" );
        REQUIRE( MM == "11" );
        REQUIRE( DD == "21" );
        REQUIRE( hh == "06" );
        REQUIRE( mm == "33" );
        REQUIRE( ss == "00" );
        REQUIRE( nn == "000000000" );
    }

    GIVEN( "A valid MagAO-X filename without extension" )
    {
        std::string devName, YYYY, MM, DD, hh, mm, ss, nn;

        mx::error_t errc =
            MagAOX::file::parseFilePath( devName, YYYY, MM, DD, hh, mm, ss, nn, "device_20241121063300000000000" );

        REQUIRE( errc == mx::error_t::noerror );
        REQUIRE( devName == "device" );
        REQUIRE( YYYY == "2024" );
        REQUIRE( MM == "11" );
        REQUIRE( DD == "21" );
        REQUIRE( hh == "06" );
        REQUIRE( mm == "33" );
        REQUIRE( ss == "00" );
        REQUIRE( nn == "000000000" );
    }

    GIVEN( "A valid MagAO-X filepath without extension" )
    {
        std::string devName, YYYY, MM, DD, hh, mm, ss, nn;

        mx::error_t errc = MagAOX::file::parseFilePath(
            devName, YYYY, MM, DD, hh, mm, ss, nn, "/path/to/device_20241121063300000000000" );

        REQUIRE( errc == mx::error_t::noerror );
        REQUIRE( devName == "device" );
        REQUIRE( YYYY == "2024" );
        REQUIRE( MM == "11" );
        REQUIRE( DD == "21" );
        REQUIRE( hh == "06" );
        REQUIRE( mm == "33" );
        REQUIRE( ss == "00" );
        REQUIRE( nn == "000000000" );
    }

    GIVEN( "A valid MagAO-X filename without device, no _" )
    {
        std::string devName, YYYY, MM, DD, hh, mm, ss, nn;

        mx::error_t errc =
            MagAOX::file::parseFilePath( devName, YYYY, MM, DD, hh, mm, ss, nn, "20241121063300000000000.txt" );

        REQUIRE( errc == mx::error_t::noerror );
        REQUIRE( devName == "" );
        REQUIRE( YYYY == "2024" );
        REQUIRE( MM == "11" );
        REQUIRE( DD == "21" );
        REQUIRE( hh == "06" );
        REQUIRE( mm == "33" );
        REQUIRE( ss == "00" );
        REQUIRE( nn == "000000000" );
    }

    GIVEN( "A valid MagAO-X filepath without device, no _" )
    {
        std::string devName, YYYY, MM, DD, hh, mm, ss, nn;

        mx::error_t errc = MagAOX::file::parseFilePath(
            devName, YYYY, MM, DD, hh, mm, ss, nn, "/path/to/20241121063300000000000.txt" );

        REQUIRE( errc == mx::error_t::noerror );
        REQUIRE( devName == "" );
        REQUIRE( YYYY == "2024" );
        REQUIRE( MM == "11" );
        REQUIRE( DD == "21" );
        REQUIRE( hh == "06" );
        REQUIRE( mm == "33" );
        REQUIRE( ss == "00" );
        REQUIRE( nn == "000000000" );
    }

    GIVEN( "A valid MagAO-X filename without device or extension, no _" )
    {
        std::string devName, YYYY, MM, DD, hh, mm, ss, nn;

        mx::error_t errc =
            MagAOX::file::parseFilePath( devName, YYYY, MM, DD, hh, mm, ss, nn, "20241121063300000000000" );

        REQUIRE( errc == mx::error_t::noerror );
        REQUIRE( devName == "" );
        REQUIRE( YYYY == "2024" );
        REQUIRE( MM == "11" );
        REQUIRE( DD == "21" );
        REQUIRE( hh == "06" );
        REQUIRE( mm == "33" );
        REQUIRE( ss == "00" );
        REQUIRE( nn == "000000000" );
    }

    GIVEN( "A valid MagAO-X filepath without device or extension, no _" )
    {
        std::string devName, YYYY, MM, DD, hh, mm, ss, nn;

        mx::error_t errc =
            MagAOX::file::parseFilePath( devName, YYYY, MM, DD, hh, mm, ss, nn, "/path/to/20241121063300000000000" );

        REQUIRE( errc == mx::error_t::noerror );
        REQUIRE( devName == "" );
        REQUIRE( YYYY == "2024" );
        REQUIRE( MM == "11" );
        REQUIRE( DD == "21" );
        REQUIRE( hh == "06" );
        REQUIRE( mm == "33" );
        REQUIRE( ss == "00" );
        REQUIRE( nn == "000000000" );
    }

    GIVEN( "A valid MagAO-X filename without device, with _" )
    {
        std::string devName, YYYY, MM, DD, hh, mm, ss, nn;

        mx::error_t errc =
            MagAOX::file::parseFilePath( devName, YYYY, MM, DD, hh, mm, ss, nn, "_20241121063300000000000.txt" );

        REQUIRE( errc == mx::error_t::noerror );
        REQUIRE( devName == "" );
        REQUIRE( YYYY == "2024" );
        REQUIRE( MM == "11" );
        REQUIRE( DD == "21" );
        REQUIRE( hh == "06" );
        REQUIRE( mm == "33" );
        REQUIRE( ss == "00" );
        REQUIRE( nn == "000000000" );
    }

    GIVEN( "A valid MagAO-X filepath without device, with _" )
    {
        std::string devName, YYYY, MM, DD, hh, mm, ss, nn;

        mx::error_t errc = MagAOX::file::parseFilePath(
            devName, YYYY, MM, DD, hh, mm, ss, nn, "/path/to/_20241121063300000000000.txt" );

        REQUIRE( errc == mx::error_t::noerror );
        REQUIRE( devName == "" );
        REQUIRE( YYYY == "2024" );
        REQUIRE( MM == "11" );
        REQUIRE( DD == "21" );
        REQUIRE( hh == "06" );
        REQUIRE( mm == "33" );
        REQUIRE( ss == "00" );
        REQUIRE( nn == "000000000" );
    }

    GIVEN( "A valid MagAO-X filename without device or extension, with _" )
    {
        std::string devName, YYYY, MM, DD, hh, mm, ss, nn;

        mx::error_t errc =
            MagAOX::file::parseFilePath( devName, YYYY, MM, DD, hh, mm, ss, nn, "_20241121063300000000000" );

        REQUIRE( errc == mx::error_t::noerror );
        REQUIRE( devName == "" );
        REQUIRE( YYYY == "2024" );
        REQUIRE( MM == "11" );
        REQUIRE( DD == "21" );
        REQUIRE( hh == "06" );
        REQUIRE( mm == "33" );
        REQUIRE( ss == "00" );
        REQUIRE( nn == "000000000" );
    }

    GIVEN( "A valid MagAO-X filepath without device or extension, with _" )
    {
        std::string devName, YYYY, MM, DD, hh, mm, ss, nn;

        mx::error_t errc =
            MagAOX::file::parseFilePath( devName, YYYY, MM, DD, hh, mm, ss, nn, "/path/to/_20241121063300000000000" );

        REQUIRE( errc == mx::error_t::noerror );
        REQUIRE( devName == "" );
        REQUIRE( YYYY == "2024" );
        REQUIRE( MM == "11" );
        REQUIRE( DD == "21" );
        REQUIRE( hh == "06" );
        REQUIRE( mm == "33" );
        REQUIRE( ss == "00" );
        REQUIRE( nn == "000000000" );
    }
}

/** \test Scenario: Parsing filenames and paths with errors
 *
 * \anchor tests_libMagAOX_file_fileTimes_parse_filenames_errors
 */
SCENARIO( "Parsing filenames and paths with errors", "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "An invalid MagAO-X filepath with too short timestamp" )
    {
        std::string devName, YYYY, MM, DD, hh, mm, ss, nn;

        mx::error_t errc = MagAOX::file::parseFilePath(
            devName, YYYY, MM, DD, hh, mm, ss, nn, "/path/to/device_2024112106330000000000.txt" );

        REQUIRE( errc == mx::error_t::invalidarg );
    }

    GIVEN( "An invalid MagAO-X filepath with too long timestamp" )
    {
        std::string devName, YYYY, MM, DD, hh, mm, ss, nn;

        mx::error_t errc = MagAOX::file::parseFilePath(
            devName, YYYY, MM, DD, hh, mm, ss, nn, "/path/to/device_202411210633000000000001.txt" );
        REQUIRE( errc == mx::error_t::invalidarg );
    }

    GIVEN( "An valid MagAO-X filepath but bad_alloc is thrown in parseTimeStamp" )
    {
        std::string devName, YYYY, MM, DD, hh, mm, ss, nn;

        bool caught = false;
        try
        {
            MagAOX::file::XWCTEST_PARSETIMESTAMP_THROW_BAD_ALLOC_ns::parseFilePath(
                devName, YYYY, MM, DD, hh, mm, ss, nn, "/path/to/device_20241121063300000000000.txt" );
        }
        catch( const MagAOX::xwcException &e )
        {
            caught = true;
        }
        REQUIRE( caught == true );
    }

    GIVEN( "An valid MagAO-X filepath but out_of_range is thrown in parseTimeStamp" )
    {
        std::string devName, YYYY, MM, DD, hh, mm, ss, nn;

        mx::error_t errc = MagAOX::file::XWCTEST_PARSETIMESTAMP_THROW_OUT_OF_RANGE_ns::parseFilePath(
            devName, YYYY, MM, DD, hh, mm, ss, nn, "/path/to/device_20241121063300000000000.txt" );

        REQUIRE( errc == mx::error_t::std_out_of_range );
    }

    GIVEN( "An valid MagAO-X filepath but exception is thrown in parseTimeStamp" )
    {
        std::string devName, YYYY, MM, DD, hh, mm, ss, nn;

        mx::error_t errc = MagAOX::file::XWCTEST_PARSETIMESTAMP_THROW_EXCEPTION_ns::parseFilePath(
            devName, YYYY, MM, DD, hh, mm, ss, nn, "/path/to/device_20241121063300000000000.txt" );

        REQUIRE( errc == mx::error_t::std_exception );
    }

    GIVEN( "An valid MagAO-X filepath but bad_alloc is thrown in parseFilePath" )
    {
        std::string devName, YYYY, MM, DD, hh, mm, ss, nn;

        bool caught = false;
        try
        {
            MagAOX::file::XWCTEST_PARSEFILEPATH_THROW_BAD_ALLOC_ns::parseFilePath(
                devName, YYYY, MM, DD, hh, mm, ss, nn, "/path/to/device_20241121063300000000000.txt" );
        }
        catch( const MagAOX::xwcException &e )
        {
            caught = true;
        }
        REQUIRE( caught == true );
    }

    GIVEN( "An valid MagAO-X filepath but out_of_range is thrown in parseFilePAth" )
    {
        std::string devName, YYYY, MM, DD, hh, mm, ss, nn;

        mx::error_t errc = MagAOX::file::XWCTEST_PARSEFILEPATH_THROW_OUT_OF_RANGE_ns::parseFilePath(
            devName, YYYY, MM, DD, hh, mm, ss, nn, "/path/to/device_20241121063300000000000.txt" );

        REQUIRE( errc == mx::error_t::std_out_of_range );
    }

    GIVEN( "An valid MagAO-X filepath but exception is thrown in parseFilePAth" )
    {
        std::string devName, YYYY, MM, DD, hh, mm, ss, nn;

        mx::error_t errc = MagAOX::file::XWCTEST_PARSEFILEPATH_THROW_EXCEPTION_ns::parseFilePath(
            devName, YYYY, MM, DD, hh, mm, ss, nn, "/path/to/device_20241121063300000000000.txt" );

        REQUIRE( errc == mx::error_t::std_exception );
    }
}

/** \test Scenario: Parsing timestamps with errors
 *
 * Tests only size errors.  Exceptions tested with parseFilePath tests.
 *
 * \anchor tests_libMagAOX_file_fileTimes_timestamps_errors
 */
SCENARIO( "Parsing timestamps with errors", "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "An invalid MagAO-X timestamp, too short" )
    {
        std::string YYYY, MM, DD, hh, mm, ss, nn;

        mx::error_t errc = MagAOX::file::parseTimestamp( YYYY, MM, DD, hh, mm, ss, nn, "202411210633000000000" );

        REQUIRE( errc == mx::error_t::invalidarg );
    }

    GIVEN( "An invalid MagAO-X timestamp, too long" )
    {
        std::string YYYY, MM, DD, hh, mm, ss, nn;

        mx::error_t errc = MagAOX::file::parseTimestamp( YYYY, MM, DD, hh, mm, ss, nn, "202411210633000000000001" );
        REQUIRE( errc == mx::error_t::invalidarg );
    }
}
