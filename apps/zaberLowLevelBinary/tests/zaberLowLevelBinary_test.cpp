/** \file zaberLowLevelBinary_test.cpp
 * \brief Catch2 tests for the zaberLowLevelBinary app.
 * \author Jared R. Males (jaredmales@gmail.com)
 *
 * \ingroup zaberLowLevelBinary_files
 */

extern "C"
{
#include "../zb_serial.c"
}

#include "../../../tests/catch2/catch.hpp"
#include "../../tests/testMacrosINDI.hpp"

#include "../zaberLowLevelBinary.hpp"

using namespace MagAOX::app;

namespace ZLLBTEST
{

class zaberLowLevelBinary_test : public zaberLowLevelBinary
{
  public:
    /// Construct the test harness and set up INDI callback fixtures.
    zaberLowLevelBinary_test( const std::string &device )
    {
        m_configName = device;

        XWCTEST_SETUP_INDI_NEW_PROP( tgt_pos );
        XWCTEST_SETUP_INDI_NEW_PROP( req_home );
        XWCTEST_SETUP_INDI_NEW_PROP( req_halt );
        XWCTEST_SETUP_INDI_NEW_PROP( req_ehalt );
    }
};

SCENARIO( "INDI Callbacks", "[zaberLowLevelBinary]" )
{
    XWCTEST_INDI_NEW_CALLBACK( zaberLowLevelBinary, tgt_pos );
    XWCTEST_INDI_NEW_CALLBACK( zaberLowLevelBinary, req_home );
    XWCTEST_INDI_NEW_CALLBACK( zaberLowLevelBinary, req_halt );
    XWCTEST_INDI_NEW_CALLBACK( zaberLowLevelBinary, req_ehalt );
}

} // namespace ZLLBTEST
