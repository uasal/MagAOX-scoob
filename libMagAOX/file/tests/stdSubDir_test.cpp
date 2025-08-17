/** \file stdSubDir_test.hpp
 * \brief Tests for the stdSubDir class
 * \ingroup file_files
 */

#include "../../../tests/catch2/catch.hpp"

#include "../stdSubDir.hpp"

namespace stdSubDir_test
{

/** \test Scenario: Initializing stdSubDir
 *
 * \anchor tests_libMagAOX_file_stdSubDir_initializing
 */
SCENARIO( "Initializing stdSubDir", "[libMagAOX::file::stdSubDir]" )
{
    GIVEN( "default construction, setting from YYYY_MM_DD" )
    {
        MagAOX::file::stdSubDir ssd;

        // Test that access while invalid throws
        bool caught = false;

        try
        {
            ssd.path();
        }
        catch( const std::exception &e )
        {
            static_cast<void>( e );
            caught = true;
        }

        REQUIRE( caught == true );

        caught = false;

        try
        {
            ssd.year();
        }
        catch( const std::exception &e )
        {
            static_cast<void>( e );
            caught = true;
        }

        REQUIRE( caught == true );

        caught = false;

        try
        {
            ssd.month();
        }
        catch( const std::exception &e )
        {
            static_cast<void>( e );
            caught = true;
        }

        REQUIRE( caught == true );

        caught = false;

        try
        {
            ssd.day();
        }
        catch( const std::exception &e )
        {
            static_cast<void>( e );
            caught = true;
        }

        REQUIRE( caught == true );

        ssd.path( "2024_11_21" );

        REQUIRE( ssd.valid() == true );
        REQUIRE( ssd.path() == "2024_11_21" );
        REQUIRE( ssd.year() == 2024 );
        REQUIRE( ssd.month() == 11 );
        REQUIRE( ssd.day() == 21 );
    }

    GIVEN( "default construction, setting from string giving exceptions" )
    {
        MagAOX::file::stdSubDir ssd;

        mx::error_t errc;

        errc = ssd.path( "2024_11_2" );
        REQUIRE( errc == mx::error_t::invalidarg );

        errc = ssd.path( "2024x11_21" );
        REQUIRE( errc == mx::error_t::invalidarg );

        errc = ssd.path( "2024_11x21" );
        REQUIRE( errc == mx::error_t::invalidarg );

        errc = ssd.path( "Y024_11_24" );
        REQUIRE( errc == mx::error_t::invalidarg );

        errc = ssd.path( "2Y24_11_24" );
        REQUIRE( errc == mx::error_t::invalidarg );

        errc = ssd.path( "20Y4_11_24" );
        REQUIRE( errc == mx::error_t::invalidarg );

        errc = ssd.path( "202Y_11_24" );
        REQUIRE( errc == mx::error_t::invalidarg );

        errc = ssd.path( "2024_M1_24" );
        REQUIRE( errc == mx::error_t::invalidarg );

        errc = ssd.path( "2024_2M_24" );
        REQUIRE( errc == mx::error_t::invalidarg );

        errc = ssd.path( "2024_21_D4" );
        REQUIRE( errc == mx::error_t::invalidarg );

        errc = ssd.path( "2024_21_2D" );
        REQUIRE( errc == mx::error_t::invalidarg );
    }

    GIVEN( "default construction, setting from Y/M/D" )
    {
        MagAOX::file::stdSubDir ssd;

        bool caught = false;

        try
        {
            ssd.path();
        }
        catch( const std::exception &e )
        {
            static_cast<void>( e );
            caught = true;
        }

        REQUIRE( caught == true );

        ssd.ymd( 2024, 11, 21 );

        REQUIRE( ssd.valid() == true );
        REQUIRE( ssd.path() == "2024_11_21" );
        REQUIRE( ssd.year() == 2024 );
        REQUIRE( ssd.month() == 11 );
        REQUIRE( ssd.day() == 21 );
    }

    GIVEN( "construction from Y/M/D" )
    {
        MagAOX::file::stdSubDir ssd( 2024, 11, 21 );

        REQUIRE( ssd.valid() == true );
        REQUIRE( ssd.path() == "2024_11_21" );
        REQUIRE( ssd.year() == 2024 );
        REQUIRE( ssd.month() == 11 );
        REQUIRE( ssd.day() == 21 );
    }

    GIVEN( "construction from Y/M/D, single digit M and D" )
    {
        MagAOX::file::stdSubDir ssd( 2024, 1, 1 );

        REQUIRE( ssd.valid() == true );
        REQUIRE( ssd.path() == "2024_01_01" );
        REQUIRE( ssd.year() == 2024 );
        REQUIRE( ssd.month() == 1 );
        REQUIRE( ssd.day() == 1 );
    }

    GIVEN( "construction from YYYY_MM_DD" )
    {
        MagAOX::file::stdSubDir ssd( "2024_11_21" );

        REQUIRE( ssd.valid() == true );
        REQUIRE( ssd.path() == "2024_11_21" );
        REQUIRE( ssd.year() == 2024 );
        REQUIRE( ssd.month() == 11 );
        REQUIRE( ssd.day() == 21 );
    }

    GIVEN( "construction from YYYY_MM_DD, single digit M and D" )
    {
        MagAOX::file::stdSubDir ssd( "2024_01_01" );

        REQUIRE( ssd.valid() == true );
        REQUIRE( ssd.path() == "2024_01_01" );
        REQUIRE( ssd.year() == 2024 );
        REQUIRE( ssd.month() == 1 );
        REQUIRE( ssd.day() == 1 );
    }
}

/** \test Scenario: Incrementing and decrementing stdSubDirs
 *
 * \anchor tests_libMagAOX_file_stdSubDir_inc_and_dec
 */
SCENARIO( "Incrementing and decrementing stdSubDirs", "[libMagAOX::file::stdSubDir]" )
{
    GIVEN( "decrementing subdir by days" )
    {
        MagAOX::file::stdSubDir ssd( "2024_11_21" );

        REQUIRE( ssd.valid() == true );

        ssd.subDay();

        REQUIRE( ssd.valid() == true );
        REQUIRE( ssd.path() == "2024_11_20" );
        REQUIRE( ssd.year() == 2024 );
        REQUIRE( ssd.month() == 11 );
        REQUIRE( ssd.day() == 20 );

        ssd.subDay();

        REQUIRE( ssd.valid() == true );
        REQUIRE( ssd.path() == "2024_11_19" );
        REQUIRE( ssd.year() == 2024 );
        REQUIRE( ssd.month() == 11 );
        REQUIRE( ssd.day() == 19 );
    }

    GIVEN( "incrementing subdir by days" )
    {
        MagAOX::file::stdSubDir ssd( "2024_11_21" );

        REQUIRE( ssd.valid() == true );

        ssd.addDay();

        REQUIRE( ssd.valid() == true );
        REQUIRE( ssd.path() == "2024_11_22" );
        REQUIRE( ssd.year() == 2024 );
        REQUIRE( ssd.month() == 11 );
        REQUIRE( ssd.day() == 22 );

        ssd.addDay();

        REQUIRE( ssd.valid() == true );
        REQUIRE( ssd.path() == "2024_11_23" );
        REQUIRE( ssd.year() == 2024 );
        REQUIRE( ssd.month() == 11 );
        REQUIRE( ssd.day() == 23 );
    }

    GIVEN( "incrementing then decrementing subdir by days" )
    {
        MagAOX::file::stdSubDir ssd( "2024_11_21" );

        REQUIRE( ssd.valid() == true );

        ssd.addDay();

        REQUIRE( ssd.valid() == true );
        REQUIRE( ssd.path() == "2024_11_22" );
        REQUIRE( ssd.year() == 2024 );
        REQUIRE( ssd.month() == 11 );
        REQUIRE( ssd.day() == 22 );

        ssd.subDay();

        REQUIRE( ssd.valid() == true );
        REQUIRE( ssd.path() == "2024_11_21" );
        REQUIRE( ssd.year() == 2024 );
        REQUIRE( ssd.month() == 11 );
        REQUIRE( ssd.day() == 21 );
    }
}

/** \test Scenario: Getting previous and following stdSubDirs
 *
 * \anchor tests_libMagAOX_file_stdSubDir_prev_and_foll
 */
SCENARIO( "Getting previous and following stdSubDirs", "[libMagAOX::file::stdSubDir]" )
{
    GIVEN( "getting previous subdir" )
    {
        MagAOX::file::stdSubDir ssd( "2024_11_21" );

        REQUIRE( ssd.valid() == true );

        MagAOX::file::stdSubDir psd = ssd.previousSubdir();

        REQUIRE( psd.valid() == true );
        REQUIRE( psd.path() == "2024_11_20" );
        REQUIRE( psd.year() == 2024 );
        REQUIRE( psd.month() == 11 );
        REQUIRE( psd.day() == 20 );
    }

    GIVEN( "getting previous subdir, month changes" )
    {
        MagAOX::file::stdSubDir ssd( "2024_11_01" );

        REQUIRE( ssd.valid() == true );

        MagAOX::file::stdSubDir psd = ssd.previousSubdir();

        REQUIRE( psd.valid() == true );
        REQUIRE( psd.path() == "2024_10_31" );
        REQUIRE( psd.year() == 2024 );
        REQUIRE( psd.month() == 10 );
        REQUIRE( psd.day() == 31 );
    }

    GIVEN( "getting previous subdir, month changes to feb, leap year" )
    {
        MagAOX::file::stdSubDir ssd( "2024_03_01" );

        REQUIRE( ssd.valid() == true );

        MagAOX::file::stdSubDir psd = ssd.previousSubdir();

        REQUIRE( psd.valid() == true );
        REQUIRE( psd.path() == "2024_02_29" );
        REQUIRE( psd.year() == 2024 );
        REQUIRE( psd.month() == 02 );
        REQUIRE( psd.day() == 29 );
    }

    GIVEN( "getting previous subdir, month changes to feb, not a leap year" )
    {
        MagAOX::file::stdSubDir ssd( "2025_03_01" );

        REQUIRE( ssd.valid() == true );

        MagAOX::file::stdSubDir psd = ssd.previousSubdir();

        REQUIRE( psd.valid() == true );
        REQUIRE( psd.path() == "2025_02_28" );
        REQUIRE( psd.year() == 2025 );
        REQUIRE( psd.month() == 02 );
        REQUIRE( psd.day() == 28 );
    }

    GIVEN( "getting previous subdir, year changes" )
    {
        MagAOX::file::stdSubDir ssd( "2024_01_01" );

        REQUIRE( ssd.valid() == true );

        MagAOX::file::stdSubDir psd = ssd.previousSubdir();

        REQUIRE( psd.valid() == true );
        REQUIRE( psd.path() == "2023_12_31" );
        REQUIRE( psd.year() == 2023 );
        REQUIRE( psd.month() == 12 );
        REQUIRE( psd.day() == 31 );
    }

    GIVEN( "getting following subdir" )
    {
        MagAOX::file::stdSubDir ssd( "2024_11_21" );

        REQUIRE( ssd.valid() == true );

        MagAOX::file::stdSubDir psd = ssd.followingSubdir();

        REQUIRE( psd.valid() == true );
        REQUIRE( psd.path() == "2024_11_22" );
        REQUIRE( psd.year() == 2024 );
        REQUIRE( psd.month() == 11 );
        REQUIRE( psd.day() == 22 );
    }

    GIVEN( "getting following subdir, month changes" )
    {
        MagAOX::file::stdSubDir ssd( "2024_11_30" );

        REQUIRE( ssd.valid() == true );

        MagAOX::file::stdSubDir psd = ssd.followingSubdir();

        REQUIRE( psd.valid() == true );
        REQUIRE( psd.path() == "2024_12_01" );
        REQUIRE( psd.year() == 2024 );
        REQUIRE( psd.month() == 12 );
        REQUIRE( psd.day() == 1 );
    }

    GIVEN( "getting following subdir, month changes to March, leap year" )
    {
        MagAOX::file::stdSubDir ssd( "2024_02_29" );

        REQUIRE( ssd.valid() == true );

        MagAOX::file::stdSubDir psd = ssd.followingSubdir();

        REQUIRE( psd.valid() == true );
        REQUIRE( psd.path() == "2024_03_01" );
        REQUIRE( psd.year() == 2024 );
        REQUIRE( psd.month() == 03 );
        REQUIRE( psd.day() == 1 );
    }

    GIVEN( "getting following subdir, month changes to March, not a leap year" )
    {
        MagAOX::file::stdSubDir ssd( "2025_02_28" );

        REQUIRE( ssd.valid() == true );

        MagAOX::file::stdSubDir psd = ssd.followingSubdir();

        REQUIRE( psd.valid() == true );
        REQUIRE( psd.path() == "2025_03_01" );
        REQUIRE( psd.year() == 2025 );
        REQUIRE( psd.month() == 03 );
        REQUIRE( psd.day() == 1 );
    }
}

} // namespace stdSubDir_test
