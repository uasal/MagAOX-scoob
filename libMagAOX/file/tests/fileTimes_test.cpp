/** \file fileTimes_test.hpp
 * \brief Tests for file timestamps
 * \ingroup file_files
 */

#include "../../../tests/catch2/catch.hpp"

#include <iostream>

#include "../fileTimes.hpp"

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
        MagAOX::file::internal::initbdtime( uttime );

        MagAOX::file::timestamp( tstamp, uttime, ts_sec, ts_nsec );

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
        MagAOX::file::internal::initbdtime( uttime );

        MagAOX::file::timestamp( tstamp, uttime, ts_sec, ts_nsec );

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
        MagAOX::file::internal::initbdtime( uttime );

        MagAOX::file::timestamp( tstamp, uttime, ts_sec, ts_nsec );

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
        MagAOX::file::internal::initbdtime( uttime );

        MagAOX::file::timestamp( tstamp, uttime, ts_sec, ts_nsec );

        REQUIRE( tstamp == "20241121063305000000292" );
        REQUIRE( uttime.tm_year == 124 );
        REQUIRE( uttime.tm_mon == 10 );
        REQUIRE( uttime.tm_mday == 21 );
        REQUIRE( uttime.tm_hour == 6 );
        REQUIRE( uttime.tm_min == 33 );
        REQUIRE( uttime.tm_sec == 5 );
    }
}

/** \test Scenario: Getting timestamp string only for a given time
 *
 * \anchor tests_libMagAOX_file_fileTimes_timestamp_only
 */
SCENARIO( "Getting timestamp string only for a given time", "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "A time with 0 sec and 0 nsec" )
    {
        time_t        ts_sec  = 1732170780;
        unsigned long ts_nsec = 0;

        std::string tstamp;

        MagAOX::file::timestamp( tstamp, ts_sec, ts_nsec );

        REQUIRE( tstamp == "20241121063300000000000" );
    }

    GIVEN( "A time with non-0 sec and 0 nsec" )
    {
        time_t        ts_sec  = 1732170781;
        unsigned long ts_nsec = 0;

        std::string tstamp;

        MagAOX::file::timestamp( tstamp, ts_sec, ts_nsec );

        REQUIRE( tstamp == "20241121063301000000000" );
    }

    GIVEN( "A time with non-0 sec and 9-digit nsec" )
    {
        time_t        ts_sec  = 1732170785;
        unsigned long ts_nsec = 434878292;

        std::string tstamp;
        MagAOX::file::timestamp( tstamp, ts_sec, ts_nsec );

        REQUIRE( tstamp == "20241121063305434878292" );
    }

    GIVEN( "A time with non-0 sec and 3-digit nsec" )
    {
        time_t        ts_sec  = 1732170785;
        unsigned long ts_nsec = 292;

        std::string tstamp;

        MagAOX::file::timestamp( tstamp, ts_sec, ts_nsec );

        REQUIRE( tstamp == "20241121063305000000292" );
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

        MagAOX::file::fileTimeRelPath( fileName, relPath, "tdevice", "txt", ts_sec, ts_nsec );

        REQUIRE( fileName == "tdevice_20241121063300000000000.txt" );
        REQUIRE( relPath == "tdevice/2024_11_21" );
    }
}

/** \test Scenario: Parsing filenames, paths and timestamps
 *
 * \anchor tests_libMagAOX_file_fileTimes_parse_filenames_timestamps
 */
