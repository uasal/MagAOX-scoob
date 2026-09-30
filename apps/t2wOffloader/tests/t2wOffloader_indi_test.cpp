/** \file t2wOffloader_indi_test.cpp
 * \brief Catch2 INDI callback validation tests for the t2wOffloader app.
 * \author Claude Code
 *
 * This translation unit includes testMacrosINDI.hpp before the app header, so each callback returns right after
 * the device/name check.  The callback bodies are tested in t2wOffloader_test.cpp.
 *
 * \ingroup t2wOffloader_files
 */

#include "../../../tests/testXWC.hpp"
#include "../../../tests/testMacrosINDI.hpp"

#include "../t2wOffloader.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

namespace t2wOffloaderTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Test harness giving the t2wOffloader INDI properties their device and names, and exposing the callbacks.
class t2wOffloader_test : public t2wOffloader
{
  public:
    /// Construct a harness with the given device name.
    explicit t2wOffloader_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        XWCTEST_SETUP_INDI_NEW_PROP( gain );
        XWCTEST_SETUP_INDI_NEW_PROP( leak );
        XWCTEST_SETUP_INDI_NEW_PROP( actLim );
        XWCTEST_SETUP_INDI_NEW_PROP( zero );
        XWCTEST_SETUP_INDI_NEW_PROP( numModes );
        XWCTEST_SETUP_INDI_NEW_PROP( offloadToggle );

        XWCTEST_SETUP_INDI_ARB_PROP( m_indiP_fpsSource, camwfs, fps )
        XWCTEST_SETUP_INDI_ARB_PROP( m_indiP_navgSource, navgdev, nAverage )
    }

    using t2wOffloader::newCallBack_m_indiP_actLim;
    using t2wOffloader::newCallBack_m_indiP_gain;
    using t2wOffloader::newCallBack_m_indiP_leak;
    using t2wOffloader::newCallBack_m_indiP_numModes;
    using t2wOffloader::newCallBack_m_indiP_offloadToggle;
    using t2wOffloader::newCallBack_m_indiP_zero;
    using t2wOffloader::setCallBack_m_indiP_fpsSource;
    using t2wOffloader::setCallBack_m_indiP_navgSource;
};

/// \endcond

/// Verify the t2wOffloader INDI callback validators accept only the expected properties.
/**
 * \ingroup t2wOffloader_unit_test
 */
TEST_CASE( "t2wOffloader INDI callbacks validate device and property names", "[t2wOffloader]" )
{
    // clang-format off
    #ifdef T2WOFFLOADER_TEST_DOXYGEN_REF
    t2wOffloader::newCallBack_m_indiP_gain( pcf::IndiProperty() );
    t2wOffloader::newCallBack_m_indiP_leak( pcf::IndiProperty() );
    t2wOffloader::newCallBack_m_indiP_actLim( pcf::IndiProperty() );
    t2wOffloader::newCallBack_m_indiP_zero( pcf::IndiProperty() );
    t2wOffloader::newCallBack_m_indiP_numModes( pcf::IndiProperty() );
    t2wOffloader::newCallBack_m_indiP_offloadToggle( pcf::IndiProperty() );
    t2wOffloader::setCallBack_m_indiP_fpsSource( pcf::IndiProperty() );
    t2wOffloader::setCallBack_m_indiP_navgSource( pcf::IndiProperty() );
    #endif
    // clang-format on

    XWCTEST_INDI_NEW_CALLBACK( t2wOffloader, gain );
    XWCTEST_INDI_NEW_CALLBACK( t2wOffloader, leak );
    XWCTEST_INDI_NEW_CALLBACK( t2wOffloader, actLim );
    XWCTEST_INDI_NEW_CALLBACK( t2wOffloader, zero );
    XWCTEST_INDI_NEW_CALLBACK( t2wOffloader, numModes );
    XWCTEST_INDI_NEW_CALLBACK( t2wOffloader, offloadToggle );
    XWCTEST_INDI_SET_CALLBACK( t2wOffloader, m_indiP_fpsSource, camwfs, fps )
    XWCTEST_INDI_SET_CALLBACK( t2wOffloader, m_indiP_navgSource, navgdev, nAverage )
}

} // namespace t2wOffloaderTest

} // namespace libXWCTest
