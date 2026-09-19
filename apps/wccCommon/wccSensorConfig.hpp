/** \file wccSensorConfig.hpp
 * \brief Load a WCC sensor's geometry and detector parameters from a config section.
 * \author Adam Schilperoort
 *
 * wccSim and wccCtrl must agree exactly on the sensor array layout, otherwise the
 * controller solves astrometry against a different geometry than the simulator
 * rendered. This header holds the one loader they both use, so the two cannot
 * drift apart. Each application still reads its own extra keys, such as the
 * output or input stream name.
 *
 * The sensor sections are read through the *unused* configuration interface,
 * because the set of sensors is a runtime quantity and so cannot be declared up
 * front with `config.add`. This is the same approach `dev::stdCamera` takes for
 * its camera mode sections.
 *
 * This is the only wccCommon header that depends on mxlib. The rest are plain
 * standard library plus Eigen, which keeps them usable from a standalone test.
 *
 * \ingroup wccCommon_files
 */

#ifndef wccSensorConfig_hpp
#define wccSensorConfig_hpp

#include <algorithm>
#include <string>

#include <mx/app/appConfigurator.hpp>

#include "wccFocalPlane.hpp"

namespace MagAOX
{
namespace wcc
{

/// Read a sensor's geometry and detector parameters from its own config section.
/** Every key is optional except that the resulting geometry must be usable: the
 * defaults in sensorConfig describe an IMX detector on axis. Keys recognized in
 * section `[<name>]`:
 *
 * | key                | meaning                                              |
 * |--------------------|------------------------------------------------------|
 * | pixel_size         | detector pixel pitch [um]                            |
 * | full_w, full_h     | full frame size [pixels]                             |
 * | field_x, field_y   | field angle of the sensor optical center [arcsec]    |
 * | rotation           | sensor rotation about its own center [deg]           |
 * | center_x, center_y | sensor optical center pixel, negative for geometric  |
 * | filter             | filter name, selecting the bandpass                  |
 * | bandwidth          | bandpass width [um]                                  |
 * | pivot_wavelength   | bandpass pivot wavelength [nm]                       |
 * | dark_current       | dark current [e-/pixel/s]                            |
 * | read_noise         | read noise [e- rms]                                  |
 * | quantum_efficiency | detector quantum efficiency [e-/photon]              |
 * | full_well          | saturation level [e-]                                |
 * | conversion_gain    | electrons per DN at 0 dB analog gain                 |
 * | gain_step_db       | analog gain register step [dB per code], IMX is 0.1  |
 * | gain_code_max      | inclusive maximum analog gain code, default 255      |
 *
 * \returns 0 on success
 * \returns -1 if the resulting geometry or bandpass is unusable, with err set
 *
 * \ingroup wccCommon
 */
template <class configT>
int loadSensorGeometry( configT &cfg /**< [in,out] the application configuration */,
                        const std::string &name /**< [in] sensor name, also the section name */,
                        sensorConfig &sc /**< [out] the sensor geometry and detector parameters */,
                        std::string &err /**< [out] description of any failure */ )
{
    auto key = [&]( const std::string &k ) { return mx::app::iniFile::makeKey( name, k ); };

    auto getD = [&]( const std::string &k, double &v ) {
        if( cfg.isSetUnused( key( k ) ) )
        {
            cfg.configUnused( v, key( k ) );
            return true;
        }
        return false;
    };

    auto getI = [&]( const std::string &k, int &v ) {
        if( cfg.isSetUnused( key( k ) ) )
        {
            cfg.configUnused( v, key( k ) );
            return true;
        }
        return false;
    };

    auto getS = [&]( const std::string &k, std::string &v ) {
        if( cfg.isSetUnused( key( k ) ) )
        {
            cfg.configUnused( v, key( k ) );
            return true;
        }
        return false;
    };

    sc = sensorConfig();
    sc.m_name = name;

    getS( "filter", sc.m_filter );

    getD( "pixel_size", sc.m_pixelSize );
    getI( "full_w", sc.m_fullW );
    getI( "full_h", sc.m_fullH );
    getD( "field_x", sc.m_fieldX );
    getD( "field_y", sc.m_fieldY );
    getD( "rotation", sc.m_rotation );
    getD( "center_x", sc.m_centerX );
    getD( "center_y", sc.m_centerY );
    getD( "dark_current", sc.m_darkCurrent );
    getD( "read_noise", sc.m_readNoise );
    getD( "quantum_efficiency", sc.m_quantumEfficiency );
    getD( "bandwidth", sc.m_bandwidth );
    getD( "pivot_wavelength", sc.m_pivotWavelength );
    getD( "full_well", sc.m_fullWellDepth );
    getD( "conversion_gain", sc.m_conversionGain );
    getD( "gain_step_db", sc.m_gainStepDb );
    getI( "gain_code_max", sc.m_gainCodeMax );

    if( sc.m_conversionGain <= 0 )
    {
        sc.m_conversionGain = 1.0;
    }

    if( sc.m_gainStepDb <= 0 )
    {
        sc.m_gainStepDb = 0.1;
    }

    if( sc.m_gainCodeMax < 0 )
    {
        sc.m_gainCodeMax = 255;
    }

    if( sc.m_pixelSize <= 0 || sc.m_fullW < 1 || sc.m_fullH < 1 )
    {
        err = "sensor section [" + name + "] has an invalid pixel_size, full_w or full_h";
        return -1;
    }

    if( sc.m_bandwidth <= 0 || sc.m_pivotWavelength <= 0 )
    {
        err = "sensor section [" + name + "] has an invalid bandwidth or pivot_wavelength";
        return -1;
    }

    return 0;
}

/// Read a startup region of interest from a sensor's config section.
/** Defaults to the sensor full frame. A camera device normally supersedes this
 * within a second of connecting, so it only matters before the camera reports.
 *
 * \returns the region of interest
 *
 * \ingroup wccCommon
 */
template <class configT>
roiSpec loadSensorROI( configT &cfg /**< [in,out] the application configuration */,
                       const std::string &name /**< [in] sensor name, also the section name */,
                       const sensorConfig &sc /**< [in] the sensor, supplying the full frame default */ )
{
    auto key = [&]( const std::string &k ) { return mx::app::iniFile::makeKey( name, k ); };

    roiSpec roi = sc.fullFrameROI();

    double rx = roi.m_centerX, ry = roi.m_centerY;
    int rw = roi.m_w, rh = roi.m_h, rbx = roi.m_binX, rby = roi.m_binY;

    if( cfg.isSetUnused( key( "roi_x" ) ) )
    {
        cfg.configUnused( rx, key( "roi_x" ) );
    }
    if( cfg.isSetUnused( key( "roi_y" ) ) )
    {
        cfg.configUnused( ry, key( "roi_y" ) );
    }
    if( cfg.isSetUnused( key( "roi_w" ) ) )
    {
        cfg.configUnused( rw, key( "roi_w" ) );
    }
    if( cfg.isSetUnused( key( "roi_h" ) ) )
    {
        cfg.configUnused( rh, key( "roi_h" ) );
    }
    if( cfg.isSetUnused( key( "roi_bin_x" ) ) )
    {
        cfg.configUnused( rbx, key( "roi_bin_x" ) );
    }
    if( cfg.isSetUnused( key( "roi_bin_y" ) ) )
    {
        cfg.configUnused( rby, key( "roi_bin_y" ) );
    }

    roi.m_centerX = rx;
    roi.m_centerY = ry;
    roi.m_w = std::max( 1, std::min( rw, sc.m_fullW ) );
    roi.m_h = std::max( 1, std::min( rh, sc.m_fullH ) );
    roi.m_binX = std::max( 1, rbx );
    roi.m_binY = std::max( 1, rby );

    return roi;
}

/// Read one string key from a sensor's config section.
/** \returns true if the key was present, in which case v is set
 *
 * \ingroup wccCommon
 */
template <class configT>
bool sensorConfigString( configT &cfg /**< [in,out] the application configuration */,
                         const std::string &name /**< [in] sensor name, also the section name */,
                         const std::string &k /**< [in] key within the section */,
                         std::string &v /**< [out] the value, untouched if absent */ )
{
    const std::string key = mx::app::iniFile::makeKey( name, k );

    if( !cfg.isSetUnused( key ) )
    {
        return false;
    }

    cfg.configUnused( v, key );

    return true;
}

} // namespace wcc
} // namespace MagAOX

#endif // wccSensorConfig_hpp
