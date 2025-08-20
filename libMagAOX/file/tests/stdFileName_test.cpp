/** \file stdFileName_test.hpp
 * \brief Tests for the stdFileName class
 * \ingroup file_files
 */

#include "../../../tests/catch2/catch.hpp"

#include <filesystem>

#include "../stdFileName.hpp"

#undef file_stdFileName_hpp
#define XWCTEST_NAMESPACE XWCTEST_STDFILENAME_FULLNAME_BAD_ALLOC_ns
#define XWCTEST_STDFILENAME_FULLNAME_BAD_ALLOC
#include "../stdFileName.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_STDFILENAME_FULLNAME_BAD_ALLOC

#undef file_stdFileName_hpp
#define XWCTEST_NAMESPACE XWCTEST_STDFILENAME_FULLNAME_EXCEPTION_ns
#define XWCTEST_STDFILENAME_FULLNAME_EXCEPTION
#include "../stdFileName.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_STDFILENAME_FULLNAME_EXCEPTION

#undef file_stdFileName_hpp
#define XWCTEST_NAMESPACE XWCTEST_STDFILENAME_FULLNAME_FS_BAD_ALLOC_ns
#define XWCTEST_STDFILENAME_FULLNAME_FS_BAD_ALLOC
#include "../stdFileName.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_STDFILENAME_FULLNAME_FS_BAD_ALLOC

#undef file_stdFileName_hpp
#define XWCTEST_NAMESPACE XWCTEST_STDFILENAME_FULLNAME_FS_FILESYSTEM_ERROR_ns
#define XWCTEST_STDFILENAME_FULLNAME_FS_FILESYSTEM_ERROR
#include "../stdFileName.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_STDFILENAME_FULLNAME_FS_FILESYSTEM_ERROR

#undef file_stdFileName_hpp
#define XWCTEST_NAMESPACE XWCTEST_STDFILENAME_FULLNAME_FS_EXCEPTION_ns
#define XWCTEST_STDFILENAME_FULLNAME_FS_EXCEPTION
#include "../stdFileName.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_STDFILENAME_FULLNAME_FS_EXCEPTION

#undef file_fileTimes_hpp
#undef file_stdFileName_hpp
#define XWCTEST_NAMESPACE XWCTEST_PARSEFILEPATH_THROW_BAD_ALLOC_ns
#define XWCTEST_PARSEFILEPATH_THROW_BAD_ALLOC
#include "../fileTimes.hpp"
#include "../stdFileName.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_PARSEFILEPATH_THROW_BAD_ALLOC

#undef file_fileTimes_hpp
#undef file_stdFileName_hpp
#define XWCTEST_NAMESPACE XWCTEST_PARSEFILEPATH_THROW_OUT_OF_RANGE_ns
#define XWCTEST_PARSEFILEPATH_THROW_OUT_OF_RANGE
#include "../fileTimes.hpp"
#include "../stdFileName.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_PARSEFILEPATH_THROW_OUT_OF_RANGE

#undef file_stdSubDir_hpp
#undef file_stdFileName_hpp
#define XWCTEST_NAMESPACE XWCTEST_STDSUBDIR_YMD_BAD_ALLOC_ns
#define XWCTEST_STDSUBDIR_YMD_BAD_ALLOC
#include "../stdSubDir.hpp"
#include "../stdFileName.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_STDSUBDIR_YMD_BAD_ALLOC

#undef file_stdSubDir_hpp
#undef file_stdFileName_hpp
#define XWCTEST_NAMESPACE XWCTEST_STDSUBDIR_YMD_EXCEPTION_ns
#define XWCTEST_STDSUBDIR_YMD_EXCEPTION
#include "../stdSubDir.hpp"
#include "../stdFileName.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_STDSUBDIR_YMD_EXCEPTION

#undef file_stdFileName_hpp
#define XWCTEST_NAMESPACE XWCTEST_STDFILENAME_FULLNAME_TIMEGM_ns
#define XWCTEST_STDFILENAME_FULLNAME_TIMEGM
#include "../stdFileName.hpp"
#undef XWCTEST_NAMESPACE
#undef XWCTEST_STDFILENAME_FULLNAME_TIMEGM