SCENARIO( "Parsing filenames, paths and timestamps", "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "A valid MagAO-X filename" )
    {
        std::string devName, YYYY, MM, DD, hh, mm, ss, nn;

        MagAOX::file::parseFilePath( devName, YYYY, MM, DD, hh, mm, ss, nn, "device_20241121063300000000000.txt" );

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

        MagAOX::file::parseFilePath(
            devName, YYYY, MM, DD, hh, mm, ss, nn, "/path/to/device_20241121063300000000000.txt" );

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

        MagAOX::file::parseFilePath( devName, YYYY, MM, DD, hh, mm, ss, nn, "device_20241121063300000000000" );

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

        MagAOX::file::parseFilePath( devName, YYYY, MM, DD, hh, mm, ss, nn, "/path/to/device_20241121063300000000000" );

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

        MagAOX::file::parseFilePath( devName, YYYY, MM, DD, hh, mm, ss, nn, "20241121063300000000000.txt" );

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

        MagAOX::file::parseFilePath( devName, YYYY, MM, DD, hh, mm, ss, nn, "/path/to/20241121063300000000000.txt" );

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

        MagAOX::file::parseFilePath( devName, YYYY, MM, DD, hh, mm, ss, nn, "20241121063300000000000" );

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

        MagAOX::file::parseFilePath( devName, YYYY, MM, DD, hh, mm, ss, nn, "/path/to/20241121063300000000000" );

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

        MagAOX::file::parseFilePath( devName, YYYY, MM, DD, hh, mm, ss, nn, "_20241121063300000000000.txt" );

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

        MagAOX::file::parseFilePath( devName, YYYY, MM, DD, hh, mm, ss, nn, "/path/to/_20241121063300000000000.txt" );

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

        MagAOX::file::parseFilePath( devName, YYYY, MM, DD, hh, mm, ss, nn, "_20241121063300000000000" );

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

        MagAOX::file::parseFilePath( devName, YYYY, MM, DD, hh, mm, ss, nn, "/path/to/_20241121063300000000000" );

        REQUIRE( devName == "" );
        REQUIRE( YYYY == "2024" );
        REQUIRE( MM == "11" );
        REQUIRE( DD == "21" );
        REQUIRE( hh == "06" );
        REQUIRE( mm == "33" );
        REQUIRE( ss == "00" );
        REQUIRE( nn == "000000000" );
    }

    GIVEN( "An invalid MagAO-X filepath with too short timestamp" )
    {
        std::string devName, YYYY, MM, DD, hh, mm, ss, nn;

        bool caught = false;
        int  code   = 0;

        try
        {
            MagAOX::file::parseFilePath(
                devName, YYYY, MM, DD, hh, mm, ss, nn, "/path/to/device_2024112106330000000000.txt" );
        }
        catch( const MagAOX::xwcException &e )
        {
            caught = true;
            code   = e.code();
        }
        catch( ... )
        {
            caught = true;
        }

        REQUIRE( caught == true );
        REQUIRE( code == -24 );
    }

    GIVEN( "An invalid MagAO-X filepath with too long timestamp" )
    {
        std::string devName, YYYY, MM, DD, hh, mm, ss, nn;

        bool caught = false;
        int  code   = 0;

        try
        {
            MagAOX::file::parseFilePath(
                devName, YYYY, MM, DD, hh, mm, ss, nn, "/path/to/device_202411210633000000000001.txt" );
        }
        catch( const MagAOX::xwcException &e )
        {
            caught = true;
            code   = e.code();
        }
        catch( ... )
        {
            caught = true;
        }

        REQUIRE( caught == true );
        REQUIRE( code == -24 );
    }

    GIVEN( "An invalid MagAO-X timestamp, too short" )
    {
        std::string YYYY, MM, DD, hh, mm, ss, nn;

        bool caught = false;
        int  code   = 0;

        try
        {
            MagAOX::file::parseTimestamp( YYYY, MM, DD, hh, mm, ss, nn, "202411210633000000000" );
        }
        catch( const MagAOX::xwcException &e )
        {
            caught = true;
            code   = e.code();
        }
        catch( ... )
        {
            caught = true;
        }

        REQUIRE( caught == true );
        REQUIRE( code == -19 );
    }

    GIVEN( "An invalid MagAO-X timestamp, too long" )
    {
        std::string YYYY, MM, DD, hh, mm, ss, nn;

        bool caught = false;
        int  code   = 0;

        try
        {
            MagAOX::file::parseTimestamp( YYYY, MM, DD, hh, mm, ss, nn, "202411210633000000000001" );
        }
        catch( const MagAOX::xwcException &e )
        {
            caught = true;
            code   = e.code();
        }
        catch( ... )
        {
            caught = true;
        }

        REQUIRE( caught == true );
        REQUIRE( code == -19 );
    }
}

