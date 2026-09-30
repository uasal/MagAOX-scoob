/** \file andorCtrl_test.cpp
 * \brief Catch2 tests for the andorCtrl app.
 * \author Claude Code
 *
 * \ingroup andorCtrl_files
 */

#include "../../../tests/testXWC.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

// andorCtrl derives from dev::edtCamera, which is compiled out by -DMAGAOX_NOEDT.  The EDT SDK is replaced by
// the shared stub declarations in tests/edtinc.h, with the definitions below.
#undef MAGAOX_NOEDT

#define protected public
#include "../andorCtrl.hpp"
#undef protected

using namespace MagAOX::app;

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
namespace
{

/// Fake Andor SDK2 state shared by the stub functions below.
struct andorStubState
{
    std::map<std::string, unsigned int> returns; ///< Forced return codes, by SDK function name.

    std::map<std::string, int> calls; ///< Number of calls, by SDK function name.

    std::string initDir; ///< Directory passed to Initialize.

    at_32 numCameras{ 1 }; ///< Value returned by GetAvailableCameras.

    int serialNumber{ 12345 }; ///< Value returned by GetCameraSerialNumber.

    at_32 cameraHandle{ 42 }; ///< Handle returned by GetCameraHandle.

    at_32 handleIndex{ -1 }; ///< Camera index passed to GetCameraHandle.

    at_32 currentCamera{ -1 }; ///< Handle passed to SetCurrentCamera.

    std::string headModel{ "DU897_BV" }; ///< Name returned by GetHeadModel.

    std::vector<std::array<int, 4>> shutterCalls; ///< Arguments of each SetShutter call.

    int cameraLinkMode{ -1 }; ///< Value passed to SetCameraLinkMode.

    int readMode{ -1 }; ///< Value passed to SetReadMode.

    int acquisitionMode{ -1 }; ///< Value passed to SetAcquisitionMode.

    int frameTransferMode{ -1 }; ///< Value passed to SetFrameTransferMode.

    int emGainMode{ -1 }; ///< Value passed to SetEMGainMode.

    int hsTyp{ -1 }; ///< Amplifier passed to SetHSSpeed.

    int hsIndex{ -1 }; ///< Speed index passed to SetHSSpeed.

    int vsIndex{ -1 }; ///< Speed index passed to SetVSSpeed.

    int outputAmp{ -1 }; ///< Amplifier passed to SetOutputAmplifier.

    float exposureSet{ -1 }; ///< Value passed to SetExposureTime.

    int emccdGain{ 0 }; ///< Value returned by GetEMCCDGain.

    int emccdGainSet{ -1 }; ///< Value passed to SetEMCCDGain.

    int temperatureSet{ -999 }; ///< Value passed to SetTemperature.

    unsigned int temperatureStatus{ DRV_TEMPERATURE_OFF }; ///< Status returned by GetTemperatureF.

    float temperature{ 20 }; ///< Temperature returned by GetTemperatureF.

    float expTiming{ 0 }; ///< Exposure time returned by GetAcquisitionTimings.

    float accumTiming{ 1 }; ///< Accumulation cycle time returned by GetAcquisitionTimings.

    float kinTiming{ 0 }; ///< Kinetic cycle time returned by GetAcquisitionTimings.

    float readoutTime{ 0 }; ///< Value returned by GetReadOutTime.

    int status{ DRV_IDLE }; ///< Value returned by GetStatus.

    std::array<int, 7> cropEx{ { -1, -1, -1, -1, -1, -1, -1 } }; ///< Arguments of the last SetIsolatedCropModeEx.

    int cropType{ -1 }; ///< Value passed to SetIsolatedCropModeType.

    std::array<int, 6> image{ { -1, -1, -1, -1, -1, -1 } }; ///< Arguments of the last SetImage.

    /// Get the number of calls to an SDK function.
    int count( const std::string &fn /**< [in] the SDK function name */ ) const
    {
        auto it = calls.find( fn );
        if( it == calls.end() )
        {
            return 0;
        }

        return it->second;
    }
};

/// The global fake Andor SDK2 state.
andorStubState g_andorStub;

/// Fake EDT PDV state shared by the EDT stub functions below.
struct edtStubState
{
    int readcfgReturn{ 0 }; ///< Return code of pdv_readcfg.

    bool openFail{ false }; ///< If true, pdv_open_channel returns nullptr.

    int openCalls{ 0 }; ///< Number of pdv_open_channel calls.

    int multibuf{ -1 }; ///< Value passed to pdv_multibuf.

    int startImagesCalls{ 0 }; ///< Number of pdv_start_images calls.

    int startNumBuffs{ -1 }; ///< Buffer count passed to pdv_start_images.

    int startImageCalls{ 0 }; ///< Number of pdv_start_image calls.

    unsigned int tsSec{ 0 }; ///< DMA timestamp seconds returned by pdv_wait_last_image_timed.

    unsigned int tsNsec{ 0 }; ///< DMA timestamp nanoseconds returned by pdv_wait_last_image_timed.

    std::vector<u_char> image; ///< Image returned by pdv_wait_last_image_timed.
};

/// The global fake EDT state.
edtStubState g_edtStub;

/// Reset the fake SDK states before a test.
void resetStubs()
{
    g_andorStub = andorStubState();
    g_edtStub   = edtStubState();
}

/// Record a call to an SDK function and return its forced return code, or DRV_SUCCESS.
unsigned int sdkCall( const std::string &fn /**< [in] the SDK function name */ )
{
    ++g_andorStub.calls[fn];

    auto it = g_andorStub.returns.find( fn );
    if( it == g_andorStub.returns.end() )
    {
        return DRV_SUCCESS;
    }

    return it->second;
}

} // namespace

