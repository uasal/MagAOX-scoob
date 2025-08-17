/** \file logMap_test.hpp
 * \brief Tests for the logMap class
 * \ingroup logger_files
 */

#include "../../../tests/catch2/catch.hpp"

#include <filesystem>
#include <fstream>

#include "../../file/fileTimes.hpp"

#include "../logMap.hpp"
#include "../logMap.cpp"

namespace logMap_test
{

// simple time struct to enable log structure creation
struct tmpt
{
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int nanosec;
};

// create a bunch of logs on disk to process
void createTestPaths( const std::string &basedir )
{
    std::filesystem::create_directory( basedir );

    std::vector<std::string> devs( { "dev1", "dev2", "dev3" } );

    std::vector<std::vector<tmpt>> ftimes( { /*dev1:*/ { { 2024, 11, 19, 0, 0, 0, 0 },
                                                         { 2024, 11, 19, 0, 0, 30, 0 },
                                                         { 2024, 11, 19, 2, 55, 26, 4000 },
                                                         { 2024, 11, 19, 5, 23, 0, 0 },
                                                         { 2024, 11, 21, 22, 0, 0, 0 },
                                                         { 2024, 11, 21, 23, 59, 59, 999999999 },
                                                         { 2024, 11, 23, 2, 30, 2, 2000 },
                                                         { 2024, 11, 23, 4, 45, 10, 12 } },
                                             /*dev2:*/
                                             { { 2024, 11, 19, 0, 0, 0, 0 },
                                               { 2024, 11, 19, 0, 0, 30, 0 },
                                               { 2024, 11, 19, 2, 55, 26, 4000 },
                                               { 2024, 11, 19, 5, 23, 0, 0 },
                                               { 2024, 11, 21, 22, 0, 0, 0 },
                                               { 2024, 11, 21, 23, 59, 59, 999999999 },
                                               { 2024, 11, 23, 2, 30, 2, 2000 },
                                               { 2024, 11, 23, 4, 45, 10, 12 } },
                                             /*dev3:*/
                                             { { 2024, 11, 19, 0, 0, 0, 0 },
                                               { 2024, 11, 19, 0, 0, 30, 0 },
                                               { 2024, 11, 19, 2, 55, 26, 4000 },
                                               { 2024, 11, 19, 5, 23, 0, 0 },
                                               { 2024, 11, 21, 22, 0, 0, 0 },
                                               { 2024, 11, 21, 23, 59, 59, 999999999 },
                                               { 2024, 11, 23, 2, 30, 2, 2000 },
                                               { 2024, 11, 23, 4, 45, 10, 12 } } } );

    for( size_t d = 0; d < devs.size(); ++d )
    {
        for( size_t f = 0; f < ftimes[d].size(); ++f )
        {
            tm uttime;
            uttime.tm_year = ftimes[d][f].year - 1900;
            uttime.tm_mon  = ftimes[d][f].month - 1;
            uttime.tm_mday = ftimes[d][f].day;
            uttime.tm_hour = ftimes[d][f].hour;
            uttime.tm_min  = ftimes[d][f].minute;
            uttime.tm_sec  = ftimes[d][f].second;

            time_t secs = timegm( &uttime );

            std::string fileName, relPath;

            MagAOX::file::fileTimeRelPath( fileName, relPath, devs[d], "xlog", secs, ftimes[d][f].nanosec );

            std::filesystem::create_directories( basedir + '/' + relPath );

            std::ofstream fout;
            fout.open( basedir + '/' + relPath + '/' + fileName );
            fout.close();
        }
    }
}

/** \test Scenario: Building the app-to-file map
 *
 * \anchor tests_libMagAOX_logger_logMap_apptofile
 */
SCENARIO( "Building the app-to-file map", "[libMagAOX::logger::logMap]" )
{
    createTestPaths( "/tmp/logMap_test" );

    GIVEN( "File matches middle file and one later on same day" )
    {
        MagAOX::file::stdFileName firstFile( "cam1/2024_11_19/cam1_20241119030000000000000.xrif" );
        MagAOX::file::stdFileName lastFile( "cam1/2024_11_19/cam1_20241119040000000000000.xrif" );

        MagAOX::logger::logMap lm;

        lm.loadAppToFileMap( "/tmp/logMap_test", "dev1", ".xlog", firstFile, lastFile );

        REQUIRE( lm.m_appToFileMap["dev1"].size() == 2 );
        auto it = lm.m_appToFileMap["dev1"].begin();
        REQUIRE( it->fullName() == "/tmp/logMap_test/dev1/2024_11_19/dev1_20241119025526000004000.xlog" );
        ++it;
        REQUIRE( it->fullName() == "/tmp/logMap_test/dev1/2024_11_19/dev1_20241119052300000000000.xlog" );
    }

    GIVEN( "File matches first file by delta-t and last file on same day" )
    {
        // This is inside second log, but will pick the first log to get the 60 second buffer
        MagAOX::file::stdFileName firstFile( "cam1/2024_11_19/cam1_20241119000061000000000.xrif" );
        MagAOX::file::stdFileName lastFile( "cam1/2024_11_19/cam1_20241119042200000000000.xrif" );

        MagAOX::logger::logMap lm;

        lm.loadAppToFileMap( "/tmp/logMap_test", "dev1", ".xlog", firstFile, lastFile );

        REQUIRE( lm.m_appToFileMap["dev1"].size() == 4 );
        auto it = lm.m_appToFileMap["dev1"].begin();
        REQUIRE( it->fullName() == "/tmp/logMap_test/dev1/2024_11_19/dev1_20241119000000000000000.xlog" );
        ++it;
        REQUIRE( it->fullName() == "/tmp/logMap_test/dev1/2024_11_19/dev1_20241119000030000000000.xlog" );
        ++it;
        REQUIRE( it->fullName() == "/tmp/logMap_test/dev1/2024_11_19/dev1_20241119025526000004000.xlog" );
        ++it;
        REQUIRE( it->fullName() == "/tmp/logMap_test/dev1/2024_11_19/dev1_20241119052300000000000.xlog" );
    }

    GIVEN( "File matches first file by delta-t and first file on next day" )
    {
        // This is inside second log, but will pick the first log to get the 60 second buffer
        MagAOX::file::stdFileName firstFile( "cam1/2024_11_19/cam1_20241119000061000000000.xrif" );

        // This is inside the 2nd to last log, but since next log is < 3600 seconds we have to go to next day
        MagAOX::file::stdFileName lastFile( "cam1/2024_11_19/cam1_20241119052200000000000.xrif" );

        MagAOX::logger::logMap lm;

        lm.loadAppToFileMap( "/tmp/logMap_test", "dev1", ".xlog", firstFile, lastFile );

        REQUIRE( lm.m_appToFileMap["dev1"].size() == 5 );
        auto it = lm.m_appToFileMap["dev1"].begin();
        REQUIRE( it->fullName() == "/tmp/logMap_test/dev1/2024_11_19/dev1_20241119000000000000000.xlog" );
        ++it;
        REQUIRE( it->fullName() == "/tmp/logMap_test/dev1/2024_11_19/dev1_20241119000030000000000.xlog" );
        ++it;
        REQUIRE( it->fullName() == "/tmp/logMap_test/dev1/2024_11_19/dev1_20241119025526000004000.xlog" );
        ++it;
        REQUIRE( it->fullName() == "/tmp/logMap_test/dev1/2024_11_19/dev1_20241119052300000000000.xlog" );
    }

    GIVEN( "File matches last file on previous day and first file on current day (times are the same)" )
    {
        // This will pick the first log of 11/19 to get the 60 second buffer
        MagAOX::file::stdFileName firstFile( "cam1/2024_11_19/cam1_20241121200030000000000.xrif" );

        // Same time is more than an hour before first log of 11/21
        MagAOX::file::stdFileName lastFile( "cam1/2024_11_19/cam1_20241121200030000000000.xrif" );

        MagAOX::logger::logMap lm;

        lm.loadAppToFileMap( "/tmp/logMap_test", "dev1", ".xlog", firstFile, lastFile );

        REQUIRE( lm.m_appToFileMap["dev1"].size() == 5 );
        auto it = lm.m_appToFileMap["dev1"].begin();
        REQUIRE( it->fullName() == "/tmp/logMap_test/dev1/2024_11_19/dev1_20241119000000000000000.xlog" );
        ++it;
        REQUIRE( it->fullName() == "/tmp/logMap_test/dev1/2024_11_19/dev1_20241119000030000000000.xlog");
        ++it;
        REQUIRE( it->fullName() == "/tmp/logMap_test/dev1/2024_11_19/dev1_20241119025526000004000.xlog");
        ++it;
        REQUIRE( it->fullName() == "/tmp/logMap_test/dev1/2024_11_19/dev1_20241119052300000000000.xlog");
        ++it;
        REQUIRE( it->fullName() == "/tmp/logMap_test/dev1/2024_11_21/dev1_20241121220000000000000.xlog" );


    }

    GIVEN( "Matches first and last overall files, last one is not > 1 hr" )
    {
        // this is 50 seconds into 2nd file, so will pick the first file which is > 60 secs
        MagAOX::file::stdFileName firstFile( "cam1/2024_11_19/cam1_20241119000120000000000.xrif" );

        // this is 10 minutes before end of last log
        MagAOX::file::stdFileName lastFile( "cam1/2024_11_23/cam1_20241123044500000000000.xrif" );

        MagAOX::logger::logMap lm;

        lm.loadAppToFileMap( "/tmp/logMap_test", "dev1", ".xlog", firstFile, lastFile );

        REQUIRE( lm.m_appToFileMap["dev1"].size() == 8 );
        auto it = lm.m_appToFileMap["dev1"].begin();
        REQUIRE( it->fullName() == "/tmp/logMap_test/dev1/2024_11_19/dev1_20241119000000000000000.xlog" );
        ++it;
        REQUIRE( it->fullName() == "/tmp/logMap_test/dev1/2024_11_19/dev1_20241119000030000000000.xlog" );
        ++it;
        REQUIRE( it->fullName() == "/tmp/logMap_test/dev1/2024_11_19/dev1_20241119025526000004000.xlog" );
        ++it;
        REQUIRE( it->fullName() == "/tmp/logMap_test/dev1/2024_11_19/dev1_20241119052300000000000.xlog" );
        ++it;
        REQUIRE( it->fullName() == "/tmp/logMap_test/dev1/2024_11_21/dev1_20241121220000000000000.xlog" );
        ++it;
        REQUIRE( it->fullName() == "/tmp/logMap_test/dev1/2024_11_21/dev1_20241121235959999999999.xlog" );
        ++it;
        REQUIRE( it->fullName() == "/tmp/logMap_test/dev1/2024_11_23/dev1_20241123023002000002000.xlog" );
        ++it;
        REQUIRE( it->fullName() == "/tmp/logMap_test/dev1/2024_11_23/dev1_20241123044510000000012.xlog" );
    }
}

/** \test Scenario: Building the app-to-file map with errors
 *
 * \anchor tests_libMagAOX_logger_logMap_apptofile_errors
 */
SCENARIO( "Building the app-to-file map with errors", "[libMagAOX::logger::logMap]" )
{
    createTestPaths( "/tmp/logMap_test" );

    GIVEN( "No prior log" )
    {
        MagAOX::file::stdFileName firstFile( "cam1/2024_11_19/cam1_20241118030000000000000.xrif" );
        MagAOX::file::stdFileName lastFile( "cam1/2024_11_19/cam1_20241119040000000000000.xrif" );

        MagAOX::logger::logMap lm;

        lm.loadAppToFileMap( "/tmp/logMap_test", "dev1", ".xlog", firstFile, lastFile );

        REQUIRE( lm.m_appToFileMap.size() == 0 );
    }
}

} // namespace logMap_test