/** \test Scenario: Getting timestamp and broken-down time with errors
 *
 * This is in a separate file due to need to define a buffer size too small to generate errors
 *
 * \anchor tests_libMagAOX_file_fileTimes_parse_filenames_timestamp_bdtime_errors
 */
SCENARIO( "Getting timestamp and broken-down time with errors", "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "A year that's too big (gmtime_r error)" )
    {
        time_t        ts_sec  = 1.355388599402496e+17; // huge year
        unsigned long ts_nsec = 0;

        std::string tstamp;
        tm          uttime;
        MagAOX::file::internal::initbdtime( uttime );

        bool caught = false;
        int  code   = 0;

        try
        {
            MagAOX::file::timestamp( tstamp, uttime, ts_sec, ts_nsec );
        }
        catch( const MagAOX::xwcException &e )
        {
            caught = true;
            code   = e.code();
        }
        catch( ... )
        {
            caught = true;
        }

        REQUIRE( caught == true );
        REQUIRE( code == -1 );
    }
}

/** \test Scenario: Getting timestamp only with errors
 *
 * This is in a separate file due to need to define a buffer size too small to generate errors
 *
 * \anchor tests_libMagAOX_file_fileTimes_parse_filenames_timestamp_only_errors
 */
SCENARIO( "Getting timestamp only with errors", "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "A year that's too big (gmtime_r error)" )
    {
        time_t        ts_sec  = 1.355388599402496e+17; // huge year
        unsigned long ts_nsec = 0;

        std::string tstamp;

        bool caught = false;
        int  code   = 0;
        try
        {
            MagAOX::file::timestamp( tstamp, ts_sec, ts_nsec );
        }
        catch( const MagAOX::xwcException &e )
        {
            caught = true;
            code   = e.code();
        }
        catch( ... )
        {
            caught = true;
        }

        REQUIRE( caught == true );
        REQUIRE( code == -6 );
    }
}

/** \test Scenario: Getting filename and relative path with errors
 *
 * This is in a separate file due to need to define a buffer size too small to generate errors
 *
 * \anchor tests_libMagAOX_file_fileTimes_parse_filename_relpath_only_errors
 */
SCENARIO( "Getting filename and relative path for a given time with errors", "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "A year that's too big (gmtime_r error)" )
    {
        time_t        ts_sec  = 1.355388599402496e+17; // huge year
        unsigned long ts_nsec = 0;

        std::string fileName, relPath;

        bool caught = false;
        int  code   = 0;

        try
        {
            MagAOX::file::fileTimeRelPath( fileName, relPath, "tdevice", "txt", ts_sec, ts_nsec );
        }
        catch( const MagAOX::xwcException &e )
        {
            caught = true;
            code   = e.code();
        }
        catch( ... )
        {
            caught = true;
        }

        REQUIRE( caught == true );
        REQUIRE( code == -17 );
    }
}

#undef file_fileTimes_hpp
#define XWCTEST_NAMESPACE XWCTEST_FILETIMES_ERR20_ns
#define XWCTEST_FILETIMES_ERR20
#include "../fileTimes.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_FILETIMES_ERR20

SCENARIO( "Parsing filenames, paths and timestamps with injected error 20", "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "A valid MagAO-X filename but error occurs" )
    {
        std::string devName, YYYY, MM, DD, hh, mm, ss, nn;

        bool caught = false;
        int  code   = 0;

        try
        {
            MagAOX::file::XWCTEST_FILETIMES_ERR20_ns::parseFilePath(
                devName, YYYY, MM, DD, hh, mm, ss, nn, "device_20241121063300000000000.txt" );
        }
        catch( const MagAOX::xwcException &e )
        {
            caught = true;
            code   = e.code();
        }
        catch( ... )
        {
            caught = true;
        }

        REQUIRE( caught == true );
        REQUIRE( code == -20 );
    }
}