extern "C"
{

    unsigned int AbortAcquisition()
    {
        return sdkCall( "AbortAcquisition" );
    }

    unsigned int CoolerOFF()
    {
        return sdkCall( "CoolerOFF" );
    }

    unsigned int CoolerON()
    {
        return sdkCall( "CoolerON" );
    }

    unsigned int GetAcquisitionTimings( float *exposure, float *accumulate, float *kinetic )
    {
        unsigned int rv = sdkCall( "GetAcquisitionTimings" );
        if( rv == DRV_SUCCESS )
        {
            *exposure   = g_andorStub.expTiming;
            *accumulate = g_andorStub.accumTiming;
            *kinetic    = g_andorStub.kinTiming;
        }
        return rv;
    }

    unsigned int GetAvailableCameras( at_32 *totalCameras )
    {
        unsigned int rv = sdkCall( "GetAvailableCameras" );
        if( rv == DRV_SUCCESS )
        {
            *totalCameras = g_andorStub.numCameras;
        }
        return rv;
    }

    unsigned int GetCameraHandle( at_32 cameraIndex, at_32 *cameraHandle )
    {
        g_andorStub.handleIndex = cameraIndex;
        unsigned int rv         = sdkCall( "GetCameraHandle" );
        if( rv == DRV_SUCCESS )
        {
            *cameraHandle = g_andorStub.cameraHandle;
        }
        return rv;
    }

    unsigned int GetCameraSerialNumber( int *number )
    {
        unsigned int rv = sdkCall( "GetCameraSerialNumber" );
        if( rv == DRV_SUCCESS )
        {
            *number = g_andorStub.serialNumber;
        }
        return rv;
    }

    unsigned int GetEMCCDGain( int *gain )
    {
        unsigned int rv = sdkCall( "GetEMCCDGain" );
        if( rv == DRV_SUCCESS )
        {
            *gain = g_andorStub.emccdGain;
        }
        return rv;
    }

    unsigned int GetHardwareVersion( unsigned int *PCB,
                                     unsigned int *Decode,
                                     unsigned int *dummy1,
                                     unsigned int *dummy2,
                                     unsigned int *CameraFirmwareVersion,
                                     unsigned int *CameraFirmwareBuild )
    {
        *PCB                   = 1;
        *Decode                = 2;
        *dummy1                = 0;
        *dummy2                = 0;
        *CameraFirmwareVersion = 3;
        *CameraFirmwareBuild   = 4;
        return sdkCall( "GetHardwareVersion" );
    }

    unsigned int GetHeadModel( char *name )
    {
        strncpy( name, g_andorStub.headModel.c_str(), MAX_PATH - 1 );
        name[MAX_PATH - 1] = '\0';
        return sdkCall( "GetHeadModel" );
    }

    unsigned int GetNumberADChannels( int *channels )
    {
        *channels = 1;
        return sdkCall( "GetNumberADChannels" );
    }

    unsigned int GetNumberAmp( int *amp )
    {
        *amp = 2;
        return sdkCall( "GetNumberAmp" );
    }

    unsigned int GetReadOutTime( float *ReadOutTime )
    {
        unsigned int rv = sdkCall( "GetReadOutTime" );
        if( rv == DRV_SUCCESS )
        {
            *ReadOutTime = g_andorStub.readoutTime;
        }
        return rv;
    }

    unsigned int GetSoftwareVersion( unsigned int *eprom,
                                     unsigned int *coffile,
                                     unsigned int *vxdrev,
                                     unsigned int *vxdver,
                                     unsigned int *dllrev,
                                     unsigned int *dllver )
    {
        *eprom   = 1;
        *coffile = 2;
        *vxdrev  = 3;
        *vxdver  = 4;
        *dllrev  = 5;
        *dllver  = 6;
        return sdkCall( "GetSoftwareVersion" );
    }

    unsigned int GetStatus( int *status )
    {
        unsigned int rv = sdkCall( "GetStatus" );
        if( rv == DRV_SUCCESS )
        {
            *status = g_andorStub.status;
        }
        return rv;
    }

    unsigned int GetTemperatureF( float *temperature )
    {
        ++g_andorStub.calls["GetTemperatureF"];
        *temperature = g_andorStub.temperature;
        return g_andorStub.temperatureStatus;
    }

    unsigned int Initialize( char *dir )
    {
        g_andorStub.initDir = dir;
        return sdkCall( "Initialize" );
    }

    unsigned int SetAcquisitionMode( int mode )
    {
        g_andorStub.acquisitionMode = mode;
        return sdkCall( "SetAcquisitionMode" );
    }

    unsigned int SetCameraLinkMode( int mode )
    {
        g_andorStub.cameraLinkMode = mode;
        return sdkCall( "SetCameraLinkMode" );
    }

    unsigned int SetCurrentCamera( at_32 cameraHandle )
    {
        g_andorStub.currentCamera = cameraHandle;
        return sdkCall( "SetCurrentCamera" );
    }

    unsigned int SetEMCCDGain( int gain )
    {
        g_andorStub.emccdGainSet = gain;
        return sdkCall( "SetEMCCDGain" );
    }

    unsigned int SetEMGainMode( int mode )
    {
        g_andorStub.emGainMode = mode;
        return sdkCall( "SetEMGainMode" );
    }

    unsigned int SetExposureTime( float time )
    {
        g_andorStub.exposureSet = time;
        return sdkCall( "SetExposureTime" );
    }

    unsigned int SetFrameTransferMode( int mode )
    {
        g_andorStub.frameTransferMode = mode;
        return sdkCall( "SetFrameTransferMode" );
    }

    unsigned int SetHSSpeed( int typ, int index )
    {
        g_andorStub.hsTyp   = typ;
        g_andorStub.hsIndex = index;
        return sdkCall( "SetHSSpeed" );
    }

    unsigned int SetImage( int hbin, int vbin, int hstart, int hend, int vstart, int vend )
    {
        g_andorStub.image = { { hbin, vbin, hstart, hend, vstart, vend } };
        return sdkCall( "SetImage" );
    }

    unsigned int
    SetIsolatedCropModeEx( int active, int cropheight, int cropwidth, int vbin, int hbin, int cropleft, int cropbottom )
    {
        g_andorStub.cropEx = { { active, cropheight, cropwidth, vbin, hbin, cropleft, cropbottom } };
        return sdkCall( "SetIsolatedCropModeEx" );
    }

    unsigned int SetIsolatedCropModeType( int type )
    {
        g_andorStub.cropType = type;
        return sdkCall( "SetIsolatedCropModeType" );
    }

    unsigned int SetOutputAmplifier( int typ )
    {
        g_andorStub.outputAmp = typ;
        return sdkCall( "SetOutputAmplifier" );
    }

    unsigned int SetReadMode( int mode )
    {
        g_andorStub.readMode = mode;
        return sdkCall( "SetReadMode" );
    }

    unsigned int SetShutter( int typ, int mode, int closingtime, int openingtime )
    {
        g_andorStub.shutterCalls.push_back( { { typ, mode, closingtime, openingtime } } );
        return sdkCall( "SetShutter" );
    }

    unsigned int SetTemperature( int temperature )
    {
        g_andorStub.temperatureSet = temperature;
        return sdkCall( "SetTemperature" );
    }

    unsigned int SetVSSpeed( int index )
    {
        g_andorStub.vsIndex = index;
        return sdkCall( "SetVSSpeed" );
    }

    unsigned int ShutDown()
    {
        return sdkCall( "ShutDown" );
    }

    unsigned int StartAcquisition()
    {
        return sdkCall( "StartAcquisition" );
    }

    // ---- EDT PDV stubs (declared in tests/edtinc.h) ----

    Dependent *pdv_alloc_dependent()
    {
        return reinterpret_cast<Dependent *>( malloc( sizeof( Dependent ) ) );
    }

    int pdv_readcfg( const char *configFile, Dependent *dd_p, Edtinfo *edtinfo )
    {
        static_cast<void>( configFile );
        static_cast<void>( dd_p );
        static_cast<void>( edtinfo );
        return g_edtStub.readcfgReturn;
    }

    EdtDev *edt_open_channel( const char *deviceName, int unit, int channel )
    {
        static EdtDev device;
        static_cast<void>( deviceName );
        static_cast<void>( unit );
        static_cast<void>( channel );
        return &device;
    }

    void edt_perror( char *errstr )
    {
        if( errstr != nullptr )
        {
            errstr[0] = '\0';
        }
    }

    int pdv_initcam( EdtDev     *edt_p,
                     Dependent  *dd_p,
                     int         unit,
                     Edtinfo    *edtinfo,
                     const char *configFile,
                     char       *bitdir,
                     int         pdv_debug )
    {
        static_cast<void>( edt_p );
        static_cast<void>( dd_p );
        static_cast<void>( unit );
        static_cast<void>( edtinfo );
        static_cast<void>( configFile );
        static_cast<void>( bitdir );
        static_cast<void>( pdv_debug );
        return 0;
    }

    void edt_close( EdtDev *edt_p )
    {
        static_cast<void>( edt_p );
    }

    PdvDev *pdv_open_channel( const char *deviceName, int unit, int channel )
    {
        static PdvDev device;
        static_cast<void>( deviceName );
        static_cast<void>( unit );
        static_cast<void>( channel );

        ++g_edtStub.openCalls;

        if( g_edtStub.openFail )
        {
            return nullptr;
        }

        return &device;
    }

    void pdv_close( PdvDev *pdv_p )
    {
        static_cast<void>( pdv_p );
    }

    void pdv_flush_fifo( PdvDev *pdv_p )
    {
        static_cast<void>( pdv_p );
    }

    void pdv_serial_read_enable( PdvDev *pdv_p )
    {
        static_cast<void>( pdv_p );
    }

    int pdv_get_width( PdvDev *pdv_p )
    {
        static_cast<void>( pdv_p );
        return 512;
    }

    int pdv_get_height( PdvDev *pdv_p )
    {
        static_cast<void>( pdv_p );
        return 256;
    }

    int pdv_get_depth( PdvDev *pdv_p )
    {
        static_cast<void>( pdv_p );
        return 16;
    }

    char *pdv_get_cameratype( PdvDev *pdv_p )
    {
        static char cameraType[] = "stub_andor";
        static_cast<void>( pdv_p );
        return cameraType;
    }

    void pdv_multibuf( PdvDev *pdv_p, int numBuffs )
    {
        static_cast<void>( pdv_p );
        g_edtStub.multibuf = numBuffs;
    }

    void pdv_start_images( PdvDev *pdv_p, int numBuffs )
    {
        static_cast<void>( pdv_p );
        ++g_edtStub.startImagesCalls;
        g_edtStub.startNumBuffs = numBuffs;
    }

    u_char *pdv_wait_last_image_timed( PdvDev *pdv_p, uint dmaTimeStamp[2] )
    {
        static_cast<void>( pdv_p );
        dmaTimeStamp[0] = g_edtStub.tsSec;
        dmaTimeStamp[1] = g_edtStub.tsNsec;
        return g_edtStub.image.data();
    }

    void pdv_start_image( PdvDev *pdv_p )
    {
        static_cast<void>( pdv_p );
        ++g_edtStub.startImageCalls;
    }

    int pdv_serial_read( PdvDev *pdv_p, char *buf, int size )
    {
        static_cast<void>( pdv_p );
        if( buf != nullptr && size > 0 )
        {
            buf[0] = '\0';
        }
        return 0;
    }

    int pdv_serial_command( PdvDev *pdv_p, const char *command )
    {
        static_cast<void>( pdv_p );
        static_cast<void>( command );
        return 0;
    }

    int pdv_serial_wait( PdvDev *pdv_p, int timeout, int count )
    {
        static_cast<void>( pdv_p );
        static_cast<void>( timeout );
        static_cast<void>( count );
        return 0;
    }

    int pdv_get_waitchar( PdvDev *pdv_p, u_char *waitc )
    {
        static_cast<void>( pdv_p );
        if( waitc != nullptr )
        {
            *waitc = 0;
        }
        return 0;
    }
}
/// \endcond

namespace libXWCTest
{

/** \defgroup andorCtrl_unit_test andorCtrl Unit Tests
 * \brief Unit tests for the andorCtrl application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `andorCtrl` unit tests.
/** \ingroup andorCtrl_unit_test
 */
namespace andorCtrlTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness exposing andorCtrl internals.
class andorCtrl_test : public andorCtrl
{
  public:
    /// Construct a harness with the given device name.
    explicit andorCtrl_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        // Power is on
        m_powerState       = 1;
        m_powerTargetState = 1;

        setupProp( m_indiP_emGain, "emgain" );
        setupProp( m_indiP_temp, "temp_ccd" );
        setupProp( m_indiP_exptime, "exptime" );
        setupProp( m_indiP_roi_x, "roi_region_x" );
    }

    /// Destructor, removes the EDT config file written by writeConfig().
    ~andorCtrl_test() noexcept
    {
        std::remove( configFilePath().c_str() );
    }

    /// The path of the EDT config file written by writeConfig().
    std::string configFilePath() const
    {
        return "/tmp/andor_" + m_configName + ".cfg";
    }

    /// Set the device and name of a property owned by this app.
    void setupProp( pcf::IndiProperty &prop, /**< [out] the property to set up */
                    const std::string &name  /**< [in] the INDI property name */
    )
    {
        prop.setDevice( m_configName );
        prop.setName( name );
    }

    /// Add the single camera mode that loadConfig() creates.
    void setOnlyMode()
    {
        dev::cameraConfig cc;
        cc.m_configFile           = configFilePath();
        m_cameraModes["onlymode"] = cc;
        m_modeName                = "onlymode";
    }

    /// Set up the configuration, read a config file, and load it.
    void loadTestConfig( const std::string &path /**< [in] the config file path */ )
    {
        setupConfig();
        config.readConfig( path );
        loadConfig();
    }
};
/// \endcond

/// Build a number property with a single element.
pcf::IndiProperty numberProp( const std::string &device, /**< [in] the device name */
                              const std::string &name,   /**< [in] the property name */
                              const std::string &el,     /**< [in] the element name */
                              float              val     /**< [in] the element value */
)
{
    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( device );
    ip.setName( name );
    ip.add( pcf::IndiElement( el, val ) );
    return ip;
}

