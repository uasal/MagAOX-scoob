/** \file stdFileName_test.hpp
 * \brief Tests for the stdFileName class
 * \ingroup file_files
 */

#include "../../../tests/catch2/catch.hpp"

#include <filesystem>

#include "../stdFileName.hpp"

namespace libXWCTest
{
namespace fileTest
{
namespace stdFileNameTest
{

/** \test Scenario: Using stdFileName
 *
 * \anchor libXWC_logger_file_stdFileName_using
 */
SCENARIO( "Using stdFileName", "[libMagAOX::file::stdFileName]" )
{
    GIVEN( "default construction and parsing and member access" )
    {
        std::string               fullName = "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20241121063321000000001.binlog";
        MagAOX::file::stdFileName sfn;

        sfn.fullName( fullName );

        REQUIRE( sfn.fullName() == fullName );
        REQUIRE( sfn.baseName() == "bamm_20241121063321000000001.binlog" );
        REQUIRE( sfn.appName() == "bamm" );
        REQUIRE( sfn.extension() == ".binlog" );
        REQUIRE( sfn.subDir().path() == "2024_11_21" );
        REQUIRE( sfn.year() == 2024 );
        REQUIRE( sfn.month() == 11 );
        REQUIRE( sfn.day() == 21 );
        REQUIRE( sfn.hour() == 6 );
        REQUIRE( sfn.minute() == 33 );
        REQUIRE( sfn.second() == 21 );
        REQUIRE( sfn.nsec() == 1 );

        flatlogs::timespecX ts = sfn.timestamp();

        REQUIRE( ts.time_s == 1732170801 );
        REQUIRE( ts.time_ns == 1 );

        REQUIRE( sfn.valid() == true );
    }

    GIVEN( "default construction, assignment and member access" )
    {
        std::string               fullName = "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20241121063321000000001.binlog";
        MagAOX::file::stdFileName sfn;

        sfn = fullName;

        REQUIRE( sfn.fullName() == fullName );
        REQUIRE( sfn.baseName() == "bamm_20241121063321000000001.binlog" );
        REQUIRE( sfn.appName() == "bamm" );
        REQUIRE( sfn.extension() == ".binlog" );
        REQUIRE( sfn.subDir().path() == "2024_11_21" );
        REQUIRE( sfn.year() == 2024 );
        REQUIRE( sfn.month() == 11 );
        REQUIRE( sfn.day() == 21 );
        REQUIRE( sfn.hour() == 6 );
        REQUIRE( sfn.minute() == 33 );
        REQUIRE( sfn.second() == 21 );
        REQUIRE( sfn.nsec() == 1 );

        flatlogs::timespecX ts = sfn.timestamp();

        REQUIRE( ts.time_s == 1732170801 );
        REQUIRE( ts.time_ns == 1 );

        REQUIRE( sfn.valid() == true );
    }

    GIVEN( "construction by parsing and member access" )
    {
        std::string               fullName = "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20241121063321000000001.binlog";
        MagAOX::file::stdFileName sfn( fullName );

        REQUIRE( sfn.fullName() == fullName );
        REQUIRE( sfn.baseName() == "bamm_20241121063321000000001.binlog" );
        REQUIRE( sfn.appName() == "bamm" );
        REQUIRE( sfn.extension() == ".binlog" );
        REQUIRE( sfn.subDir().path() == "2024_11_21" );
        REQUIRE( sfn.year() == 2024 );
        REQUIRE( sfn.month() == 11 );
        REQUIRE( sfn.day() == 21 );
        REQUIRE( sfn.hour() == 6 );
        REQUIRE( sfn.minute() == 33 );
        REQUIRE( sfn.second() == 21 );
        REQUIRE( sfn.nsec() == 1 );

        flatlogs::timespecX ts = sfn.timestamp();

        REQUIRE( ts.time_s == 1732170801 );
        REQUIRE( ts.time_ns == 1 );

        REQUIRE( sfn.valid() == true );
    }
}

} // namespace stdFileNameTest
} // namespace fileTest
} // namespace libXWCTest
