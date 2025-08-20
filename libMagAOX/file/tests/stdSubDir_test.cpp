/** \file stdSubDir_test.hpp
 * \brief Tests for the stdSubDir class
 * \ingroup file_files
 */

#include "../../../tests/catch2/catch.hpp"

#include "../stdSubDir.hpp"

#undef file_stdSubDir_hpp
#define XWCTEST_NAMESPACE XWCTEST_STDSUBDIR_CTORSYSDAYS_BAD_ALLOC_ns
#define XWCTEST_STDSUBDIR_CTORSYSDAYS_BAD_ALLOC
#include "../stdSubDir.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_STDSUBDIR_CTORSYSDAYS_BAD_ALLOC

#undef file_stdSubDir_hpp
#define XWCTEST_NAMESPACE XWCTEST_STDSUBDIR_CTORSYSDAYS_EXCEPTION_ns
#define XWCTEST_STDSUBDIR_CTORSYSDAYS_EXCEPTION
#include "../stdSubDir.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_STDSUBDIR_CTORSYSDAYS_EXCEPTION

#undef file_stdSubDir_hpp
#define XWCTEST_NAMESPACE XWCTEST_STDSUBDIR_YMD_BAD_ALLOC_ns
#define XWCTEST_STDSUBDIR_YMD_BAD_ALLOC
#include "../stdSubDir.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_STDSUBDIR_YMD_BAD_ALLOC

#undef file_stdSubDir_hpp
#define XWCTEST_NAMESPACE XWCTEST_STDSUBDIR_YMD_EXCEPTION_ns
#define XWCTEST_STDSUBDIR_YMD_EXCEPTION
#include "../stdSubDir.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_STDSUBDIR_YMD_EXCEPTION

#undef file_stdSubDir_hpp
#define XWCTEST_NAMESPACE XWCTEST_STDSUBDIR_SETPATH_BAD_ALLOC_ns
#define XWCTEST_STDSUBDIR_SETPATH_BAD_ALLOC
#include "../stdSubDir.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_STDSUBDIR_SETPATH_BAD_ALLOC

#undef file_stdSubDir_hpp
#define XWCTEST_NAMESPACE XWCTEST_STDSUBDIR_SETPATH_OUT_OF_RANGE_ns
#define XWCTEST_STDSUBDIR_SETPATH_OUT_OF_RANGE
#include "../stdSubDir.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_STDSUBDIR_SETPATH_OUT_OF_RANGE

#undef file_stdSubDir_hpp
#define XWCTEST_NAMESPACE XWCTEST_STDSUBDIR_SETPATH_EXCEPTION_ns
#define XWCTEST_STDSUBDIR_SETPATH_EXCEPTION
#include "../stdSubDir.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_STDSUBDIR_SETPATH_EXCEPTION

#undef file_stdSubDir_hpp
#define XWCTEST_NAMESPACE XWCTEST_STDSUBDIR_SETPATH_BAD_ALLOC2_ns
#define XWCTEST_STDSUBDIR_SETPATH_BAD_ALLOC2
#include "../stdSubDir.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_STDSUBDIR_SETPATH_BAD_ALLOC2

#undef file_stdSubDir_hpp
#define XWCTEST_NAMESPACE XWCTEST_STDSUBDIR_SETPATH_EXCEPTION2_ns
#define XWCTEST_STDSUBDIR_SETPATH_EXCEPTION2
#include "../stdSubDir.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_STDSUBDIR_SETPATH_EXCEPTION2

#undef file_stdSubDir_hpp
#define XWCTEST_NAMESPACE XWCTEST_STDSUBDIR_MAKEPATH_BAD_ALLOC_ns
#define XWCTEST_STDSUBDIR_MAKEPATH_BAD_ALLOC
#include "../stdSubDir.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_STDSUBDIR_MAKEPATH_BAD_ALLOC

#undef file_stdSubDir_hpp
#define XWCTEST_NAMESPACE XWCTEST_STDSUBDIR_MAKEPATH_FORMAT_ERROR_ns
#define XWCTEST_STDSUBDIR_MAKEPATH_FORMAT_ERROR
#include "../stdSubDir.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_STDSUBDIR_MAKEPATH_FORMAT_ERROR

#undef file_stdSubDir_hpp
#define XWCTEST_NAMESPACE XWCTEST_STDSUBDIR_MAKEPATH_EXCEPTION_ns
#define XWCTEST_STDSUBDIR_MAKEPATH_EXCEPTION
#include "../stdSubDir.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_STDSUBDIR_MAKEPATH_EXCEPTION

#undef file_stdSubDir_hpp
#define XWCTEST_NAMESPACE XWCTEST_STDSUBDIR_GYMD_BAD_ALLOC_ns
#define XWCTEST_STDSUBDIR_GYMD_BAD_ALLOC
#include "../stdSubDir.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_STDSUBDIR_GYMD_BAD_ALLOC

#undef file_stdSubDir_hpp
#define XWCTEST_NAMESPACE XWCTEST_STDSUBDIR_GYMD_EXCEPTION_ns
#define XWCTEST_STDSUBDIR_GYMD_EXCEPTION
#include "../stdSubDir.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_STDSUBDIR_GYMD_EXCEPTION

#undef file_stdSubDir_hpp
#define XWCTEST_NAMESPACE XWCTEST_STDSUBDIR_INC_BAD_ALLOC_ns
#define XWCTEST_STDSUBDIR_INC_BAD_ALLOC
#include "../stdSubDir.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_STDSUBDIR_INC_BAD_ALLOC