/// Build a switch property with a single element.
pcf::IndiProperty switchProp( const std::string                       &device, /**< [in] the device name */
                              const std::string                       &name,   /**< [in] the property name */
                              const std::string                       &el,     /**< [in] the element name */
                              const pcf::IndiElement::SwitchStateType &state   /**< [in] the switch state */
)
{
    pcf::IndiProperty ip( pcf::IndiProperty::Switch );
    ip.setDevice( device );
    ip.setName( name );
    ip.add( pcf::IndiElement( el, state ) );
    return ip;
}

/// Read an integer `key: value` entry from an EDT config file, or -1 if not found.
int readConfigInt( const std::string &path, /**< [in] the config file path */
                   const std::string &key   /**< [in] the key, including the colon */
)
{
    std::ifstream fin( path );
    std::string   line;
    while( std::getline( fin, line ) )
    {
        std::istringstream iss( line );
        std::string        k;
        iss >> k;
        if( k == key )
        {
            int v = -1;
            iss >> v;
            return v;
        }
    }

    return -1;
}

/// Verify the free helper functions andorSDKErrorName(), readoutParams() and vshiftParams().
/**
 * \ingroup andorCtrl_unit_test
 */
TEST_CASE( "andorCtrl SDK helper functions", "[andorCtrl][helpers]" )
{
    // clang-format off
    #ifdef ANDORCTRL_TEST_DOXYGEN_REF
    andorSDKErrorName(0);
    readoutParams(a, h, "");
    vshiftParams(v, "", vs);
    #endif
    // clang-format on

    SECTION( "andorSDKErrorName" )
    {
        REQUIRE( MagAOX::app::andorSDKErrorName( DRV_SUCCESS ) == "DRV_SUCCESS" );
        REQUIRE( MagAOX::app::andorSDKErrorName( DRV_IDLE ) == "DRV_IDLE" );
        REQUIRE( MagAOX::app::andorSDKErrorName( DRV_P1INVALID ) == "DRV_P1INVALID" );
        REQUIRE( MagAOX::app::andorSDKErrorName( DRV_TEMPERATURE_STABILIZED ) == "DRV_TEMPERATURE_STABILIZED" );
        REQUIRE( MagAOX::app::andorSDKErrorName( DRV_ERROR_NOCAMERA ) == "DRV_ERROR_NOCAMERA" );
        REQUIRE( MagAOX::app::andorSDKErrorName( DRV_PROCESSING_FAILED ) == "DRV_PROCESSING_FAILED" );
        REQUIRE( MagAOX::app::andorSDKErrorName( 12345 ) == "UNKNOWN: 12345" );
    }

    SECTION( "readoutParams" )
    {
        /// Expected amplifier and horizontal shift speed for each readout speed name.
        struct expected
        {
            std::string name; ///< The readout speed name.
            int         amp;  ///< The expected amplifier.
            int         hss;  ///< The expected horizontal shift speed index.
        };

        std::vector<expected> cases = { { "ccd_00_08MHz", 1, 2 },
                                        { "ccd_01MHz", 1, 1 },
                                        { "ccd_03MHz", 1, 0 },
                                        { "emccd_01MHz", 0, 3 },
                                        { "emccd_05MHz", 0, 2 },
                                        { "emccd_10MHz", 0, 1 },
                                        { "emccd_17MHz", 0, 0 } };

        for( const auto &c : cases )
        {
            int a = -1;
            int h = -1;
            CHECK( MagAOX::app::readoutParams( a, h, c.name ) == 0 );
            CHECK( a == c.amp );
            CHECK( h == c.hss );
        }

        int a = -7;
        int h = -8;
        REQUIRE( MagAOX::app::readoutParams( a, h, "bogus" ) == -1 );
        REQUIRE( a == -7 );
        REQUIRE( h == -8 );
    }

    SECTION( "vshiftParams" )
    {
        /// Expected index and speed for each vertical shift speed name.
        struct expected
        {
            std::string name;  ///< The vertical shift speed name.
            int         index; ///< The expected speed index.
            float       speed; ///< The expected speed [us].
        };

        std::vector<expected> cases = {
            { "0_3us", 0, 0.3 }, { "0_5us", 1, 0.5 }, { "0_9us", 2, 0.9 }, { "1_7us", 3, 1.7 }, { "3_3us", 4, 3.3 } };

        for( const auto &c : cases )
        {
            int   v  = -1;
            float vs = -1;
            CHECK( MagAOX::app::vshiftParams( v, c.name, vs ) == 0 );
            CHECK( v == c.index );
            CHECK( vs == Approx( c.speed ) );
        }

        int   v  = 3;
        float vs = 1.7;
        REQUIRE( MagAOX::app::vshiftParams( v, "bogus", vs ) == -1 );
        REQUIRE( v == 0 );
        REQUIRE( vs == Approx( 0.3 ) );
    }
}

/// Verify the andorCtrl constructor defaults and base-class configuration flags.
/**
 * \ingroup andorCtrl_unit_test
 */
TEST_CASE( "andorCtrl constructor defaults", "[andorCtrl]" )
{
    // clang-format off
    #ifdef ANDORCTRL_TEST_DOXYGEN_REF
    andorCtrl::andorCtrl();
    #endif
    // clang-format on

    resetStubs();

    andorCtrl_test app( "camandor" );

    REQUIRE( app.m_powerMgtEnabled == true );
    REQUIRE( app.m_powerOnWait == 10 );
    REQUIRE( app.m_startupTemp == Approx( -45 ) );

    REQUIRE( app.m_defaultReadoutSpeed == "emccd_17MHz" );
    REQUIRE( app.m_readoutSpeedNames.size() == 7 );
    REQUIRE( app.m_readoutSpeedNameLabels.size() == 7 );
    REQUIRE( app.m_defaultVShiftSpeed == "3_3us" );
    REQUIRE( app.m_vShiftSpeedNames.size() == 5 );
    REQUIRE( app.m_vShiftSpeedNameLabels.size() == 5 );
    REQUIRE( app.m_maxEMGain == Approx( 300 ) );

    REQUIRE( app.m_default_x == Approx( 255.5 ) );
    REQUIRE( app.m_default_y == Approx( 255.5 ) );
    REQUIRE( app.m_default_w == 512 );
    REQUIRE( app.m_default_h == 512 );
    REQUIRE( app.m_nextROI.x == Approx( 255.5 ) );
    REQUIRE( app.m_nextROI.w == 512 );
    REQUIRE( app.m_nextROI.bin_x == 1 );
    REQUIRE( app.m_nextROI.bin_y == 1 );
    REQUIRE( app.m_full_x == Approx( 255.5 ) );
    REQUIRE( app.m_full_w == 512 );
    REQUIRE( app.m_full_h == 512 );

    REQUIRE( app.m_libInit == false );
    REQUIRE( app.m_poweredOn == false );

    REQUIRE( andorCtrl::c_stdCamera_tempControl == true );
    REQUIRE( andorCtrl::c_stdCamera_readoutSpeed == true );
    REQUIRE( andorCtrl::c_stdCamera_vShiftSpeed == true );
    REQUIRE( andorCtrl::c_stdCamera_emGain == true );
    REQUIRE( andorCtrl::c_stdCamera_exptimeCtrl == true );
    REQUIRE( andorCtrl::c_stdCamera_fpsCtrl == false );
    REQUIRE( andorCtrl::c_stdCamera_fps == true );
    REQUIRE( andorCtrl::c_stdCamera_usesROI == true );
    REQUIRE( andorCtrl::c_stdCamera_cropMode == true );
    REQUIRE( andorCtrl::c_stdCamera_hasShutter == true );
    REQUIRE( andorCtrl::c_stdCamera_usesModes == false );
    REQUIRE( andorCtrl::c_edtCamera_relativeConfigPath == false );
    REQUIRE( andorCtrl::c_frameGrabber_flippable == false );
}

/// Verify andorCtrl configuration defaults, including the generated EDT camera mode and config file.
/**
 * \ingroup andorCtrl_unit_test
 */
TEST_CASE( "andorCtrl configuration defaults", "[andorCtrl][config]" )
{
    // clang-format off
    #ifdef ANDORCTRL_TEST_DOXYGEN_REF
    andorCtrl::setupConfig();
    andorCtrl::loadConfig();
    #endif
    // clang-format on

    resetStubs();

    andorCtrl_test app( "camandortest" );

    const std::string path = "/tmp/andorCtrl_test_defaults.conf";
    mx::app::writeConfigFile( path, { "none" }, { "nada" }, { "0" } );
    app.loadTestConfig( path );
    std::remove( path.c_str() );

    REQUIRE( app.m_shutdown == 0 );

    // single EDT mode pointing at the generated config file
    REQUIRE( app.m_configFile == app.configFilePath() );
    REQUIRE( app.m_startupMode == "onlymode" );
    REQUIRE( app.m_cameraModes.count( "onlymode" ) == 1 );
    REQUIRE( app.m_cameraModes["onlymode"].m_configFile == app.configFilePath() );
    REQUIRE( app.m_cameraModes["onlymode"].m_sizeX == 512 );
    REQUIRE( app.m_cameraModes["onlymode"].m_sizeY == 512 );
    REQUIRE( readConfigInt( app.configFilePath(), "width:" ) == 512 );
    REQUIRE( readConfigInt( app.configFilePath(), "height:" ) == 512 );

    // stdCamera
    REQUIRE( app.m_startupTemp == Approx( -45 ) );
    REQUIRE( app.m_defaultReadoutSpeed == "emccd_17MHz" );
    REQUIRE( app.m_defaultVShiftSpeed == "3_3us" );
    REQUIRE( app.m_maxEMGain == Approx( 300 ) );
    REQUIRE( app.m_currentROI.x == Approx( 255.5 ) );
    REQUIRE( app.m_currentROI.w == 512 );

    // edtCamera and ioDevice
    REQUIRE( app.m_unit == 0 );
    REQUIRE( app.m_channel == 0 );
    REQUIRE( app.m_numBuffs == 4 );
    REQUIRE( app.m_readTimeout == 1000 );
    REQUIRE( app.m_writeTimeout == 1000 );

    // frameGrabber
    REQUIRE( app.m_shmimName == "camandortest" );
}