namespace libXWCTest
{
namespace fileTest
{
namespace stdFileNameTest
{

/// Constructing and Initializing stdFileName
/**
 * \test
 */
SCENARIO( "Construction and Initializing stdFileName", "[libMagAOX::file::stdFileName]" )
{
    GIVEN( "default construction and parsing and member access" )
    {
        std::string fullName = "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20241121063321000000001.binlog";

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

        REQUIRE( sfn.valid() );
    }

    GIVEN( "default construction, assignment and member access" )
    {
        std::string fullName = "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20241121063321000000001.binlog";

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

        REQUIRE( sfn.valid() );
    }

    GIVEN( "construction by parsing and member access" )
    {
        std::string fullName = "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20241121063321000000001.binlog";

        MagAOX::file::stdFileName sfn( fullName );

        mx::error_t errc = mx::error_t::error;
        REQUIRE( sfn.fullName( &errc ) == fullName );
        REQUIRE( errc == mx::error_t::noerror );

        errc = mx::error_t::error;
        REQUIRE( sfn.baseName( &errc ) == "bamm_20241121063321000000001.binlog" );
        REQUIRE( errc == mx::error_t::noerror );

        errc = mx::error_t::error;
        REQUIRE( sfn.appName( &errc ) == "bamm" );
        REQUIRE( errc == mx::error_t::noerror );

        errc = mx::error_t::error;
        REQUIRE( sfn.extension( &errc ) == ".binlog" );
        REQUIRE( errc == mx::error_t::noerror );

        errc              = mx::error_t::error;
        mx::error_t errc2 = mx::error_t::error;
        REQUIRE( sfn.subDir( &errc ).path( &errc2 ) == "2024_11_21" );
        REQUIRE( errc == mx::error_t::noerror );
        REQUIRE( errc2 == mx::error_t::noerror );

        errc = mx::error_t::error;
        REQUIRE( sfn.year( &errc ) == 2024 );
        REQUIRE( errc == mx::error_t::noerror );

        errc = mx::error_t::error;
        REQUIRE( sfn.month( &errc ) == 11 );
        REQUIRE( errc == mx::error_t::noerror );

        errc = mx::error_t::error;
        REQUIRE( sfn.day( &errc ) == 21 );
        REQUIRE( errc == mx::error_t::noerror );

        errc = mx::error_t::error;
        REQUIRE( sfn.hour( &errc ) == 6 );
        REQUIRE( errc == mx::error_t::noerror );

        errc = mx::error_t::error;
        REQUIRE( sfn.minute( &errc ) == 33 );
        REQUIRE( errc == mx::error_t::noerror );

        errc = mx::error_t::error;
        REQUIRE( sfn.second( &errc ) == 21 );
        REQUIRE( errc == mx::error_t::noerror );

        errc = mx::error_t::error;
        REQUIRE( sfn.nsec( &errc ) == 1 );
        REQUIRE( errc == mx::error_t::noerror );

        errc                   = mx::error_t::error;
        flatlogs::timespecX ts = sfn.timestamp( &errc );
        REQUIRE( ts.time_s == 1732170801 );
        REQUIRE( ts.time_ns == 1 );
        REQUIRE( errc == mx::error_t::noerror );

        REQUIRE( sfn.valid() );
    }
}

/// Member Access Errors
/**
 * \test
 */
SCENARIO( "Member Access Errors", "[libMagAOX::file::stdFileName]" )
{
    GIVEN( "default construction, access while invalid" )
    {
        MagAOX::file::stdFileName sfn;

        mx::error_t errc = mx::error_t::noerror;
        REQUIRE( sfn.fullName( &errc ) == "" );
        REQUIRE( errc == mx::error_t::invalidconfig );

        errc = mx::error_t::noerror;
        REQUIRE( sfn.baseName( &errc ) == "" );
        REQUIRE( errc == mx::error_t::invalidconfig );

        errc = mx::error_t::noerror;
        REQUIRE( sfn.appName( &errc ) == "" );
        REQUIRE( errc == mx::error_t::invalidconfig );

        errc = mx::error_t::noerror;
        REQUIRE( sfn.extension( &errc ) == "" );
        REQUIRE( errc == mx::error_t::invalidconfig );

        errc              = mx::error_t::noerror;
        mx::error_t errc2 = mx::error_t::noerror;
        REQUIRE( sfn.subDir( &errc ).path( &errc2 ) == "" );
        REQUIRE( errc == mx::error_t::invalidconfig );
        REQUIRE( errc2 == mx::error_t::invalidconfig );

        errc = mx::error_t::noerror;
        REQUIRE( sfn.year( &errc ) == std::numeric_limits<int>::max() );
        REQUIRE( errc == mx::error_t::invalidconfig );

        errc = mx::error_t::noerror;
        REQUIRE( sfn.month( &errc ) == std::numeric_limits<unsigned int>::max() );
        REQUIRE( errc == mx::error_t::invalidconfig );

        errc = mx::error_t::noerror;
        REQUIRE( sfn.day( &errc ) == std::numeric_limits<unsigned int>::max() );
        REQUIRE( errc == mx::error_t::invalidconfig );

        errc = mx::error_t::noerror;
        REQUIRE( sfn.hour( &errc ) == std::numeric_limits<int>::max() );
        REQUIRE( errc == mx::error_t::invalidconfig );

        errc = mx::error_t::noerror;
        REQUIRE( sfn.minute( &errc ) == std::numeric_limits<int>::max() );
        REQUIRE( errc == mx::error_t::invalidconfig );

        errc = mx::error_t::noerror;
        REQUIRE( sfn.second( &errc ) == std::numeric_limits<int>::max() );
        REQUIRE( errc == mx::error_t::invalidconfig );

        errc = mx::error_t::noerror;
        REQUIRE( sfn.nsec( &errc ) == std::numeric_limits<int>::max() );
        REQUIRE( errc == mx::error_t::invalidconfig );

        errc                   = mx::error_t::noerror;
        flatlogs::timespecX ts = sfn.timestamp( &errc );
        REQUIRE( ts.time_s == 0 );
        REQUIRE( ts.time_ns == 0 );
        REQUIRE( errc == mx::error_t::invalidconfig );

        REQUIRE( !sfn.valid() );
    }
}
/// Setting fullName errors
/**
 * \test
 */
SCENARIO( "Setting fullName Errors", "[libMagAOX::file::stdFileName]" )
{
    GIVEN( "Construction with fullname throws bad_alloc" )
    {
        bool caught = false;

        try
        {
            MagAOX::file::XWCTEST_STDFILENAME_FULLNAME_BAD_ALLOC_ns::stdFileName sfn(
                "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20241121063321000000001.binlog" );
        }
        catch( const MagAOX::xwcException &e )
        {
            caught = true;
        }

        REQUIRE( caught == true );
    }

    GIVEN( "Construction with fullname throws exception" )
    {

        MagAOX::file::XWCTEST_STDFILENAME_FULLNAME_EXCEPTION_ns::stdFileName sfn(
            "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20241121063321000000001.binlog" );

        REQUIRE( !sfn.valid() );
    }

    GIVEN( "Default Construction, setting fullname throws exception" )
    {

        MagAOX::file::XWCTEST_STDFILENAME_FULLNAME_EXCEPTION_ns::stdFileName sfn;

        mx::error_t errc = sfn.fullName( "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20241121063321000000001.binlog" );

        REQUIRE( !sfn.valid() );
        REQUIRE( errc == mx::error_t::std_exception );
    }

    GIVEN( "Construction with fullname throws bad_alloc from fs" )
    {
        bool caught = false;

        try
        {
            MagAOX::file::XWCTEST_STDFILENAME_FULLNAME_FS_BAD_ALLOC_ns::stdFileName sfn(
                "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20241121063321000000001.binlog" );
        }
        catch( const MagAOX::xwcException &e )
        {
            caught = true;
        }

        REQUIRE( caught == true );
    }

    GIVEN( "Construction with fullname throws filesystem_error from fs" )
    {

        MagAOX::file::XWCTEST_STDFILENAME_FULLNAME_FS_FILESYSTEM_ERROR_ns::stdFileName sfn(
            "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20241121063321000000001.binlog" );

        REQUIRE( !sfn.valid() );
    }

    GIVEN( "Construction with fullname throws exception from fs" )
    {

        MagAOX::file::XWCTEST_STDFILENAME_FULLNAME_FS_EXCEPTION_ns::stdFileName sfn(
            "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20241121063321000000001.binlog" );

        REQUIRE( !sfn.valid() );
    }

    GIVEN( "Default Construction, setting fullname throws filesystem_error from fs" )
    {

        MagAOX::file::XWCTEST_STDFILENAME_FULLNAME_FS_FILESYSTEM_ERROR_ns::stdFileName sfn;

        mx::error_t errc = sfn.fullName( "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20241121063321000000001.binlog" );

        REQUIRE( !sfn.valid() );
        REQUIRE( errc == mx::error_t::std_filesystem_error );
    }

    GIVEN( "Default Construction, setting fullname throws exception from fs" )
    {

        MagAOX::file::XWCTEST_STDFILENAME_FULLNAME_FS_EXCEPTION_ns::stdFileName sfn;

        mx::error_t errc = sfn.fullName( "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20241121063321000000001.binlog" );

        REQUIRE( !sfn.valid() );
        REQUIRE( errc == mx::error_t::std_exception );
    }

    GIVEN( "Default Construction, setting fullname without extension" )
    {

        MagAOX::file::stdFileName sfn;

        mx::error_t errc = sfn.fullName( "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20241121063321000000001" );

        REQUIRE( !sfn.valid() );
        REQUIRE( errc == mx::error_t::invalidarg );
    }

    GIVEN( "Default Construction, setting fullname throws exception from parseFilePath" )
    {

        MagAOX::file::XWCTEST_PARSEFILEPATH_THROW_BAD_ALLOC_ns::stdFileName sfn;

        bool caught = false;

        try
        {
            sfn.fullName( "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20241121063321000000001.binlog" );
        }
        catch( const MagAOX::xwcException &e )
        {
            caught = true;
        }
        REQUIRE( caught == true );
    }

    GIVEN( "Default Construction, setting fullname causes error from parseFilePath" )
    {

        MagAOX::file::XWCTEST_PARSEFILEPATH_THROW_OUT_OF_RANGE_ns::stdFileName sfn;

        mx::error_t errc = sfn.fullName( "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20241121063321000000001.binlog" );

        REQUIRE( !sfn.valid() );
        REQUIRE( errc == mx::error_t::std_out_of_range );
    }

    GIVEN( "Default Construction, setting fullname error in year" )
    {

        MagAOX::file::stdFileName sfn;

        mx::error_t errc = sfn.fullName( "/opt/MagAOX/stds/bamm/2024_11_21/bamm_X0241121063321000000001.binlog" );

        REQUIRE( !sfn.valid() );
        REQUIRE( errc == mx::error_t::invalidarg );
    }

    GIVEN( "Default Construction, setting fullname error in month" )
    {

        MagAOX::file::stdFileName sfn;

        mx::error_t errc = sfn.fullName( "/opt/MagAOX/stds/bamm/2024_11_21/bamm_2024X121063321000000001.binlog" );

        REQUIRE( !sfn.valid() );
        REQUIRE( errc == mx::error_t::invalidarg );
    }

    GIVEN( "Default Construction, setting fullname error in day" )
    {

        MagAOX::file::stdFileName sfn;

        mx::error_t errc = sfn.fullName( "/opt/MagAOX/stds/bamm/2024_11_21/bamm_202411X1063321000000001.binlog" );

        REQUIRE( !sfn.valid() );
        REQUIRE( errc == mx::error_t::invalidarg );
    }

    GIVEN( "Default Construction, setting fullname error in day" )
    {

        MagAOX::file::stdFileName sfn;

        mx::error_t errc = sfn.fullName( "/opt/MagAOX/stds/bamm/2024_11_21/bamm_202411X1063321000000001.binlog" );

        REQUIRE( !sfn.valid() );
        REQUIRE( errc == mx::error_t::invalidarg );
    }

    GIVEN( "Default Construction, setting fullname error in hour" )
    {

        MagAOX::file::stdFileName sfn;

        mx::error_t errc = sfn.fullName( "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20241121H63321000000001.binlog" );

        REQUIRE( !sfn.valid() );
        REQUIRE( errc == mx::error_t::invalidarg );
    }

    GIVEN( "Default Construction, setting fullname error in minute" )
    {

        MagAOX::file::stdFileName sfn;

        mx::error_t errc = sfn.fullName( "/opt/MagAOX/stds/bamm/2024_11_21/bamm_2024112106M321000000001.binlog" );

        REQUIRE( !sfn.valid() );
        REQUIRE( errc == mx::error_t::invalidarg );
    }

    GIVEN( "Default Construction, setting fullname error in second" )
    {

        MagAOX::file::stdFileName sfn;

        mx::error_t errc = sfn.fullName( "/opt/MagAOX/stds/bamm/2024_11_21/bamm_202411210633S1000000001.binlog" );

        REQUIRE( !sfn.valid() );
        REQUIRE( errc == mx::error_t::invalidarg );
    }

    GIVEN( "Default Construction, setting fullname error in nanosecond" )
    {

        MagAOX::file::stdFileName sfn;

        mx::error_t errc = sfn.fullName( "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20241121063321N00000001.binlog" );

        REQUIRE( !sfn.valid() );
        REQUIRE( errc == mx::error_t::invalidarg );
    }

    GIVEN( "Default Construction, setting fullname throws exception from stdSubDir.ymd" )
    {
        MagAOX::file::XWCTEST_STDSUBDIR_YMD_BAD_ALLOC_ns::stdFileName sfn;

        bool caught = false;

        try
        {
            sfn.fullName( "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20241121063321000000001.binlog" );
        }
        catch( const MagAOX::xwcException &e )
        {
            caught = true;
        }
        REQUIRE( caught == true );
    }

    GIVEN( "Default Construction, setting fullname causes error from stdSubDir.ymd" )
    {

        MagAOX::file::XWCTEST_STDSUBDIR_YMD_EXCEPTION_ns::stdFileName sfn;

        mx::error_t errc = sfn.fullName( "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20241121063321000000001.binlog" );

        REQUIRE( !sfn.valid() );
        REQUIRE( errc == mx::error_t::std_exception );
    }

    GIVEN( "Default Construction, setting fullname causes error from timegm" )
    {

        MagAOX::file::XWCTEST_STDFILENAME_FULLNAME_TIMEGM_ns::stdFileName sfn;

        mx::error_t errc = sfn.fullName( "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20241121063321000000001.binlog" );

        REQUIRE( !sfn.valid() );
        REQUIRE( errc == mx::error_t::eoverflow );
    }
}

/// Comparing stdFileNames
/**
 * \test
 */
SCENARIO( "Comparing stdFileNames", "[libMagAOX::file::stdFileName]" )
{
    GIVEN( "filenames to compare" )
    {
        MagAOX::file::stdFileName sfn1( "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20241121063321000000001.binlog" );
        MagAOX::file::stdFileName sfn2( "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20251121063321000000001.binlog" );
        MagAOX::file::stdFileName sfn3( "/opt/MagAOX/stds/bamm/2024_11_21/bamm_20251121063321000000001.binlog" );

        MagAOX::file::compStdFileName<MagAOX::file::stdFileName<XWC_DEFAULT_VERBOSITY>> csfn;

        REQUIRE( csfn( sfn1, sfn2 ) == true );  // less-than
        REQUIRE( csfn( sfn2, sfn1 ) == false ); // greater-than
        REQUIRE( csfn( sfn2, sfn3 ) == false ); // equal
    }
}

} // namespace stdFileNameTest
} // namespace fileTest
} // namespace libXWCTest
