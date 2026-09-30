/** \file baslerCtrl_test.cpp
 * \brief Catch2 tests for the baslerCtrl app.
 * \author Claude Code
 *
 * \ingroup baslerCtrl_files
 */

#include "../../../tests/testXWC.hpp"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#define protected public
#include "../baslerCtrl.hpp"
#undef protected

using namespace MagAOX::app;

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
namespace
{

/// Fake Basler Pylon SDK state shared by the stub functions below.
struct baslerStubState
{
    bool createThrows{ false }; ///< If true CTlFactory::CreateFirstDevice throws (no camera found).

    int createCalls{ 0 }; ///< Number of CreateFirstDevice calls.

    std::string requestedSerial; ///< Serial number passed to the last CreateFirstDevice.

    std::string modelName{ "acA640-750um" }; ///< Model name reported by the camera.

    std::string serialNumber{ "22334455" }; ///< Serial number reported by the camera.

    int64_t sensorW{ 672 }; ///< Sensor width.

    int64_t sensorH{ 512 }; ///< Sensor height.

    int64_t maxBinH{ 4 }; ///< Maximum horizontal binning.

    int64_t maxBinV{ 2 }; ///< Maximum vertical binning.

    int64_t minW{ 16 }; ///< Minimum ROI width.

    int64_t incW{ 4 }; ///< ROI width increment.

    int64_t minH{ 8 }; ///< Minimum ROI height.

    int64_t incH{ 2 }; ///< ROI height increment.

    int64_t incOffX{ 4 }; ///< OffsetX increment.

    int64_t incOffY{ 4 }; ///< OffsetY increment.

    bool centerWritable{ true }; ///< Whether CenterX/CenterY are writable on new cameras.

    bool centerThrows{ false }; ///< If true CenterX/CenterY SetValue throws on new cameras.

    bool openThrows{ false }; ///< If true Open() throws.

    bool exposureAutoThrows{ false }; ///< If true ExposureAuto.SetValue throws on new cameras.

    bool pixelFormatThrows{ false }; ///< If true PixelFormat.SetValue throws on new cameras.

    int ctorCalls{ 0 }; ///< Number of cameras constructed.

    int dtorCalls{ 0 }; ///< Number of cameras destroyed.

    int registerCalls{ 0 }; ///< Number of RegisterConfiguration calls.

    Pylon::ERegistrationMode lastRegMode{ Pylon::RegistrationMode_Append }; ///< Last registration mode.

    Pylon::ECleanup lastCleanup{ Pylon::Cleanup_None }; ///< Last configuration cleanup mode.

    int openCalls{ 0 }; ///< Number of Open calls.

    int closeCalls{ 0 }; ///< Number of Close calls.

    int startCalls{ 0 }; ///< Number of StartGrabbing calls.

    bool startThrows{ false }; ///< If true StartGrabbing throws.

    Pylon::EGrabStrategy lastStrategy{ Pylon::GrabStrategy_OneByOne }; ///< Last grab strategy.

    int stopCalls{ 0 }; ///< Number of StopGrabbing calls.

    int retrieveCalls{ 0 }; ///< Number of RetrieveResult calls.

    bool retrieveThrows{ false }; ///< If true RetrieveResult throws (timeout).

    bool grabSucceeded{ true }; ///< GrabSucceeded() of the results RetrieveResult returns.

    std::vector<int16_t> frame; ///< Pixels of the results RetrieveResult returns.

    unsigned int lastTimeout{ 0 }; ///< Timeout passed to the last RetrieveResult.

    Pylon::ETimeoutHandling lastTimeoutHandling{ Pylon::TimeoutHandling_Return }; ///< Last timeout handling.

    int initCalls{ 0 }; ///< Number of PylonInitialize calls.

    int termCalls{ 0 }; ///< Number of PylonTerminate calls.
};

/// The global fake Pylon SDK state.
baslerStubState g_baslerStub;

/// The device returned by CreateFirstDevice.
Pylon::IPylonDevice g_baslerDevice;

/// Reset the fake Pylon SDK state before a test.
void resetBaslerStub()
{
    g_baslerStub = baslerStubState();
}

} // namespace

namespace Pylon
{

CTlFactory &CTlFactory::GetInstance()
{
    static CTlFactory factory;
    return factory;
}

IPylonDevice *CTlFactory::CreateFirstDevice( const CDeviceInfo &di )
{
    ++g_baslerStub.createCalls;
    g_baslerStub.requestedSerial = di.GetSerialNumber();

    if( g_baslerStub.createThrows )
    {
        throw GenericException( "no device found" );
    }

    g_baslerDevice.m_info.SetSerialNumber( g_baslerStub.serialNumber );
    g_baslerDevice.m_info.SetModelName( g_baslerStub.modelName );

    return &g_baslerDevice;
}

CBaslerUsbInstantCamera::CBaslerUsbInstantCamera( IPylonDevice *pDevice, ECleanup cleanupProcedure )
{
    static_cast<void>( cleanupProcedure );

    ++g_baslerStub.ctorCalls;

    if( pDevice )
    {
        m_deviceInfo = pDevice->m_info;
    }

    SensorWidth.m_value  = g_baslerStub.sensorW;
    SensorHeight.m_value = g_baslerStub.sensorH;

    BinningHorizontal.m_min   = 1;
    BinningHorizontal.m_max   = g_baslerStub.maxBinH;
    BinningHorizontal.m_inc   = 1;
    BinningHorizontal.m_value = 1;

    BinningVertical.m_min   = 1;
    BinningVertical.m_max   = g_baslerStub.maxBinV;
    BinningVertical.m_inc   = 1;
    BinningVertical.m_value = 1;

    Width.m_min   = g_baslerStub.minW;
    Width.m_inc   = g_baslerStub.incW;
    Width.m_max   = g_baslerStub.sensorW;
    Width.m_value = g_baslerStub.sensorW;

    Height.m_min   = g_baslerStub.minH;
    Height.m_inc   = g_baslerStub.incH;
    Height.m_max   = g_baslerStub.sensorH;
    Height.m_value = g_baslerStub.sensorH;

    OffsetX.m_inc = g_baslerStub.incOffX;
    OffsetY.m_inc = g_baslerStub.incOffY;

    // The maximum ROI size depends on the binning
    BinningHorizontal.m_onSet = [this]( int64_t b ) { Width.m_max = SensorWidth.m_value / b; };
    BinningVertical.m_onSet   = [this]( int64_t b ) { Height.m_max = SensorHeight.m_value / b; };

    CenterX.m_writable   = g_baslerStub.centerWritable;
    CenterY.m_writable   = g_baslerStub.centerWritable;
    CenterX.m_throwOnSet = g_baslerStub.centerThrows;
    CenterY.m_throwOnSet = g_baslerStub.centerThrows;

    ExposureAuto.m_value      = Basler_UsbCameraParams::ExposureAuto_Continuous;
    ExposureAuto.m_throwOnSet = g_baslerStub.exposureAutoThrows;
    PixelFormat.m_throwOnSet  = g_baslerStub.pixelFormatThrows;
}

CBaslerUsbInstantCamera::~CBaslerUsbInstantCamera()
{
    ++g_baslerStub.dtorCalls;
}

void CBaslerUsbInstantCamera::RegisterConfiguration( CConfigurationEventHandler *pConfigurator,
                                                     ERegistrationMode           mode,
                                                     ECleanup                    cleanupProcedure )
{
    ++g_baslerStub.registerCalls;
    g_baslerStub.lastRegMode = mode;
    g_baslerStub.lastCleanup = cleanupProcedure;

    // The camera would own it with Cleanup_Delete
    if( cleanupProcedure == Cleanup_Delete )
    {
        delete pConfigurator;
    }
}

void CBaslerUsbInstantCamera::Open()
{
    ++g_baslerStub.openCalls;

    if( g_baslerStub.openThrows )
    {
        throw GenericException( "open failed" );
    }
}

void CBaslerUsbInstantCamera::Close()
{
    ++g_baslerStub.closeCalls;
}

void CBaslerUsbInstantCamera::StartGrabbing( EGrabStrategy strategy )
{
    ++g_baslerStub.startCalls;
    g_baslerStub.lastStrategy = strategy;

    if( g_baslerStub.startThrows )
    {
        throw GenericException( "start failed" );
    }
}

void CBaslerUsbInstantCamera::StopGrabbing()
{
    ++g_baslerStub.stopCalls;
}

bool CBaslerUsbInstantCamera::RetrieveResult( unsigned int     timeoutMs,
                                              CGrabResultPtr  &grabResult,
                                              ETimeoutHandling timeoutHandling )
{
    ++g_baslerStub.retrieveCalls;
    g_baslerStub.lastTimeout         = timeoutMs;
    g_baslerStub.lastTimeoutHandling = timeoutHandling;

    if( g_baslerStub.retrieveThrows )
    {
        throw GenericException( "timeout" );
    }

    grabResult.m_ptr              = std::make_shared<CGrabResultData>();
    grabResult.m_ptr->m_succeeded = g_baslerStub.grabSucceeded;
    grabResult.m_ptr->m_buffer    = g_baslerStub.frame;

    return true;
}

void PylonInitialize()
{
    ++g_baslerStub.initCalls;
}

void PylonTerminate()
{
    ++g_baslerStub.termCalls;
}

} // namespace Pylon
/// \endcond