/// Verify andorCtrl configuration overrides, the EM gain limits, and the config-file write failure.
/**
 * \ingroup andorCtrl_unit_test
 */
TEST_CASE( "andorCtrl configuration overrides", "[andorCtrl][config]" )
{
    // clang-format off
    #ifdef ANDORCTRL_TEST_DOXYGEN_REF
    andorCtrl::setupConfig();
    andorCtrl::loadConfig();
    #endif
    // clang-format on

    resetStubs();

    const std::string path = "/tmp/andorCtrl_test_overrides.conf";

    SECTION( "overrides" )
    {
        andorCtrl_test app( "camandortest" );

        mx::app::writeConfigFile( path,
                                  { "camera",
                                    "camera",
                                    "camera",
                                    "camera",
                                    "framegrabber",
                                    "framegrabber",
                                    "framegrabber",
                                    "framegrabber",
                                    "device" },
                                  { "maxEMGain",
                                    "defaultReadoutSpeed",
                                    "defaultVShiftSpeed",
                                    "startupTemp",
                                    "pdv_unit",
                                    "pdv_channel",
                                    "numBuffs",
                                    "shmimName",
                                    "readTimeout" },
                                  { "150", "ccd_03MHz", "0_9us", "-60", "1", "2", "8", "andortest", "2000" } );
        app.loadTestConfig( path );
        std::remove( path.c_str() );

        REQUIRE( app.m_shutdown == 0 );
        REQUIRE( app.m_maxEMGain == Approx( 150 ) );
        REQUIRE( app.m_defaultReadoutSpeed == "ccd_03MHz" );
        REQUIRE( app.m_defaultVShiftSpeed == "0_9us" );
        REQUIRE( app.m_startupTemp == Approx( -60 ) );
        REQUIRE( app.m_unit == 1 );
        REQUIRE( app.m_channel == 2 );
        REQUIRE( app.m_numBuffs == 8 );
        REQUIRE( app.m_shmimName == "andortest" );
        REQUIRE( app.m_readTimeout == 2000 );
    }

    SECTION( "maxEMGain above 300 is limited" )
    {
        andorCtrl_test app( "camandortest" );

        mx::app::writeConfigFile( path, { "camera" }, { "maxEMGain" }, { "500" } );
        app.loadTestConfig( path );
        std::remove( path.c_str() );

        REQUIRE( app.m_maxEMGain == Approx( 300 ) );
    }

    SECTION( "maxEMGain below 1 is limited" )
    {
        andorCtrl_test app( "camandortest" );

        mx::app::writeConfigFile( path, { "camera" }, { "maxEMGain" }, { "0" } );
        app.loadTestConfig( path );
        std::remove( path.c_str() );

        REQUIRE( app.m_maxEMGain == Approx( 1 ) );
    }

    SECTION( "config file can not be written" )
    {
        andorCtrl_test app( "andorCtrl_test_no_such_dir/cam" );

        mx::app::writeConfigFile( path, { "none" }, { "nada" }, { "0" } );
        app.loadTestConfig( path );
        std::remove( path.c_str() );

        REQUIRE( app.m_shutdown != 0 );
    }
}

/// Verify writeConfig() writes the binned ROI size to the EDT config file.
/**
 * \ingroup andorCtrl_unit_test
 */
TEST_CASE( "andorCtrl writeConfig", "[andorCtrl][config]" )
{
    // clang-format off
    #ifdef ANDORCTRL_TEST_DOXYGEN_REF
    andorCtrl::writeConfig();
    #endif
    // clang-format on

    resetStubs();

    andorCtrl_test app( "camandortest" );

    app.m_nextROI.w     = 512;
    app.m_nextROI.h     = 256;
    app.m_nextROI.bin_x = 2;
    app.m_nextROI.bin_y = 4;

    REQUIRE( app.writeConfig() == 0 );

    REQUIRE( readConfigInt( app.configFilePath(), "width:" ) == 256 );
    REQUIRE( readConfigInt( app.configFilePath(), "height:" ) == 64 );
    REQUIRE( readConfigInt( app.configFilePath(), "depth:" ) == 16 );
}

/// Verify powerOnDefaults() resets temperature control status and the ROIs.
/**
 * \ingroup andorCtrl_unit_test
 */
TEST_CASE( "andorCtrl powerOnDefaults", "[andorCtrl]" )
{
    // clang-format off
    #ifdef ANDORCTRL_TEST_DOXYGEN_REF
    andorCtrl::powerOnDefaults();
    #endif
    // clang-format on

    resetStubs();

    andorCtrl_test app( "camandor" );

    app.m_tempControlStatus    = true;
    app.m_tempControlStatusSet = true;
    app.m_tempControlOnTarget  = true;
    app.m_tempControlStatusStr = "STABILIZED";
    app.m_currentROI.x         = 10;
    app.m_nextROI.w            = 16;
    app.m_nextROI.bin_x        = 4;

    REQUIRE( app.powerOnDefaults() == 0 );

    REQUIRE( app.m_tempControlStatus == false );
    REQUIRE( app.m_tempControlStatusSet == false );
    REQUIRE( app.m_tempControlOnTarget == false );
    REQUIRE( app.m_tempControlStatusStr == "OFF" );

    REQUIRE( app.m_currentROI.x == Approx( 255.5 ) );
    REQUIRE( app.m_currentROI.w == 512 );
    REQUIRE( app.m_currentROI.bin_x == 1 );
    REQUIRE( app.m_nextROI.x == Approx( 255.5 ) );
    REQUIRE( app.m_nextROI.w == 512 );
    REQUIRE( app.m_nextROI.bin_x == 1 );
}

/// Verify cameraSelect() initializes the SDK and configures the camera, and handles missing cameras and errors.
/**
 * \ingroup andorCtrl_unit_test
 */
TEST_CASE( "andorCtrl cameraSelect", "[andorCtrl][connect]" )
{
    // clang-format off
    #ifdef ANDORCTRL_TEST_DOXYGEN_REF
    andorCtrl::cameraSelect();
    #endif
    // clang-format on

    resetStubs();

    SECTION( "success" )
    {
        andorCtrl_test app( "camandor" );

        REQUIRE( app.cameraSelect() == 0 );

        REQUIRE( g_andorStub.initDir == "/usr/local/etc/andor/" );
        REQUIRE( app.m_libInit == true );
        REQUIRE( app.state() == stateCodes::CONNECTED );

        REQUIRE( g_andorStub.handleIndex == 0 );
        REQUIRE( g_andorStub.currentCamera == 42 );

        // shutter starts shut
        REQUIRE( g_andorStub.shutterCalls.size() == 1 );
        REQUIRE( g_andorStub.shutterCalls[0][0] == 1 );
        REQUIRE( g_andorStub.shutterCalls[0][1] == 2 );
        REQUIRE( g_andorStub.shutterCalls[0][2] == 500 );
        REQUIRE( g_andorStub.shutterCalls[0][3] == 500 );
        REQUIRE( app.m_shutterState == 0 );

        REQUIRE( g_andorStub.cameraLinkMode == 1 );
        REQUIRE( g_andorStub.readMode == 4 );
        REQUIRE( g_andorStub.acquisitionMode == 5 );
        REQUIRE( g_andorStub.frameTransferMode == 1 );
        REQUIRE( g_andorStub.emGainMode == 3 );

        // default readout emccd_17MHz and vertical shift 3_3us
        REQUIRE( app.m_readoutSpeedName == "emccd_17MHz" );
        REQUIRE( app.m_readoutSpeedNameSet == "emccd_17MHz" );
        REQUIRE( g_andorStub.hsTyp == 0 );
        REQUIRE( g_andorStub.hsIndex == 0 );
        REQUIRE( g_andorStub.outputAmp == 0 );
        REQUIRE( app.m_vShiftSpeedName == "3_3us" );
        REQUIRE( g_andorStub.vsIndex == 4 );
        REQUIRE( app.m_vshiftSpeed == Approx( 3.3 ) );

        REQUIRE( g_andorStub.exposureSet == Approx( 0.1 ) );

        // no setpoint, so the cooler is left alone
        REQUIRE( g_andorStub.count( "CoolerON" ) == 0 );
    }

    SECTION( "open shutter is kept open and cooling starts with a setpoint" )
    {
        andorCtrl_test app( "camandor" );
        app.m_shutterState = 1;
        app.m_ccdTempSetpt = -40;

        REQUIRE( app.cameraSelect() == 0 );

        REQUIRE( g_andorStub.shutterCalls.size() == 1 );
        REQUIRE( g_andorStub.shutterCalls[0][1] == 1 );
        REQUIRE( app.m_shutterState == 1 );

        REQUIRE( g_andorStub.count( "CoolerON" ) == 1 );
        REQUIRE( app.m_tempControlStatus == true );
        REQUIRE( app.m_tempControlStatusStr == "COOLING" );
    }

    SECTION( "configured default readout and vertical shift speeds" )
    {
        andorCtrl_test app( "camandor" );
        app.m_defaultReadoutSpeed = "ccd_01MHz";
        app.m_defaultVShiftSpeed  = "0_5us";

        REQUIRE( app.cameraSelect() == 0 );

        REQUIRE( g_andorStub.hsTyp == 1 );
        REQUIRE( g_andorStub.hsIndex == 1 );
        REQUIRE( g_andorStub.outputAmp == 1 );
        REQUIRE( g_andorStub.vsIndex == 1 );
        REQUIRE( app.m_vshiftSpeed == Approx( 0.5 ) );
    }

    SECTION( "library already initialized" )
    {
        andorCtrl_test app( "camandor" );
        app.m_libInit = true;

        REQUIRE( app.cameraSelect() == 0 );
        REQUIRE( g_andorStub.count( "Initialize" ) == 0 );
    }

    SECTION( "no camera at initialization" )
    {
        std::vector<unsigned int> codes = { DRV_USBERROR, DRV_ERROR_NOCAMERA, DRV_VXDNOTINSTALLED };

        for( auto code : codes )
        {
            resetStubs();
            g_andorStub.returns["Initialize"] = code;

            andorCtrl_test app( "camandor" );

            CHECK( app.cameraSelect() == 0 );
            CHECK( app.state() == stateCodes::NODEVICE );
            CHECK( app.m_libInit == false );
            CHECK( g_andorStub.count( "ShutDown" ) == 1 );
            CHECK( g_andorStub.count( "GetAvailableCameras" ) == 0 );
        }
    }

    SECTION( "initialization error" )
    {
        g_andorStub.returns["Initialize"] = DRV_ERROR_ACK;

        andorCtrl_test app( "camandor" );

        REQUIRE( app.cameraSelect() == -1 );
        REQUIRE( app.m_libInit == false );
        REQUIRE( g_andorStub.count( "ShutDown" ) == 1 );
    }

    SECTION( "no cameras available" )
    {
        g_andorStub.numCameras = 0;

        andorCtrl_test app( "camandor" );

        REQUIRE( app.cameraSelect() == 0 );
        REQUIRE( app.state() == stateCodes::NODEVICE );
        REQUIRE( app.m_libInit == true );
        REQUIRE( g_andorStub.count( "GetCameraHandle" ) == 0 );
    }

    SECTION( "invalid default readout speed" )
    {
        andorCtrl_test app( "camandor" );
        app.m_defaultReadoutSpeed = "bogus";

        REQUIRE( app.cameraSelect() == -1 );
        REQUIRE( g_andorStub.count( "SetHSSpeed" ) == 0 );
    }

    SECTION( "invalid default vertical shift speed" )
    {
        andorCtrl_test app( "camandor" );
        app.m_defaultVShiftSpeed = "bogus";

        REQUIRE( app.cameraSelect() == -1 );
        REQUIRE( g_andorStub.count( "SetVSSpeed" ) == 0 );
    }

    SECTION( "SDK errors" )
    {
        std::vector<std::string> fns = { "GetAvailableCameras",
                                         "GetCameraSerialNumber",
                                         "GetCameraHandle",
                                         "SetCurrentCamera",
                                         "GetHeadModel",
                                         "GetSoftwareVersion",
                                         "GetHardwareVersion",
                                         "SetShutter",
                                         "SetCameraLinkMode",
                                         "SetReadMode",
                                         "SetAcquisitionMode",
                                         "SetFrameTransferMode",
                                         "SetEMGainMode",
                                         "SetHSSpeed",
                                         "SetVSSpeed",
                                         "SetOutputAmplifier",
                                         "SetExposureTime" };

        for( const auto &fn : fns )
        {
            resetStubs();
            g_andorStub.returns[fn] = DRV_P1INVALID;

            andorCtrl_test app( "camandor" );

            INFO( fn );
            CHECK( app.cameraSelect() == -1 );
        }
    }
}