#undef file_stdSubDir_hpp
#define XWCTEST_NAMESPACE XWCTEST_STDSUBDIR_INC_EXCEPTION_ns
#define XWCTEST_STDSUBDIR_INC_EXCEPTION
#include "../stdSubDir.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_STDSUBDIR_INC_EXCEPTION

#undef file_stdSubDir_hpp
#define XWCTEST_NAMESPACE XWCTEST_STDSUBDIR_PREV_INVAL_ns
#define XWCTEST_STDSUBDIR_PREV_INVAL
#include "../stdSubDir.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_STDSUBDIR_PREV_INVAL

namespace libXWCTest
{
namespace fileTest
{
namespace stdSubDirTest
{

/// Initializing stdSubDir
/**
 * \test
 */
SCENARIO( "Initializing stdSubDir", "[libMagAOX::file::stdSubDir]" )
{
    GIVEN( "default construction" )
    {
        MagAOX::file::stdSubDir ssd;
        mx::error_t             errc;

        REQUIRE( !ssd.valid()  );

        std::string path = ssd.path();

        REQUIRE( path == "" );

        errc = mx::error_t::noerror;
        path = ssd.path( &errc );

        REQUIRE( errc == mx::error_t::invalidconfig );
        REQUIRE( path == "" );

        int year = ssd.year();

        REQUIRE( year == std::numeric_limits<int>::max() );

        errc = mx::error_t::noerror;
        year = ssd.year( &errc );

        REQUIRE( errc == mx::error_t::invalidconfig );
        REQUIRE( year == std::numeric_limits<int>::max() );

        unsigned int month = ssd.month();

        REQUIRE( month == std::numeric_limits<unsigned int>::max() );

        errc  = mx::error_t::noerror;
        month = ssd.month( &errc );

        REQUIRE( errc == mx::error_t::invalidconfig );
        REQUIRE( month == std::numeric_limits<unsigned int>::max() );

        unsigned int day = ssd.day();
        REQUIRE( day == std::numeric_limits<unsigned int>::max() );

        errc = mx::error_t::noerror;
        day  = ssd.day( &errc );

        REQUIRE( errc == mx::error_t::invalidconfig );
        REQUIRE( day == std::numeric_limits<unsigned int>::max() );
    }

    GIVEN( "default construction, then setting subdir" )
    {
        MagAOX::file::stdSubDir ssd;

        REQUIRE( !ssd.valid()  );

        ssd.path( "2024_11_21" );

        REQUIRE( ssd.valid()  );
        REQUIRE( ssd.path() == "2024_11_21" );
        REQUIRE( ssd.year() == 2024 );
        REQUIRE( ssd.month() == 11 );
        REQUIRE( ssd.day() == 21 );

        mx::error_t errc = mx::error_t::error;
        REQUIRE( ssd.year( &errc ) == 2024 );
        REQUIRE( errc == mx::error_t::noerror );

        errc = mx::error_t::error;
        REQUIRE( ssd.month( &errc ) == 11 );
        REQUIRE( errc == mx::error_t::noerror );

        errc = mx::error_t::error;
        REQUIRE( ssd.day( &errc ) == 21 );
        REQUIRE( errc == mx::error_t::noerror );
    }

    GIVEN( "construction by chrono::sys_days" )
    {
        std::chrono::year_month_day ymd{ std::chrono::year( 2024 ), std::chrono::month( 11 ), std::chrono::day( 21 ) };

        std::chrono::sys_days sysday = ymd;

        MagAOX::file::stdSubDir ssd( sysday );

        REQUIRE( ssd.valid()  );

        REQUIRE( ssd.path() == "2024_11_21" );
        REQUIRE( ssd.year() == 2024 );
        REQUIRE( ssd.month() == 11 );
        REQUIRE( ssd.day() == 21 );

        mx::error_t errc = mx::error_t::error;
        REQUIRE( ssd.year( &errc ) == 2024 );
        REQUIRE( errc == mx::error_t::noerror );

        errc = mx::error_t::error;
        REQUIRE( ssd.month( &errc ) == 11 );
        REQUIRE( errc == mx::error_t::noerror );

        errc = mx::error_t::error;
        REQUIRE( ssd.day( &errc ) == 21 );
        REQUIRE( errc == mx::error_t::noerror );
    }

    GIVEN( "construction by YMD" )
    {
        MagAOX::file::stdSubDir ssd( 2024, 11, 21 );

        REQUIRE( ssd.valid()  );

        REQUIRE( ssd.path() == "2024_11_21" );
        REQUIRE( ssd.year() == 2024 );
        REQUIRE( ssd.month() == 11 );
        REQUIRE( ssd.day() == 21 );

        mx::error_t errc = mx::error_t::error;
        REQUIRE( ssd.year( &errc ) == 2024 );
        REQUIRE( errc == mx::error_t::noerror );

        errc = mx::error_t::error;
        REQUIRE( ssd.month( &errc ) == 11 );
        REQUIRE( errc == mx::error_t::noerror );

        errc = mx::error_t::error;
        REQUIRE( ssd.day( &errc ) == 21 );
        REQUIRE( errc == mx::error_t::noerror );
    }

    GIVEN( "construction by path" )
    {
        MagAOX::file::stdSubDir ssd( "2024_11_21" );

        REQUIRE( ssd.valid()  );

        REQUIRE( ssd.path() == "2024_11_21" );
        REQUIRE( ssd.year() == 2024 );
        REQUIRE( ssd.month() == 11 );
        REQUIRE( ssd.day() == 21 );

        mx::error_t errc = mx::error_t::error;
        REQUIRE( ssd.year( &errc ) == 2024 );
        REQUIRE( errc == mx::error_t::noerror );

        errc = mx::error_t::error;
        REQUIRE( ssd.month( &errc ) == 11 );
        REQUIRE( errc == mx::error_t::noerror );

        errc = mx::error_t::error;
        REQUIRE( ssd.day( &errc ) == 21 );
        REQUIRE( errc == mx::error_t::noerror );
    }
}

/// Construction with errors
/**
 * \test
 */
SCENARIO( "Construction with errors", "[libMagAOX::file::stdSubDir]" )
{
    GIVEN( "Construction from sys_days causes bad_alloc" )
    {
        std::chrono::year_month_day ymd{ std::chrono::year( 2024 ), std::chrono::month( 11 ), std::chrono::day( 21 ) };

        std::chrono::sys_days sysday = ymd;

        bool caught = false;

        try
        {
            MagAOX::file::XWCTEST_STDSUBDIR_CTORSYSDAYS_BAD_ALLOC_ns::stdSubDir<mx::verbose::vv> ssd( sysday );
        }
        catch( const MagAOX::xwcException &e )
        {
            static_cast<void>( e );
            caught = true;
        }

        REQUIRE( caught == true );
    }

    GIVEN( "Construction from sys_days causes exception" )
    {
        std::chrono::year_month_day ymd{ std::chrono::year( 2024 ), std::chrono::month( 11 ), std::chrono::day( 21 ) };

        std::chrono::sys_days sysday = ymd;

        MagAOX::file::XWCTEST_STDSUBDIR_CTORSYSDAYS_EXCEPTION_ns::stdSubDir<mx::verbose::vv> ssd( sysday );

        REQUIRE( !ssd.valid()  );
    }

    GIVEN( "Construction from ymd causes bad_alloc" )
    {
        bool caught = false;

        try
        {
            MagAOX::file::XWCTEST_STDSUBDIR_YMD_BAD_ALLOC_ns::stdSubDir<mx::verbose::vv> ssd( 2024, 11, 21 );
        }
        catch( const MagAOX::xwcException &e )
        {
            static_cast<void>( e );
            caught = true;
        }

        REQUIRE( caught == true );
    }

    GIVEN( "Construction from ymd causes exception" )
    {
        MagAOX::file::XWCTEST_STDSUBDIR_YMD_EXCEPTION_ns::stdSubDir<mx::verbose::vv> ssd( 2024, 11, 21 );

        REQUIRE( !ssd.valid()  );
    }

    GIVEN( "Construction from path throws bad_alloc in stoT" )
    {
        bool caught = false;

        try
        {
            MagAOX::file::XWCTEST_STDSUBDIR_SETPATH_BAD_ALLOC_ns::stdSubDir<mx::verbose::vv> ssd( "2024_11_20" );
        }
        catch( const MagAOX::xwcException &e )
        {
            static_cast<void>( e );
            caught = true;
        }

        REQUIRE( caught == true );
    }

    GIVEN( "Constructing from path throws out_of_range in stoT" )
    {
        MagAOX::file::XWCTEST_STDSUBDIR_SETPATH_OUT_OF_RANGE_ns::stdSubDir<mx::verbose::vv> ssd( "2024_11_20" );

        REQUIRE( !ssd.valid()  );
    }

    GIVEN( "Construction from path throws exception in stoT" )
    {
        MagAOX::file::XWCTEST_STDSUBDIR_SETPATH_EXCEPTION_ns::stdSubDir<mx::verbose::vv> ssd( "2024_11_20" );

        REQUIRE( !ssd.valid()  );

        REQUIRE( !ssd.valid()  );
    }

    GIVEN( "Construction from path throws bad_alloc setting ymd" )
    {

        bool caught = false;

        try
        {
            MagAOX::file::XWCTEST_STDSUBDIR_SETPATH_BAD_ALLOC2_ns::stdSubDir<mx::verbose::vv> ssd( "2024_11_20" );
        }
        catch( const MagAOX::xwcException &e )
        {
            static_cast<void>( e );
            caught = true;
        }

        REQUIRE( caught == true );
    }

    GIVEN( "Construction from path throws exception setting ymd" )
    {
        MagAOX::file::XWCTEST_STDSUBDIR_SETPATH_EXCEPTION2_ns::stdSubDir<mx::verbose::vv> ssd( "2024_11_20" );

        REQUIRE( !ssd.valid()  );
    }
}

/// Setting path with errors
/**
 * \test
 */
SCENARIO( "Setting path with errors", "[libMagAOX::file::stdSubDir]" )
{
    GIVEN( "Setting path from string giving errors" )
    {
        MagAOX::file::stdSubDir ssd;

        REQUIRE( !ssd.valid()  );
        mx::error_t errc;

        errc = ssd.path( "2024_11_2" );
        REQUIRE( errc == mx::error_t::invalidarg );
        REQUIRE( !ssd.valid()  );

        errc = ssd.path( "2024x11_21" );
        REQUIRE( errc == mx::error_t::invalidarg );
        REQUIRE( !ssd.valid()  );

        errc = ssd.path( "2024_11x21" );
        REQUIRE( errc == mx::error_t::invalidarg );
        REQUIRE( !ssd.valid()  );

        errc = ssd.path( "Y024_11_24" );
        REQUIRE( errc == mx::error_t::invalidarg );
        REQUIRE( !ssd.valid()  );

        errc = ssd.path( "2Y24_11_24" );
        REQUIRE( errc == mx::error_t::invalidarg );
        REQUIRE( !ssd.valid()  );

        errc = ssd.path( "20Y4_11_24" );
        REQUIRE( errc == mx::error_t::invalidarg );
        REQUIRE( !ssd.valid()  );

        errc = ssd.path( "202Y_11_24" );
        REQUIRE( errc == mx::error_t::invalidarg );
        REQUIRE( !ssd.valid()  );

        errc = ssd.path( "2024_M1_24" );
        REQUIRE( errc == mx::error_t::invalidarg );
        REQUIRE( !ssd.valid()  );

        errc = ssd.path( "2024_2M_24" );
        REQUIRE( errc == mx::error_t::invalidarg );
        REQUIRE( !ssd.valid()  );

        errc = ssd.path( "2024_21_D4" );
        REQUIRE( errc == mx::error_t::invalidarg );
        REQUIRE( !ssd.valid()  );

        errc = ssd.path( "2024_21_2D" );
        REQUIRE( errc == mx::error_t::invalidarg );
        REQUIRE( !ssd.valid()  );
    }

    GIVEN( "Setting path throws bad_alloc in stoT" )
    {
        // This is slightly different than the construction test b/c we can test valid()
        MagAOX::file::XWCTEST_STDSUBDIR_SETPATH_BAD_ALLOC_ns::stdSubDir<mx::verbose::vv> ssd;

        REQUIRE( !ssd.valid()  );

        bool caught = false;

        try
        {
            ssd.path( "2024_11_20" );
        }
        catch( const MagAOX::xwcException &e )
        {
            static_cast<void>( e );
            caught = true;
        }

        REQUIRE( caught == true );
        REQUIRE( !ssd.valid()  );
    }

    GIVEN( "Setting path throws out_of_range in stoT" )
    {
        MagAOX::file::XWCTEST_STDSUBDIR_SETPATH_OUT_OF_RANGE_ns::stdSubDir<mx::verbose::vv> ssd;

        REQUIRE( !ssd.valid()  );

        mx::error_t errc = ssd.path( "2024_11_20" );

        REQUIRE( errc == mx::error_t::std_out_of_range );
        REQUIRE( !ssd.valid()  );
    }

    GIVEN( "Setting path throws exception in stoT" )
    {
        MagAOX::file::XWCTEST_STDSUBDIR_SETPATH_EXCEPTION_ns::stdSubDir<mx::verbose::vv> ssd;

        REQUIRE( !ssd.valid()  );

        mx::error_t errc = ssd.path( "2024_11_20" );

        REQUIRE( errc == mx::error_t::std_exception );
        REQUIRE( !ssd.valid()  );
    }

    GIVEN( "Setting path throws bad_alloc setting ymd" )
    {
        MagAOX::file::XWCTEST_STDSUBDIR_SETPATH_BAD_ALLOC2_ns::stdSubDir<mx::verbose::vv> ssd;

        REQUIRE( !ssd.valid()  );

        bool caught = false;

        try
        {
            ssd.path( "2024_11_20" );
        }
        catch( const MagAOX::xwcException &e )
        {
            static_cast<void>( e );
            caught = true;
        }

        REQUIRE( caught == true );
        REQUIRE( !ssd.valid()  );
    }

    GIVEN( "Setting path throws exception setting ymd" )
    {
        MagAOX::file::XWCTEST_STDSUBDIR_SETPATH_EXCEPTION2_ns::stdSubDir<mx::verbose::vv> ssd;

        REQUIRE( !ssd.valid()  );

        mx::error_t errc = ssd.path( "2024_11_20" );

        REQUIRE( errc == mx::error_t::std_exception );
        REQUIRE( !ssd.valid()  );
    }
}

/// Getting path with errors
/**
 * \test
 */
SCENARIO( "Getting path with errors", "[libMagAOX::file::stdSubDir]" )
{
    GIVEN( "Getting path throws bad_alloc making path" )
    {
        MagAOX::file::XWCTEST_STDSUBDIR_MAKEPATH_BAD_ALLOC_ns::stdSubDir<mx::verbose::vv> ssd( 2024, 11, 20 );

        REQUIRE( ssd.valid()  );

        bool caught = false;

        try
        {
            std::string p = ssd.path();
        }
        catch( const MagAOX::xwcException &e )
        {
            static_cast<void>( e );
            caught = true;
        }

        REQUIRE( caught == true );
    }

    GIVEN( "Getting path throws format_error making path" )
    {
        MagAOX::file::XWCTEST_STDSUBDIR_MAKEPATH_FORMAT_ERROR_ns::stdSubDir<mx::verbose::vv> ssd( 2024, 11, 20 );

        REQUIRE( ssd.valid()  );

        mx::error_t errc;
        std::string p = ssd.path( &errc );

        REQUIRE( errc == mx::error_t::std_format_error );
        REQUIRE( p == "" );
    }

    GIVEN( "Getting path throws exception making path" )
    {
        MagAOX::file::XWCTEST_STDSUBDIR_MAKEPATH_EXCEPTION_ns::stdSubDir<mx::verbose::vv> ssd( 2024, 11, 20 );

        REQUIRE( ssd.valid()  );

        mx::error_t errc;
        std::string p = ssd.path( &errc );

        REQUIRE( errc == mx::error_t::std_exception );
        REQUIRE( p == "" );
    }
}

/// Setting from YMD
/**
 * \test
 */
SCENARIO( "Setting from YMD", "[libMagAOX::file::stdSubDir]" )
{
    GIVEN( "default construction, setting from Y/M/D" )
    {
        MagAOX::file::stdSubDir ssd;

        std::string p = ssd.path();
        REQUIRE( p == "" );

        ssd.ymd( 2024, 11, 21 );

        REQUIRE( ssd.valid()  );
        REQUIRE( ssd.path() == "2024_11_21" );
        REQUIRE( ssd.year() == 2024 );
        REQUIRE( ssd.month() == 11 );
        REQUIRE( ssd.day() == 21 );
    }

    GIVEN( "construction from Y/M/D" )
    {
        MagAOX::file::stdSubDir ssd( 2024, 11, 21 );

        REQUIRE( ssd.valid()  );
        REQUIRE( ssd.path() == "2024_11_21" );
        REQUIRE( ssd.year() == 2024 );
        REQUIRE( ssd.month() == 11 );
        REQUIRE( ssd.day() == 21 );
    }

    GIVEN( "construction from Y/M/D, single digit M and D" )
    {
        MagAOX::file::stdSubDir ssd( 2024, 1, 1 );

        REQUIRE( ssd.valid()  );
        REQUIRE( ssd.path() == "2024_01_01" );
        REQUIRE( ssd.year() == 2024 );
        REQUIRE( ssd.month() == 1 );
        REQUIRE( ssd.day() == 1 );
    }

    GIVEN( "construction from YYYY_MM_DD" )
    {
        MagAOX::file::stdSubDir ssd( "2024_11_21" );

        REQUIRE( ssd.valid()  );
        REQUIRE( ssd.path() == "2024_11_21" );
        REQUIRE( ssd.year() == 2024 );
        REQUIRE( ssd.month() == 11 );
        REQUIRE( ssd.day() == 21 );
    }

    GIVEN( "construction from YYYY_MM_DD, single digit M and D" )
    {
        MagAOX::file::stdSubDir ssd( "2024_01_01" );

        REQUIRE( ssd.valid()  );
        REQUIRE( ssd.path() == "2024_01_01" );
        REQUIRE( ssd.year() == 2024 );
        REQUIRE( ssd.month() == 1 );
        REQUIRE( ssd.day() == 1 );
    }
}

/// Getting YMD with errors
/** \test
 */
SCENARIO( "Getting YMD with errors", "[libMagAOX::file::stdSubDir]" )
{
    GIVEN( "Getting year while invalid" )
    {
        MagAOX::file::stdSubDir ssd;

        mx::error_t errc;
        int         y = ssd.year( &errc );
        REQUIRE( y == std::numeric_limits<int>::max() );
        REQUIRE( errc == mx::error_t::invalidconfig );
    }

    GIVEN( "Getting year causes bad_alloc" )
    {
        MagAOX::file::XWCTEST_STDSUBDIR_GYMD_BAD_ALLOC_ns::stdSubDir ssd( 2024, 11, 20 );

        mx::error_t errc;
        bool        caught = false;
        try
        {
            int y = ssd.year( &errc );
            static_cast<void>( y );
        }
        catch( const MagAOX::xwcException &e )
        {
            caught = true;
        }

        REQUIRE( caught == true );
        REQUIRE( errc == mx::error_t::std_bad_alloc );
    }

    GIVEN( "Getting year causes exception" )
    {
        MagAOX::file::XWCTEST_STDSUBDIR_GYMD_EXCEPTION_ns::stdSubDir ssd( 2024, 11, 20 );

        mx::error_t errc;
        int         y = ssd.year( &errc );
        REQUIRE( y == std::numeric_limits<int>::max() );
        REQUIRE( errc == mx::error_t::std_exception );
    }

    GIVEN( "Getting month while invalid" )
    {
        MagAOX::file::stdSubDir ssd;

        mx::error_t  errc;
        unsigned int m = ssd.month( &errc );
        REQUIRE( m == std::numeric_limits<unsigned int>::max() );
        REQUIRE( errc == mx::error_t::invalidconfig );
    }

    GIVEN( "Getting month causes bad_alloc" )
    {
        MagAOX::file::XWCTEST_STDSUBDIR_GYMD_BAD_ALLOC_ns::stdSubDir ssd( 2024, 11, 20 );

        mx::error_t errc;
        bool        caught = false;
        try
        {
            unsigned int m = ssd.month( &errc );
            static_cast<void>( m );
        }
        catch( const MagAOX::xwcException &e )
        {
            caught = true;
        }

        REQUIRE( caught == true );
        REQUIRE( errc == mx::error_t::std_bad_alloc );
    }

    GIVEN( "Getting month causes exception" )
    {
        MagAOX::file::XWCTEST_STDSUBDIR_GYMD_EXCEPTION_ns::stdSubDir ssd( 2024, 11, 20 );

        mx::error_t  errc;
        unsigned int m = ssd.month( &errc );
        REQUIRE( m == std::numeric_limits<unsigned int>::max() );
        REQUIRE( errc == mx::error_t::std_exception );
    }

    GIVEN( "Getting day while invalid" )
    {
        MagAOX::file::stdSubDir ssd;

        mx::error_t  errc;
        unsigned int d = ssd.day( &errc );
        REQUIRE( d == std::numeric_limits<unsigned int>::max() );
        REQUIRE( errc == mx::error_t::invalidconfig );
    }

    GIVEN( "Getting day causes bad_alloc" )
    {
        MagAOX::file::XWCTEST_STDSUBDIR_GYMD_BAD_ALLOC_ns::stdSubDir ssd( 2024, 11, 20 );

        mx::error_t errc;
        bool        caught = false;
        try
        {
            unsigned int d = ssd.day( &errc );
            static_cast<void>( d );
        }
        catch( const MagAOX::xwcException &e )
        {
            caught = true;
        }

        REQUIRE( caught == true );
        REQUIRE( errc == mx::error_t::std_bad_alloc );
    }

    GIVEN( "Getting day causes exception" )
    {
        MagAOX::file::XWCTEST_STDSUBDIR_GYMD_EXCEPTION_ns::stdSubDir ssd( 2024, 11, 20 );

        mx::error_t  errc;
        unsigned int d = ssd.day( &errc );
        REQUIRE( d == std::numeric_limits<unsigned int>::max() );
        REQUIRE( errc == mx::error_t::std_exception );
    }
}

/// Incrementing and decrementing stdSubDirs
/** \test
 */
SCENARIO( "Incrementing and decrementing stdSubDirs", "[libMagAOX::file::stdSubDir]" )
{
    GIVEN( "decrementing subdir by days" )
    {
        MagAOX::file::stdSubDir ssd( "2024_11_21" );

        REQUIRE( ssd.valid()  );

        ssd.subDay();

        REQUIRE( ssd.valid()  );
        REQUIRE( ssd.path() == "2024_11_20" );
        REQUIRE( ssd.year() == 2024 );
        REQUIRE( ssd.month() == 11 );
        REQUIRE( ssd.day() == 20 );

        ssd.subDay();

        REQUIRE( ssd.valid()  );
        REQUIRE( ssd.path() == "2024_11_19" );
        REQUIRE( ssd.year() == 2024 );
        REQUIRE( ssd.month() == 11 );
        REQUIRE( ssd.day() == 19 );
    }

    GIVEN( "incrementing subdir by days" )
    {
        MagAOX::file::stdSubDir ssd( "2024_11_21" );

        REQUIRE( ssd.valid()  );

        ssd.addDay();

        REQUIRE( ssd.valid()  );
        REQUIRE( ssd.path() == "2024_11_22" );
        REQUIRE( ssd.year() == 2024 );
        REQUIRE( ssd.month() == 11 );
        REQUIRE( ssd.day() == 22 );

        ssd.addDay();

        REQUIRE( ssd.valid()  );
        REQUIRE( ssd.path() == "2024_11_23" );
        REQUIRE( ssd.year() == 2024 );
        REQUIRE( ssd.month() == 11 );
        REQUIRE( ssd.day() == 23 );
    }

    GIVEN( "incrementing then decrementing subdir by days" )
    {
        MagAOX::file::stdSubDir ssd( "2024_11_21" );

        REQUIRE( ssd.valid()  );

        ssd.addDay();

        REQUIRE( ssd.valid()  );
        REQUIRE( ssd.path() == "2024_11_22" );
        REQUIRE( ssd.year() == 2024 );
        REQUIRE( ssd.month() == 11 );
        REQUIRE( ssd.day() == 22 );

        ssd.subDay();

        REQUIRE( ssd.valid()  );
        REQUIRE( ssd.path() == "2024_11_21" );
        REQUIRE( ssd.year() == 2024 );
        REQUIRE( ssd.month() == 11 );
        REQUIRE( ssd.day() == 21 );
    }
}

/// Incrementing and decrementing stdSubDirs with errors
/** \test
 */
SCENARIO( "Incrementing and decrementing stdSubDirs with errors", "[libMagAOX::file::stdSubDir]" )
{
    GIVEN( "decrementing while invalid" )
    {
        MagAOX::file::stdSubDir ssd;

        mx::error_t errc = ssd.subDay();

        REQUIRE( !ssd.valid()  );
        REQUIRE( errc == mx::error_t::invalidconfig );
    }

    GIVEN( "decrementing causes bad_alloc" )
    {
        bool caught = false;

        try
        {
            MagAOX::file::XWCTEST_STDSUBDIR_INC_BAD_ALLOC_ns::stdSubDir ssd( 2024, 11, 20 );
            ssd.subDay();
        }
        catch( const MagAOX::xwcException &e )
        {
            caught = true;
        }

        REQUIRE( caught == true );
    }

    GIVEN( "decrementing causes exception" )
    {
        MagAOX::file::XWCTEST_STDSUBDIR_INC_EXCEPTION_ns::stdSubDir ssd( 2024, 11, 20 );

        mx::error_t errc = ssd.subDay();

        REQUIRE( !ssd.valid() );
        REQUIRE( errc == mx::error_t::std_exception );
    }

    GIVEN( "incrementing while invalid" )
    {
        MagAOX::file::stdSubDir ssd;

        mx::error_t errc = ssd.addDay();

        REQUIRE( !ssd.valid()  );
        REQUIRE( errc == mx::error_t::invalidconfig );
    }

    GIVEN( "incrementing causes bad_alloc" )
    {
        bool caught = false;

        try
        {
            MagAOX::file::XWCTEST_STDSUBDIR_INC_BAD_ALLOC_ns::stdSubDir ssd( 2024, 11, 20 );
            ssd.addDay();
        }
        catch( const MagAOX::xwcException &e )
        {
            caught = true;
        }

        REQUIRE( caught == true );
    }

    GIVEN( "incrementing causes exception" )
    {
        MagAOX::file::XWCTEST_STDSUBDIR_INC_EXCEPTION_ns::stdSubDir ssd( 2024, 11, 20 );

        mx::error_t errc = ssd.addDay();

        REQUIRE( !ssd.valid()  );
        REQUIRE( errc == mx::error_t::std_exception );
    }
}

/// Getting previous and following stdSubDirs
/**
 * \test
 */
SCENARIO( "Getting previous and following stdSubDirs", "[libMagAOX::file::stdSubDir]" )
{
    GIVEN( "getting previous subdir" )
    {
        MagAOX::file::stdSubDir ssd( "2024_11_21" );

        REQUIRE( ssd.valid()  );

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

        REQUIRE( ssd.valid()  );

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

        REQUIRE( ssd.valid()  );

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

        REQUIRE( ssd.valid()  );

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

        REQUIRE( ssd.valid()  );

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

        REQUIRE( ssd.valid()  );

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

        REQUIRE( ssd.valid()  );

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

        REQUIRE( ssd.valid()  );

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

        REQUIRE( ssd.valid()  );

        MagAOX::file::stdSubDir psd = ssd.followingSubdir();

        REQUIRE( psd.valid() == true );
        REQUIRE( psd.path() == "2025_03_01" );
        REQUIRE( psd.year() == 2025 );
        REQUIRE( psd.month() == 03 );
        REQUIRE( psd.day() == 1 );
    }
}

/// Getting previous and following stdSubDirs with errors
/**
 * \test
 */
SCENARIO( "Getting previous and following stdSubDirs with errors", "[libMagAOX::file::stdSubDir]" )
{
    GIVEN( "getting previous subdir while invalid" )
    {
        MagAOX::file::stdSubDir ssd;

        REQUIRE( !ssd.valid()  );

        mx::error_t             errc;
        MagAOX::file::stdSubDir psd = ssd.previousSubdir( &errc );

        REQUIRE( psd.valid() == false );
        REQUIRE( errc == mx::error_t::invalidconfig );
    }

    GIVEN( "getting previous subdir error creating new subdir" )
    {
        MagAOX::file::XWCTEST_STDSUBDIR_PREV_INVAL_ns::stdSubDir ssd( 2024, 11, 20 );

        REQUIRE( ssd.valid()  );

        mx::error_t                                              errc;
        MagAOX::file::XWCTEST_STDSUBDIR_PREV_INVAL_ns::stdSubDir psd = ssd.previousSubdir( &errc );

        REQUIRE( psd.valid() == false );
        REQUIRE( errc == mx::error_t::error );
    }

    GIVEN( "getting previous subdir causes bad_alloc" )
    {
        MagAOX::file::XWCTEST_STDSUBDIR_INC_BAD_ALLOC_ns::stdSubDir ssd( 2024, 11, 20 );

        REQUIRE( ssd.valid()  );

        bool caught = false;

        try
        {
            ssd.previousSubdir();
        }
        catch( const MagAOX::xwcException &e )
        {
            caught = true;
        }

        REQUIRE( caught == true );
    }

    GIVEN( "getting previous subdir causes exception" )
    {
        MagAOX::file::XWCTEST_STDSUBDIR_INC_EXCEPTION_ns::stdSubDir ssd( 2024, 11, 20 );

        REQUIRE( ssd.valid()  );

        mx::error_t                                                 errc;
        MagAOX::file::XWCTEST_STDSUBDIR_INC_EXCEPTION_ns::stdSubDir pssd = ssd.previousSubdir( &errc );

        REQUIRE( !pssd.valid()  );
        REQUIRE( errc == mx::error_t::std_exception );
    }

    GIVEN( "getting following subdir while invalid" )
    {
        MagAOX::file::stdSubDir ssd;

        REQUIRE( !ssd.valid()  );

        mx::error_t             errc;
        MagAOX::file::stdSubDir psd = ssd.followingSubdir( &errc );

        REQUIRE( psd.valid() == false );
        REQUIRE( errc == mx::error_t::invalidconfig );
    }

    GIVEN( "getting following subdir error creating new subdir" )
    {
        MagAOX::file::XWCTEST_STDSUBDIR_PREV_INVAL_ns::stdSubDir ssd( 2024, 11, 20 );

        REQUIRE( ssd.valid()  );

        mx::error_t                                              errc;
        MagAOX::file::XWCTEST_STDSUBDIR_PREV_INVAL_ns::stdSubDir psd = ssd.followingSubdir( &errc );

        REQUIRE( psd.valid() == false );
        REQUIRE( errc == mx::error_t::error );
    }

    GIVEN( "getting following subdir causes bad_alloc" )
    {
        MagAOX::file::XWCTEST_STDSUBDIR_INC_BAD_ALLOC_ns::stdSubDir ssd( 2024, 11, 20 );

        REQUIRE( ssd.valid()  );

        bool caught = false;

        try
        {
            ssd.followingSubdir();
        }
        catch( const MagAOX::xwcException &e )
        {
            caught = true;
        }

        REQUIRE( caught == true );
    }

    GIVEN( "getting following subdir causes exception" )
    {
        MagAOX::file::XWCTEST_STDSUBDIR_INC_EXCEPTION_ns::stdSubDir ssd( 2024, 11, 20 );

        REQUIRE( ssd.valid()  );

        mx::error_t                                                 errc;
        MagAOX::file::XWCTEST_STDSUBDIR_INC_EXCEPTION_ns::stdSubDir pssd = ssd.followingSubdir( &errc );

        REQUIRE( !pssd.valid()  );
        REQUIRE( errc == mx::error_t::std_exception );
    }
}

/// Using comparison operators
/**
 * \test
 */
SCENARIO( "Using comparison operators", "[libMagAOX::file::stdSubDir]" )
{
    GIVEN( "testing equality while equal" )
    {
        MagAOX::file::stdSubDir ssd1( 2024, 11, 20 );
        MagAOX::file::stdSubDir ssd2( 2024, 11, 20 );

        REQUIRE( ssd1 == ssd2 );
    }

    GIVEN( "testing equality while not equal" )
    {
        MagAOX::file::stdSubDir ssd1( 2024, 11, 20 );
        MagAOX::file::stdSubDir ssd2( 2024, 11, 21 );

        REQUIRE( !( ssd1 == ssd2 ) );
    }

    GIVEN( "testing inequality while equal" )
    {
        MagAOX::file::stdSubDir ssd1( 2024, 11, 20 );
        MagAOX::file::stdSubDir ssd2( 2024, 11, 20 );

        REQUIRE( !( ssd1 != ssd2 ) );
    }

    GIVEN( "testing inequality while not equal" )
    {
        MagAOX::file::stdSubDir ssd1( 2024, 11, 20 );
        MagAOX::file::stdSubDir ssd2( 2024, 11, 21 );

        REQUIRE( ssd1 != ssd2 );
    }

    GIVEN( "testing less-than while equal" )
    {
        MagAOX::file::stdSubDir ssd1( 2024, 11, 20 );
        MagAOX::file::stdSubDir ssd2( 2024, 11, 20 );

        REQUIRE( !( ssd1 < ssd2 ) );
    }

    GIVEN( "testing less-than while less-than" )
    {
        MagAOX::file::stdSubDir ssd1( 2024, 11, 20 );
        MagAOX::file::stdSubDir ssd2( 2024, 11, 21 );

        REQUIRE( ssd1 < ssd2 );
    }

    GIVEN( "testing less-than while greater-than" )
    {
        MagAOX::file::stdSubDir ssd1( 2024, 11, 20 );
        MagAOX::file::stdSubDir ssd2( 2024, 11, 21 );

        REQUIRE( !( ssd2 < ssd2 ) );
    }

    GIVEN( "testing less-than-or-equal while equal" )
    {
        MagAOX::file::stdSubDir ssd1( 2024, 11, 20 );
        MagAOX::file::stdSubDir ssd2( 2024, 11, 20 );

        REQUIRE( ( ssd1 <= ssd2 ) );
    }

    GIVEN( "testing less-than-or-equal while less-than" )
    {
        MagAOX::file::stdSubDir ssd1( 2024, 11, 20 );
        MagAOX::file::stdSubDir ssd2( 2024, 11, 21 );

        REQUIRE( ssd1 <= ssd2 );
    }

    GIVEN( "testing less-than-or-equal while greater-than" )
    {
        MagAOX::file::stdSubDir ssd1( 2024, 11, 20 );
        MagAOX::file::stdSubDir ssd2( 2024, 11, 21 );

        REQUIRE( !( ssd2 <= ssd1 ) );
    }

    GIVEN( "testing greater-than while equal" )
    {
        MagAOX::file::stdSubDir ssd1( 2024, 11, 20 );
        MagAOX::file::stdSubDir ssd2( 2024, 11, 20 );

        REQUIRE( !( ssd1 > ssd2 ) );
    }

    GIVEN( "testing greater-than while greater-than" )
    {
        MagAOX::file::stdSubDir ssd1( 2024, 11, 20 );
        MagAOX::file::stdSubDir ssd2( 2024, 11, 21 );

        REQUIRE( ssd2 > ssd1 );
    }

    GIVEN( "testing greater-than while less-than" )
    {
        MagAOX::file::stdSubDir ssd1( 2024, 11, 20 );
        MagAOX::file::stdSubDir ssd2( 2024, 11, 21 );

        REQUIRE( !( ssd1 > ssd2 ) );
    }

    GIVEN( "testing greater-than-or-equal while equal" )
    {
        MagAOX::file::stdSubDir ssd1( 2024, 11, 20 );
        MagAOX::file::stdSubDir ssd2( 2024, 11, 20 );

        REQUIRE( ssd1 >= ssd2 );
    }

    GIVEN( "testing greater-than-or-equal while greater-than" )
    {
        MagAOX::file::stdSubDir ssd1( 2024, 11, 20 );
        MagAOX::file::stdSubDir ssd2( 2024, 11, 21 );

        REQUIRE( ssd2 >= ssd1 );
    }

    GIVEN( "testing greater-than-or-equal while less-than" )
    {
        MagAOX::file::stdSubDir ssd1( 2024, 11, 20 );
        MagAOX::file::stdSubDir ssd2( 2024, 11, 21 );

        REQUIRE( !( ssd1 >= ssd2 ) );
    }
}

} // namespace stdSubDirTest
} // namespace fileTest
} // namespace libXWCTest