namespace libXWCTest
{

/** \defgroup baslerCtrl_unit_test baslerCtrl Unit Tests
 * \brief Unit tests for the baslerCtrl application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `baslerCtrl` unit tests.
/** \ingroup baslerCtrl_unit_test
 */
namespace baslerCtrlTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness which initializes the baslerCtrl state normally set up in appStartup().
class baslerCtrl_test : public baslerCtrl
{
  public:
    /// Construct a harness with the given device name.
    explicit baslerCtrl_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        // Normally done in appStartup()
        m_tempHist.maxEntries( 30 );

        // Power is on
        m_powerState       = 1;
        m_powerTargetState = 1;

        setupProp( m_indiP_exptime, "exptime" );
        setupProp( m_indiP_fps, "fps" );
        setupProp( m_indiP_temp, "temp_ccd" );
        setupProp( m_indiP_blacklevel, "blacklevel" );
        setupProp( m_indiP_roi_w, "roi_region_w" );
        setupProp( m_indiP_roi_check, "roi_region_check" );
        setupProp( m_indiP_roi_set, "roi_set" );
    }

    /// Destructor, deletes the camera created by connect().
    ~baslerCtrl_test() noexcept
    {
        delete m_camera;
        m_camera = nullptr;
    }

    /// Set the device and name of a property owned by this app.
    void setupProp( pcf::IndiProperty &prop, /**< [out] the property to set up */
                    const std::string &name  /**< [in] the INDI property name */
    )
    {
        prop.setDevice( m_configName );
        prop.setName( name );
    }

    /// Connect to the stub camera with the default serial number.
    int connectCamera()
    {
        m_serialNumber = "22334455";
        return connect();
    }