/// Verify getTemp() maps each GetTemperatureF status and records the temperature.
/**
 * \ingroup andorCtrl_unit_test
 */
TEST_CASE( "andorCtrl getTemp", "[andorCtrl][temp]" )
{
    // clang-format off
    #ifdef ANDORCTRL_TEST_DOXYGEN_REF
    andorCtrl::getTemp();
    #endif
    // clang-format on

    resetStubs();

    andorCtrl_test app( "camandor" );

    SECTION( "status codes" )
    {
        /// Expected results for each temperature status.
        struct expected
        {
            unsigned int code;     ///< The status returned by GetTemperatureF.
            std::string  str;      ///< Expected m_tempControlStatusStr.
            bool         status;   ///< Expected m_tempControlStatus.
            bool         onTarget; ///< Expected m_tempControlOnTarget.
        };

        std::vector<expected> cases = { { DRV_TEMPERATURE_OFF, "OFF", false, false },
                                        { DRV_TEMPERATURE_STABILIZED, "STABILIZED", true, true },
                                        { DRV_TEMPERATURE_NOT_REACHED, "COOLING", true, false },
                                        { DRV_TEMPERATURE_NOT_STABILIZED, "NOT STABILIZED", true, false },
                                        { DRV_TEMPERATURE_DRIFT, "DRIFTING", true, false } };

        float t = -30;
        for( const auto &c : cases )
        {
            g_andorStub.temperatureStatus = c.code;
            g_andorStub.temperature       = t;

            app.m_tempControlStatus   = !c.status;
            app.m_tempControlOnTarget = !c.onTarget;

            REQUIRE( app.getTemp() == 0 );

            CHECK( app.m_tempControlStatusStr == c.str );
            CHECK( app.m_tempControlStatus == c.status );
            CHECK( app.m_tempControlOnTarget == c.onTarget );
            CHECK( app.m_ccdTemp == Approx( t ) );

            t += 1;
        }
    }

    SECTION( "error status" )
    {
        g_andorStub.temperatureStatus = DRV_NOT_INITIALIZED;
        g_andorStub.temperature       = -30;
        app.m_tempControlStatus       = true;
        app.m_tempControlOnTarget     = true;
        app.m_ccdTemp                 = -30;

        REQUIRE( app.getTemp() == -1 );

        REQUIRE( app.m_tempControlStatus == false );
        REQUIRE( app.m_tempControlOnTarget == false );
        REQUIRE( app.m_ccdTemp == Approx( -999 ) );
    }
}

/// Verify setTempControl() and setTempSetPt() command the cooler and setpoint, and report SDK errors.
/**
 * \ingroup andorCtrl_unit_test
 */
TEST_CASE( "andorCtrl temperature control", "[andorCtrl][temp]" )
{
    // clang-format off
    #ifdef ANDORCTRL_TEST_DOXYGEN_REF
    andorCtrl::setTempControl();
    andorCtrl::setTempSetPt();
    #endif
    // clang-format on

    resetStubs();

    andorCtrl_test app( "camandor" );

    SECTION( "cooler on" )
    {
        app.m_tempControlStatusSet = true;
        REQUIRE( app.setTempControl() == 0 );
        REQUIRE( g_andorStub.count( "CoolerON" ) == 1 );
        REQUIRE( app.m_tempControlStatus == true );
        REQUIRE( app.m_tempControlStatusStr == "COOLING" );
    }

    SECTION( "cooler off" )
    {
        app.m_tempControlStatus    = true;
        app.m_tempControlStatusSet = false;
        REQUIRE( app.setTempControl() == 0 );
        REQUIRE( g_andorStub.count( "CoolerOFF" ) == 1 );
        REQUIRE( app.m_tempControlStatus == false );
        REQUIRE( app.m_tempControlStatusStr == "OFF" );
    }

    SECTION( "cooler errors" )
    {
        g_andorStub.returns["CoolerON"]  = DRV_NOT_INITIALIZED;
        g_andorStub.returns["CoolerOFF"] = DRV_NOT_INITIALIZED;

        app.m_tempControlStatusSet = true;
        REQUIRE( app.setTempControl() == -1 );
        REQUIRE( app.m_tempControlStatus == false );

        app.m_tempControlStatus    = true;
        app.m_tempControlStatusSet = false;
        REQUIRE( app.setTempControl() == -1 );
        REQUIRE( app.m_tempControlStatus == true );
    }

    SECTION( "setpoint is rounded to an integer" )
    {
        app.m_ccdTempSetpt = 10.4;
        REQUIRE( app.setTempSetPt() == 0 );
        REQUIRE( g_andorStub.temperatureSet == 10 );

        app.m_ccdTempSetpt = 10.6;
        REQUIRE( app.setTempSetPt() == 0 );
        REQUIRE( g_andorStub.temperatureSet == 11 );
    }

    SECTION( "setpoint error" )
    {
        g_andorStub.returns["SetTemperature"] = DRV_TEMPERATURE_OUT_RANGE;
        app.m_ccdTempSetpt                    = 10;
        REQUIRE( app.setTempSetPt() == -1 );
    }
}

/// Verify getEMGain() and setEMGain(), including limits and the conventional-amplifier guard.
/**
 * \ingroup andorCtrl_unit_test
 */
TEST_CASE( "andorCtrl EM gain", "[andorCtrl][emgain]" )
{
    // clang-format off
    #ifdef ANDORCTRL_TEST_DOXYGEN_REF
    andorCtrl::getEMGain();
    andorCtrl::setEMGain();
    #endif
    // clang-format on

    resetStubs();

    andorCtrl_test app( "camandor" );
    app.m_readoutSpeedName = "emccd_17MHz";

    SECTION( "getEMGain" )
    {
        g_andorStub.emccdGain = 150;
        REQUIRE( app.getEMGain() == 0 );
        REQUIRE( app.m_emGain == Approx( 150 ) );

        // a gain of 0 (EM off) is reported as 1
        g_andorStub.emccdGain = 0;
        REQUIRE( app.getEMGain() == 0 );
        REQUIRE( app.m_emGain == Approx( 1 ) );
    }

    SECTION( "getEMGain error" )
    {
        g_andorStub.returns["GetEMCCDGain"] = DRV_NOT_INITIALIZED;
        app.m_emGain                        = 77;
        REQUIRE( app.getEMGain() == -1 );
        REQUIRE( app.m_emGain == Approx( 77 ) );
    }

    SECTION( "setEMGain values and limits" )
    {
        app.m_emGainSet = 100;
        REQUIRE( app.setEMGain() == 0 );
        REQUIRE( g_andorStub.emccdGainSet == 100 );

        // a gain of 1 turns EM gain off
        app.m_emGainSet = 1;
        REQUIRE( app.setEMGain() == 0 );
        REQUIRE( g_andorStub.emccdGainSet == 0 );

        app.m_emGainSet = -5;
        REQUIRE( app.setEMGain() == 0 );
        REQUIRE( g_andorStub.emccdGainSet == 0 );

        app.m_emGainSet = 500;
        REQUIRE( app.setEMGain() == 0 );
        REQUIRE( g_andorStub.emccdGainSet == 300 );

        app.m_maxEMGain = 200;
        app.m_emGainSet = 250;
        REQUIRE( app.setEMGain() == 0 );
        REQUIRE( g_andorStub.emccdGainSet == 200 );
    }

    SECTION( "setEMGain in the conventional amplifier does nothing" )
    {
        app.m_readoutSpeedName = "ccd_01MHz";
        app.m_emGainSet        = 100;
        REQUIRE( app.setEMGain() == 0 );
        REQUIRE( g_andorStub.count( "SetEMCCDGain" ) == 0 );
    }

    SECTION( "setEMGain error" )
    {
        g_andorStub.returns["SetEMCCDGain"] = DRV_P1INVALID;
        app.m_emGainSet                     = 100;
        REQUIRE( app.setEMGain() == -1 );
    }
}

