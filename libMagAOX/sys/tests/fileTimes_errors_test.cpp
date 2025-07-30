/** \file fileTimes_errors_test.hpp
 * \brief Tests for file timestamps with errors
 * \ingroup sys_files
 */

#include "../../../tests/catch2/catch.hpp"

// Separate file so that we can make this definition:
//  this will cause a compiler warning
#define XWC_TIMESTAMP_BUFFER_SIZE ( 20 )
#include "../fileTimes.hpp"

#include <iostream>

namespace fileTimes_errors_test
{

SCENARIO( "Getting timestamp and broken-down time with errors", "[libMagAOX::sys::fileTimes]" )
{
    GIVEN( "A year that's too big (gmtime_r error)" )
    {
        time_t        ts_sec  = 1.355388599402496e+17; // huge year
        unsigned long ts_nsec = 0;

        std::string tstamp;
        tm          uttime;
        MagAOX::sys::internal::initbdtime( uttime );

        int rv = MagAOX::sys::timestamp( tstamp, uttime, ts_sec, ts_nsec );

        REQUIRE( rv == -1 );
    }

    GIVEN( "A timestamp buffer that's too small (gmtime_r error)" )
    {
        // Buffer size was #define-ed above

        time_t        ts_sec  = 1732170785;
        unsigned long ts_nsec = 434878292;

        std::string tstamp;
        tm          uttime;
        MagAOX::sys::internal::initbdtime( uttime );

        int rv = MagAOX::sys::timestamp( tstamp, uttime, ts_sec, ts_nsec );

        REQUIRE( rv == -3 );
        REQUIRE( uttime.tm_year == 124 );
        REQUIRE( uttime.tm_mon == 10 );
        REQUIRE( uttime.tm_mday == 21 );
        REQUIRE( uttime.tm_hour == 6 );
        REQUIRE( uttime.tm_min == 33 );
        REQUIRE( uttime.tm_sec == 5 );
    }
}

SCENARIO( "Getting timestamp only with errors", "[libMagAOX::sys::fileTimes]" )
{
    GIVEN( "A year that's too big (gmtime_r error)" )
    {
        time_t        ts_sec  = 1.355388599402496e+17; // huge year
        unsigned long ts_nsec = 0;

        std::string tstamp;

        int rv = MagAOX::sys::timestamp( tstamp, ts_sec, ts_nsec );

        REQUIRE( rv == -1 );
    }

    GIVEN( "A timestamp buffer that's too small (gmtime_r error)" )
    {
        // Buffer size was #define-ed above

        time_t        ts_sec  = 1732170785;
        unsigned long ts_nsec = 434878292;

        std::string tstamp;

        int rv = MagAOX::sys::timestamp( tstamp, ts_sec, ts_nsec );

        REQUIRE( rv == -3 );
    }
}

SCENARIO( "Getting filename and relative path for a given time with errors", "[libMagAOX::sys::fileTimes]" )
{
    GIVEN( "A year that's too big (gmtime_r error)" )
    {
        time_t        ts_sec  = 1.355388599402496e+17; // huge year
        unsigned long ts_nsec = 0;

        std::string fileName, relPath;

        int rv = MagAOX::sys::fileTimeRelPath( fileName, relPath, "tdevice", "txt", ts_sec, ts_nsec );

        REQUIRE( rv == -1 );
    }

    GIVEN( "A timestamp buffer that's too small (gmtime_r error)" )
    {
        time_t        ts_sec  = 1732170780;
        unsigned long ts_nsec = 0;

        std::string fileName, relPath;

        int rv = MagAOX::sys::fileTimeRelPath( fileName, relPath, "tdevice", "txt", ts_sec, ts_nsec );

        REQUIRE( rv == -3 );
    }
}

} // namespace fileTimes_errors_test
