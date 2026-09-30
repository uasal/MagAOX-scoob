/** \file closedLoopIndi_indi_test.cpp
 * \brief Catch2 INDI callback validation tests for the closedLoopIndi app.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 *
 * This translation unit includes testMacrosINDI.hpp before the app header, so each callback returns right after
 * the device/name check.  The callback bodies are tested in closedLoopIndi_test.cpp.
 *
 * \ingroup closedLoopIndi_files
 */

#include "../../../tests/testXWC.hpp"
#include "../../../tests/testMacrosINDI.hpp"

#include "../closedLoopIndi.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

namespace closedLoopIndiTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Test harness giving the closedLoopIndi INDI properties their device and names.
class closedLoopIndi_test : public closedLoopIndi
{
  public:
    /// Construct a harness with the given device name.
    explicit closedLoopIndi_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        XWCTEST_SETUP_INDI_NEW_PROP( reference0 );
        XWCTEST_SETUP_INDI_NEW_PROP( reference1 );
        XWCTEST_SETUP_INDI_NEW_PROP( ggain );
        XWCTEST_SETUP_INDI_NEW_PROP( ctrlEnabled );
        XWCTEST_SETUP_INDI_NEW_PROP( counterReset );

        XWCTEST_SETUP_INDI_ARB_PROP( m_indiP_inputs, inputdev, measurement )
        XWCTEST_SETUP_INDI_ARB_PROP( m_indiP_ctrl0_fsm, ctrl0dev, fsm )
        XWCTEST_SETUP_INDI_ARB_PROP( m_indiP_ctrl0, ctrl0dev, prop0 )
        XWCTEST_SETUP_INDI_ARB_PROP( m_indiP_ctrl1_fsm, ctrl1dev, fsm )
        XWCTEST_SETUP_INDI_ARB_PROP( m_indiP_ctrl1, ctrl1dev, prop1 )
        XWCTEST_SETUP_INDI_ARB_PROP( m_indiP_upstream, updev, loop_state )
    }
};

/// \endcond

/// Verify the closedLoopIndi INDI callback validators accept only the expected properties.
/**
 * \ingroup closedLoopIndi_unit_test
 */
TEST_CASE( "closedLoopIndi INDI callbacks validate device and property names", "[closedLoopIndi]" )
{
    // clang-format off
    #ifdef CLOSEDLOOPINDI_TEST_DOXYGEN_REF
    closedLoopIndi::newCallBack_m_indiP_reference0( pcf::IndiProperty() );
    closedLoopIndi::newCallBack_m_indiP_reference1( pcf::IndiProperty() );
    closedLoopIndi::newCallBack_m_indiP_ggain( pcf::IndiProperty() );
    closedLoopIndi::newCallBack_m_indiP_ctrlEnabled( pcf::IndiProperty() );
    closedLoopIndi::newCallBack_m_indiP_counterReset( pcf::IndiProperty() );
    closedLoopIndi::setCallBack_m_indiP_inputs( pcf::IndiProperty() );
    closedLoopIndi::setCallBack_m_indiP_ctrl0_fsm( pcf::IndiProperty() );
    closedLoopIndi::setCallBack_m_indiP_ctrl0( pcf::IndiProperty() );
    closedLoopIndi::setCallBack_m_indiP_ctrl1_fsm( pcf::IndiProperty() );
    closedLoopIndi::setCallBack_m_indiP_ctrl1( pcf::IndiProperty() );
    closedLoopIndi::setCallBack_m_indiP_upstream( pcf::IndiProperty() );
    #endif
    // clang-format on

    XWCTEST_INDI_NEW_CALLBACK( closedLoopIndi, reference0 );
    XWCTEST_INDI_NEW_CALLBACK( closedLoopIndi, reference1 );
    XWCTEST_INDI_NEW_CALLBACK( closedLoopIndi, ggain );
    XWCTEST_INDI_NEW_CALLBACK( closedLoopIndi, ctrlEnabled );
    XWCTEST_INDI_NEW_CALLBACK( closedLoopIndi, counterReset );
    XWCTEST_INDI_SET_CALLBACK( closedLoopIndi, m_indiP_inputs, inputdev, measurement )
    XWCTEST_INDI_SET_CALLBACK( closedLoopIndi, m_indiP_ctrl0_fsm, ctrl0dev, fsm )
    XWCTEST_INDI_SET_CALLBACK( closedLoopIndi, m_indiP_ctrl0, ctrl0dev, prop0 )
    XWCTEST_INDI_SET_CALLBACK( closedLoopIndi, m_indiP_ctrl1_fsm, ctrl1dev, fsm )
    XWCTEST_INDI_SET_CALLBACK( closedLoopIndi, m_indiP_ctrl1, ctrl1dev, prop1 )
    XWCTEST_INDI_SET_CALLBACK( closedLoopIndi, m_indiP_upstream, updev, loop_state )
}

} // namespace closedLoopIndiTest

} // namespace libXWCTest
