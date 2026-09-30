/** \file dmPokeCenter_indi_test.cpp
 * \brief Catch2 INDI callback validation tests for the dmPokeCenter app.
 * \author Claude Code
 *
 * This translation unit includes testMacrosINDI.hpp before the app header, so callback
 * bodies return immediately after the device/name check.
 *
 * \ingroup dmPokeCenter_files
 */

#include "../../../tests/testXWC.hpp"
#include "../../../tests/testMacrosINDI.hpp"

#include <string>

#include "../dmPokeCenter.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

namespace dmPokeCenterTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness setting up dmPokeCenter INDI properties for callback validation.
class dmPokeCenter_test : public dmPokeCenter
{
  public:
    /// Construct a harness with the given device name.
    explicit dmPokeCenter_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        XWCTEST_SETUP_INDI_NEW_PROP( poke_amp );
        XWCTEST_SETUP_INDI_NEW_PROP( nPupilImages );
        XWCTEST_SETUP_INDI_NEW_PROP( nPokeImages );
        XWCTEST_SETUP_INDI_NEW_PROP( single );
        XWCTEST_SETUP_INDI_NEW_PROP( continuous );
        XWCTEST_SETUP_INDI_NEW_PROP( stop );
        XWCTEST_SETUP_INDI_ARB_PROP( m_indiP_wfsFps, camwfs, fps );
        XWCTEST_SETUP_INDI_ARB_PROP( m_indiP_shutter, camwfs, shutter );
    }

    using dmPokeCenter::newCallBack_m_indiP_continuous;
    using dmPokeCenter::newCallBack_m_indiP_nPokeImages;
    using dmPokeCenter::newCallBack_m_indiP_nPupilImages;
    using dmPokeCenter::newCallBack_m_indiP_poke_amp;
    using dmPokeCenter::newCallBack_m_indiP_single;
    using dmPokeCenter::newCallBack_m_indiP_stop;
    using dmPokeCenter::setCallBack_m_indiP_shutter;
    using dmPokeCenter::setCallBack_m_indiP_wfsFps;
};
/// \endcond

/// Verify the dmPokeCenter INDI callbacks validate device and property names.
/**
 * \ingroup dmPokeCenter_unit_test
 */
SCENARIO( "dmPokeCenter INDI callbacks validate device and property names", "[dmPokeCenter]" )
{
    // clang-format off
    #ifdef DMPOKECENTER_TEST_DOXYGEN_REF
    dmPokeCenter::newCallBack_m_indiP_poke_amp(const pcf::IndiProperty &);
    dmPokeCenter::newCallBack_m_indiP_nPupilImages(const pcf::IndiProperty &);
    dmPokeCenter::newCallBack_m_indiP_nPokeImages(const pcf::IndiProperty &);
    dmPokeCenter::newCallBack_m_indiP_single(const pcf::IndiProperty &);
    dmPokeCenter::newCallBack_m_indiP_continuous(const pcf::IndiProperty &);
    dmPokeCenter::newCallBack_m_indiP_stop(const pcf::IndiProperty &);
    dmPokeCenter::setCallBack_m_indiP_wfsFps(const pcf::IndiProperty &);
    dmPokeCenter::setCallBack_m_indiP_shutter(const pcf::IndiProperty &);
    #endif
    // clang-format on

    XWCTEST_INDI_NEW_CALLBACK( dmPokeCenter, poke_amp );
    XWCTEST_INDI_NEW_CALLBACK( dmPokeCenter, nPupilImages );
    XWCTEST_INDI_NEW_CALLBACK( dmPokeCenter, nPokeImages );
    XWCTEST_INDI_NEW_CALLBACK( dmPokeCenter, single );
    XWCTEST_INDI_NEW_CALLBACK( dmPokeCenter, continuous );
    XWCTEST_INDI_NEW_CALLBACK( dmPokeCenter, stop );
    XWCTEST_INDI_SET_CALLBACK( dmPokeCenter, m_indiP_wfsFps, camwfs, fps );
    XWCTEST_INDI_SET_CALLBACK( dmPokeCenter, m_indiP_shutter, camwfs, shutter );
}

} // namespace dmPokeCenterTest

} // namespace libXWCTest
