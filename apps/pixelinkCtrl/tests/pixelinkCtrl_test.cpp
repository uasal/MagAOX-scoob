/** \file pixelinkCtrl_test.cpp
 * \brief Catch2 tests for the pixelinkCtrl app.
 * \author Claude Code
 *
 * \ingroup pixelinkCtrl_files
 */

#include "../../../tests/testXWC.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#define protected public
#include "../pixelinkCtrl.hpp"
#undef protected

using namespace MagAOX::app;

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
namespace
{

/// A recorded PxLSetFeature call.
struct pxlSetCall
{
    U32 featureId; ///< The feature set.

    U32 flags; ///< The flags passed.

    std::vector<F32> params; ///< The parameters passed.
};

/// Fake PixeLINK SDK state shared by the stub functions below.
struct pxlStubState
{
    HANDLE handle{ reinterpret_cast<HANDLE>( static_cast<uintptr_t>( 0x1234 ) ) }; ///< Handle PxLInitializeEx returns.

    PXL_RETURN_CODE initReturn{ ApiSuccess }; ///< Forced PxLInitializeEx return code.

    int initCalls{ 0 }; ///< Number of PxLInitializeEx calls.

    U32 lastSerial{ 99 }; ///< Serial number passed to the last PxLInitializeEx.

    int uninitCalls{ 0 }; ///< Number of PxLUninitialize calls.

    HANDLE lastUninit{ nullptr }; ///< Handle passed to the last PxLUninitialize.

    std::vector<U32> streamStates; ///< Every state passed to PxLSetStreamState, in order.

    std::map<U32, PXL_RETURN_CODE> streamReturns; ///< Forced PxLSetStreamState return codes, by state.

    std::map<U32, std::vector<F32>> features; ///< Stored feature parameters, by feature.

    std::map<U32, std::vector<F32>> readbackOverrides; ///< Parameters returned by PxLGetFeature instead of stored ones.

    std::map<U32, PXL_RETURN_CODE> setReturns; ///< Forced PxLSetFeature return codes, by feature.

    std::map<U32, PXL_RETURN_CODE> getReturns; ///< Forced PxLGetFeature return codes, by feature.

    std::vector<pxlSetCall> setCalls; ///< Every PxLSetFeature call, in order.

    PXL_RETURN_CODE frameReturn{ ApiSuccess }; ///< Forced PxLGetNextFrame return code.

    int frameCalls{ 0 }; ///< Number of PxLGetNextFrame calls.

    U32 lastBufferSize{ 0 }; ///< Buffer size passed to the last PxLGetNextFrame.

    U32 lastDescSize{ 0 }; ///< Descriptor uSize seen by the last PxLGetNextFrame.

    std::vector<uint16_t> frameData; ///< Pixels PxLGetNextFrame writes into the frame buffer.

    /// Get the parameters of the last PxLSetFeature call for a feature, or an empty vector.
    std::vector<F32> lastSet( U32 featureId /**< [in] the feature to look up */ ) const
    {
        for( auto it = setCalls.rbegin(); it != setCalls.rend(); ++it )
        {
            if( it->featureId == featureId )
            {
                return it->params;
            }
        }

        return {};
    }

    /// Get the flags of the last PxLSetFeature call for a feature, or 0.
    U32 lastSetFlags( U32 featureId /**< [in] the feature to look up */ ) const
    {
        for( auto it = setCalls.rbegin(); it != setCalls.rend(); ++it )
        {
            if( it->featureId == featureId )
            {
                return it->flags;
            }
        }

        return 0;
    }

    /// Get the number of PxLSetFeature calls for a feature.
    int setCount( U32 featureId /**< [in] the feature to count */ ) const
    {
        int n = 0;
        for( const auto &c : setCalls )
        {
            if( c.featureId == featureId )
            {
                ++n;
            }
        }

        return n;
    }
};

/// The global fake PixeLINK SDK state.
pxlStubState g_pxlStub;

/// Reset the fake PixeLINK SDK state before a test.
void resetPxlStub()
{
    g_pxlStub = pxlStubState();
}

} // namespace

extern "C"
{

    PXL_RETURN_CODE PxLInitializeEx( U32 serialNumber, HANDLE *phCamera, U32 flags )
    {
        static_cast<void>( flags );

        ++g_pxlStub.initCalls;
        g_pxlStub.lastSerial = serialNumber;

        if( g_pxlStub.initReturn != ApiSuccess )
        {
            return g_pxlStub.initReturn;
        }

        *phCamera = g_pxlStub.handle;

        return ApiSuccess;
    }

    PXL_RETURN_CODE PxLUninitialize( HANDLE hCamera )
    {
        ++g_pxlStub.uninitCalls;
        g_pxlStub.lastUninit = hCamera;
        return ApiSuccess;
    }

    PXL_RETURN_CODE PxLSetStreamState( HANDLE hCamera, U32 streamState )
    {
        static_cast<void>( hCamera );

        g_pxlStub.streamStates.push_back( streamState );

        if( g_pxlStub.streamReturns.count( streamState ) > 0 )
        {
            return g_pxlStub.streamReturns[streamState];
        }

        return ApiSuccess;
    }

    PXL_RETURN_CODE PxLGetFeature( HANDLE hCamera, U32 featureId, U32 *pFlags, U32 *pNumberParms, F32 *pParms )
    {
        static_cast<void>( hCamera );

        if( g_pxlStub.getReturns.count( featureId ) > 0 )
        {
            return g_pxlStub.getReturns[featureId];
        }

        const std::vector<F32> &vals = g_pxlStub.readbackOverrides.count( featureId ) > 0
                                           ? g_pxlStub.readbackOverrides[featureId]
                                           : g_pxlStub.features[featureId];

        size_t n = std::min( static_cast<size_t>( *pNumberParms ), vals.size() );
        for( size_t i = 0; i < n; ++i )
        {
            pParms[i] = vals[i];
        }

        *pFlags = FEATURE_FLAG_MANUAL;

        return ApiSuccess;
    }

    PXL_RETURN_CODE PxLSetFeature( HANDLE hCamera, U32 featureId, U32 flags, U32 numberParms, const F32 *pParms )
    {
        static_cast<void>( hCamera );

        std::vector<F32> params( pParms, pParms + numberParms );
        g_pxlStub.setCalls.push_back( { featureId, flags, params } );

        if( g_pxlStub.setReturns.count( featureId ) > 0 )
        {
            return g_pxlStub.setReturns[featureId];
        }

        g_pxlStub.features[featureId] = params;

        return ApiSuccess;
    }

    PXL_RETURN_CODE PxLGetNextFrame( HANDLE hCamera, U32 bufferSize, LPVOID pFrame, PFRAME_DESC pDescriptor )
    {
        static_cast<void>( hCamera );

        ++g_pxlStub.frameCalls;
        g_pxlStub.lastBufferSize = bufferSize;
        g_pxlStub.lastDescSize   = pDescriptor->uSize;

        if( g_pxlStub.frameReturn != ApiSuccess )
        {
            return g_pxlStub.frameReturn;
        }

        // Only write what the test provided, since pixelinkCtrl passes a fixed buffer size
        if( pFrame != nullptr && g_pxlStub.frameData.size() > 0 )
        {
            memcpy( pFrame, g_pxlStub.frameData.data(), g_pxlStub.frameData.size() * sizeof( uint16_t ) );
        }

        return ApiSuccess;
    }

} // extern "C"
/// \endcond

