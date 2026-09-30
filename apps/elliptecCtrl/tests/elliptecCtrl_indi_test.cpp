/** \file elliptecCtrl_indi_test.cpp
 * \brief Catch2 INDI callback validation tests for the elliptecCtrl app.
 * \author Claude Code
 *
 * This translation unit includes testMacrosINDI.hpp before the app header, so each callback returns right after
 * the device/name check.  The callback bodies are tested in elliptecCtrl_test.cpp.
 */

#include "../../../tests/testXWC.hpp"
#include "../../../tests/testMacrosINDI.hpp"

#include "../elliptecCtrl.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

namespace elliptecCtrlTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Test harness giving the elliptecCtrl INDI properties their device and names.
class elliptecCtrl_test : public elliptecCtrl
{
  public:
    /// Construct a harness with the given device name.
    explicit elliptecCtrl_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_ipAbsDeg, absDeg );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_ipRelDeg, relDeg );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_ipRelMove, relMove );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_ipVelPct, velocity );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_ipOptimize, optimize );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_ipSave, save );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_ipHome, home );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_ipStop, stop );
        XWCTEST_SETUP_INDI_ARB_NEW_PROP( m_ipStageGoto, stageGoto );
    }
};

/// \endcond

/// Verify the elliptecCtrl INDI callbacks accept only their own device and property names.
/**
 * \ingroup elliptecCtrl_unit_test
 */
TEST_CASE( "elliptecCtrl INDI callbacks validate device and property names", "[elliptecCtrl]" )
{
    // clang-format off
    #ifdef ELLIPTECCTRL_TEST_DOXYGEN_REF
    elliptecCtrl::newCallBack_m_ipAbsDeg( pcf::IndiProperty() );
    elliptecCtrl::newCallBack_m_ipRelDeg( pcf::IndiProperty() );
    elliptecCtrl::newCallBack_m_ipRelMove( pcf::IndiProperty() );
    elliptecCtrl::newCallBack_m_ipVelPct( pcf::IndiProperty() );
    elliptecCtrl::newCallBack_m_ipOptimize( pcf::IndiProperty() );
    elliptecCtrl::newCallBack_m_ipSave( pcf::IndiProperty() );
    elliptecCtrl::newCallBack_m_ipHome( pcf::IndiProperty() );
    elliptecCtrl::newCallBack_m_ipStop( pcf::IndiProperty() );
    elliptecCtrl::newCallBack_m_ipStageGoto( pcf::IndiProperty() );
    #endif
    // clang-format on

    XWCTEST_INDI_ARBNEW_CALLBACK( elliptecCtrl, newCallBack_m_ipAbsDeg, absDeg );
    XWCTEST_INDI_ARBNEW_CALLBACK( elliptecCtrl, newCallBack_m_ipRelDeg, relDeg );
    XWCTEST_INDI_ARBNEW_CALLBACK( elliptecCtrl, newCallBack_m_ipRelMove, relMove );
    XWCTEST_INDI_ARBNEW_CALLBACK( elliptecCtrl, newCallBack_m_ipVelPct, velocity );
    XWCTEST_INDI_ARBNEW_CALLBACK( elliptecCtrl, newCallBack_m_ipOptimize, optimize );
    XWCTEST_INDI_ARBNEW_CALLBACK( elliptecCtrl, newCallBack_m_ipSave, save );
    XWCTEST_INDI_ARBNEW_CALLBACK( elliptecCtrl, newCallBack_m_ipHome, home );
    XWCTEST_INDI_ARBNEW_CALLBACK( elliptecCtrl, newCallBack_m_ipStop, stop );
    XWCTEST_INDI_ARBNEW_CALLBACK( elliptecCtrl, newCallBack_m_ipStageGoto, stageGoto );
}

} // namespace elliptecCtrlTest

} // namespace libXWCTest