    /// Set the next ROI.
    void setNext( float x,  /**< [in] center x */
                  float y,  /**< [in] center y */
                  int   w,  /**< [in] width */
                  int   h,  /**< [in] height */
                  int   bx, /**< [in] x binning */
                  int   by  /**< [in] y binning */
    )
    {
        m_nextROI.x     = x;
        m_nextROI.y     = y;
        m_nextROI.w     = w;
        m_nextROI.h     = h;
        m_nextROI.bin_x = bx;
        m_nextROI.bin_y = by;
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

/// Verify the baslerCtrl constructor defaults and stdCamera configuration flags, including blacklevel off.
/**
 * \ingroup baslerCtrl_unit_test
 */
TEST_CASE( "baslerCtrl constructor defaults", "[baslerCtrl]" )
{
    // clang-format off
    #ifdef BASLERCTRL_TEST_DOXYGEN_REF
    baslerCtrl::baslerCtrl();
    #endif
    // clang-format on

    resetBaslerStub();

    baslerCtrl_test app( "basler" );

    REQUIRE( app.m_powerMgtEnabled == false );
    REQUIRE( app.m_full_x == Approx( 335.5 ) );
    REQUIRE( app.m_full_y == Approx( 255.5 ) );
    REQUIRE( app.m_full_w == 672 );
    REQUIRE( app.m_full_h == 512 );
    REQUIRE( app.m_bits == 10 );
    REQUIRE( app.m_camera == nullptr );

    REQUIRE( baslerCtrl::c_stdCamera_tempControl == false );
    REQUIRE( baslerCtrl::c_stdCamera_temp == true );
    REQUIRE( baslerCtrl::c_stdCamera_readoutSpeed == false );
    REQUIRE( baslerCtrl::c_stdCamera_vShiftSpeed == false );
    REQUIRE( baslerCtrl::c_stdCamera_fanSpeed == false );
    REQUIRE( baslerCtrl::c_stdCamera_blacklevel == false );
    REQUIRE( baslerCtrl::c_stdCamera_emGain == false );
    REQUIRE( baslerCtrl::c_stdCamera_exptimeCtrl == true );
    REQUIRE( baslerCtrl::c_stdCamera_fpsCtrl == true );
    REQUIRE( baslerCtrl::c_stdCamera_fps == true );
    REQUIRE( baslerCtrl::c_stdCamera_synchro == false );
    REQUIRE( baslerCtrl::c_stdCamera_usesModes == false );
    REQUIRE( baslerCtrl::c_stdCamera_usesROI == true );
    REQUIRE( baslerCtrl::c_stdCamera_cropMode == false );
    REQUIRE( baslerCtrl::c_stdCamera_hasShutter == false );
    REQUIRE( baslerCtrl::c_stdCamera_usesStateString == true );
    REQUIRE( baslerCtrl::c_frameGrabber_flippable == true );
}

/// Verify baslerCtrl configuration defaults: the power-on ROI falls back to the full frame.
/**
 * \ingroup baslerCtrl_unit_test
 */
TEST_CASE( "baslerCtrl configuration defaults", "[baslerCtrl][config]" )
{
    // clang-format off
    #ifdef BASLERCTRL_TEST_DOXYGEN_REF
    baslerCtrl::setupConfig();
    baslerCtrl::loadConfig();
    baslerCtrl::loadConfigImpl(config);
    #endif
    // clang-format on

    resetBaslerStub();

    baslerCtrl_test app( "basler" );

    const std::string path = "/tmp/baslerCtrl_test_defaults.conf";
    mx::app::writeConfigFile( path, { "none" }, { "nada" }, { "0" } );
    app.loadTestConfig( path );
    std::remove( path.c_str() );

    REQUIRE( app.m_shutdown == 0 );
    REQUIRE( app.m_serialNumber == "" );
    REQUIRE( app.m_bits == 10 );

    REQUIRE( app.m_default_x == Approx( 335.5 ) );
    REQUIRE( app.m_default_y == Approx( 255.5 ) );
    REQUIRE( app.m_default_w == 672 );
    REQUIRE( app.m_default_h == 512 );
    REQUIRE( app.m_default_bin_x == 1 );
    REQUIRE( app.m_default_bin_y == 1 );

    REQUIRE( app.m_currentROI.x == Approx( 335.5 ) );
    REQUIRE( app.m_currentROI.w == 672 );
    REQUIRE( app.m_nextROI.y == Approx( 255.5 ) );
    REQUIRE( app.m_nextROI.h == 512 );

    REQUIRE( app.m_shmimName == "basler" );
    REQUIRE( app.m_defaultFlip == dev::frameGrabber<baslerCtrl>::fgFlipNone );
    REQUIRE( app.m_maxInterval == Approx( 10.0 ) );
}

/// Verify baslerCtrl configuration overrides are loaded.
/**
 * \ingroup baslerCtrl_unit_test
 */
TEST_CASE( "baslerCtrl configuration overrides", "[baslerCtrl][config]" )
{
    // clang-format off
    #ifdef BASLERCTRL_TEST_DOXYGEN_REF
    baslerCtrl::setupConfig();
    baslerCtrl::loadConfig();
    baslerCtrl::loadConfigImpl(config);
    #endif
    // clang-format on

    resetBaslerStub();

    baslerCtrl_test app( "basler" );

    const std::string path = "/tmp/baslerCtrl_test_overrides.conf";
    mx::app::writeConfigFile( path,
                              { "camera",
                                "camera",
                                "camera",
                                "camera",
                                "camera",
                                "camera",
                                "camera",
                                "camera",
                                "framegrabber",
                                "framegrabber",
                                "telemeter" },
                              { "serialNumber",
                                "bits",
                                "default_x",
                                "default_y",
                                "default_w",
                                "default_h",
                                "default_bin_x",
                                "default_bin_y",
                                "shmimName",
                                "defaultFlip",
                                "maxInterval" },
                              { "12345678", "12", "101.5", "49.5", "100", "52", "2", "2", "camtip", "flipUD", "3" } );
    app.loadTestConfig( path );
    std::remove( path.c_str() );

    REQUIRE( app.m_shutdown == 0 );
    REQUIRE( app.m_serialNumber == "12345678" );
    REQUIRE( app.m_bits == 12 );

    REQUIRE( app.m_default_x == Approx( 101.5 ) );
    REQUIRE( app.m_default_y == Approx( 49.5 ) );
    REQUIRE( app.m_default_w == 100 );
    REQUIRE( app.m_default_h == 52 );
    REQUIRE( app.m_default_bin_x == 2 );
    REQUIRE( app.m_default_bin_y == 2 );

    REQUIRE( app.m_currentROI.x == Approx( 101.5 ) );
    REQUIRE( app.m_nextROI.bin_y == 2 );

    REQUIRE( app.m_shmimName == "camtip" );
    REQUIRE( app.m_defaultFlip == dev::frameGrabber<baslerCtrl>::fgFlipUD );
    REQUIRE( app.m_maxInterval == Approx( 3.0 ) );
}

/// Verify a partially configured power-on ROI keeps the configured values and fills the rest from the full ROI.
/** This is the per-value fallback from commit 28e0f0d ("Fix clobbered camera defaults").
 *
 * \ingroup baslerCtrl_unit_test
 */
TEST_CASE( "baslerCtrl partial default ROI configuration", "[baslerCtrl][config]" )
{
    // clang-format off
    #ifdef BASLERCTRL_TEST_DOXYGEN_REF
    baslerCtrl::loadConfig();
    baslerCtrl::powerOnDefaults();
    #endif
    // clang-format on

    resetBaslerStub();

    baslerCtrl_test app( "basler" );

    SECTION( "only width and height configured" )
    {
        const std::string path = "/tmp/baslerCtrl_test_partial_wh.conf";
        mx::app::writeConfigFile( path, { "camera", "camera" }, { "default_w", "default_h" }, { "320", "240" } );
        app.loadTestConfig( path );
        std::remove( path.c_str() );

        REQUIRE( app.m_default_x == Approx( 335.5 ) );
        REQUIRE( app.m_default_y == Approx( 255.5 ) );
        REQUIRE( app.m_default_w == 320 );
        REQUIRE( app.m_default_h == 240 );
        REQUIRE( app.m_default_bin_x == 1 );
        REQUIRE( app.m_default_bin_y == 1 );

        REQUIRE( app.m_currentROI.w == 320 );
        REQUIRE( app.m_currentROI.h == 240 );
        REQUIRE( app.m_nextROI.x == Approx( 335.5 ) );
    }

    SECTION( "only x and y binning configured" )
    {
        const std::string path = "/tmp/baslerCtrl_test_partial_xb.conf";
        mx::app::writeConfigFile( path, { "camera", "camera" }, { "default_x", "default_bin_y" }, { "100", "2" } );
        app.loadTestConfig( path );
        std::remove( path.c_str() );

        REQUIRE( app.m_default_x == Approx( 100 ) );
        REQUIRE( app.m_default_y == Approx( 255.5 ) );
        REQUIRE( app.m_default_w == 672 );
        REQUIRE( app.m_default_h == 512 );
        REQUIRE( app.m_default_bin_x == 1 );
        REQUIRE( app.m_default_bin_y == 2 );
    }

    SECTION( "powerOnDefaults does not clobber the ROI" )
    {
        app.m_currentROI.x = 11;
        app.m_currentROI.w = 22;
        app.m_nextROI.y    = 33;
        app.m_nextROI.h    = 44;

        REQUIRE( app.powerOnDefaults() == 0 );

        REQUIRE( app.m_currentROI.x == Approx( 11 ) );
        REQUIRE( app.m_currentROI.w == 22 );
        REQUIRE( app.m_nextROI.y == Approx( 33 ) );
        REQUIRE( app.m_nextROI.h == 44 );
    }
}

/// Verify connect() opens the camera by serial number and interrogates the valid ROI settings.
/**
 * \ingroup baslerCtrl_unit_test
 */
TEST_CASE( "baslerCtrl connect", "[baslerCtrl][connect]" )
{
    // clang-format off
    #ifdef BASLERCTRL_TEST_DOXYGEN_REF
    baslerCtrl::connect();
    #endif
    // clang-format on

    resetBaslerStub();

    baslerCtrl_test app( "basler" );

    SECTION( "success" )
    {
        REQUIRE( app.connectCamera() == 0 );
        REQUIRE( app.state() == stateCodes::CONNECTED );
        REQUIRE( app.m_camera != nullptr );

        REQUIRE( g_baslerStub.createCalls == 1 );
        REQUIRE( g_baslerStub.requestedSerial == "22334455" );
        REQUIRE( g_baslerStub.registerCalls == 1 );
        REQUIRE( g_baslerStub.lastRegMode == Pylon::RegistrationMode_ReplaceAll );
        REQUIRE( g_baslerStub.lastCleanup == Pylon::Cleanup_Delete );
        REQUIRE( g_baslerStub.openCalls == 1 );
        REQUIRE( g_baslerStub.stopCalls == 1 );

        // shmim name from model and serial, since it was not configured
        REQUIRE( app.m_shmimName == "acA640-750um_22334455" );

        REQUIRE( app.m_camera->ExposureAuto.m_value == Basler_UsbCameraParams::ExposureAuto_Off );
        REQUIRE( app.m_camera->PixelFormat.m_value == Basler_UsbCameraParams::PixelFormat_Mono10 );
        REQUIRE( app.m_camera->BinningHorizontalMode.m_value == Basler_UsbCameraParams::BinningHorizontalMode_Sum );
        REQUIRE( app.m_camera->BinningHorizontalMode.m_setCalls == 1 );
        REQUIRE( app.m_camera->BinningVerticalMode.m_value == Basler_UsbCameraParams::BinningVerticalMode_Sum );
        REQUIRE( app.m_camera->BinningVerticalMode.m_setCalls == 1 );
        REQUIRE( app.m_camera->OffsetX.m_setHistory == std::vector<int64_t>{ 0 } );
        REQUIRE( app.m_camera->OffsetY.m_setHistory == std::vector<int64_t>{ 0 } );

        REQUIRE( app.m_binXs == std::vector<int>{ 1, 2, 3, 4 } );
        REQUIRE( app.m_binYs == std::vector<int>{ 1, 2 } );

        REQUIRE( app.m_maxWs == std::vector<int>{ 672, 336, 224, 168 } );
        REQUIRE( app.m_minWs == std::vector<int>{ 16, 16, 16, 16 } );
        REQUIRE( app.m_incWs == std::vector<int>{ 4, 4, 4, 4 } );
        REQUIRE( app.m_incXs == std::vector<int>{ 4, 4, 4, 4 } );

        REQUIRE( app.m_maxHs == std::vector<int>{ 512, 256 } );
        REQUIRE( app.m_minHs == std::vector<int>{ 8, 8 } );
        REQUIRE( app.m_incHs == std::vector<int>{ 2, 2 } );
        REQUIRE( app.m_incYs == std::vector<int>{ 4, 4 } );
    }

    SECTION( "configured shmim name is kept" )
    {
        app.m_shmimName = "camtip";
        REQUIRE( app.connectCamera() == 0 );
        REQUIRE( app.m_shmimName == "camtip" );
    }

    SECTION( "y offset increments are read from OffsetX" )
    {
        // Current behavior: m_incYs uses OffsetX.GetInc(), not OffsetY.GetInc()
        g_baslerStub.incOffX = 4;
        g_baslerStub.incOffY = 2;

        REQUIRE( app.connectCamera() == 0 );
        REQUIRE( app.m_incYs == std::vector<int>{ 4, 4 } );
    }

    SECTION( "8 bits" )
    {
        app.m_bits = 8;
        REQUIRE( app.connectCamera() == 0 );
        REQUIRE( app.m_camera->PixelFormat.m_value == Basler_UsbCameraParams::PixelFormat_Mono8 );
    }

    SECTION( "12 bits" )
    {
        app.m_bits = 12;
        REQUIRE( app.connectCamera() == 0 );
        REQUIRE( app.m_camera->PixelFormat.m_value == Basler_UsbCameraParams::PixelFormat_Mono12 );
    }

    SECTION( "unsupported bits leave the pixel format alone" )
    {
        app.m_bits = 14;
        REQUIRE( app.connectCamera() == 0 );
        REQUIRE( app.m_camera->PixelFormat.m_setCalls == 0 );
        REQUIRE( app.state() == stateCodes::CONNECTED );
    }

    SECTION( "no camera" )
    {
        g_baslerStub.createThrows = true;

        REQUIRE( app.connectCamera() == 0 );
        REQUIRE( app.state() == stateCodes::NODEVICE );
        REQUIRE( app.m_camera == nullptr );
        REQUIRE( g_baslerStub.ctorCalls == 0 );
    }

    SECTION( "open fails" )
    {
        g_baslerStub.openThrows = true;

        REQUIRE( app.connectCamera() == -1 );
        REQUIRE( app.state() == stateCodes::NODEVICE );
        REQUIRE( app.m_camera == nullptr );
        REQUIRE( g_baslerStub.closeCalls == 1 );
        REQUIRE( g_baslerStub.dtorCalls == 1 );
    }

    SECTION( "exposure auto fails" )
    {
        g_baslerStub.exposureAutoThrows = true;

        REQUIRE( app.connectCamera() == -1 );
        REQUIRE( app.state() == stateCodes::NODEVICE );
        REQUIRE( app.m_camera == nullptr );
        REQUIRE( g_baslerStub.dtorCalls == 1 );
    }

    SECTION( "pixel format fails" )
    {
        g_baslerStub.pixelFormatThrows = true;

        REQUIRE( app.connectCamera() == -1 );
        REQUIRE( app.state() == stateCodes::NODEVICE );
        REQUIRE( app.m_camera == nullptr );
    }

    SECTION( "reconnect closes and deletes the old camera" )
    {
        REQUIRE( app.connectCamera() == 0 );
        REQUIRE( app.connectCamera() == 0 );

        REQUIRE( g_baslerStub.ctorCalls == 2 );
        REQUIRE( g_baslerStub.dtorCalls == 1 );
        REQUIRE( g_baslerStub.closeCalls == 1 );
        REQUIRE( app.m_camera != nullptr );
    }

    SECTION( "sensor width mismatch" )
    {
        g_baslerStub.sensorW = 640;

        REQUIRE( app.connectCamera() == -1 );
    }

    SECTION( "sensor height mismatch" )
    {
        g_baslerStub.sensorH = 480;

        REQUIRE( app.connectCamera() == -1 );
    }

    SECTION( "full ROI center mismatch" )
    {
        app.m_full_x = 300;

        REQUIRE( app.connectCamera() == -1 );
    }
}

/// Verify checkNextROI() snaps binning, size and position to valid camera values.
/**
 * \ingroup baslerCtrl_unit_test
 */
TEST_CASE( "baslerCtrl checkNextROI", "[baslerCtrl][roi]" )
{
    // clang-format off
    #ifdef BASLERCTRL_TEST_DOXYGEN_REF
    baslerCtrl::checkNextROI();
    #endif
    // clang-format on

    resetBaslerStub();

    baslerCtrl_test app( "basler" );
    REQUIRE( app.connectCamera() == 0 );

    SECTION( "full frame is unchanged" )
    {
        app.setNext( 335.5, 255.5, 672, 512, 1, 1 );
        REQUIRE( app.checkNextROI() == 0 );

        REQUIRE( app.m_nextROI.x == Approx( 335.5 ) );
        REQUIRE( app.m_nextROI.y == Approx( 255.5 ) );
        REQUIRE( app.m_nextROI.w == 672 );
        REQUIRE( app.m_nextROI.h == 512 );
        REQUIRE( app.m_nextROI.bin_x == 1 );
        REQUIRE( app.m_nextROI.bin_y == 1 );
    }

    SECTION( "size and offset are rounded to the increments" )
    {
        // w 101 -> 100 (inc 4), offset 50 -> 52 (inc 4); h 51 -> 52 (inc 2), offset 24 stays
        app.setNext( 100, 50, 101, 51, 1, 1 );
        REQUIRE( app.checkNextROI() == 0 );

        REQUIRE( app.m_nextROI.w == 100 );
        REQUIRE( app.m_nextROI.x == Approx( 101.5 ) );
        REQUIRE( app.m_nextROI.h == 52 );
        REQUIRE( app.m_nextROI.y == Approx( 49.5 ) );

        // A second check is stable
        REQUIRE( app.checkNextROI() == 0 );
        REQUIRE( app.m_nextROI.x == Approx( 101.5 ) );
        REQUIRE( app.m_nextROI.y == Approx( 49.5 ) );
    }

    SECTION( "width rounds up past half an increment" )
    {
        app.setNext( 335.5, 255.5, 103, 512, 1, 1 );
        REQUIRE( app.checkNextROI() == 0 );
        REQUIRE( app.m_nextROI.w == 104 );
    }

    SECTION( "limits and invalid binning" )
    {
        app.setNext( 335.5, 255.5, 1000, 4, 7, 3 );
        REQUIRE( app.checkNextROI() == 0 );

        REQUIRE( app.m_nextROI.bin_x == 1 );
        REQUIRE( app.m_nextROI.bin_y == 1 );
        REQUIRE( app.m_nextROI.w == 672 );
        REQUIRE( app.m_nextROI.h == 8 );
        REQUIRE( app.m_nextROI.x == Approx( 335.5 ) );
        REQUIRE( app.m_nextROI.y == Approx( 255.5 ) );
    }

    SECTION( "position is kept on the sensor" )
    {
        app.setNext( 700, 255.5, 100, 52, 1, 1 );
        REQUIRE( app.checkNextROI() == 0 );
        REQUIRE( app.m_nextROI.x == Approx( 621.5 ) ); // offset clamped to 672-100

        app.setNext( 10, 255.5, 100, 52, 1, 1 );
        REQUIRE( app.checkNextROI() == 0 );
        REQUIRE( app.m_nextROI.x == Approx( 49.5 ) ); // offset clamped to 0
    }

    SECTION( "binned limits" )
    {
        app.setNext( 167.5, 127.5, 100, 52, 2, 2 );
        REQUIRE( app.checkNextROI() == 0 );

        REQUIRE( app.m_nextROI.bin_x == 2 );
        REQUIRE( app.m_nextROI.bin_y == 2 );
        REQUIRE( app.m_nextROI.x == Approx( 169.5 ) );
        REQUIRE( app.m_nextROI.y == Approx( 129.5 ) );

        app.setNext( 167.5, 127.5, 400, 300, 2, 2 );
        REQUIRE( app.checkNextROI() == 0 );
        REQUIRE( app.m_nextROI.w == 336 );
        REQUIRE( app.m_nextROI.h == 256 );
    }

    SECTION( "left-right flip" )
    {
        app.m_defaultFlip = dev::frameGrabber<baslerCtrl>::fgFlipLR;
        app.setNext( 100, 49.5, 100, 52, 1, 1 );
        REQUIRE( app.checkNextROI() == 0 );

        // flipped offset 521 -> 520
        REQUIRE( app.m_nextROI.x == Approx( 101.5 ) );
        REQUIRE( app.m_nextROI.y == Approx( 49.5 ) );
    }
}

/// Verify configureAcquisition() programs binning, size and offsets and reads back the current ROI.
/**
 * \ingroup baslerCtrl_unit_test
 */
TEST_CASE( "baslerCtrl configureAcquisition", "[baslerCtrl][roi]" )
{
    // clang-format off
    #ifdef BASLERCTRL_TEST_DOXYGEN_REF
    baslerCtrl::configureAcquisition();
    #endif
    // clang-format on

    resetBaslerStub();

    baslerCtrl_test app( "basler" );

    SECTION( "no camera" )
    {
        REQUIRE( app.configureAcquisition() == -1 );
    }

    SECTION( "full frame" )
    {
        REQUIRE( app.connectCamera() == 0 );
        app.m_camera->ResultingFrameRate.m_value = 123.5;
        app.setNext( 335.5, 255.5, 672, 512, 1, 1 );

        REQUIRE( app.configureAcquisition() == 0 );

        REQUIRE( g_baslerStub.stopCalls == 2 );
        REQUIRE( app.m_camera->CenterX.m_value == false );
        REQUIRE( app.m_camera->CenterX.m_setCalls == 1 );
        REQUIRE( app.m_camera->CenterY.m_value == false );
        REQUIRE( app.m_camera->CenterY.m_setCalls == 1 );

        REQUIRE( app.m_camera->Width.m_value == 672 );
        REQUIRE( app.m_camera->Height.m_value == 512 );
        REQUIRE( app.m_camera->OffsetX.m_value == 0 );
        REQUIRE( app.m_camera->OffsetY.m_value == 0 );

        REQUIRE( app.m_currentROI.x == Approx( 335.5 ) );
        REQUIRE( app.m_currentROI.y == Approx( 255.5 ) );
        REQUIRE( app.m_currentROI.w == 672 );
        REQUIRE( app.m_currentROI.h == 512 );
        REQUIRE( app.m_currentROI.bin_x == 1 );
        REQUIRE( app.m_currentROI.bin_y == 1 );

        REQUIRE( app.m_full_currbin_w == 672 );
        REQUIRE( app.m_full_currbin_x == Approx( 335.5 ) );
        REQUIRE( app.m_full_currbin_h == 512 );
        REQUIRE( app.m_full_currbin_y == Approx( 255.5 ) );

        REQUIRE( app.m_width == 672 );
        REQUIRE( app.m_height == 512 );
        REQUIRE( app.m_dataType == _DATATYPE_INT16 );
        REQUIRE( app.m_fps == Approx( 123.5 ) );
    }

    SECTION( "sub-window" )
    {
        REQUIRE( app.connectCamera() == 0 );
        app.setNext( 100, 50, 101, 51, 1, 1 );

        REQUIRE( app.configureAcquisition() == 0 );

        REQUIRE( app.m_camera->Width.m_value == 100 );
        REQUIRE( app.m_camera->Height.m_value == 52 );
        REQUIRE( app.m_camera->OffsetX.m_value == 52 );
        REQUIRE( app.m_camera->OffsetY.m_value == 24 );

        REQUIRE( app.m_currentROI.x == Approx( 101.5 ) );
        REQUIRE( app.m_currentROI.y == Approx( 49.5 ) );
        REQUIRE( app.m_currentROI.w == 100 );
        REQUIRE( app.m_currentROI.h == 52 );

        // next ROI is updated to what was set
        REQUIRE( app.m_nextROI.x == Approx( 101.5 ) );
        REQUIRE( app.m_nextROI.w == 100 );
        REQUIRE( app.m_nextROI.h == 52 );

        REQUIRE( app.m_width == 100 );
        REQUIRE( app.m_height == 52 );
    }

    SECTION( "binned" )
    {
        REQUIRE( app.connectCamera() == 0 );
        app.setNext( 167.5, 127.5, 100, 52, 2, 2 );

        REQUIRE( app.configureAcquisition() == 0 );

        REQUIRE( app.m_camera->BinningHorizontal.m_value == 2 );
        REQUIRE( app.m_camera->BinningVertical.m_value == 2 );
        REQUIRE( app.m_camera->OffsetX.m_value == 120 );
        REQUIRE( app.m_camera->OffsetY.m_value == 104 );

        REQUIRE( app.m_currentROI.bin_x == 2 );
        REQUIRE( app.m_currentROI.bin_y == 2 );
        REQUIRE( app.m_currentROI.x == Approx( 169.5 ) );
        REQUIRE( app.m_currentROI.y == Approx( 129.5 ) );

        REQUIRE( app.m_full_currbin_w == 336 );
        REQUIRE( app.m_full_currbin_x == Approx( 167.5 ) );
        REQUIRE( app.m_full_currbin_h == 256 );
        REQUIRE( app.m_full_currbin_y == Approx( 127.5 ) );
    }

    SECTION( "left-right flip" )
    {
        REQUIRE( app.connectCamera() == 0 );
        app.m_defaultFlip = dev::frameGrabber<baslerCtrl>::fgFlipLR;
        app.setNext( 100, 49.5, 100, 52, 1, 1 );

        REQUIRE( app.configureAcquisition() == 0 );

        REQUIRE( app.m_camera->OffsetX.m_value == 520 );
        REQUIRE( app.m_camera->OffsetY.m_value == 24 );
        REQUIRE( app.m_currentROI.x == Approx( 101.5 ) );
        REQUIRE( app.m_currentROI.y == Approx( 49.5 ) );
    }

    SECTION( "up-down flip" )
    {
        REQUIRE( app.connectCamera() == 0 );
        app.m_defaultFlip = dev::frameGrabber<baslerCtrl>::fgFlipUD;
        app.setNext( 101.5, 49.5, 100, 52, 1, 1 );

        REQUIRE( app.configureAcquisition() == 0 );

        REQUIRE( app.m_camera->OffsetX.m_value == 52 );
        REQUIRE( app.m_camera->OffsetY.m_value == 436 );
        REQUIRE( app.m_currentROI.x == Approx( 101.5 ) );
        REQUIRE( app.m_currentROI.y == Approx( 49.5 ) );
    }

    SECTION( "CenterX/CenterY are only set when writable" )
    {
        // Commit 39b3a5e: cameras without writable CenterX/Y must still configure
        g_baslerStub.centerWritable = false;
        g_baslerStub.centerThrows   = true;
        REQUIRE( app.connectCamera() == 0 );
        app.setNext( 335.5, 255.5, 672, 512, 1, 1 );

        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( app.m_camera->CenterX.m_setCalls == 0 );
        REQUIRE( app.m_camera->CenterY.m_setCalls == 0 );
        REQUIRE( app.m_currentROI.w == 672 );
    }

    SECTION( "writable CenterX that fails is an error" )
    {
        g_baslerStub.centerThrows = true;
        REQUIRE( app.connectCamera() == 0 );
        app.setNext( 335.5, 255.5, 672, 512, 1, 1 );

        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( app.state() == stateCodes::NOTCONNECTED );
    }

    SECTION( "camera rejects the ROI" )
    {
        REQUIRE( app.connectCamera() == 0 );
        app.m_camera->Width.m_throwOnSet = true;
        app.setNext( 335.5, 255.5, 672, 512, 1, 1 );

        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( app.state() == stateCodes::NOTCONNECTED );
    }
}

/// Verify getTemp() averages the device temperature and handles errors.
/**
 * \ingroup baslerCtrl_unit_test
 */
TEST_CASE( "baslerCtrl getTemp", "[baslerCtrl][temperature]" )
{
    // clang-format off
    #ifdef BASLERCTRL_TEST_DOXYGEN_REF
    baslerCtrl::getTemp();
    #endif
    // clang-format on

    resetBaslerStub();

    baslerCtrl_test app( "basler" );

    SECTION( "no camera" )
    {
        REQUIRE( app.getTemp() == 0 );
        REQUIRE( app.m_ccdTemp == Approx( -999 ) );
    }

    SECTION( "running average" )
    {
        REQUIRE( app.connectCamera() == 0 );

        app.m_camera->DeviceTemperature.m_value = 40;
        REQUIRE( app.getTemp() == 0 );
        REQUIRE( app.m_ccdTemp == Approx( 40 ) );

        app.m_camera->DeviceTemperature.m_value = 42;
        REQUIRE( app.getTemp() == 0 );
        REQUIRE( app.m_ccdTemp == Approx( 41 ) );

        app.m_camera->DeviceTemperature.m_value = 44;
        REQUIRE( app.getTemp() == 0 );
        REQUIRE( app.m_ccdTemp == Approx( 42 ) );
    }

    SECTION( "error" )
    {
        REQUIRE( app.connectCamera() == 0 );
        app.m_camera->DeviceTemperature.m_throwOnGet = true;

        REQUIRE( app.getTemp() == -1 );
        REQUIRE( app.m_ccdTemp == Approx( -999 ) );
        REQUIRE( app.state() == stateCodes::NOTCONNECTED );
    }
}

/// Verify getExpTime() and getFPS() read the camera and handle errors.
/**
 * \ingroup baslerCtrl_unit_test
 */
TEST_CASE( "baslerCtrl getExpTime and getFPS", "[baslerCtrl][exposure]" )
{
    // clang-format off
    #ifdef BASLERCTRL_TEST_DOXYGEN_REF
    baslerCtrl::getExpTime();
    baslerCtrl::getFPS();
    baslerCtrl::fps();
    #endif
    // clang-format on

    resetBaslerStub();

    baslerCtrl_test app( "basler" );

    SECTION( "no camera" )
    {
        app.m_expTime = 1;
        app.m_fps     = 2;
        REQUIRE( app.getExpTime() == 0 );
        REQUIRE( app.getFPS() == 0 );
        REQUIRE( app.m_expTime == Approx( 1 ) );
        REQUIRE( app.m_fps == Approx( 2 ) );
    }

    SECTION( "success" )
    {
        REQUIRE( app.connectCamera() == 0 );
        app.m_camera->ExposureTime.m_value       = 5000; // microseconds
        app.m_camera->ResultingFrameRate.m_value = 199.5;

        REQUIRE( app.getExpTime() == 0 );
        REQUIRE( app.m_expTime == Approx( 0.005 ) );

        REQUIRE( app.getFPS() == 0 );
        REQUIRE( app.m_fps == Approx( 199.5 ) );
        REQUIRE( app.fps() == Approx( 199.5 ) );
    }

    SECTION( "exposure error" )
    {
        REQUIRE( app.connectCamera() == 0 );
        app.m_camera->ExposureTime.m_throwOnGet = true;

        REQUIRE( app.getExpTime() == -1 );
        REQUIRE( app.m_expTime == Approx( -999 ) );
        REQUIRE( app.state() == stateCodes::NOTCONNECTED );
    }

    SECTION( "fps error" )
    {
        REQUIRE( app.connectCamera() == 0 );
        app.m_camera->ResultingFrameRate.m_throwOnGet = true;

        REQUIRE( app.getFPS() == -1 );
        REQUIRE( app.m_fps == Approx( -999 ) );
        REQUIRE( app.state() == stateCodes::NOTCONNECTED );
    }
}

/// Verify setExpTime() and setFPS() program the camera and handle errors.
/**
 * \ingroup baslerCtrl_unit_test
 */
TEST_CASE( "baslerCtrl setExpTime and setFPS", "[baslerCtrl][exposure]" )
{
    // clang-format off
    #ifdef BASLERCTRL_TEST_DOXYGEN_REF
    baslerCtrl::setExpTime();
    baslerCtrl::setFPS();
    #endif
    // clang-format on

    resetBaslerStub();

    baslerCtrl_test app( "basler" );

    SECTION( "no camera" )
    {
        REQUIRE( app.setExpTime() == 0 );
        REQUIRE( app.setFPS() == 0 );
    }

    SECTION( "exposure time in microseconds" )
    {
        REQUIRE( app.connectCamera() == 0 );
        app.m_expTimeSet = 0.02;

        REQUIRE( app.setExpTime() == 0 );
        REQUIRE( app.m_camera->ExposureTime.m_value == Approx( 20000 ) );
    }

    SECTION( "exposure time error" )
    {
        REQUIRE( app.connectCamera() == 0 );
        app.m_camera->ExposureTime.m_throwOnSet = true;

        REQUIRE( app.setExpTime() == -1 );
    }

    SECTION( "fps limit" )
    {
        REQUIRE( app.connectCamera() == 0 );
        app.m_fpsSet = 50;

        REQUIRE( app.setFPS() == 0 );
        REQUIRE( app.m_camera->AcquisitionFrameRateEnable.m_value == true );
        REQUIRE( app.m_camera->AcquisitionFrameRate.m_value == Approx( 50 ) );
    }

    SECTION( "fps 0 disables the limit" )
    {
        REQUIRE( app.connectCamera() == 0 );
        app.m_fpsSet = 0;

        REQUIRE( app.setFPS() == 0 );
        REQUIRE( app.m_camera->AcquisitionFrameRateEnable.m_value == false );
        REQUIRE( app.m_camera->AcquisitionFrameRate.m_setCalls == 0 );
    }

    SECTION( "fps errors" )
    {
        REQUIRE( app.connectCamera() == 0 );
        app.m_camera->AcquisitionFrameRateEnable.m_throwOnSet = true;

        app.m_fpsSet = 0;
        REQUIRE( app.setFPS() == -1 );

        app.m_fpsSet = 10;
        REQUIRE( app.setFPS() == -1 );
    }
}

/// Verify startAcquisition(), acquireAndCheckValid() and reconfig().
/**
 * \ingroup baslerCtrl_unit_test
 */
TEST_CASE( "baslerCtrl acquisition", "[baslerCtrl][framegrabber]" )
{
    // clang-format off
    #ifdef BASLERCTRL_TEST_DOXYGEN_REF
    baslerCtrl::startAcquisition();
    baslerCtrl::acquireAndCheckValid();
    baslerCtrl::reconfig();
    #endif
    // clang-format on

    resetBaslerStub();

    baslerCtrl_test app( "basler" );
    REQUIRE( app.connectCamera() == 0 );
    app.state( stateCodes::READY );

    SECTION( "start" )
    {
        REQUIRE( app.startAcquisition() == 0 );
        REQUIRE( g_baslerStub.startCalls == 1 );
        REQUIRE( g_baslerStub.lastStrategy == Pylon::GrabStrategy_LatestImageOnly );
        REQUIRE( app.state() == stateCodes::OPERATING );
    }

    SECTION( "start error" )
    {
        g_baslerStub.startThrows = true;
        REQUIRE( app.startAcquisition() == -1 );
        REQUIRE( app.state() == stateCodes::NOTCONNECTED );
    }

    SECTION( "frame" )
    {
        app.m_currImageTimestamp = { 0, 0 };

        REQUIRE( app.acquireAndCheckValid() == 0 );
        REQUIRE( g_baslerStub.retrieveCalls == 1 );
        REQUIRE( g_baslerStub.lastTimeout == 1000 );
        REQUIRE( g_baslerStub.lastTimeoutHandling == Pylon::TimeoutHandling_ThrowException );
        REQUIRE( app.m_currImageTimestamp.tv_sec > 0 );
        REQUIRE( app.state() == stateCodes::READY );
    }

    SECTION( "timeout" )
    {
        g_baslerStub.retrieveThrows = true;
        REQUIRE( app.acquireAndCheckValid() == -1 );
        REQUIRE( app.state() == stateCodes::NOTCONNECTED );
    }

    SECTION( "failed grab" )
    {
        g_baslerStub.grabSucceeded = false;
        REQUIRE( app.acquireAndCheckValid() == -1 );
        REQUIRE( app.state() == stateCodes::NOTCONNECTED );
    }

    SECTION( "reconfig" )
    {
        REQUIRE( app.reconfig() == 0 );
    }
}

/// Verify loadImageIntoStream() copies the grabbed frame with the configured flip.
/**
 * \ingroup baslerCtrl_unit_test
 */
TEST_CASE( "baslerCtrl loadImageIntoStream", "[baslerCtrl][framegrabber]" )
{
    // clang-format off
    #ifdef BASLERCTRL_TEST_DOXYGEN_REF
    baslerCtrl::loadImageIntoStream(nullptr);
    #endif
    // clang-format on

    resetBaslerStub();

    baslerCtrl_test app( "basler" );
    app.state( stateCodes::OPERATING );

    app.m_width  = 3;
    app.m_height = 2;

    std::vector<int16_t> dest( 6, 0 );

    SECTION( "no grab result" )
    {
        REQUIRE( app.loadImageIntoStream( dest.data() ) == -1 );
        REQUIRE( app.state() == stateCodes::NOTCONNECTED );
    }

    SECTION( "empty buffer" )
    {
        app.ptrGrabResult.m_ptr = std::make_shared<Pylon::CGrabResultData>();
        REQUIRE( app.loadImageIntoStream( dest.data() ) == -1 );
    }

    SECTION( "copies" )
    {
        app.ptrGrabResult.m_ptr           = std::make_shared<Pylon::CGrabResultData>();
        app.ptrGrabResult.m_ptr->m_buffer = { 1, 2, 3, 4, 5, 6 };

        SECTION( "no flip" )
        {
            app.m_defaultFlip = dev::frameGrabber<baslerCtrl>::fgFlipNone;
            REQUIRE( app.loadImageIntoStream( dest.data() ) == 0 );
            REQUIRE( dest == std::vector<int16_t>{ 1, 2, 3, 4, 5, 6 } );
        }

        SECTION( "up-down flip" )
        {
            app.m_defaultFlip = dev::frameGrabber<baslerCtrl>::fgFlipUD;
            REQUIRE( app.loadImageIntoStream( dest.data() ) == 0 );
            REQUIRE( dest == std::vector<int16_t>{ 4, 5, 6, 1, 2, 3 } );
        }

        SECTION( "invalid flip" )
        {
            app.m_defaultFlip = 99;
            REQUIRE( app.loadImageIntoStream( dest.data() ) == -1 );
        }
    }
}

/// Verify stateString() and stateStringValid() describe the camera state for dark management.
/**
 * \ingroup baslerCtrl_unit_test
 */
TEST_CASE( "baslerCtrl stateString", "[baslerCtrl]" )
{
    // clang-format off
    #ifdef BASLERCTRL_TEST_DOXYGEN_REF
    baslerCtrl::stateString();
    baslerCtrl::stateStringValid();
    #endif
    // clang-format on

    resetBaslerStub();

    baslerCtrl_test app( "basler" );

    app.m_currentROI.x     = 335.5;
    app.m_currentROI.y     = 255.5;
    app.m_currentROI.w     = 672;
    app.m_currentROI.h     = 512;
    app.m_currentROI.bin_x = 1;
    app.m_currentROI.bin_y = 2;
    app.m_expTime          = 0.005;
    app.m_ccdTemp          = 40.6;

    REQUIRE( app.stateString() == "335.500000_255.500000_672x512_1x2_0.005000_41.000000" );
    REQUIRE( app.stateStringValid() == true );
}

/// Verify appShutdown() closes the camera and terminates Pylon.
/**
 * \ingroup baslerCtrl_unit_test
 */
TEST_CASE( "baslerCtrl appShutdown", "[baslerCtrl]" )
{
    // clang-format off
    #ifdef BASLERCTRL_TEST_DOXYGEN_REF
    baslerCtrl::appShutdown();
    #endif
    // clang-format on

    resetBaslerStub();

    baslerCtrl_test app( "basler" );

    SECTION( "no camera" )
    {
        REQUIRE( app.appShutdown() == 0 );
        REQUIRE( g_baslerStub.closeCalls == 0 );
        REQUIRE( g_baslerStub.termCalls == 1 );
    }

    SECTION( "open camera" )
    {
        REQUIRE( app.connectCamera() == 0 );
        REQUIRE( app.appShutdown() == 0 );
        REQUIRE( g_baslerStub.closeCalls == 1 );
        REQUIRE( g_baslerStub.termCalls == 1 );
    }
}

/// Verify the stdCamera INDI callbacks reach the baslerCtrl hooks, and blacklevel is not exposed.
/**
 * \ingroup baslerCtrl_unit_test
 */
TEST_CASE( "baslerCtrl stdCamera INDI callbacks", "[baslerCtrl][indi]" )
{
    // clang-format off
    #ifdef BASLERCTRL_TEST_DOXYGEN_REF
    baslerCtrl::newCallBack_stdCamera(pcf::IndiProperty());
    baslerCtrl::newCallBack_blacklevel(pcf::IndiProperty());
    baslerCtrl::setExpTime();
    baslerCtrl::setFPS();
    baslerCtrl::checkNextROI();
    baslerCtrl::setNextROI();
    #endif
    // clang-format on

    resetBaslerStub();

    baslerCtrl_test app( "basler" );
    REQUIRE( app.connectCamera() == 0 );

    SECTION( "wrong device" )
    {
        REQUIRE( app.newCallBack_stdCamera( numberProp( "other", "exptime", "target", 0.01 ) ) == -1 );
        REQUIRE( app.m_camera->ExposureTime.m_setCalls == 0 );
    }

    SECTION( "blacklevel is not exposed" )
    {
        float bl = app.m_blacklevelSet;

        REQUIRE( app.newCallBack_stdCamera( numberProp( "basler", "blacklevel", "target", 5 ) ) == -1 );
        REQUIRE( app.m_blacklevelSet == Approx( bl ) );

        // The handler itself is a no-op when c_stdCamera_blacklevel is false
        REQUIRE( app.newCallBack_blacklevel( numberProp( "basler", "blacklevel", "target", 5 ) ) == 0 );
        REQUIRE( app.m_blacklevelSet == Approx( bl ) );
    }

    SECTION( "emgain is not exposed" )
    {
        REQUIRE( app.newCallBack_stdCamera( numberProp( "basler", "emgain", "target", 5 ) ) == -1 );
    }

    SECTION( "temp_ccd is read-only" )
    {
        REQUIRE( app.newCallBack_stdCamera( numberProp( "basler", "temp_ccd", "target", -10 ) ) == 0 );
        REQUIRE( app.m_ccdTempSetpt == Approx( -999 ) );
    }

    SECTION( "exptime" )
    {
        REQUIRE( app.newCallBack_stdCamera( numberProp( "basler", "exptime", "target", 0.01 ) ) == 0 );
        REQUIRE( app.m_expTimeSet == Approx( 0.01 ) );
        REQUIRE( app.m_camera->ExposureTime.m_value == Approx( 10000 ) );
    }

    SECTION( "fps" )
    {
        REQUIRE( app.newCallBack_stdCamera( numberProp( "basler", "fps", "target", 30 ) ) == 0 );
        REQUIRE( app.m_fpsSet == Approx( 30 ) );
        REQUIRE( app.m_camera->AcquisitionFrameRateEnable.m_value == true );
        REQUIRE( app.m_camera->AcquisitionFrameRate.m_value == Approx( 30 ) );

        REQUIRE( app.newCallBack_stdCamera( numberProp( "basler", "fps", "current", 0 ) ) == 0 );
        REQUIRE( app.m_fpsSet == Approx( 0 ) );
        REQUIRE( app.m_camera->AcquisitionFrameRateEnable.m_value == false );
    }

    SECTION( "roi width and check" )
    {
        app.setNext( 335.5, 255.5, 672, 512, 1, 1 );

        REQUIRE( app.newCallBack_stdCamera( numberProp( "basler", "roi_region_w", "target", 103 ) ) == 0 );
        REQUIRE( app.m_nextROI.w == 103 );

        REQUIRE( app.newCallBack_stdCamera(
                     switchProp( "basler", "roi_region_check", "request", pcf::IndiElement::On ) ) == 0 );
        REQUIRE( app.m_nextROI.w == 104 );
    }

    SECTION( "roi_set" )
    {
        app.m_currentROI.x = 11;
        app.m_currentROI.w = 22;
        app.m_reconfig     = false;

        REQUIRE( app.newCallBack_stdCamera( switchProp( "basler", "roi_set", "request", pcf::IndiElement::On ) ) == 0 );
        REQUIRE( app.m_reconfig == true );
        REQUIRE( app.m_lastROI.x == Approx( 11 ) );
        REQUIRE( app.m_lastROI.w == 22 );
    }
}

/// Verify the telemetry interface records the camera state.
/**
 * \ingroup baslerCtrl_unit_test
 */
TEST_CASE( "baslerCtrl telemetry", "[baslerCtrl][telemetry]" )
{
    // clang-format off
    #ifdef BASLERCTRL_TEST_DOXYGEN_REF
    baslerCtrl::recordTelem(nullptr);
    baslerCtrl::checkRecordTimes();
    #endif
    // clang-format on

    resetBaslerStub();

    baslerCtrl_test app( "basler" );

    REQUIRE( app.recordTelem( nullptr ) == 0 );
    REQUIRE( app.checkRecordTimes() == 0 );
}

} // namespace baslerCtrlTest

} // namespace libXWCTest