namespace libXWCTest
{

/** \defgroup pixelinkCtrl_unit_test pixelinkCtrl Unit Tests
 * \brief Unit tests for the pixelinkCtrl application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `pixelinkCtrl` unit tests.
/** \ingroup pixelinkCtrl_unit_test
 */
namespace pixelinkCtrlTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness which initializes the pixelinkCtrl state the constructor leaves unset.
class pixelinkCtrl_test : public pixelinkCtrl
{
  public:
    /// Construct a harness with the given device name.
    explicit pixelinkCtrl_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        // pixelinkCtrl() leaves these uninitialized
        m_running    = false;
        m_bodyTemp   = -999;
        m_sensorTemp = -999;

        // Power is on
        m_powerState       = 1;
        m_powerTargetState = 1;

        setupProp( m_indiP_exptime, "exptime" );
        setupProp( m_indiP_emGain, "emgain" );
        setupProp( m_indiP_fps, "fps" );
        setupProp( m_indiP_roi_x, "roi_region_x" );
        setupProp( m_indiP_roi_set, "roi_set" );
        setupProp( m_indiP_streamSwitch, "streaming" );
    }

    /// Destructor, frees the frame buffer allocated by configureAcquisition.
    ~pixelinkCtrl_test() noexcept
    {
        releaseFrameBuffer();
    }

    /// Set the device and name of a property owned by this app.
    void setupProp( pcf::IndiProperty &prop, /**< [out] the property to set up */
                    const std::string &name  /**< [in] the INDI property name */
    )
    {
        prop.setDevice( m_configName );
        prop.setName( name );
    }

    /// Free the frame buffer allocated (with malloc) by configureAcquisition or setFrame.
    void releaseFrameBuffer()
    {
        free( m_frameBuffer );
        m_frameBuffer = nullptr;
    }

