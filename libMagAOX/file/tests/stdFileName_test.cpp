/** \file stdFileName_test.hpp
 * \brief Tests for the stdFileName class
 * \ingroup file_files
 */

#include "../../../tests/catch2/catch.hpp"

#include <filesystem>

#include "../stdFileName.hpp"

namespace stdFileName_test
{

SCENARIO( "Using stdFileName", "[libMagAOX::file::stdFileName]" )
{
    GIVEN( "default construction and parsing and member access" )
    {
        std::string fullName = "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20241121063321000000001.binstd";
        MagAOX::file::stdFileName sfn;

        sfn.fullName(fullName);

        REQUIRE(sfn.fullName() == fullName);
        REQUIRE(sfn.baseName() == "bamm_20241121063321000000001.binstd");
        REQUIRE(sfn.appName() == "bamm");
        REQUIRE(sfn.extension() == ".binstd");
        REQUIRE(sfn.subDir() == "2024_11_21");
        REQUIRE(sfn.year() == 2024);
        REQUIRE(sfn.month() == 11);
        REQUIRE(sfn.day() == 21);
        REQUIRE(sfn.hour() == 6);
        REQUIRE(sfn.minute() == 33);
        REQUIRE(sfn.second() == 21);
        REQUIRE(sfn.nsec() == 1);

        flatlogs::timespecX ts = sfn.timestamp();

        REQUIRE(ts.time_s == 1732170801);
        REQUIRE(ts.time_ns == 1);


        REQUIRE(sfn.valid() == true);

    }

    GIVEN( "default construction, assignment and member access" )
    {
        std::string fullName = "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20241121063321000000001.binstd";
        MagAOX::file::stdFileName sfn;

        sfn = fullName;

        REQUIRE(sfn.fullName() == fullName);
        REQUIRE(sfn.baseName() == "bamm_20241121063321000000001.binstd");
        REQUIRE(sfn.appName() == "bamm");
        REQUIRE(sfn.extension() == ".binstd");
        REQUIRE(sfn.subDir() == "2024_11_21");
        REQUIRE(sfn.year() == 2024);
        REQUIRE(sfn.month() == 11);
        REQUIRE(sfn.day() == 21);
        REQUIRE(sfn.hour() == 6);
        REQUIRE(sfn.minute() == 33);
        REQUIRE(sfn.second() == 21);
        REQUIRE(sfn.nsec() == 1);

        flatlogs::timespecX ts = sfn.timestamp();

        REQUIRE(ts.time_s == 1732170801);
        REQUIRE(ts.time_ns == 1);



        REQUIRE(sfn.valid() == true);

    }

    GIVEN( "construction by parsing and member access" )
    {
        std::string fullName = "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20241121063321000000001.binstd";
        MagAOX::file::stdFileName sfn(fullName);

        REQUIRE(sfn.fullName() == fullName);
        REQUIRE(sfn.baseName() == "bamm_20241121063321000000001.binstd");
        REQUIRE(sfn.appName() == "bamm");
        REQUIRE(sfn.extension() == ".binstd");
        REQUIRE(sfn.subDir() == "2024_11_21");
        REQUIRE(sfn.year() == 2024);
        REQUIRE(sfn.month() == 11);
        REQUIRE(sfn.day() == 21);
        REQUIRE(sfn.hour() == 6);
        REQUIRE(sfn.minute() == 33);
        REQUIRE(sfn.second() == 21);
        REQUIRE(sfn.nsec() == 1);

        flatlogs::timespecX ts = sfn.timestamp();

        REQUIRE(ts.time_s == 1732170801);
        REQUIRE(ts.time_ns == 1);



        REQUIRE(sfn.valid() == true);

    }
}

SCENARIO( "manipulating subdirs", "[libMagAOX::file::stdFileName]" )
{
    GIVEN( "a filename to find previous and following subdir from, no change in month" )
    {
        std::string fullName = "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20241121063321000000001.binstd";
        MagAOX::file::stdFileName sfn;

        sfn.fullName(fullName);
        REQUIRE(sfn.valid() == true);

        std::string psd = sfn.previousSubdir();

        REQUIRE(psd == "2024_11_20");

        psd = sfn.followingSubdir();

        REQUIRE(psd == "2024_11_22");
    }

    GIVEN( "a filename to find previous and following subdir from, change to previous month" )
    {
        std::string fullName = "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20241101063321000000001.binstd";
        MagAOX::file::stdFileName sfn;

        sfn.fullName(fullName);
        REQUIRE(sfn.valid() == true);

        std::string psd = sfn.previousSubdir();

        REQUIRE(psd == "2024_10_31");

        psd = sfn.followingSubdir();

        REQUIRE(psd == "2024_11_02");
    }

    GIVEN( "a filename to find previous and following subdir from, change to next month" )
    {
        std::string fullName = "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20241130063321000000001.binstd";
        MagAOX::file::stdFileName sfn;

        sfn.fullName(fullName);
        REQUIRE(sfn.valid() == true);

        std::string psd = sfn.previousSubdir();

        REQUIRE(psd == "2024_11_29");

        psd = sfn.followingSubdir();

        REQUIRE(psd == "2024_12_01");
    }

    GIVEN( "a filename to find previous and following subdir from, change to next month leap" )
    {
        std::string fullName = "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20240229063321000000001.binstd";
        MagAOX::file::stdFileName sfn;

        sfn.fullName(fullName);
        REQUIRE(sfn.valid() == true);

        std::string psd = sfn.previousSubdir();

        REQUIRE(psd == "2024_02_28");

        psd = sfn.followingSubdir();

        REQUIRE(psd == "2024_03_01");
    }

    GIVEN( "a filename to find previous and following subdir from, change to previous year" )
    {
        std::string fullName = "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20240101063321000000001.binstd";
        MagAOX::file::stdFileName sfn;

        sfn.fullName(fullName);
        REQUIRE(sfn.valid() == true);

        std::string psd = sfn.previousSubdir();

        REQUIRE(psd == "2023_12_31");

        psd = sfn.followingSubdir();

        REQUIRE(psd == "2024_01_02");
    }

    GIVEN( "a filename to find previous and following subdir from, change to next year" )
    {
        std::string fullName = "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20241231063321000000001.binstd";
        MagAOX::file::stdFileName sfn;

        sfn.fullName(fullName);
        REQUIRE(sfn.valid() == true);

        std::string psd = sfn.previousSubdir();

        REQUIRE(psd == "2024_12_30");

        psd = sfn.followingSubdir();

        REQUIRE(psd == "2025_01_01");
    }
}

} // namespace stdFileRaw_test