/// Verify setReadoutSpeed() and setVShiftSpeed() command the SDK and flag a reconfiguration.
/**
 * \ingroup andorCtrl_unit_test
 */
TEST_CASE( "andorCtrl readout and vertical shift speed", "[andorCtrl][readout]" )
{
    // clang-format off
    #ifdef ANDORCTRL_TEST_DOXYGEN_REF
    andorCtrl::setReadoutSpeed();
    andorCtrl::setVShiftSpeed();
    #endif
    // clang-format on

    resetStubs();

    andorCtrl_test app( "camandor" );
    app.m_modeName         = "onlymode";
    app.m_readoutSpeedName = "emccd_17MHz";
    app.m_vShiftSpeedName  = "3_3us";
    app.m_reconfig         = false;

    SECTION( "set conventional readout" )
    {
        app.m_readoutSpeedNameSet = "ccd_01MHz";

        REQUIRE( app.setReadoutSpeed() == 0 );

        REQUIRE( g_andorStub.count( "AbortAcquisition" ) == 1 );
        REQUIRE( app.state() == stateCodes::CONFIGURING );
        REQUIRE( g_andorStub.hsTyp == 1 );
        REQUIRE( g_andorStub.hsIndex == 1 );
        REQUIRE( g_andorStub.outputAmp == 1 );
        REQUIRE( app.m_readoutSpeedName == "ccd_01MHz" );
        REQUIRE( app.m_nextMode == "onlymode" );
        REQUIRE( app.m_reconfig == true );
    }

    SECTION( "conventional readout disables crop mode" )
    {
        app.m_cropMode            = true;
        app.m_cropModeSet         = true;
        app.m_readoutSpeedNameSet = "ccd_03MHz";

        REQUIRE( app.setReadoutSpeed() == 0 );
        REQUIRE( app.m_cropModeSet == false );
    }

    SECTION( "EM readout keeps crop mode" )
    {
        app.m_cropMode            = true;
        app.m_cropModeSet         = true;
        app.m_readoutSpeedNameSet = "emccd_05MHz";

        REQUIRE( app.setReadoutSpeed() == 0 );
        REQUIRE( app.m_cropModeSet == true );
        REQUIRE( g_andorStub.hsTyp == 0 );
        REQUIRE( g_andorStub.hsIndex == 2 );
    }

    SECTION( "invalid readout speed" )
    {
        app.m_readoutSpeedNameSet = "bogus";

        REQUIRE( app.setReadoutSpeed() == -1 );
        REQUIRE( app.m_readoutSpeedName == "emccd_17MHz" );
        REQUIRE( g_andorStub.count( "SetHSSpeed" ) == 0 );
        REQUIRE( app.m_reconfig == false );
    }

    SECTION( "readout SDK errors" )
    {
        app.m_readoutSpeedNameSet = "ccd_01MHz";

        g_andorStub.returns["SetHSSpeed"] = DRV_P1INVALID;
        REQUIRE( app.setReadoutSpeed() == -1 );

        g_andorStub.returns.clear();
        g_andorStub.returns["SetOutputAmplifier"] = DRV_P1INVALID;
        REQUIRE( app.setReadoutSpeed() == -1 );

        REQUIRE( app.m_readoutSpeedName == "emccd_17MHz" );
        REQUIRE( app.m_reconfig == false );
    }

    SECTION( "set vertical shift speed" )
    {
        app.m_vShiftSpeedNameSet = "0_9us";

        REQUIRE( app.setVShiftSpeed() == 0 );

        REQUIRE( g_andorStub.count( "AbortAcquisition" ) == 1 );
        REQUIRE( g_andorStub.vsIndex == 2 );
        REQUIRE( app.m_vShiftSpeedName == "0_9us" );
        REQUIRE( app.m_vshiftSpeed == Approx( 0.9 ) );
        REQUIRE( app.m_nextMode == "onlymode" );
        REQUIRE( app.m_reconfig == true );
    }

    SECTION( "invalid vertical shift speed" )
    {
        app.m_vShiftSpeedNameSet = "bogus";

        REQUIRE( app.setVShiftSpeed() == -1 );
        REQUIRE( app.m_vShiftSpeedName == "3_3us" );
        REQUIRE( g_andorStub.count( "SetVSSpeed" ) == 0 );
    }

    SECTION( "vertical shift SDK error" )
    {
        app.m_vShiftSpeedNameSet          = "0_9us";
        g_andorStub.returns["SetVSSpeed"] = DRV_P1INVALID;

        REQUIRE( app.setVShiftSpeed() == -1 );
        REQUIRE( app.m_vShiftSpeedName == "3_3us" );
        REQUIRE( app.m_reconfig == false );
    }
}

/// Verify getFPS(), setExpTime(), setNextROI(), checkNextROI(), setShutter() and setCropMode().
/**
 * \ingroup andorCtrl_unit_test
 */
TEST_CASE( "andorCtrl exposure, ROI, shutter and crop mode hooks", "[andorCtrl]" )
{
    // clang-format off
    #ifdef ANDORCTRL_TEST_DOXYGEN_REF
    andorCtrl::getFPS();
    andorCtrl::fps();
    andorCtrl::setExpTime();
    andorCtrl::setNextROI();
    andorCtrl::checkNextROI();
    andorCtrl::setShutter(0);
    andorCtrl::setCropMode();
    #endif
    // clang-format on

    resetStubs();

    andorCtrl_test app( "camandor" );
    app.m_modeName         = "onlymode";
    app.m_readoutSpeedName = "emccd_17MHz";
    app.m_reconfig         = false;

    SECTION( "getFPS" )
    {
        g_andorStub.expTiming   = 0.01;
        g_andorStub.accumTiming = 0.02;

        REQUIRE( app.getFPS() == 0 );
        REQUIRE( app.m_expTime == Approx( 0.01 ) );
        REQUIRE( app.m_fps == Approx( 50 ) );
        REQUIRE( app.fps() == Approx( 50 ) );
    }

    SECTION( "getFPS errors" )
    {
        app.m_fps = 7;

        g_andorStub.returns["GetAcquisitionTimings"] = DRV_NOT_INITIALIZED;
        REQUIRE( app.getFPS() == -1 );

        g_andorStub.returns.clear();
        g_andorStub.returns["GetReadOutTime"] = DRV_NOT_INITIALIZED;
        REQUIRE( app.getFPS() == -1 );

        REQUIRE( app.m_fps == Approx( 7 ) );
    }

    SECTION( "setExpTime" )
    {
        app.m_expTimeSet = 0.25;

        REQUIRE( app.setExpTime() == 0 );
        REQUIRE( g_andorStub.count( "AbortAcquisition" ) == 1 );
        REQUIRE( g_andorStub.exposureSet == Approx( 0.25 ) );
        REQUIRE( app.state() == stateCodes::CONFIGURING );
        REQUIRE( app.m_nextMode == "onlymode" );
        REQUIRE( app.m_reconfig == true );
    }

    SECTION( "setExpTime error" )
    {
        g_andorStub.returns["SetExposureTime"] = DRV_P1INVALID;

        REQUIRE( app.setExpTime() == -1 );
        REQUIRE( app.m_reconfig == false );
    }

    SECTION( "setNextROI and checkNextROI" )
    {
        REQUIRE( app.checkNextROI() == 0 );
        REQUIRE( app.m_reconfig == false );

        REQUIRE( app.setNextROI() == 0 );
        REQUIRE( g_andorStub.count( "AbortAcquisition" ) == 1 );
        REQUIRE( app.m_nextMode == "onlymode" );
        REQUIRE( app.m_reconfig == true );
    }

    SECTION( "setShutter shut and open" )
    {
        REQUIRE( app.setShutter( 0 ) == 0 );
        REQUIRE( g_andorStub.shutterCalls.back()[1] == 2 );
        REQUIRE( app.m_shutterState == 0 );
        REQUIRE( app.m_reconfig == true );

        REQUIRE( app.setShutter( 1 ) == 0 );
        REQUIRE( g_andorStub.shutterCalls.back()[0] == 1 );
        REQUIRE( g_andorStub.shutterCalls.back()[1] == 1 );
        REQUIRE( g_andorStub.shutterCalls.back()[2] == 500 );
        REQUIRE( g_andorStub.shutterCalls.back()[3] == 500 );
        REQUIRE( app.m_shutterState == 1 );
    }

    SECTION( "setShutter error" )
    {
        app.m_shutterState                = 1;
        g_andorStub.returns["SetShutter"] = DRV_P1INVALID;

        REQUIRE( app.setShutter( 0 ) == -1 );
        REQUIRE( app.m_shutterState == 1 );
        REQUIRE( app.m_reconfig == false );
    }

    SECTION( "setCropMode in the EM amplifier" )
    {
        app.m_cropModeSet = true;

        REQUIRE( app.setCropMode() == 0 );
        REQUIRE( app.m_cropModeSet == true );
        REQUIRE( app.m_reconfig == true );
        REQUIRE( app.m_nextMode == "onlymode" );
    }

    SECTION( "setCropMode in the conventional amplifier is refused" )
    {
        app.m_readoutSpeedName = "ccd_01MHz";
        app.m_cropModeSet      = true;

        REQUIRE( app.setCropMode() == 0 );
        REQUIRE( app.m_cropModeSet == false );
        REQUIRE( app.m_reconfig == true );
    }
}

