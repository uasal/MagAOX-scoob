/** \file logFileRaw_test.hpp
 * \brief Tests for the logFileRaw class
 * \ingroup logger_files
 */

#include "../../../tests/catch2/catch.hpp"

#include <filesystem>

// Separate file so that we can make this definition:
//  this will cause a compiler warning
#define XWC_TIMESTAMP_BUFFER_SIZE ( 20 )
#include "../logFileRaw.cpp"

namespace logFileRaw_test
{

class logFileRawTest : public MagAOX::logger::logFileRaw
{
  public:
    std::string testPath;

    logFileRawTest()
    {
        m_logPath = "/tmp";

        testPath = m_logPath + '/' + m_logName;
    }

    explicit logFileRawTest( const std::string &lp )
    {
        m_logPath = lp;

        testPath = m_logPath + '/' + m_logName;
    }

    int test_createFile( flatlogs::timespecX &ts )
    {
        return createFile( ts );
    }
};


SCENARIO( "Creating a log file with error from fileTimeRelPath", "[libMagAOX::logger::logFileRaw]" )
{
    GIVEN( "A valid timestamp" )
    {
        logFileRawTest lfr;

        // safety check to make sure we don't delete all of /tmp
        if( lfr.testPath == "/tmp" )
        {
            std::cerr << "\nTESTING-ERROR: testPath is just /tmp, so logName is null.  Can't go on\n";
            std::cerr << __FILE__ << ' ' << __LINE__ << "\n\n";
            REQUIRE( false );
            return;
        }

        // First delete the directory and files in case this is a repeat call
        std::filesystem::remove_all( lfr.testPath );

        flatlogs::timespecX ts1( 1732170780, 1 );

        int rv = lfr.test_createFile( ts1 );

        REQUIRE( rv == -1 );

    }

}

} // namespace logFileRaw_test
