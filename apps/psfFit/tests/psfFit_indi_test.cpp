/** \file psfFit_indi_test.cpp
 * \brief Catch2 INDI callback validation tests for the psfFit app.
 * \author Claude Code
 *
 * This translation unit includes testMacrosINDI.hpp before the app header, so callback
 * bodies return immediately after the device/name check.
 *
 * \ingroup psfFit_files
 */

#include "../../../tests/testXWC.hpp"
#include "../../../tests/testMacrosINDI.hpp"

#include <string>

#include "../psfFit.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

namespace psfFitTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness setting up psfFit INDI properties for callback validation.
class psfFit_test : public psfFit
{
  public:
    /// Construct a harness with the given device name.
    explicit psfFit_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        XWCTEST_SETUP_INDI_NEW_PROP( reset );
        XWCTEST_SETUP_INDI_NEW_PROP( statsTime );
        XWCTEST_SETUP_INDI_NEW_PROP( deltaPixThresh );
        XWCTEST_SETUP_INDI_NEW_PROP( sigmaMaxThreshUp );
        XWCTEST_SETUP_INDI_NEW_PROP( fractionMaxThreshDown );
        XWCTEST_SETUP_INDI_NEW_PROP( sigmaPixThresh );
        XWCTEST_SETUP_INDI_NEW_PROP( dx );
        XWCTEST_SETUP_INDI_NEW_PROP( dy );
        XWCTEST_SETUP_INDI_ARB_PROP( m_indiP_fpsSource, camtip, fps );
        XWCTEST_SETUP_INDI_ARB_PROP( m_indiP_shutter, camtip, shutter );
    }

    using psfFit::newCallBack_m_indiP_deltaPixThresh;
    using psfFit::newCallBack_m_indiP_dx;
    using psfFit::newCallBack_m_indiP_dy;
    using psfFit::newCallBack_m_indiP_fractionMaxThreshDown;
    using psfFit::newCallBack_m_indiP_reset;
    using psfFit::newCallBack_m_indiP_sigmaMaxThreshUp;
    using psfFit::newCallBack_m_indiP_sigmaPixThresh;
    using psfFit::newCallBack_m_indiP_statsTime;
    using psfFit::setCallBack_m_indiP_fpsSource;
    using psfFit::setCallBack_m_indiP_shutter;
};
/// \endcond

/// Verify the psfFit INDI callbacks validate device and property names.
/**
 * \ingroup psfFit_unit_test
 */
SCENARIO( "psfFit INDI callbacks validate device and property names", "[psfFit]" )
{
    // clang-format off
    #ifdef PSFFIT_TEST_DOXYGEN_REF
    psfFit::newCallBack_m_indiP_reset(const pcf::IndiProperty &);
    psfFit::newCallBack_m_indiP_statsTime(const pcf::IndiProperty &);
    psfFit::newCallBack_m_indiP_deltaPixThresh(const pcf::IndiProperty &);
    psfFit::newCallBack_m_indiP_sigmaMaxThreshUp(const pcf::IndiProperty &);
    psfFit::newCallBack_m_indiP_fractionMaxThreshDown(const pcf::IndiProperty &);
    psfFit::newCallBack_m_indiP_sigmaPixThresh(const pcf::IndiProperty &);
    psfFit::newCallBack_m_indiP_dx(const pcf::IndiProperty &);
    psfFit::newCallBack_m_indiP_dy(const pcf::IndiProperty &);
    psfFit::setCallBack_m_indiP_fpsSource(const pcf::IndiProperty &);
    psfFit::setCallBack_m_indiP_shutter(const pcf::IndiProperty &);
    #endif
    // clang-format on

    XWCTEST_INDI_NEW_CALLBACK( psfFit, reset );
    XWCTEST_INDI_NEW_CALLBACK( psfFit, statsTime );
    XWCTEST_INDI_NEW_CALLBACK( psfFit, deltaPixThresh );
    XWCTEST_INDI_NEW_CALLBACK( psfFit, sigmaMaxThreshUp );
    XWCTEST_INDI_NEW_CALLBACK( psfFit, fractionMaxThreshDown );
    XWCTEST_INDI_NEW_CALLBACK( psfFit, sigmaPixThresh );
    XWCTEST_INDI_NEW_CALLBACK( psfFit, dx );
    XWCTEST_INDI_NEW_CALLBACK( psfFit, dy );
    XWCTEST_INDI_SET_CALLBACK( psfFit, m_indiP_fpsSource, camtip, fps );
    XWCTEST_INDI_SET_CALLBACK( psfFit, m_indiP_shutter, camtip, shutter );
}

} // namespace psfFitTest

} // namespace libXWCTest