/// Verify configureAcquisition() sets the image region or crop mode and updates the current ROI.
/**
 * \ingroup andorCtrl_unit_test
 */
TEST_CASE( "andorCtrl configureAcquisition", "[andorCtrl][acquisition]" )
{
    // clang-format off
    #ifdef ANDORCTRL_TEST_DOXYGEN_REF
    andorCtrl::configureAcquisition();
    #endif
    // clang-format on

    resetStubs();

    andorCtrl_test app( "camandor" );
    app.m_modeName = "onlymode";

    app.m_nextROI.x     = 127.5;
    app.m_nextROI.y     = 63.5;
    app.m_nextROI.w     = 128;
    app.m_nextROI.h     = 64;
    app.m_nextROI.bin_x = 2;
    app.m_nextROI.bin_y = 2;

    SECTION( "camera not idle" )
    {
        g_andorStub.status = DRV_ACQUIRING;

        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( g_andorStub.count( "SetImage" ) == 0 );
        REQUIRE( g_andorStub.count( "SetIsolatedCropModeEx" ) == 0 );
    }

    SECTION( "GetStatus error" )
    {
        g_andorStub.returns["GetStatus"] = DRV_NOT_INITIALIZED;

        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
    }

    SECTION( "full-frame image mode" )
    {
        REQUIRE( app.configureAcquisition() == 0 );

        // crop mode is turned off
        REQUIRE( g_andorStub.cropEx[0] == 0 );
        REQUIRE( app.m_cropMode == false );

        // 1-based inclusive region: x0 = 127.5 - 63.5 + 1 = 65, y0 = 63.5 - 31.5 + 1 = 33
        REQUIRE( g_andorStub.image[0] == 2 );
        REQUIRE( g_andorStub.image[1] == 2 );
        REQUIRE( g_andorStub.image[2] == 65 );
        REQUIRE( g_andorStub.image[3] == 192 );
        REQUIRE( g_andorStub.image[4] == 33 );
        REQUIRE( g_andorStub.image[5] == 96 );

        REQUIRE( app.m_currentROI.x == Approx( 127.5 ) );
        REQUIRE( app.m_currentROI.y == Approx( 63.5 ) );
        REQUIRE( app.m_currentROI.w == 128 );
        REQUIRE( app.m_currentROI.h == 64 );
        REQUIRE( app.m_currentROI.bin_x == 2 );
        REQUIRE( app.m_currentROI.bin_y == 2 );

        REQUIRE( app.m_nextROI.x == Approx( 127.5 ) );
        REQUIRE( app.m_nextROI.w == 128 );

        REQUIRE( app.m_width == 64 );
        REQUIRE( app.m_height == 32 );
        REQUIRE( app.m_dataType == _DATATYPE_INT16 );
    }

    SECTION( "crop mode" )
    {
        app.m_cropModeSet = true;

        REQUIRE( app.configureAcquisition() == 0 );

        REQUIRE( app.m_cropMode == true );
        REQUIRE( g_andorStub.cropEx[0] == 1 );
        REQUIRE( g_andorStub.cropEx[1] == 64 );
        REQUIRE( g_andorStub.cropEx[2] == 128 );
        REQUIRE( g_andorStub.cropEx[3] == 2 );
        REQUIRE( g_andorStub.cropEx[4] == 2 );
        REQUIRE( g_andorStub.cropEx[5] == 65 );
        REQUIRE( g_andorStub.cropEx[6] == 33 );
        REQUIRE( g_andorStub.cropType == 1 );
        REQUIRE( g_andorStub.count( "SetImage" ) == 0 );

        REQUIRE( app.m_currentROI.x == Approx( 127.5 ) );
        REQUIRE( app.m_width == 64 );
        REQUIRE( app.m_height == 32 );
    }

    SECTION( "SetImage error reverts the next ROI" )
    {
        app.m_currentROI.x     = 255.5;
        app.m_currentROI.y     = 255.5;
        app.m_currentROI.w     = 512;
        app.m_currentROI.h     = 512;
        app.m_currentROI.bin_x = 1;
        app.m_currentROI.bin_y = 1;

        std::vector<unsigned int> codes = {
            DRV_P1INVALID, DRV_P2INVALID, DRV_P3INVALID, DRV_P4INVALID, DRV_P5INVALID, DRV_P6INVALID, DRV_ERROR_ACK };

        for( auto code : codes )
        {
            app.m_nextROI.x     = 127.5;
            app.m_nextROI.w     = 128;
            app.m_nextROI.bin_x = 2;
            app.m_nextMode      = "";

            g_andorStub.returns["SetImage"] = code;

            CHECK( app.configureAcquisition() == -1 );
            CHECK( app.m_nextROI.x == Approx( 255.5 ) );
            CHECK( app.m_nextROI.w == 512 );
            CHECK( app.m_nextROI.bin_x == 1 );
            CHECK( app.m_nextMode == "onlymode" );
            CHECK( app.state() == stateCodes::ERROR );
        }
    }

    SECTION( "crop mode error reverts the next ROI" )
    {
        app.m_cropModeSet  = true;
        app.m_currentROI.x = 255.5;
        app.m_currentROI.w = 512;

        std::vector<unsigned int> codes = {
            DRV_P2INVALID, DRV_P3INVALID, DRV_P4INVALID, DRV_P5INVALID, DRV_P6INVALID, DRV_P7INVALID, DRV_ERROR_ACK };

        for( auto code : codes )
        {
            app.m_nextROI.x = 127.5;
            app.m_nextROI.w = 128;

            g_andorStub.returns["SetIsolatedCropModeEx"] = code;

            CHECK( app.configureAcquisition() == -1 );
            CHECK( app.m_nextROI.x == Approx( 255.5 ) );
            CHECK( app.m_nextROI.w == 512 );
            CHECK( app.state() == stateCodes::ERROR );
        }

        REQUIRE( g_andorStub.count( "SetIsolatedCropModeType" ) == 0 );
    }
}

/// Verify startAcquisition(), acquireAndCheckValid(), loadImageIntoStream() and reconfig().
/**
 * \ingroup andorCtrl_unit_test
 */