    /// Load a frame into the frame buffer, as if read from the camera.
    void setFrame( uint32_t                     w, /**< [in] image width */
                   uint32_t                     h, /**< [in] image height */
                   const std::vector<uint16_t> &im /**< [in] the pixel values, w*h of them */
    )
    {
        releaseFrameBuffer();
        m_width       = w;
        m_height      = h;
        m_typeSize    = sizeof( uint16_t );
        m_frameBuffer = static_cast<U16 *>( malloc( w * h * sizeof( uint16_t ) ) );
        memcpy( m_frameBuffer, im.data(), w * h * sizeof( uint16_t ) );
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

/// A PixeLINK error code, with the high bit set.
const PXL_RETURN_CODE c_pxlError = ApiUnknownError;

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

/// Verify the pixelinkCtrl constructor defaults and stdCamera configuration flags.
/**
 * \ingroup pixelinkCtrl_unit_test
 */
TEST_CASE( "pixelinkCtrl constructor defaults", "[pixelinkCtrl]" )
{
    // clang-format off
    #ifdef PIXELINKCTRL_TEST_DOXYGEN_REF
    pixelinkCtrl::pixelinkCtrl();
    #endif
    // clang-format on

    resetPxlStub();

    pixelinkCtrl_test app( "campl" );

    REQUIRE( app.m_powerMgtEnabled == false );

    REQUIRE( app.m_default_x == Approx( 512 ) );
    REQUIRE( app.m_default_y == Approx( 512 ) );
    REQUIRE( app.m_default_w == 512 );
    REQUIRE( app.m_default_h == 512 );
    REQUIRE( app.m_default_bin_x == 1 );
    REQUIRE( app.m_default_bin_y == 1 );

    REQUIRE( app.m_full_w == 1936 );
    REQUIRE( app.m_full_h == 1464 );
    REQUIRE( app.m_full_x == Approx( 967.5 ) );
    REQUIRE( app.m_full_y == Approx( 731.5 ) );

    REQUIRE( app.m_maxEMGain == Approx( 100 ) );
    REQUIRE( app.m_expTimeSet == Approx( 1e-3 ) );
    REQUIRE( app.m_depth == 16 );
    REQUIRE( app.m_cameraHandle == nullptr );
    REQUIRE( app.m_streaming == true );
    REQUIRE( app.m_frameBuffer == nullptr );

    REQUIRE( pixelinkCtrl::c_stdCamera_tempControl == false );
    REQUIRE( pixelinkCtrl::c_stdCamera_temp == false );
    REQUIRE( pixelinkCtrl::c_stdCamera_readoutSpeed == false );
    REQUIRE( pixelinkCtrl::c_stdCamera_vShiftSpeed == false );
    REQUIRE( pixelinkCtrl::c_stdCamera_emGain == true );
    REQUIRE( pixelinkCtrl::c_stdCamera_blacklevel == false );
    REQUIRE( pixelinkCtrl::c_stdCamera_exptimeCtrl == true );
    REQUIRE( pixelinkCtrl::c_stdCamera_fpsCtrl == true );
    REQUIRE( pixelinkCtrl::c_stdCamera_fps == true );
    REQUIRE( pixelinkCtrl::c_stdCamera_synchro == false );
    REQUIRE( pixelinkCtrl::c_stdCamera_usesModes == false );
    REQUIRE( pixelinkCtrl::c_stdCamera_usesROI == true );
    REQUIRE( pixelinkCtrl::c_stdCamera_cropMode == false );
    REQUIRE( pixelinkCtrl::c_stdCamera_hasShutter == false );
    REQUIRE( pixelinkCtrl::c_stdCamera_usesStateString == false );
    REQUIRE( pixelinkCtrl::c_frameGrabber_flippable == true );
}

/// Verify pixelinkCtrl configuration defaults load from an empty config file.
/**
 * \ingroup pixelinkCtrl_unit_test
 */
TEST_CASE( "pixelinkCtrl configuration defaults", "[pixelinkCtrl][config]" )
{
    // clang-format off
    #ifdef PIXELINKCTRL_TEST_DOXYGEN_REF
    pixelinkCtrl::setupConfig();
    pixelinkCtrl::loadConfig();
    #endif
    // clang-format on

    resetPxlStub();

    pixelinkCtrl_test app( "campl" );

    const std::string path = "/tmp/pixelinkCtrl_test_defaults.conf";
    mx::app::writeConfigFile( path, { "none" }, { "nada" }, { "0" } );
    app.loadTestConfig( path );
    std::remove( path.c_str() );

    REQUIRE( app.m_maxEMGain == Approx( 100 ) );

    // stdCamera starts current and next ROI at the defaults
    REQUIRE( app.m_currentROI.x == Approx( 512 ) );
    REQUIRE( app.m_currentROI.y == Approx( 512 ) );
    REQUIRE( app.m_currentROI.w == 512 );
    REQUIRE( app.m_currentROI.h == 512 );
    REQUIRE( app.m_currentROI.bin_x == 1 );
    REQUIRE( app.m_currentROI.bin_y == 1 );
    REQUIRE( app.m_nextROI.x == Approx( 512 ) );
    REQUIRE( app.m_nextROI.w == 512 );

    // frameGrabber and telemeter defaults
    REQUIRE( app.m_shmimName == "campl" );
    REQUIRE( app.m_circBuffLength == 1 );
    REQUIRE( app.m_defaultFlip == dev::frameGrabber<pixelinkCtrl>::fgFlipNone );
    REQUIRE( app.m_maxInterval == Approx( 10.0 ) );
}

/// Verify pixelinkCtrl configuration overrides are loaded.
/**
 * \ingroup pixelinkCtrl_unit_test
 */
TEST_CASE( "pixelinkCtrl configuration overrides", "[pixelinkCtrl][config]" )
{
    // clang-format off
    #ifdef PIXELINKCTRL_TEST_DOXYGEN_REF
    pixelinkCtrl::setupConfig();
    pixelinkCtrl::loadConfig();
    #endif
    // clang-format on

    resetPxlStub();

    pixelinkCtrl_test app( "campl" );

    const std::string path = "/tmp/pixelinkCtrl_test_overrides.conf";
    mx::app::writeConfigFile( path,
                              { "camera",
                                "camera",
                                "camera",
                                "camera",
                                "camera",
                                "camera",
                                "camera",
                                "framegrabber",
                                "framegrabber",
                                "telemeter" },
                              { "maxEMGain",
                                "default_x",
                                "default_y",
                                "default_w",
                                "default_h",
                                "default_bin_x",
                                "default_bin_y",
                                "shmimName",
                                "defaultFlip",
                                "maxInterval" },
                              { "48", "100", "200", "256", "128", "2", "2", "pltest", "flipLR", "5" } );
    app.loadTestConfig( path );
    std::remove( path.c_str() );

    REQUIRE( app.m_maxEMGain == Approx( 48 ) );

    REQUIRE( app.m_default_x == Approx( 100 ) );
    REQUIRE( app.m_default_y == Approx( 200 ) );
    REQUIRE( app.m_default_w == 256 );
    REQUIRE( app.m_default_h == 128 );
    REQUIRE( app.m_default_bin_x == 2 );
    REQUIRE( app.m_default_bin_y == 2 );

    REQUIRE( app.m_currentROI.x == Approx( 100 ) );
    REQUIRE( app.m_currentROI.h == 128 );
    REQUIRE( app.m_nextROI.y == Approx( 200 ) );
    REQUIRE( app.m_nextROI.bin_x == 2 );

    REQUIRE( app.m_shmimName == "pltest" );
    REQUIRE( app.m_defaultFlip == dev::frameGrabber<pixelinkCtrl>::fgFlipLR );
    REQUIRE( app.m_maxInterval == Approx( 5.0 ) );
}

/// Verify powerOnDefaults() resets the current and next ROI to the defaults.
/**
 * \ingroup pixelinkCtrl_unit_test
 */
TEST_CASE( "pixelinkCtrl powerOnDefaults", "[pixelinkCtrl]" )
{
    // clang-format off
    #ifdef PIXELINKCTRL_TEST_DOXYGEN_REF
    pixelinkCtrl::powerOnDefaults();
    #endif
    // clang-format on

    resetPxlStub();

    pixelinkCtrl_test app( "campl" );

    app.m_currentROI.x = 10;
    app.m_currentROI.w = 10;
    app.m_nextROI.y    = 20;
    app.m_nextROI.h    = 20;

    REQUIRE( app.powerOnDefaults() == 0 );

    REQUIRE( app.m_currentROI.x == Approx( 512 ) );
    REQUIRE( app.m_currentROI.y == Approx( 512 ) );
    REQUIRE( app.m_currentROI.w == 512 );
    REQUIRE( app.m_currentROI.h == 512 );
    REQUIRE( app.m_currentROI.bin_x == 1 );
    REQUIRE( app.m_currentROI.bin_y == 1 );
    REQUIRE( app.m_nextROI.x == Approx( 512 ) );
    REQUIRE( app.m_nextROI.y == Approx( 512 ) );
    REQUIRE( app.m_nextROI.w == 512 );
    REQUIRE( app.m_nextROI.h == 512 );
    REQUIRE( app.m_nextROI.bin_x == 1 );
    REQUIRE( app.m_nextROI.bin_y == 1 );
}

/// Verify connect() opens the first camera, re-opens an open camera, and handles no camera.
/**
 * \ingroup pixelinkCtrl_unit_test
 */
TEST_CASE( "pixelinkCtrl connect", "[pixelinkCtrl][connect]" )
{
    // clang-format off
    #ifdef PIXELINKCTRL_TEST_DOXYGEN_REF
    pixelinkCtrl::connect();
    #endif
    // clang-format on

    resetPxlStub();

    pixelinkCtrl_test app( "campl" );

    SECTION( "camera found" )
    {
        REQUIRE( app.connect() == 0 );
        REQUIRE( app.state() == stateCodes::CONNECTED );
        REQUIRE( app.m_cameraHandle == g_pxlStub.handle );
        REQUIRE( g_pxlStub.initCalls == 1 );
        REQUIRE( g_pxlStub.lastSerial == 0 );
        REQUIRE( g_pxlStub.uninitCalls == 0 );
    }

    SECTION( "an open camera is closed first" )
    {
        HANDLE old         = reinterpret_cast<HANDLE>( static_cast<uintptr_t>( 0x999 ) );
        app.m_cameraHandle = old;

        REQUIRE( app.connect() == 0 );
        REQUIRE( g_pxlStub.uninitCalls == 1 );
        REQUIRE( g_pxlStub.lastUninit == old );
        REQUIRE( app.m_cameraHandle == g_pxlStub.handle );
        REQUIRE( app.state() == stateCodes::CONNECTED );
    }

    SECTION( "no camera" )
    {
        g_pxlStub.initReturn = c_pxlError;

        REQUIRE( app.connect() == 0 );
        REQUIRE( app.state() == stateCodes::NODEVICE );
        REQUIRE( app.m_cameraHandle == nullptr );
    }
}

/// Verify getAcquisitionState() maps the power, streaming and running state to the FSM state.
/**
 * \ingroup pixelinkCtrl_unit_test
 */
TEST_CASE( "pixelinkCtrl getAcquisitionState", "[pixelinkCtrl]" )
{
    // clang-format off
    #ifdef PIXELINKCTRL_TEST_DOXYGEN_REF
    pixelinkCtrl::getAcquisitionState();
    #endif
    // clang-format on

    resetPxlStub();

    pixelinkCtrl_test app( "campl" );
    app.state( stateCodes::CONNECTED );

    SECTION( "power off leaves the state" )
    {
        app.m_powerState = 0;
        app.m_running    = true;
        REQUIRE( app.getAcquisitionState() == 0 );
        REQUIRE( app.state() == stateCodes::CONNECTED );
    }

    SECTION( "not streaming leaves the state" )
    {
        app.m_streaming = false;
        app.m_running   = true;
        REQUIRE( app.getAcquisitionState() == 0 );
        REQUIRE( app.state() == stateCodes::CONNECTED );
    }

    SECTION( "running is OPERATING" )
    {
        app.m_running = true;
        REQUIRE( app.getAcquisitionState() == 0 );
        REQUIRE( app.state() == stateCodes::OPERATING );
    }

    SECTION( "not running is READY" )
    {
        app.m_running = false;
        REQUIRE( app.getAcquisitionState() == 0 );
        REQUIRE( app.state() == stateCodes::READY );
    }
}

/// Verify getTemps() reads the sensor and body temperatures, and handles SDK errors.
/**
 * \ingroup pixelinkCtrl_unit_test
 */
TEST_CASE( "pixelinkCtrl getTemps", "[pixelinkCtrl][temperature]" )
{
    // clang-format off
    #ifdef PIXELINKCTRL_TEST_DOXYGEN_REF
    pixelinkCtrl::getTemps();
    #endif
    // clang-format on

    resetPxlStub();

    pixelinkCtrl_test app( "campl" );
    app.state( stateCodes::READY );

    g_pxlStub.features[FEATURE_SENSOR_TEMPERATURE] = { 31.5 };
    g_pxlStub.features[FEATURE_BODY_TEMPERATURE]   = { 42.25 };

    SECTION( "success" )
    {
        REQUIRE( app.getTemps() == 0 );
        REQUIRE( app.m_sensorTemp == Approx( 31.5 ) );
        REQUIRE( app.m_bodyTemp == Approx( 42.25 ) );
        REQUIRE( app.state() == stateCodes::READY );
    }

    SECTION( "sensor temperature error" )
    {
        g_pxlStub.getReturns[FEATURE_SENSOR_TEMPERATURE] = c_pxlError;

        REQUIRE( app.getTemps() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( app.m_sensorTemp == Approx( -999 ) );
    }

    SECTION( "body temperature error" )
    {
        g_pxlStub.getReturns[FEATURE_BODY_TEMPERATURE] = c_pxlError;

        REQUIRE( app.getTemps() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( app.m_sensorTemp == Approx( 31.5 ) );
        REQUIRE( app.m_bodyTemp == Approx( -999 ) );
    }
}

/// Verify toggleStreaming() starts and stops the stream and handles SDK errors.
/**
 * \ingroup pixelinkCtrl_unit_test
 */
TEST_CASE( "pixelinkCtrl toggleStreaming", "[pixelinkCtrl][streaming]" )
{
    // clang-format off
    #ifdef PIXELINKCTRL_TEST_DOXYGEN_REF
    pixelinkCtrl::toggleStreaming();
    #endif
    // clang-format on

    resetPxlStub();

    pixelinkCtrl_test app( "campl" );

    SECTION( "stop a running stream" )
    {
        app.m_running = true;

        REQUIRE( app.toggleStreaming() == 0 );
        REQUIRE( g_pxlStub.streamStates == std::vector<U32>{ STOP_STREAM } );
        REQUIRE( app.m_running == false );
        REQUIRE( app.m_streaming == false );
        REQUIRE( app.state() == stateCodes::CONNECTED );
    }

    SECTION( "start a stopped stream" )
    {
        app.m_running   = false;
        app.m_streaming = false;

        REQUIRE( app.toggleStreaming() == 0 );
        REQUIRE( g_pxlStub.streamStates == std::vector<U32>{ START_STREAM } );
        REQUIRE( app.m_running == true );
        REQUIRE( app.m_streaming == true );
        REQUIRE( app.state() == stateCodes::OPERATING );
    }

    SECTION( "stop error" )
    {
        app.m_running                        = true;
        g_pxlStub.streamReturns[STOP_STREAM] = c_pxlError;

        REQUIRE( app.toggleStreaming() == -1 );
        REQUIRE( app.m_running == true );
        REQUIRE( app.state() == stateCodes::ERROR );
    }

    SECTION( "start error" )
    {
        app.m_running                         = false;
        g_pxlStub.streamReturns[START_STREAM] = c_pxlError;

        REQUIRE( app.toggleStreaming() == -1 );
        REQUIRE( app.m_running == false );
        REQUIRE( app.state() == stateCodes::ERROR );
    }
}

/// Verify setEMGain() sends the requested gain and reports the camera's value.
/**
 * \ingroup pixelinkCtrl_unit_test
 */
TEST_CASE( "pixelinkCtrl setEMGain", "[pixelinkCtrl][gain]" )
{
    // clang-format off
    #ifdef PIXELINKCTRL_TEST_DOXYGEN_REF
    pixelinkCtrl::setEMGain();
    #endif
    // clang-format on

    resetPxlStub();

    pixelinkCtrl_test app( "campl" );
    app.state( stateCodes::READY );
    app.m_emGainSet = 12.5;

    SECTION( "success" )
    {
        REQUIRE( app.setEMGain() == 0 );
        REQUIRE( g_pxlStub.lastSet( FEATURE_GAIN ) == std::vector<F32>{ 12.5 } );
        REQUIRE( g_pxlStub.lastSetFlags( FEATURE_GAIN ) == FEATURE_FLAG_MANUAL );
        REQUIRE( app.m_emGain == Approx( 12.5 ) );
    }

    SECTION( "camera adjusts the gain" )
    {
        g_pxlStub.readbackOverrides[FEATURE_GAIN] = { 12.0 };

        REQUIRE( app.setEMGain() == 0 );
        REQUIRE( app.m_emGain == Approx( 12.0 ) );
    }

    SECTION( "set error" )
    {
        g_pxlStub.setReturns[FEATURE_GAIN] = c_pxlError;
        app.m_emGain                       = 3;

        REQUIRE( app.setEMGain() == -1 );
        REQUIRE( app.m_emGain == Approx( 3 ) );
        REQUIRE( app.state() == stateCodes::READY );
    }

    SECTION( "readback error" )
    {
        g_pxlStub.getReturns[FEATURE_GAIN] = c_pxlError;

        REQUIRE( app.setEMGain() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
    }
}

/// Verify setExpTime() sends the requested exposure time and reports the camera's shutter value.
/**
 * \ingroup pixelinkCtrl_unit_test
 */
TEST_CASE( "pixelinkCtrl setExpTime", "[pixelinkCtrl][exposure]" )
{
    // clang-format off
    #ifdef PIXELINKCTRL_TEST_DOXYGEN_REF
    pixelinkCtrl::setExpTime();
    #endif
    // clang-format on

    resetPxlStub();

    pixelinkCtrl_test app( "campl" );
    app.state( stateCodes::READY );
    app.m_expTimeSet = 0.02;

    SECTION( "success" )
    {
        REQUIRE( app.setExpTime() == 0 );
        REQUIRE( g_pxlStub.setCount( FEATURE_EXPOSURE ) == 1 );
        REQUIRE( g_pxlStub.lastSet( FEATURE_EXPOSURE )[0] == Approx( 0.02 ) );
        REQUIRE( g_pxlStub.lastSetFlags( FEATURE_EXPOSURE ) == FEATURE_FLAG_MANUAL );
        REQUIRE( app.m_expTime == Approx( 0.02 ) );
    }

    SECTION( "camera adjusts the exposure" )
    {
        g_pxlStub.readbackOverrides[FEATURE_SHUTTER] = { 0.019 };

        REQUIRE( app.setExpTime() == 0 );
        REQUIRE( app.m_expTime == Approx( 0.019 ) );
    }

    SECTION( "set error" )
    {
        g_pxlStub.setReturns[FEATURE_EXPOSURE] = c_pxlError;

        REQUIRE( app.setExpTime() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
    }

    SECTION( "readback error" )
    {
        g_pxlStub.getReturns[FEATURE_SHUTTER] = c_pxlError;
        app.m_expTime                         = 1;

        REQUIRE( app.setExpTime() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( app.m_expTime == Approx( 1 ) );
    }
}

/// Verify setFPS() and fps(), including that setFPS() sends the current m_fps rather than m_fpsSet.
/**
 * \ingroup pixelinkCtrl_unit_test
 */
TEST_CASE( "pixelinkCtrl setFPS", "[pixelinkCtrl][fps]" )
{
    // clang-format off
    #ifdef PIXELINKCTRL_TEST_DOXYGEN_REF
    pixelinkCtrl::setFPS();
    pixelinkCtrl::fps();
    #endif
    // clang-format on

    resetPxlStub();

    pixelinkCtrl_test app( "campl" );
    app.m_fps    = 30;
    app.m_fpsSet = 60;

    REQUIRE( app.fps() == Approx( 30 ) );

    SECTION( "success" )
    {
        REQUIRE( app.setFPS() == 0 );
        REQUIRE( g_pxlStub.lastSetFlags( FEATURE_FRAME_RATE ) == FEATURE_FLAG_MANUAL );

        // Current behavior: the value sent is m_fps, not the requested m_fpsSet
        REQUIRE( g_pxlStub.lastSet( FEATURE_FRAME_RATE ) == std::vector<F32>{ 30 } );
        REQUIRE( app.m_fps == Approx( 30 ) );
    }

    SECTION( "error" )
    {
        g_pxlStub.setReturns[FEATURE_FRAME_RATE] = c_pxlError;

        REQUIRE( app.setFPS() == -1 );
    }
}

/// Verify checkNextROI() accepts the next ROI unchanged and setNextROI() triggers a reconfiguration.
/**
 * \ingroup pixelinkCtrl_unit_test
 */
TEST_CASE( "pixelinkCtrl checkNextROI and setNextROI", "[pixelinkCtrl][roi]" )
{
    // clang-format off
    #ifdef PIXELINKCTRL_TEST_DOXYGEN_REF
    pixelinkCtrl::checkNextROI();
    pixelinkCtrl::setNextROI();
    #endif
    // clang-format on

    resetPxlStub();

    pixelinkCtrl_test app( "campl" );

    app.m_nextROI.x     = 13;
    app.m_nextROI.y     = 17;
    app.m_nextROI.w     = 101;
    app.m_nextROI.h     = 99;
    app.m_nextROI.bin_x = 3;
    app.m_nextROI.bin_y = 5;

    REQUIRE( app.checkNextROI() == 0 );
    REQUIRE( app.m_nextROI.x == Approx( 13 ) );
    REQUIRE( app.m_nextROI.y == Approx( 17 ) );
    REQUIRE( app.m_nextROI.w == 101 );
    REQUIRE( app.m_nextROI.h == 99 );
    REQUIRE( app.m_nextROI.bin_x == 3 );
    REQUIRE( app.m_nextROI.bin_y == 5 );

    app.m_reconfig = false;
    REQUIRE( app.setNextROI() == 0 );
    REQUIRE( app.m_reconfig == true );

    // Nothing is sent to the camera until the framegrabber reconfigures
    REQUIRE( g_pxlStub.setCalls.empty() );
}

/// Verify configureAcquisition() programs exposure, gain, format, ROI and binning, and reads back the ROI.
/**
 * \ingroup pixelinkCtrl_unit_test
 */
TEST_CASE( "pixelinkCtrl configureAcquisition", "[pixelinkCtrl][roi]" )
{
    // clang-format off
    #ifdef PIXELINKCTRL_TEST_DOXYGEN_REF
    pixelinkCtrl::configureAcquisition();
    #endif
    // clang-format on

    resetPxlStub();

    pixelinkCtrl_test app( "campl" );
    app.state( stateCodes::READY );

    app.m_expTimeSet    = 0.01;
    app.m_emGainSet     = 4;
    app.m_nextROI.x     = 100;
    app.m_nextROI.y     = 200;
    app.m_nextROI.w     = 256;
    app.m_nextROI.h     = 128;
    app.m_nextROI.bin_x = 2;
    app.m_nextROI.bin_y = 2;
    app.m_running       = true;

    SECTION( "success" )
    {
        REQUIRE( app.configureAcquisition() == 0 );

        // start (to allow a stop), stop, then start continuous acquisition
        REQUIRE( g_pxlStub.streamStates == std::vector<U32>{ START_STREAM, STOP_STREAM, START_STREAM } );
        REQUIRE( app.m_running == false );

        REQUIRE( g_pxlStub.lastSet( FEATURE_EXPOSURE )[0] == Approx( 0.01 ) );
        REQUIRE( app.m_expTime == Approx( 0.01 ) );
        REQUIRE( g_pxlStub.lastSet( FEATURE_GAIN ) == std::vector<F32>{ 4 } );
        REQUIRE( app.m_emGain == Approx( 4 ) );

        REQUIRE( g_pxlStub.lastSet( FEATURE_PIXEL_FORMAT ) == std::vector<F32>{ PIXEL_FORMAT_MONO16 } );

        std::vector<F32> roi = g_pxlStub.lastSet( FEATURE_ROI );
        REQUIRE( roi.size() == FEATURE_ROI_NUM_PARAMS );
        REQUIRE( roi[FEATURE_ROI_PARAM_LEFT] == Approx( 100 ) );
        REQUIRE( roi[FEATURE_ROI_PARAM_TOP] == Approx( 200 ) );
        REQUIRE( roi[FEATURE_ROI_PARAM_WIDTH] == Approx( 256 ) );
        REQUIRE( roi[FEATURE_ROI_PARAM_HEIGHT] == Approx( 128 ) );

        std::vector<F32> bin = g_pxlStub.lastSet( FEATURE_PIXEL_ADDRESSING );
        REQUIRE( bin.size() == FEATURE_PIXEL_ADDRESSING_NUM_PARAMS );
        REQUIRE( bin[FEATURE_PIXEL_ADDRESSING_PARAM_VALUE] == Approx( 2 ) );
        REQUIRE( bin[FEATURE_PIXEL_ADDRESSING_PARAM_MODE] == Approx( PIXEL_ADDRESSING_MODE_BIN ) );
        REQUIRE( bin[FEATURE_PIXEL_ADDRESSING_PARAM_X_VALUE] == Approx( 2 ) );
        REQUIRE( bin[FEATURE_PIXEL_ADDRESSING_PARAM_Y_VALUE] == Approx( 2 ) );

        REQUIRE( app.m_currentROI.x == Approx( 100 ) );
        REQUIRE( app.m_currentROI.y == Approx( 200 ) );
        REQUIRE( app.m_currentROI.w == 256 );
        REQUIRE( app.m_currentROI.h == 128 );
        REQUIRE( app.m_currentROI.bin_x == 2 );
        REQUIRE( app.m_currentROI.bin_y == 2 );

        REQUIRE( app.m_width == 128 );
        REQUIRE( app.m_height == 64 );
        REQUIRE( app.m_dataType == _DATATYPE_UINT16 );
        REQUIRE( app.m_frameBuffer != nullptr );
        REQUIRE( app.state() == stateCodes::READY );
    }

    SECTION( "camera adjusts the ROI and binning" )
    {
        g_pxlStub.readbackOverrides[FEATURE_ROI]              = { 96, 192, 240, 120 };
        g_pxlStub.readbackOverrides[FEATURE_PIXEL_ADDRESSING] = { 4, PIXEL_ADDRESSING_MODE_BIN, 4, 2 };

        REQUIRE( app.configureAcquisition() == 0 );

        REQUIRE( app.m_currentROI.x == Approx( 96 ) );
        REQUIRE( app.m_currentROI.y == Approx( 192 ) );
        REQUIRE( app.m_currentROI.w == 240 );
        REQUIRE( app.m_currentROI.h == 120 );
        REQUIRE( app.m_currentROI.bin_x == 4 );
        REQUIRE( app.m_currentROI.bin_y == 2 );
        REQUIRE( app.m_width == 60 );
        REQUIRE( app.m_height == 60 );
    }

    SECTION( "stop stream error" )
    {
        g_pxlStub.streamReturns[STOP_STREAM] = c_pxlError;

        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( g_pxlStub.setCalls.empty() );
    }

    SECTION( "pixel format error" )
    {
        g_pxlStub.setReturns[FEATURE_PIXEL_FORMAT] = c_pxlError;

        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( g_pxlStub.setCount( FEATURE_ROI ) == 0 );
    }

    SECTION( "ROI error is not fatal" )
    {
        g_pxlStub.setReturns[FEATURE_ROI]        = c_pxlError;
        g_pxlStub.readbackOverrides[FEATURE_ROI] = { 0, 0, 512, 512 };

        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( g_pxlStub.setCount( FEATURE_PIXEL_ADDRESSING ) == 1 );
        REQUIRE( app.m_currentROI.w == 512 );
        REQUIRE( app.m_width == 256 );
    }

    SECTION( "binning error" )
    {
        g_pxlStub.setReturns[FEATURE_PIXEL_ADDRESSING] = c_pxlError;

        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
    }

    SECTION( "ROI readback error" )
    {
        g_pxlStub.getReturns[FEATURE_ROI] = c_pxlError;

        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
    }

    SECTION( "binning readback error" )
    {
        g_pxlStub.getReturns[FEATURE_PIXEL_ADDRESSING] = c_pxlError;

        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
    }

    SECTION( "start stream error" )
    {
        // The first START_STREAM result is ignored, the final one is checked
        g_pxlStub.streamReturns[START_STREAM] = c_pxlError;

        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( app.m_frameBuffer != nullptr );
    }
}

/// Verify startAcquisition() and reconfig() start and stop the stream.
/**
 * \ingroup pixelinkCtrl_unit_test
 */
TEST_CASE( "pixelinkCtrl startAcquisition and reconfig", "[pixelinkCtrl][framegrabber]" )
{
    // clang-format off
    #ifdef PIXELINKCTRL_TEST_DOXYGEN_REF
    pixelinkCtrl::startAcquisition();
    pixelinkCtrl::reconfig();
    #endif
    // clang-format on

    resetPxlStub();

    pixelinkCtrl_test app( "campl" );
    app.state( stateCodes::READY );

    SECTION( "start" )
    {
        REQUIRE( app.startAcquisition() == 0 );
        REQUIRE( g_pxlStub.streamStates == std::vector<U32>{ START_STREAM } );
        REQUIRE( app.m_running == true );
        REQUIRE( app.state() == stateCodes::OPERATING );
    }

    SECTION( "start error" )
    {
        g_pxlStub.streamReturns[START_STREAM] = c_pxlError;

        REQUIRE( app.startAcquisition() == -1 );
        REQUIRE( app.m_running == false );
        REQUIRE( app.state() == stateCodes::ERROR );
    }

    SECTION( "reconfig" )
    {
        app.m_running = true;

        REQUIRE( app.reconfig() == 0 );
        REQUIRE( g_pxlStub.streamStates == std::vector<U32>{ STOP_STREAM } );
        REQUIRE( app.m_running == false );
    }

    SECTION( "reconfig error" )
    {
        app.m_running                        = true;
        g_pxlStub.streamReturns[STOP_STREAM] = c_pxlError;

        REQUIRE( app.reconfig() == -1 );
        REQUIRE( app.m_running == true );
        REQUIRE( app.state() == stateCodes::ERROR );
    }
}

/// Verify acquireAndCheckValid() reads the next frame and time stamps it.
/**
 * \ingroup pixelinkCtrl_unit_test
 */
TEST_CASE( "pixelinkCtrl acquireAndCheckValid", "[pixelinkCtrl][framegrabber]" )
{
    // clang-format off
    #ifdef PIXELINKCTRL_TEST_DOXYGEN_REF
    pixelinkCtrl::acquireAndCheckValid();
    #endif
    // clang-format on

    resetPxlStub();

    pixelinkCtrl_test app( "campl" );
    app.setFrame( 2, 2, { 0, 0, 0, 0 } );
    app.m_currImageTimestamp = { 0, 0 };

    SECTION( "success" )
    {
        g_pxlStub.frameData = { 1, 2, 3, 4 };

        REQUIRE( app.acquireAndCheckValid() == 0 );
        REQUIRE( g_pxlStub.frameCalls == 1 );
        REQUIRE( g_pxlStub.lastDescSize == sizeof( FRAME_DESC ) );
        REQUIRE( g_pxlStub.lastBufferSize == 512 * 512 * 2 ); // fixed size, independent of the ROI
        REQUIRE( app.m_frameBuffer[0] == 1 );
        REQUIRE( app.m_frameBuffer[3] == 4 );
        REQUIRE( app.m_currImageTimestamp.tv_sec > 0 );
    }

    SECTION( "error" )
    {
        g_pxlStub.frameReturn = c_pxlError;

        REQUIRE( app.acquireAndCheckValid() == -1 );
        REQUIRE( app.m_currImageTimestamp.tv_sec == 0 );
    }
}

/// Verify loadImageIntoStream() byte-swaps the big-endian frame and applies the configured flip.
/**
 * \ingroup pixelinkCtrl_unit_test
 */
TEST_CASE( "pixelinkCtrl loadImageIntoStream", "[pixelinkCtrl][framegrabber]" )
{
    // clang-format off
    #ifdef PIXELINKCTRL_TEST_DOXYGEN_REF
    pixelinkCtrl::loadImageIntoStream(nullptr);
    #endif
    // clang-format on

    resetPxlStub();

    pixelinkCtrl_test app( "campl" );

    // 3 wide by 2 high, big-endian values 0x0001..0x0006
    app.setFrame( 3, 2, { 0x0100, 0x0200, 0x0300, 0x0400, 0x0500, 0x0600 } );

    std::vector<uint16_t> dest( 6, 0 );

    SECTION( "no flip" )
    {
        app.m_defaultFlip = dev::frameGrabber<pixelinkCtrl>::fgFlipNone;
        REQUIRE( app.loadImageIntoStream( dest.data() ) == 0 );

        std::vector<uint16_t> expected = { 1, 2, 3, 4, 5, 6 };
        REQUIRE( dest == expected );

        // The frame buffer is swapped in place
        REQUIRE( app.m_frameBuffer[0] == 1 );
    }

    SECTION( "flip up-down" )
    {
        app.m_defaultFlip = dev::frameGrabber<pixelinkCtrl>::fgFlipUD;
        REQUIRE( app.loadImageIntoStream( dest.data() ) == 0 );

        std::vector<uint16_t> expected = { 4, 5, 6, 1, 2, 3 };
        REQUIRE( dest == expected );
    }

    SECTION( "invalid flip is an error" )
    {
        app.m_defaultFlip = 99;
        REQUIRE( app.loadImageIntoStream( dest.data() ) == -1 );
    }
}

/// Verify onPowerOff(), whilePowerOff() and appShutdown() stop and release an open camera.
/**
 * \ingroup pixelinkCtrl_unit_test
 */
TEST_CASE( "pixelinkCtrl power off and shutdown", "[pixelinkCtrl]" )
{
    // clang-format off
    #ifdef PIXELINKCTRL_TEST_DOXYGEN_REF
    pixelinkCtrl::onPowerOff();
    pixelinkCtrl::whilePowerOff();
    pixelinkCtrl::appShutdown();
    #endif
    // clang-format on

    resetPxlStub();

    pixelinkCtrl_test app( "campl" );

    SECTION( "onPowerOff with an open camera" )
    {
        app.m_cameraHandle = g_pxlStub.handle;

        REQUIRE( app.onPowerOff() == 0 );
        REQUIRE( g_pxlStub.streamStates == std::vector<U32>{ STOP_STREAM } );
        REQUIRE( g_pxlStub.uninitCalls == 1 );
        REQUIRE( g_pxlStub.lastUninit == g_pxlStub.handle );
        REQUIRE( app.m_cameraHandle == nullptr );
    }

    SECTION( "onPowerOff with no camera" )
    {
        REQUIRE( app.onPowerOff() == 0 );
        REQUIRE( g_pxlStub.streamStates.empty() );
        REQUIRE( g_pxlStub.uninitCalls == 0 );
    }

    SECTION( "whilePowerOff" )
    {
        REQUIRE( app.whilePowerOff() == 0 );
    }

    SECTION( "appShutdown with an open camera" )
    {
        app.m_cameraHandle = g_pxlStub.handle;

        REQUIRE( app.appShutdown() == 0 );
        REQUIRE( g_pxlStub.streamStates == std::vector<U32>{ STOP_STREAM } );
        REQUIRE( g_pxlStub.uninitCalls == 1 );
        REQUIRE( app.m_cameraHandle == nullptr );
    }
}

/// Verify the streaming switch callback validates the name and toggles streaming.
/**
 * \ingroup pixelinkCtrl_unit_test
 */
TEST_CASE( "pixelinkCtrl streaming INDI callback", "[pixelinkCtrl][indi]" )
{
    // clang-format off
    #ifdef PIXELINKCTRL_TEST_DOXYGEN_REF
    pixelinkCtrl::newCallBack_m_indiP_streamSwitch(pcf::IndiProperty());
    #endif
    // clang-format on

    resetPxlStub();

    pixelinkCtrl_test app( "campl" );

    SECTION( "wrong name" )
    {
        REQUIRE( app.newCallBack_m_indiP_streamSwitch(
                     switchProp( "campl", "wrong", "toggle", pcf::IndiElement::On ) ) == -1 );
        REQUIRE( g_pxlStub.streamStates.empty() );
    }

    SECTION( "toggle starts a stopped stream" )
    {
        app.m_running = false;

        REQUIRE( app.newCallBack_m_indiP_streamSwitch(
                     switchProp( "campl", "streaming", "toggle", pcf::IndiElement::On ) ) == 0 );
        REQUIRE( app.m_running == true );
        REQUIRE( g_pxlStub.streamStates == std::vector<U32>{ START_STREAM } );
    }

    SECTION( "toggle stops a running stream, regardless of the switch state" )
    {
        app.m_running = true;

        REQUIRE( app.newCallBack_m_indiP_streamSwitch(
                     switchProp( "campl", "streaming", "toggle", pcf::IndiElement::On ) ) == 0 );
        REQUIRE( app.m_running == false );
        REQUIRE( g_pxlStub.streamStates == std::vector<U32>{ STOP_STREAM } );
    }
}

/// Verify the stdCamera exposure, gain, fps and ROI callbacks reach the pixelinkCtrl hooks.
/**
 * \ingroup pixelinkCtrl_unit_test
 */
TEST_CASE( "pixelinkCtrl stdCamera INDI callbacks", "[pixelinkCtrl][indi]" )
{
    // clang-format off
    #ifdef PIXELINKCTRL_TEST_DOXYGEN_REF
    pixelinkCtrl::newCallBack_stdCamera(pcf::IndiProperty());
    pixelinkCtrl::setExpTime();
    pixelinkCtrl::setEMGain();
    pixelinkCtrl::setFPS();
    pixelinkCtrl::setNextROI();
    #endif
    // clang-format on

    resetPxlStub();

    pixelinkCtrl_test app( "campl" );
    app.state( stateCodes::READY );

    SECTION( "wrong device" )
    {
        REQUIRE( app.newCallBack_stdCamera( numberProp( "other", "exptime", "target", 0.5 ) ) == -1 );
        REQUIRE( g_pxlStub.setCalls.empty() );
    }

    SECTION( "unknown property" )
    {
        REQUIRE( app.newCallBack_stdCamera( numberProp( "campl", "wrong", "target", 0.5 ) ) == -1 );
    }

    SECTION( "blacklevel is not exposed" )
    {
        REQUIRE( app.newCallBack_stdCamera( numberProp( "campl", "blacklevel", "target", 5 ) ) == -1 );
        REQUIRE( g_pxlStub.setCalls.empty() );
    }

    SECTION( "exptime" )
    {
        REQUIRE( app.newCallBack_stdCamera( numberProp( "campl", "exptime", "target", 0.005 ) ) == 0 );
        REQUIRE( app.m_expTimeSet == Approx( 0.005 ) );
        REQUIRE( g_pxlStub.lastSet( FEATURE_EXPOSURE )[0] == Approx( 0.005 ) );
        REQUIRE( app.m_expTime == Approx( 0.005 ) );
    }

    SECTION( "exptime with no value is rejected" )
    {
        REQUIRE( app.newCallBack_stdCamera( numberProp( "campl", "exptime", "other", 0.005 ) ) == -1 );
        REQUIRE( g_pxlStub.setCalls.empty() );
    }

    SECTION( "emgain" )
    {
        REQUIRE( app.newCallBack_stdCamera( numberProp( "campl", "emgain", "target", 6 ) ) == 0 );
        REQUIRE( app.m_emGainSet == Approx( 6 ) );
        REQUIRE( g_pxlStub.lastSet( FEATURE_GAIN ) == std::vector<F32>{ 6 } );
        REQUIRE( app.m_emGain == Approx( 6 ) );
    }

    SECTION( "fps" )
    {
        app.m_fps = 25;

        REQUIRE( app.newCallBack_stdCamera( numberProp( "campl", "fps", "target", 50 ) ) == 0 );
        REQUIRE( app.m_fpsSet == Approx( 50 ) );

        // Current behavior: setFPS sends m_fps, not m_fpsSet
        REQUIRE( g_pxlStub.lastSet( FEATURE_FRAME_RATE ) == std::vector<F32>{ 25 } );
    }

    SECTION( "roi_region_x" )
    {
        REQUIRE( app.newCallBack_stdCamera( numberProp( "campl", "roi_region_x", "target", 321 ) ) == 0 );
        REQUIRE( app.m_nextROI.x == Approx( 321 ) );
    }

    SECTION( "roi_set" )
    {
        app.m_currentROI.x = 11;
        app.m_currentROI.w = 22;
        app.m_reconfig     = false;

        REQUIRE( app.newCallBack_stdCamera( switchProp( "campl", "roi_set", "request", pcf::IndiElement::On ) ) == 0 );
        REQUIRE( app.m_reconfig == true );
        REQUIRE( app.m_lastROI.x == Approx( 11 ) );
        REQUIRE( app.m_lastROI.w == 22 );
    }
}

/// Verify the telemetry interface records the camera state.
/**
 * \ingroup pixelinkCtrl_unit_test
 */
TEST_CASE( "pixelinkCtrl telemetry", "[pixelinkCtrl][telemetry]" )
{
    // clang-format off
    #ifdef PIXELINKCTRL_TEST_DOXYGEN_REF
    pixelinkCtrl::recordTelem(nullptr);
    pixelinkCtrl::checkRecordTimes();
    #endif
    // clang-format on

    resetPxlStub();

    pixelinkCtrl_test app( "campl" );

    REQUIRE( app.recordTelem( nullptr ) == 0 );
    REQUIRE( app.checkRecordTimes() == 0 );
}

} // namespace pixelinkCtrlTest

} // namespace libXWCTest
