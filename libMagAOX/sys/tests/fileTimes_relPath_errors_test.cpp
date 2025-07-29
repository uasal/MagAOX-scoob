/** \file fileTimes_relPath_errors_test.hpp
 * \brief Tests for file relative paths with errors
 * \ingroup sys_files
 */

#include "../../../tests/catch2/catch.hpp"

// Separate file so that we can make this definition:
//  this will cause a compiler warning
#define XWC_RELPATH_BUFFER_SIZE ( 4 )
#include "../fileTimes.hpp"

#include <iostream>

namespace fileTimes_relPath_errors_test
{

SCENARIO( "Getting filename and relative path for a given time with errors", "[libMagAOX::sys::fileTimes]" )
{
    //Other errors tested in fileTimes_errors_test.cpp

    GIVEN( "relpath buffer that's too small" )
    {
        time_t        ts_sec  = 1732170780;
        unsigned long ts_nsec = 0;

        std::string fileName, relPath;

        int rv = MagAOX::sys::fileTimeRelPath( fileName, relPath, "tdevice", "txt", ts_sec, ts_nsec );

        REQUIRE( rv == -5 );
    }
}

} // namespace fileTimes_relPath_errors_test
