/** \file ttmModulator_indi_test.cpp
 * \brief Catch2 INDI callback validation tests for the ttmModulator app.
 * \author Claude Code
 *
 * This translation unit includes testMacrosINDI.hpp before the app header, so each callback returns right after
 * the device/name check.  The callback bodies are tested in ttmModulator_test.cpp.
 */

#include "../../../tests/testXWC.hpp"
#include "../../../tests/testMacrosINDI.hpp"

#include "../ttmModulator.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

namespace ttmModulatorTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Test harness giving the ttmModulator INDI properties their device and names.
class ttmModulator_test : public ttmModulator
{
  public:
    /// Construct a harness with the given device name.
    explicit ttmModulator_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        XWCTEST_SETUP_INDI_NEW_PROP( modState );
        XWCTEST_SETUP_INDI_NEW_PROP( modRadius );
        XWCTEST_SETUP_INDI_NEW_PROP( modFrequency );
        XWCTEST_SETUP_INDI_NEW_PROP( offset12 );
        XWCTEST_SETUP_INDI_NEW_PROP( offset );

        XWCTEST_SETUP_INDI_ARB_PROP( m_indiP_C1outp, fxngenmodwfs, C1outp );
        XWCTEST_SETUP_INDI_ARB_PROP( m_indiP_C1freq, fxngenmodwfs, C1freq );
        XWCTEST_SETUP_INDI_ARB_PROP( m_indiP_C1volts, fxngenmodwfs, C1amp );
        XWCTEST_SETUP_INDI_ARB_PROP( m_indiP_C1ofst, fxngenmodwfs, C1ofst );
        XWCTEST_SETUP_INDI_ARB_PROP( m_indiP_C1phse, fxngenmodwfs, C1phse );

        XWCTEST_SETUP_INDI_ARB_PROP( m_indiP_C2outp, fxngenmodwfs, C2outp );
        XWCTEST_SETUP_INDI_ARB_PROP( m_indiP_C2freq, fxngenmodwfs, C2freq );
        XWCTEST_SETUP_INDI_ARB_PROP( m_indiP_C2volts, fxngenmodwfs, C2amp );
        XWCTEST_SETUP_INDI_ARB_PROP( m_indiP_C2ofst, fxngenmodwfs, C2ofst );
        XWCTEST_SETUP_INDI_ARB_PROP( m_indiP_C2phse, fxngenmodwfs, C2phse );
    }
};

/// \endcond

/// Verify the ttmModulator new-property callbacks accept only this device's properties.
/**
 * \ingroup ttmModulator_unit_test
 */
TEST_CASE( "ttmModulator new callbacks validate device and property names", "[ttmModulator]" )
{
    // clang-format off
    #ifdef TTMMODULATOR_TEST_DOXYGEN_REF
    ttmModulator::newCallBack_m_indiP_modState( pcf::IndiProperty() );
    ttmModulator::newCallBack_m_indiP_modRadius( pcf::IndiProperty() );
    ttmModulator::newCallBack_m_indiP_modFrequency( pcf::IndiProperty() );
    ttmModulator::newCallBack_m_indiP_offset12( pcf::IndiProperty() );
    ttmModulator::newCallBack_m_indiP_offset( pcf::IndiProperty() );
    #endif
    // clang-format on

    XWCTEST_INDI_NEW_CALLBACK( ttmModulator, modState );
    XWCTEST_INDI_NEW_CALLBACK( ttmModulator, modRadius );
    XWCTEST_INDI_NEW_CALLBACK( ttmModulator, modFrequency );
    XWCTEST_INDI_NEW_CALLBACK( ttmModulator, offset12 );
    XWCTEST_INDI_NEW_CALLBACK( ttmModulator, offset );
}

/// Verify the ttmModulator function-generator set callbacks accept only the fxngenmodwfs properties.
/**
 * \ingroup ttmModulator_unit_test
 */
TEST_CASE( "ttmModulator set callbacks validate device and property names", "[ttmModulator]" )
{
    // clang-format off
    #ifdef TTMMODULATOR_TEST_DOXYGEN_REF
    ttmModulator::setCallBack_m_indiP_C1outp( pcf::IndiProperty() );
    ttmModulator::setCallBack_m_indiP_C1freq( pcf::IndiProperty() );
    ttmModulator::setCallBack_m_indiP_C1volts( pcf::IndiProperty() );
    ttmModulator::setCallBack_m_indiP_C1ofst( pcf::IndiProperty() );
    ttmModulator::setCallBack_m_indiP_C1phse( pcf::IndiProperty() );
    ttmModulator::setCallBack_m_indiP_C2outp( pcf::IndiProperty() );
    ttmModulator::setCallBack_m_indiP_C2freq( pcf::IndiProperty() );
    ttmModulator::setCallBack_m_indiP_C2volts( pcf::IndiProperty() );
    ttmModulator::setCallBack_m_indiP_C2ofst( pcf::IndiProperty() );
    ttmModulator::setCallBack_m_indiP_C2phse( pcf::IndiProperty() );
    #endif
    // clang-format on

    XWCTEST_INDI_SET_CALLBACK( ttmModulator, m_indiP_C1outp, fxngenmodwfs, C1outp );
    XWCTEST_INDI_SET_CALLBACK( ttmModulator, m_indiP_C1freq, fxngenmodwfs, C1freq );
    XWCTEST_INDI_SET_CALLBACK( ttmModulator, m_indiP_C1volts, fxngenmodwfs, C1amp );
    XWCTEST_INDI_SET_CALLBACK( ttmModulator, m_indiP_C1ofst, fxngenmodwfs, C1ofst );
    XWCTEST_INDI_SET_CALLBACK( ttmModulator, m_indiP_C1phse, fxngenmodwfs, C1phse );

    XWCTEST_INDI_SET_CALLBACK( ttmModulator, m_indiP_C2outp, fxngenmodwfs, C2outp );
    XWCTEST_INDI_SET_CALLBACK( ttmModulator, m_indiP_C2freq, fxngenmodwfs, C2freq );
    XWCTEST_INDI_SET_CALLBACK( ttmModulator, m_indiP_C2volts, fxngenmodwfs, C2amp );
    XWCTEST_INDI_SET_CALLBACK( ttmModulator, m_indiP_C2ofst, fxngenmodwfs, C2ofst );
    XWCTEST_INDI_SET_CALLBACK( ttmModulator, m_indiP_C2phse, fxngenmodwfs, C2phse );
}

} // namespace ttmModulatorTest

} // namespace libXWCTest