TEST_CASE( "andorCtrl framegrabber hooks", "[andorCtrl][acquisition]" )
{
    // clang-format off
    #ifdef ANDORCTRL_TEST_DOXYGEN_REF
    andorCtrl::startAcquisition();
    andorCtrl::acquireAndCheckValid();
    andorCtrl::loadImageIntoStream(nullptr);
    andorCtrl::reconfig();
    #endif
    // clang-format on

    resetStubs();

    andorCtrl_test app( "camandortest" );

    SECTION( "startAcquisition when already acquiring" )
    {
        g_andorStub.status = DRV_ACQUIRING;

        REQUIRE( app.startAcquisition() == 0 );
        REQUIRE( app.state() == stateCodes::OPERATING );
        REQUIRE( g_andorStub.count( "StartAcquisition" ) == 0 );
        REQUIRE( g_edtStub.startImagesCalls == 0 );
    }

    SECTION( "startAcquisition GetStatus error" )
    {
        g_andorStub.returns["GetStatus"] = DRV_NOT_INITIALIZED;

        REQUIRE( app.startAcquisition() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
    }

    SECTION( "startAcquisition StartAcquisition error" )
    {
        g_andorStub.returns["StartAcquisition"] = DRV_NOT_INITIALIZED;

        REQUIRE( app.startAcquisition() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( g_edtStub.startImagesCalls == 0 );
    }

    SECTION( "startAcquisition starts the camera and the framegrabber" )
    {
        app.m_numBuffs = 6;

        REQUIRE( app.startAcquisition() == 0 ); // sleeps 1 s
        REQUIRE( g_andorStub.count( "StartAcquisition" ) == 1 );
        REQUIRE( app.state() == stateCodes::OPERATING );
        REQUIRE( g_edtStub.startImagesCalls == 1 );
        REQUIRE( g_edtStub.startNumBuffs == 6 );
    }

    SECTION( "acquire and load an image" )
    {
        g_edtStub.image.resize( 16 );
        for( size_t n = 0; n < g_edtStub.image.size(); ++n )
        {
            g_edtStub.image[n] = static_cast<u_char>( n + 1 );
        }
        g_edtStub.tsSec  = 1234;
        g_edtStub.tsNsec = 5678;

        REQUIRE( app.acquireAndCheckValid() == 0 );
        REQUIRE( app.m_image_p == g_edtStub.image.data() );
        REQUIRE( app.m_currImageTimestamp.tv_sec == 1234 );
        REQUIRE( app.m_currImageTimestamp.tv_nsec == 5678 );
        REQUIRE( g_edtStub.startImageCalls == 1 );

        app.m_width    = 4;
        app.m_height   = 2;
        app.m_typeSize = 2;

        std::vector<u_char> dest( 16, 0 );
        REQUIRE( app.loadImageIntoStream( dest.data() ) == 0 );
        for( size_t n = 0; n < dest.size(); ++n )
        {
            REQUIRE( dest[n] == g_edtStub.image[n] );
        }
    }

    SECTION( "reconfig loads the next mode" )
    {
        app.setOnlyMode();
        app.m_nextMode      = "onlymode";
        app.m_nextROI.w     = 128;
        app.m_nextROI.h     = 64;
        app.m_nextROI.bin_x = 1;
        app.m_nextROI.bin_y = 1;
        app.m_numBuffs      = 5;

        REQUIRE( app.reconfig() == 0 );

        REQUIRE( g_andorStub.count( "AbortAcquisition" ) == 1 );
        REQUIRE( readConfigInt( app.configFilePath(), "width:" ) == 128 );
        REQUIRE( readConfigInt( app.configFilePath(), "height:" ) == 64 );

        REQUIRE( app.m_pdv != nullptr );
        REQUIRE( app.m_raw_width == 512 );
        REQUIRE( app.m_raw_height == 256 );
        REQUIRE( app.m_raw_depth == 16 );
        REQUIRE( app.m_cameraType == "stub_andor" );
        REQUIRE( g_edtStub.multibuf == 5 );

        REQUIRE( app.state() == stateCodes::READY );
        REQUIRE( app.m_modeName == "onlymode" );
        REQUIRE( app.m_nextMode == "onlymode" );
    }

    SECTION( "reconfig with an EDT failure still returns to READY" )
    {
        app.setOnlyMode();
        app.m_nextMode          = "onlymode";
        g_edtStub.readcfgReturn = -1;

        REQUIRE( app.reconfig() == 0 ); // pdvReconfig logs and sleeps 1 s
        REQUIRE( app.m_pdv == nullptr );
        REQUIRE( app.state() == stateCodes::READY );
    }
}

/// Verify onPowerOff(), whilePowerOff() and appShutdown() shut down the SDK and reset the shutter status.
/**
 * \ingroup andorCtrl_unit_test
 */
TEST_CASE( "andorCtrl power off and shutdown", "[andorCtrl]" )
{
    // clang-format off
    #ifdef ANDORCTRL_TEST_DOXYGEN_REF
    andorCtrl::onPowerOff();
    andorCtrl::whilePowerOff();
    andorCtrl::appShutdown();
    #endif
    // clang-format on

    resetStubs();

    andorCtrl_test app( "camandor" );

    SECTION( "onPowerOff with the library initialized" )
    {
        app.m_libInit        = true;
        app.m_powerOnCounter = 5;
        app.m_shutterState   = 1;
        app.m_width          = 64;
        app.m_reconfig       = false;

        REQUIRE( app.onPowerOff() == 0 );

        REQUIRE( g_andorStub.count( "ShutDown" ) == 1 );
        REQUIRE( app.m_libInit == false );
        REQUIRE( app.m_powerOnCounter == 0 );
        REQUIRE( app.m_shutterStatus == "POWEROFF" );
        REQUIRE( app.m_shutterState == 0 );
        REQUIRE( app.m_poweredOn == true );

        // frameGrabber::onPowerOff
        REQUIRE( app.m_width == 0 );
        REQUIRE( app.m_reconfig == true );
    }

    SECTION( "onPowerOff with the library not initialized" )
    {
        REQUIRE( app.onPowerOff() == 0 );
        REQUIRE( g_andorStub.count( "ShutDown" ) == 0 );
        REQUIRE( app.m_poweredOn == true );
    }

    SECTION( "whilePowerOff" )
    {
        app.m_shutterState = 1;

        REQUIRE( app.whilePowerOff() == 0 );
        REQUIRE( app.m_shutterStatus == "POWEROFF" );
        REQUIRE( app.m_shutterState == 0 );
        REQUIRE( g_andorStub.count( "ShutDown" ) == 0 );
    }

    SECTION( "appShutdown" )
    {
        app.m_libInit = true;

        REQUIRE( app.appShutdown() == 0 );
        REQUIRE( g_andorStub.count( "ShutDown" ) == 1 );
        REQUIRE( app.m_libInit == false );

        REQUIRE( app.appShutdown() == 0 );
        REQUIRE( g_andorStub.count( "ShutDown" ) == 1 );
    }
}

/// Verify the stdCamera INDI callbacks reach the andorCtrl hooks and command the SDK.
/**
 * \ingroup andorCtrl_unit_test
 */
TEST_CASE( "andorCtrl stdCamera INDI callbacks", "[andorCtrl][indi]" )
{
    // clang-format off
    #ifdef ANDORCTRL_TEST_DOXYGEN_REF
    dev::stdCamera<andorCtrl>::newCallBack_stdCamera(pcf::IndiProperty());
    andorCtrl::setReadoutSpeed();
    andorCtrl::setVShiftSpeed();
    andorCtrl::setEMGain();
    andorCtrl::setTempControl();
    andorCtrl::setTempSetPt();
    andorCtrl::setExpTime();
    andorCtrl::setCropMode();
    andorCtrl::setShutter(0);
    #endif
    // clang-format on

    resetStubs();

    andorCtrl_test app( "camandor" );
    app.m_modeName         = "onlymode";
    app.m_readoutSpeedName = "emccd_17MHz";
    app.m_vShiftSpeedName  = "3_3us";

    SECTION( "wrong device" )
    {
        REQUIRE( app.newCallBack_stdCamera( numberProp( "other", "emgain", "target", 50 ) ) == -1 );
        REQUIRE( g_andorStub.count( "SetEMCCDGain" ) == 0 );
    }

    SECTION( "unhandled properties" )
    {
        // fps is status only, and blacklevel is not exposed
        REQUIRE( app.newCallBack_stdCamera( numberProp( "camandor", "fps", "target", 50 ) ) == -1 );
        REQUIRE( app.newCallBack_stdCamera( numberProp( "camandor", "blacklevel", "target", 5 ) ) == -1 );
    }

    SECTION( "readout_speed" )
    {
        REQUIRE( app.newCallBack_stdCamera(
                     switchProp( "camandor", "readout_speed", "ccd_01MHz", pcf::IndiElement::On ) ) == 0 );
        REQUIRE( app.m_readoutSpeedNameSet == "ccd_01MHz" );
        REQUIRE( app.m_readoutSpeedName == "ccd_01MHz" );
        REQUIRE( g_andorStub.hsTyp == 1 );
        REQUIRE( g_andorStub.hsIndex == 1 );
    }

    SECTION( "vshift_speed" )
    {
        REQUIRE( app.newCallBack_stdCamera( switchProp( "camandor", "vshift_speed", "0_5us", pcf::IndiElement::On ) ) ==
                 0 );
        REQUIRE( app.m_vShiftSpeedName == "0_5us" );
        REQUIRE( g_andorStub.vsIndex == 1 );
    }

    SECTION( "emgain" )
    {
        REQUIRE( app.newCallBack_stdCamera( numberProp( "camandor", "emgain", "target", 50 ) ) == 0 );
        REQUIRE( app.m_emGainSet == Approx( 50 ) );
        REQUIRE( g_andorStub.emccdGainSet == 50 );
    }

    SECTION( "temp_controller" )
    {
        REQUIRE( app.newCallBack_stdCamera(
                     switchProp( "camandor", "temp_controller", "toggle", pcf::IndiElement::On ) ) == 0 );
        REQUIRE( g_andorStub.count( "CoolerON" ) == 1 );
        REQUIRE( app.m_tempControlStatus == true );

        REQUIRE( app.newCallBack_stdCamera(
                     switchProp( "camandor", "temp_controller", "toggle", pcf::IndiElement::Off ) ) == 0 );
        REQUIRE( g_andorStub.count( "CoolerOFF" ) == 1 );
        REQUIRE( app.m_tempControlStatus == false );
    }

    SECTION( "temp_ccd" )
    {
        REQUIRE( app.newCallBack_stdCamera( numberProp( "camandor", "temp_ccd", "target", -30 ) ) == 0 );
        REQUIRE( app.m_ccdTempSetpt == Approx( -30 ) );
        REQUIRE( g_andorStub.count( "SetTemperature" ) == 1 );
    }

    SECTION( "exptime" )
    {
        REQUIRE( app.newCallBack_stdCamera( numberProp( "camandor", "exptime", "target", 0.5 ) ) == 0 );
        REQUIRE( app.m_expTimeSet == Approx( 0.5 ) );
        REQUIRE( g_andorStub.exposureSet == Approx( 0.5 ) );
        REQUIRE( app.m_reconfig == true );
    }

    SECTION( "roi_crop_mode" )
    {
        REQUIRE( app.newCallBack_stdCamera(
                     switchProp( "camandor", "roi_crop_mode", "toggle", pcf::IndiElement::On ) ) == 0 );
        REQUIRE( app.m_cropModeSet == true );
        REQUIRE( app.m_reconfig == true );
    }

    SECTION( "shutter" )
    {
        // toggle On shuts the shutter
        REQUIRE( app.newCallBack_stdCamera( switchProp( "camandor", "shutter", "toggle", pcf::IndiElement::On ) ) ==
                 0 );
        REQUIRE( g_andorStub.shutterCalls.back()[1] == 2 );
        REQUIRE( app.m_shutterState == 0 );

        REQUIRE( app.newCallBack_stdCamera( switchProp( "camandor", "shutter", "toggle", pcf::IndiElement::Off ) ) ==
                 0 );
        REQUIRE( g_andorStub.shutterCalls.back()[1] == 1 );
        REQUIRE( app.m_shutterState == 1 );
    }

    SECTION( "roi_region_x" )
    {
        REQUIRE( app.newCallBack_stdCamera( numberProp( "camandor", "roi_region_x", "target", 100 ) ) == 0 );
        REQUIRE( app.m_nextROI.x == Approx( 100 ) );
    }
}

/// Verify the telemetry interface records the stdcam telemetry.
/**
 * \ingroup andorCtrl_unit_test
 */
TEST_CASE( "andorCtrl telemetry", "[andorCtrl][telem]" )
{
    // clang-format off
    #ifdef ANDORCTRL_TEST_DOXYGEN_REF
    andorCtrl::recordTelem(nullptr);
    andorCtrl::checkRecordTimes();
    #endif
    // clang-format on

    resetStubs();

    andorCtrl_test app( "camandor" );

    SECTION( "recordTelem" )
    {
        MagAOX::logger::telem_stdcam::lastRecord = { 0, 0 };

        REQUIRE( app.recordTelem( static_cast<const MagAOX::logger::telem_stdcam *>( nullptr ) ) == 0 );
        REQUIRE( MagAOX::logger::telem_stdcam::lastRecord.tv_sec > 0 );
    }

    SECTION( "checkRecordTimes records when overdue" )
    {
        MagAOX::logger::telem_stdcam::lastRecord = { 0, 0 };

        REQUIRE( app.checkRecordTimes() == 0 );
        REQUIRE( MagAOX::logger::telem_stdcam::lastRecord.tv_sec > 0 );
    }
}

} // namespace andorCtrlTest

} // namespace libXWCTest
