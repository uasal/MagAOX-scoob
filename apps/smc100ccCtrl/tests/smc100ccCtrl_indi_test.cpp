/** \file smc100ccCtrl_indi_test.cpp
 * \brief Catch2 INDI callback validation tests for the smc100ccCtrl app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * This translation unit includes testMacrosINDI.hpp before the app header, so each callback returns right after
 * the device/name check.  The callback bodies are tested in smc100ccCtrl_test.cpp.
 */

#include "../../../tests/testXWC.hpp"
#include "../../../tests/testMacrosINDI.hpp"

#include "../smc100ccCtrl.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

namespace smc100ccCtrlTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Test harness giving the smc100ccCtrl INDI properties their device and names.
class smc100ccCtrl_test : public smc100ccCtrl
{
  public:
    /// Construct a harness with the given device name.
    explicit smc100ccCtrl_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        XWCTEST_SETUP_INDI_NEW_PROP( position );

        XWCTEST_SETUP_INDI_NEW_PROP( preset );
        XWCTEST_SETUP_INDI_NEW_PROP( presetName );
        XWCTEST_SETUP_INDI_NEW_PROP( home );
        XWCTEST_SETUP_INDI_NEW_PROP( stop );
    }
};

/// \endcond

/// Verify the smc100ccCtrl INDI callback validators accept only the expected properties.
/**
 * \ingroup smc100ccCtrl_unit_test
 */
TEST_CASE( "smc100ccCtrl INDI callbacks validate device and property names", "[smc100ccCtrl]" )
{
    // clang-format off
    #ifdef SMC100CCCTRL_TEST_DOXYGEN_REF
    smc100ccCtrl::newCallBack_m_indiP_position( pcf::IndiProperty() );
    smc100ccCtrl::newCallBack_m_indiP_preset( pcf::IndiProperty() );
    smc100ccCtrl::newCallBack_m_indiP_presetName( pcf::IndiProperty() );
    smc100ccCtrl::newCallBack_m_indiP_home( pcf::IndiProperty() );
    smc100ccCtrl::newCallBack_m_indiP_stop( pcf::IndiProperty() );
    #endif
    // clang-format on

    XWCTEST_INDI_NEW_CALLBACK( smc100ccCtrl, position );
    XWCTEST_INDI_NEW_CALLBACK( smc100ccCtrl, preset );
    XWCTEST_INDI_NEW_CALLBACK( smc100ccCtrl, presetName );
    XWCTEST_INDI_NEW_CALLBACK( smc100ccCtrl, home );
    XWCTEST_INDI_NEW_CALLBACK( smc100ccCtrl, stop );
}

} // namespace smc100ccCtrlTest

} // namespace libXWCTest
