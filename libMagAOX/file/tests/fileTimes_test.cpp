/** \file fileTimes_test.hpp
 * \brief Tests for file timestamps
 * \ingroup file_files
 */

#include "../../../tests/catch2/catch.hpp"

#include <iostream>

#include "../fileTimes.hpp"

namespace fileTimes_test
{

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

        int rv = MagAOX::file::timestamp( tstamp, uttime, ts_sec, ts_nsec );

        REQUIRE( rv == 0 );
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

        int rv = MagAOX::file::timestamp( tstamp, uttime, ts_sec, ts_nsec );

        REQUIRE( rv == 0 );
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

        int rv = MagAOX::file::timestamp( tstamp, uttime, ts_sec, ts_nsec );

        REQUIRE( rv == 0 );
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

        int rv = MagAOX::file::timestamp( tstamp, uttime, ts_sec, ts_nsec );

        REQUIRE( rv == 0 );
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

        int rv = MagAOX::file::timestamp( tstamp, ts_sec, ts_nsec );

        REQUIRE( rv == 0 );
        REQUIRE( tstamp == "20241121063300000000000" );
    }

    GIVEN( "A time with non-0 sec and 0 nsec" )
    {
        time_t        ts_sec  = 1732170781;
        unsigned long ts_nsec = 0;

        std::string tstamp;

        int rv = MagAOX::file::timestamp( tstamp, ts_sec, ts_nsec );

        REQUIRE( rv == 0 );
        REQUIRE( tstamp == "20241121063301000000000" );
    }

    GIVEN( "A time with non-0 sec and 9-digit nsec" )
    {
        time_t        ts_sec  = 1732170785;
        unsigned long ts_nsec = 434878292;

        std::string tstamp;
        int         rv = MagAOX::file::timestamp( tstamp, ts_sec, ts_nsec );

        REQUIRE( rv == 0 );
        REQUIRE( tstamp == "20241121063305434878292" );
    }

    GIVEN( "A time with non-0 sec and 3-digit nsec" )
    {
        time_t        ts_sec  = 1732170785;
        unsigned long ts_nsec = 292;

        std::string tstamp;

        int rv = MagAOX::file::timestamp( tstamp, ts_sec, ts_nsec );

        REQUIRE( rv == 0 );
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

        int rv = MagAOX::file::fileTimeRelPath( fileName, relPath, "tdevice", "txt", ts_sec, ts_nsec );

        REQUIRE( rv == 0 );
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

        int rv =
            MagAOX::file::parseFilePath( devName, YYYY, MM, DD, hh, mm, ss, nn, "device_20241121063300000000000.txt" );

        REQUIRE( rv == 0 );
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

        int rv = MagAOX::file::parseFilePath(
            devName, YYYY, MM, DD, hh, mm, ss, nn, "/path/to/device_20241121063300000000000.txt" );

        REQUIRE( rv == 0 );
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

        int rv = MagAOX::file::parseFilePath( devName, YYYY, MM, DD, hh, mm, ss, nn, "device_20241121063300000000000" );

        REQUIRE( rv == 0 );
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

        int rv = MagAOX::file::parseFilePath(
            devName, YYYY, MM, DD, hh, mm, ss, nn, "/path/to/device_20241121063300000000000" );

        REQUIRE( rv == 0 );
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

        int rv = MagAOX::file::parseFilePath( devName, YYYY, MM, DD, hh, mm, ss, nn, "20241121063300000000000.txt" );

        REQUIRE( rv == 0 );
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

        int rv =
            MagAOX::file::parseFilePath( devName, YYYY, MM, DD, hh, mm, ss, nn, "/path/to/20241121063300000000000.txt" );

        REQUIRE( rv == 0 );
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

        int rv = MagAOX::file::parseFilePath( devName, YYYY, MM, DD, hh, mm, ss, nn, "20241121063300000000000" );

        REQUIRE( rv == 0 );
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

        int rv =
            MagAOX::file::parseFilePath( devName, YYYY, MM, DD, hh, mm, ss, nn, "/path/to/20241121063300000000000" );

        REQUIRE( rv == 0 );
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

        int rv = MagAOX::file::parseFilePath( devName, YYYY, MM, DD, hh, mm, ss, nn, "_20241121063300000000000.txt" );

        REQUIRE( rv == 0 );
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

        int rv = MagAOX::file::parseFilePath(
            devName, YYYY, MM, DD, hh, mm, ss, nn, "/path/to/_20241121063300000000000.txt" );

        REQUIRE( rv == 0 );
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

        int rv = MagAOX::file::parseFilePath( devName, YYYY, MM, DD, hh, mm, ss, nn, "_20241121063300000000000" );

        REQUIRE( rv == 0 );
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

        int rv =
            MagAOX::file::parseFilePath( devName, YYYY, MM, DD, hh, mm, ss, nn, "/path/to/_20241121063300000000000" );

        REQUIRE( rv == 0 );
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

        int rv =
            MagAOX::file::parseFilePath( devName, YYYY, MM, DD, hh, mm, ss, nn, "/path/to/device_2024112106330000000000.txt" );

        REQUIRE( rv == -1 );
    }

    GIVEN( "An invalid MagAO-X filepath with too long timestamp" )
    {
        std::string devName, YYYY, MM, DD, hh, mm, ss, nn;

        int rv =
            MagAOX::file::parseFilePath( devName, YYYY, MM, DD, hh, mm, ss, nn, "/path/to/device_202411210633000000000001.txt" );

        REQUIRE( rv == -1 );
    }

    GIVEN( "An invalid MagAO-X timestamp, too short" )
    {
        std::string YYYY, MM, DD, hh, mm, ss, nn;

        int rv =
            MagAOX::file::parseTimestamp( YYYY, MM, DD, hh, mm, ss, nn, "202411210633000000000" );

        REQUIRE( rv == -1 );
    }

    GIVEN( "An invalid MagAO-X timestamp, too long" )
    {
        std::string YYYY, MM, DD, hh, mm, ss, nn;

        int rv =
            MagAOX::file::parseTimestamp( YYYY, MM, DD, hh, mm, ss, nn, "202411210633000000000001" );

        REQUIRE( rv == -1 );
    }
}


} // namespace fileTimes_test
