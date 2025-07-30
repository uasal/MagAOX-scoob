/** \file fileTimes_test.hpp
 * \brief Tests for file timestamps
 * \ingroup sys_files
 */

#include "../../../tests/catch2/catch.hpp"

#include "../fileTimes.hpp"

#include <iostream>

namespace fileTimes_test
{

SCENARIO( "Getting timestamp string and broken-down time for a given time", "[libMagAOX::sys::fileTimes]" )
{
    GIVEN( "A time with 0 sec and 0 nsec" )
    {
        time_t        ts_sec  = 1732170780;
        unsigned long ts_nsec = 0;

        std::string tstamp;
        tm          uttime;
        MagAOX::sys::internal::initbdtime( uttime );

        int rv = MagAOX::sys::timestamp( tstamp, uttime, ts_sec, ts_nsec );

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
        MagAOX::sys::internal::initbdtime( uttime );

        int rv = MagAOX::sys::timestamp( tstamp, uttime, ts_sec, ts_nsec );

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
        MagAOX::sys::internal::initbdtime( uttime );

        int rv = MagAOX::sys::timestamp( tstamp, uttime, ts_sec, ts_nsec );

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
        MagAOX::sys::internal::initbdtime( uttime );

        int rv = MagAOX::sys::timestamp( tstamp, uttime, ts_sec, ts_nsec );

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

SCENARIO( "Getting timestamp string only for a given time", "[libMagAOX::sys::fileTimes]" )
{
    GIVEN( "A time with 0 sec and 0 nsec" )
    {
        time_t        ts_sec  = 1732170780;
        unsigned long ts_nsec = 0;

        std::string tstamp;

        int rv = MagAOX::sys::timestamp( tstamp, ts_sec, ts_nsec );

        REQUIRE( rv == 0 );
        REQUIRE( tstamp == "20241121063300000000000" );
    }

    GIVEN( "A time with non-0 sec and 0 nsec" )
    {
        time_t        ts_sec  = 1732170781;
        unsigned long ts_nsec = 0;

        std::string tstamp;

        int rv = MagAOX::sys::timestamp( tstamp, ts_sec, ts_nsec );

        REQUIRE( rv == 0 );
        REQUIRE( tstamp == "20241121063301000000000" );
    }

    GIVEN( "A time with non-0 sec and 9-digit nsec" )
    {
        time_t        ts_sec  = 1732170785;
        unsigned long ts_nsec = 434878292;

        std::string tstamp;
        int         rv = MagAOX::sys::timestamp( tstamp, ts_sec, ts_nsec );

        REQUIRE( rv == 0 );
        REQUIRE( tstamp == "20241121063305434878292" );
    }

    GIVEN( "A time with non-0 sec and 3-digit nsec" )
    {
        time_t        ts_sec  = 1732170785;
        unsigned long ts_nsec = 292;

        std::string tstamp;

        int rv = MagAOX::sys::timestamp( tstamp, ts_sec, ts_nsec );

        REQUIRE( rv == 0 );
        REQUIRE( tstamp == "20241121063305000000292" );
    }
}

SCENARIO( "Getting filename and relative path for a given time", "[libMagAOX::sys::fileTimes]" )
{
    GIVEN( "A time with 0 sec and 0 nsec" )
    {
        time_t        ts_sec  = 1732170780;
        unsigned long ts_nsec = 0;

        std::string fileName, relPath;

        int rv = MagAOX::sys::fileTimeRelPath( fileName, relPath, "tdevice", "txt", ts_sec, ts_nsec );

        REQUIRE( rv == 0 );
        REQUIRE( fileName == "tdevice_20241121063300000000000.txt" );
        REQUIRE( relPath == "tdevice/2024_11_21" );
    }
}

} // namespace fileTimes_test
