/** \file dmPokeXCorr_indi_test.cpp
 * \brief Catch2 INDI callback validation tests for the dmPokeXCorr app.
 * \author Claude Code
 *
 * This translation unit includes testMacrosINDI.hpp before the app header, so callback
 * bodies return immediately after the device/name check.
 *
 * \ingroup dmPokeXCorr_files
 */

#include "../../../tests/testXWC.hpp"
#include "../../../tests/testMacrosINDI.hpp"

#include <string>

#include "../dmPokeXCorr.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

namespace dmPokeXCorrTest
{

/// The dmPokeWFS base of dmPokeXCorr.
typedef dev::dmPokeWFS<dmPokeXCorr> pokeWFST;

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness setting up dmPokeXCorr INDI properties for callback validation.
class dmPokeXCorr_test : public dmPokeXCorr
{
  public:
    /// Construct a harness with the given device name.
    explicit dmPokeXCorr_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        XWCTEST_SETUP_INDI_NEW_PROP( poke_amp );
        XWCTEST_SETUP_INDI_NEW_PROP( nPokeImages );
        XWCTEST_SETUP_INDI_NEW_PROP( nPokeAverage );
        XWCTEST_SETUP_INDI_NEW_PROP( single );
        XWCTEST_SETUP_INDI_NEW_PROP( continuous );
        XWCTEST_SETUP_INDI_NEW_PROP( stop );
        XWCTEST_SETUP_INDI_ARB_PROP( m_indiP_wfsFps, camwfs, fps );
    }

    using pokeWFST::newCallBack_m_indiP_continuous;
    using pokeWFST::newCallBack_m_indiP_nPokeAverage;
    using pokeWFST::newCallBack_m_indiP_nPokeImages;
    using pokeWFST::newCallBack_m_indiP_poke_amp;
    using pokeWFST::newCallBack_m_indiP_single;
    using pokeWFST::newCallBack_m_indiP_stop;
    using pokeWFST::setCallBack_m_indiP_wfsFps;
};
/// \endcond

/// Verify the dmPokeXCorr INDI callbacks validate device and property names.
/**
 * \ingroup dmPokeXCorr_unit_test
 */
SCENARIO( "dmPokeXCorr INDI callbacks validate device and property names", "[dmPokeXCorr]" )
{
    // clang-format off
    #ifdef DMPOKEXCORR_TEST_DOXYGEN_REF
    dev::dmPokeWFS<dmPokeXCorr>::newCallBack_m_indiP_poke_amp(const pcf::IndiProperty &);
    dev::dmPokeWFS<dmPokeXCorr>::newCallBack_m_indiP_nPokeImages(const pcf::IndiProperty &);
    dev::dmPokeWFS<dmPokeXCorr>::newCallBack_m_indiP_nPokeAverage(const pcf::IndiProperty &);
    dev::dmPokeWFS<dmPokeXCorr>::newCallBack_m_indiP_single(const pcf::IndiProperty &);
    dev::dmPokeWFS<dmPokeXCorr>::newCallBack_m_indiP_continuous(const pcf::IndiProperty &);
    dev::dmPokeWFS<dmPokeXCorr>::newCallBack_m_indiP_stop(const pcf::IndiProperty &);
    dev::dmPokeWFS<dmPokeXCorr>::setCallBack_m_indiP_wfsFps(const pcf::IndiProperty &);
    #endif
    // clang-format on

    XWCTEST_INDI_NEW_CALLBACK( dmPokeXCorr, poke_amp );
    XWCTEST_INDI_NEW_CALLBACK( dmPokeXCorr, nPokeImages );
    XWCTEST_INDI_NEW_CALLBACK( dmPokeXCorr, nPokeAverage );
    XWCTEST_INDI_NEW_CALLBACK( dmPokeXCorr, single );
    XWCTEST_INDI_NEW_CALLBACK( dmPokeXCorr, continuous );
    XWCTEST_INDI_NEW_CALLBACK( dmPokeXCorr, stop );
    XWCTEST_INDI_SET_CALLBACK( dmPokeXCorr, m_indiP_wfsFps, camwfs, fps );
}

} // namespace dmPokeXCorrTest

} // namespace libXWCTest