#undef file_fileTimes_hpp
#define XWCTEST_NAMESPACE XWCTEST_FILETIMES_ERR21_ns
#define XWCTEST_FILETIMES_ERR21
#include "../fileTimes.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_FILETIMES_ERR21

SCENARIO( "Parsing filenames, paths and timestamps with injected error 21", "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "A valid MagAO-X filename but error occurs" )
    {
        std::string devName, YYYY, MM, DD, hh, mm, ss, nn;

        bool caught = false;
        int  code   = 0;

        try
        {
            MagAOX::file::XWCTEST_FILETIMES_ERR21_ns::parseFilePath(
                devName, YYYY, MM, DD, hh, mm, ss, nn, "20241121063300000000000.txt" );
        }
        catch( const MagAOX::xwcException &e )
        {
            caught = true;
            code   = e.code();
        }
        catch( ... )
        {
            caught = true;
        }

        REQUIRE( caught == true );
        REQUIRE( code == -21 );
    }
}

#undef file_fileTimes_hpp
#define XWCTEST_NAMESPACE XWCTEST_FILETIMES_ERR22_ns
#define XWCTEST_FILETIMES_ERR22
#include "../fileTimes.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_FILETIMES_ERR22

SCENARIO( "Parsing filenames, paths and timestamps with injected error 22", "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "A valid MagAO-X filename but error occurs" )
    {
        std::string devName, YYYY, MM, DD, hh, mm, ss, nn;

        bool caught = false;
        int  code   = 0;

        try
        {
            MagAOX::file::XWCTEST_FILETIMES_ERR22_ns::parseFilePath(
                devName, YYYY, MM, DD, hh, mm, ss, nn, "device_20241121063300000000000.txt" );
        }
        catch( const MagAOX::xwcException &e )
        {
            caught = true;
            code   = e.code();
        }
        catch( ... )
        {
            caught = true;
        }

        REQUIRE( caught == true );
        REQUIRE( code == -22 );
    }
}

#undef file_fileTimes_hpp
#define XWCTEST_NAMESPACE XWCTEST_FILETIMES_ERR23_ns
#define XWCTEST_FILETIMES_ERR23
#include "../fileTimes.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_FILETIMES_ERR23

SCENARIO( "Parsing filenames, paths and timestamps with injected error 23", "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "A valid MagAO-X filename but error occurs" )
    {
        std::string devName, YYYY, MM, DD, hh, mm, ss, nn;

        bool caught = false;
        int  code   = 0;

        try
        {
            MagAOX::file::XWCTEST_FILETIMES_ERR23_ns::parseFilePath(
                devName, YYYY, MM, DD, hh, mm, ss, nn, "device_20241121063300000000000.txt" );
        }
        catch( const MagAOX::xwcException &e )
        {
            caught = true;
            code   = e.code();
        }
        catch( ... )
        {
            caught = true;
        }

        REQUIRE( caught == true );
        REQUIRE( code == -23 );
    }
}

#undef file_fileTimes_hpp
#define XWCTEST_NAMESPACE XWCTEST_FILETIMES_ERR25_ns
#define XWCTEST_FILETIMES_ERR25
#include "../fileTimes.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_FILETIMES_ERR25

SCENARIO( "Parsing filenames, paths and timestamps with injected error 25", "[libMagAOX::file::fileTimes]" )
{
    GIVEN( "A valid MagAO-X filename but error occurs" )
    {
        std::string devName, YYYY, MM, DD, hh, mm, ss, nn;

        bool caught = false;
        int  code   = 0;

        try
        {
            MagAOX::file::XWCTEST_FILETIMES_ERR25_ns::parseFilePath(
                devName, YYYY, MM, DD, hh, mm, ss, nn, "device_20241121063300000000000.txt" );
        }
        catch( const MagAOX::xwcException &e )
        {
            caught = true;
            code   = e.code();
        }
        catch( ... )
        {
            caught = true;
        }

        REQUIRE( caught == true );
        REQUIRE( code == -25 );
    }
}
