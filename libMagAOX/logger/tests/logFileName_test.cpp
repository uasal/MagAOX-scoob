/** \file logFileName_test.hpp
 * \brief Tests for the logFileName class
 * \ingroup logger_files
 */

#include "../../../tests/catch2/catch.hpp"

#include <filesystem>

#include "../logFileName.hpp"

namespace logFileName_test
{

SCENARIO( "Using logFileName", "[libMagAOX::logger::logFileName]" )
{
    GIVEN( "default construction and parsing and member access" )
    {
        std::string fullName = "/opt/MagAOX/logs/bamm/2024_11_21/bamm_20241121063321000000001.binlog";
        MagAOX::logger::logFileName lfn;

        lfn.fullName(fullName);

        REQUIRE(lfn.fullName() == fullName);
        REQUIRE(lfn.baseName() == "bamm_20241121063321000000001.binlog");
        REQUIRE(lfn.appName() == "bamm");
        REQUIRE(lfn.subDir() == "2024_11_21");
        REQUIRE(lfn.year() == 2024);
        REQUIRE(lfn.month() == 11);
        REQUIRE(lfn.day() == 21);
        REQUIRE(lfn.hour() == 6);
        REQUIRE(lfn.minute() == 33);
        REQUIRE(lfn.second() == 21);
        REQUIRE(lfn.nsec() == 1);

        flatlogs::timespecX ts = lfn.timestamp();

        REQUIRE(ts.time_s == 1732170801);
        REQUIRE(ts.time_ns == 1);

        REQUIRE(lfn.extension() == ".binlog");

        REQUIRE(lfn.valid() == true);

    }

    GIVEN( "default construction, assignment and member access" )
    {
        std::string fullName = "/opt/MagAOX/logs/bamm/2024_11_21/bamm_20241121063321000000001.binlog";
        MagAOX::logger::logFileName lfn;

        lfn = fullName;

        REQUIRE(lfn.fullName() == fullName);
        REQUIRE(lfn.baseName() == "bamm_20241121063321000000001.binlog");
        REQUIRE(lfn.appName() == "bamm");
        REQUIRE(lfn.subDir() == "2024_11_21");
        REQUIRE(lfn.year() == 2024);
        REQUIRE(lfn.month() == 11);
        REQUIRE(lfn.day() == 21);
        REQUIRE(lfn.hour() == 6);
        REQUIRE(lfn.minute() == 33);
        REQUIRE(lfn.second() == 21);
        REQUIRE(lfn.nsec() == 1);

        flatlogs::timespecX ts = lfn.timestamp();

        REQUIRE(ts.time_s == 1732170801);
        REQUIRE(ts.time_ns == 1);

        REQUIRE(lfn.extension() == ".binlog");

        REQUIRE(lfn.valid() == true);

    }

    GIVEN( "construction by parsing and member access" )
    {
        std::string fullName = "/opt/MagAOX/logs/bamm/2024_11_21/bamm_20241121063321000000001.binlog";
        MagAOX::logger::logFileName lfn(fullName);

        REQUIRE(lfn.fullName() == fullName);
        REQUIRE(lfn.baseName() == "bamm_20241121063321000000001.binlog");
        REQUIRE(lfn.appName() == "bamm");
        REQUIRE(lfn.subDir() == "2024_11_21");
        REQUIRE(lfn.year() == 2024);
        REQUIRE(lfn.month() == 11);
        REQUIRE(lfn.day() == 21);
        REQUIRE(lfn.hour() == 6);
        REQUIRE(lfn.minute() == 33);
        REQUIRE(lfn.second() == 21);
        REQUIRE(lfn.nsec() == 1);

        flatlogs::timespecX ts = lfn.timestamp();

        REQUIRE(ts.time_s == 1732170801);
        REQUIRE(ts.time_ns == 1);

        REQUIRE(lfn.extension() == ".binlog");

        REQUIRE(lfn.valid() == true);

    }
}


} // namespace logFileRaw_test
