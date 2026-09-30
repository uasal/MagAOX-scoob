/** \file asiCtrl_test.cpp
 * \brief Catch2 tests for the asiCtrl app.
 * \author Claude Code
 *
 * \ingroup asiCtrl_files
 */

#include "../../../tests/testXWC.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

#define protected public
#include "../asiCtrl.hpp"
#undef protected

using namespace MagAOX::app;

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
namespace
{

/// Fake ZWO ASI SDK state shared by the stub functions below.
struct asiStubState
{
    int numCameras{ 0 }; ///< Value returned by ASIGetNumOfConnectedCameras.

    std::vector<std::string> cameraNames; ///< Camera names returned by ASIGetCameraProperty, by index.

    std::map<int, long> controlValues; ///< Stored control values, keyed by ASI_CONTROL_TYPE.

    std::map<int, long> readbackOverrides; ///< Values returned by ASIGetControlValue in place of the stored ones.

    std::map<int, ASI_ERROR_CODE> setControlReturns; ///< Forced ASISetControlValue return codes, by control.

    std::vector<std::pair<int, long>> setControlCalls; ///< Every (control, value) pair sent to ASISetControlValue.

    int getPropertyCalls{ 0 }; ///< Number of ASIGetCameraProperty calls.

    int openCalls{ 0 }; ///< Number of ASIOpenCamera calls.

    int initCalls{ 0 }; ///< Number of ASIInitCamera calls.

    int closeCalls{ 0 }; ///< Number of ASICloseCamera calls.

    int startCalls{ 0 }; ///< Number of ASIStartVideoCapture calls.

    int stopCalls{ 0 }; ///< Number of ASIStopVideoCapture calls.

    int videoCalls{ 0 }; ///< Number of ASIGetVideoData calls.

    int lastOpenID{ -1 }; ///< Camera ID passed to the last ASIOpenCamera.

    int lastInitID{ -1 }; ///< Camera ID passed to the last ASIInitCamera.

    int lastCloseID{ -1 }; ///< Camera ID passed to the last ASICloseCamera.

    int roiWidth{ 0 }; ///< ROI width stored by ASISetROIFormat.

    int roiHeight{ 0 }; ///< ROI height stored by ASISetROIFormat.

    int roiBin{ 1 }; ///< ROI binning stored by ASISetROIFormat.

    ASI_IMG_TYPE roiImgType{ ASI_IMG_RAW8 }; ///< Image type stored by ASISetROIFormat.

    int startX{ 0 }; ///< ROI start x stored by ASISetStartPos.

    int startY{ 0 }; ///< ROI start y stored by ASISetStartPos.

    bool roiReadbackOverride{ false }; ///< If true, ASIGetROIFormat/ASIGetStartPos return the read* values below.

    int readWidth{ 0 }; ///< ROI width reported when roiReadbackOverride is true.

    int readHeight{ 0 }; ///< ROI height reported when roiReadbackOverride is true.

    int readBin{ 1 }; ///< ROI binning reported when roiReadbackOverride is true.

    int readStartX{ 0 }; ///< ROI start x reported when roiReadbackOverride is true.

    int readStartY{ 0 }; ///< ROI start y reported when roiReadbackOverride is true.

    ASI_ERROR_CODE setROIReturn{ ASI_SUCCESS }; ///< Forced ASISetROIFormat return code.

    ASI_ERROR_CODE setStartPosReturn{ ASI_SUCCESS }; ///< Forced ASISetStartPos return code.

    long lastBuffSize{ 0 }; ///< Buffer size passed to the last ASIGetVideoData.

    int lastWaitms{ 0 }; ///< Timeout passed to the last ASIGetVideoData.

    unsigned char videoFill{ 0 }; ///< Byte value ASIGetVideoData writes into the buffer.

    /// Get the last value sent to a control, or -99999 if it was never set.
    long lastSet( ASI_CONTROL_TYPE ctrl /**< [in] the control to look up */ ) const
    {
        for( auto it = setControlCalls.rbegin(); it != setControlCalls.rend(); ++it )
        {
            if( it->first == ctrl )
            {
                return it->second;
            }
        }

        return -99999;
    }

    /// Get the number of times a control was set.
    int setCount( ASI_CONTROL_TYPE ctrl /**< [in] the control to count */ ) const
    {
        int n = 0;
        for( const auto &c : setControlCalls )
        {
            if( c.first == ctrl )
            {
                ++n;
            }
        }

        return n;
    }
};

/// The global fake ASI SDK state.
asiStubState g_asiStub;

/// Reset the fake ASI SDK state before a test.
void resetAsiStub()
{
    g_asiStub = asiStubState();
}

/// A negative error code, the only kind of failure asiCtrl detects.
const ASI_ERROR_CODE c_asiNegativeError = static_cast<ASI_ERROR_CODE>( -1 );

} // namespace

extern "C"
{

    int ASIGetNumOfConnectedCameras()
    {
        return g_asiStub.numCameras;
    }

    ASI_ERROR_CODE ASIGetCameraProperty( ASI_CAMERA_INFO *pASICameraInfo, int iCameraIndex )
    {
        ++g_asiStub.getPropertyCalls;

        if( iCameraIndex < 0 || iCameraIndex >= static_cast<int>( g_asiStub.cameraNames.size() ) )
        {
            return ASI_ERROR_INVALID_INDEX;
        }

        memset( pASICameraInfo->Name, 0, sizeof( pASICameraInfo->Name ) );
        strncpy(
            pASICameraInfo->Name, g_asiStub.cameraNames[iCameraIndex].c_str(), sizeof( pASICameraInfo->Name ) - 1 );
        pASICameraInfo->CameraID = iCameraIndex;

        return ASI_SUCCESS;
    }

    ASI_ERROR_CODE ASIOpenCamera( int iCameraID )
    {
        ++g_asiStub.openCalls;
        g_asiStub.lastOpenID = iCameraID;
        return ASI_SUCCESS;
    }

    ASI_ERROR_CODE ASIInitCamera( int iCameraID )
    {
        ++g_asiStub.initCalls;
        g_asiStub.lastInitID = iCameraID;
        return ASI_SUCCESS;
    }

    ASI_ERROR_CODE ASICloseCamera( int iCameraID )
    {
        ++g_asiStub.closeCalls;
        g_asiStub.lastCloseID = iCameraID;
        return ASI_SUCCESS;
    }

    ASI_ERROR_CODE ASIGetControlValue( int iCameraID, ASI_CONTROL_TYPE ControlType, long *plValue, ASI_BOOL *pbAuto )
    {
        static_cast<void>( iCameraID );

        if( g_asiStub.readbackOverrides.count( ControlType ) > 0 )
        {
            *plValue = g_asiStub.readbackOverrides[ControlType];
        }
        else if( g_asiStub.controlValues.count( ControlType ) > 0 )
        {
            *plValue = g_asiStub.controlValues[ControlType];
        }
        else
        {
            *plValue = 0;
        }

        *pbAuto = ASI_FALSE;

        return ASI_SUCCESS;
    }

    ASI_ERROR_CODE ASISetControlValue( int iCameraID, ASI_CONTROL_TYPE ControlType, long lValue, ASI_BOOL bAuto )
    {
        static_cast<void>( iCameraID );
        static_cast<void>( bAuto );

        g_asiStub.setControlCalls.push_back( { ControlType, lValue } );

        if( g_asiStub.setControlReturns.count( ControlType ) > 0 )
        {
            return g_asiStub.setControlReturns[ControlType];
        }

        g_asiStub.controlValues[ControlType] = lValue;

        return ASI_SUCCESS;
    }

    ASI_ERROR_CODE ASISetROIFormat( int iCameraID, int iWidth, int iHeight, int iBin, ASI_IMG_TYPE Img_type )
    {
        static_cast<void>( iCameraID );

        if( g_asiStub.setROIReturn != ASI_SUCCESS )
        {
            return g_asiStub.setROIReturn;
        }

        g_asiStub.roiWidth   = iWidth;
        g_asiStub.roiHeight  = iHeight;
        g_asiStub.roiBin     = iBin;
        g_asiStub.roiImgType = Img_type;

        return ASI_SUCCESS;
    }

    ASI_ERROR_CODE ASIGetROIFormat( int iCameraID, int *piWidth, int *piHeight, int *piBin, ASI_IMG_TYPE *pImg_type )
    {
        static_cast<void>( iCameraID );

        *piWidth   = g_asiStub.roiReadbackOverride ? g_asiStub.readWidth : g_asiStub.roiWidth;
        *piHeight  = g_asiStub.roiReadbackOverride ? g_asiStub.readHeight : g_asiStub.roiHeight;
        *piBin     = g_asiStub.roiReadbackOverride ? g_asiStub.readBin : g_asiStub.roiBin;
        *pImg_type = g_asiStub.roiImgType;

        return ASI_SUCCESS;
    }

    ASI_ERROR_CODE ASISetStartPos( int iCameraID, int iStartX, int iStartY )
    {
        static_cast<void>( iCameraID );

        if( g_asiStub.setStartPosReturn != ASI_SUCCESS )
        {
            return g_asiStub.setStartPosReturn;
        }

        g_asiStub.startX = iStartX;
        g_asiStub.startY = iStartY;

        return ASI_SUCCESS;
    }

    ASI_ERROR_CODE ASIGetStartPos( int iCameraID, int *piStartX, int *piStartY )
    {
        static_cast<void>( iCameraID );

        *piStartX = g_asiStub.roiReadbackOverride ? g_asiStub.readStartX : g_asiStub.startX;
        *piStartY = g_asiStub.roiReadbackOverride ? g_asiStub.readStartY : g_asiStub.startY;

        return ASI_SUCCESS;
    }

    ASI_ERROR_CODE ASIStartVideoCapture( int iCameraID )
    {
        static_cast<void>( iCameraID );
        ++g_asiStub.startCalls;
        return ASI_SUCCESS;
    }

    ASI_ERROR_CODE ASIStopVideoCapture( int iCameraID )
    {
        static_cast<void>( iCameraID );
        ++g_asiStub.stopCalls;
        return ASI_SUCCESS;
    }

    ASI_ERROR_CODE ASIGetVideoData( int iCameraID, unsigned char *pBuffer, long lBuffSize, int iWaitms )
    {
        static_cast<void>( iCameraID );

        ++g_asiStub.videoCalls;
        g_asiStub.lastBuffSize = lBuffSize;
        g_asiStub.lastWaitms   = iWaitms;

        if( pBuffer != nullptr && lBuffSize > 0 )
        {
            memset( pBuffer, g_asiStub.videoFill, lBuffSize );
        }

        return ASI_SUCCESS;
    }

} // extern "C"
/// \endcond

namespace libXWCTest
{

/** \defgroup asiCtrl_unit_test asiCtrl Unit Tests
 * \brief Unit tests for the asiCtrl application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `asiCtrl` unit tests.
/** \ingroup asiCtrl_unit_test
 */
namespace asiCtrlTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness which initializes the asiCtrl state the constructor leaves unset.
class asiCtrl_test : public asiCtrl
{
  public:
    /// Construct a harness with the given device name.
    explicit asiCtrl_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        // asiCtrl() leaves these uninitialized
        m_camNum  = -1;
        m_running = false;
        m_imgBuff = nullptr;
        m_imgSize = 0;

        // Power is on
        m_powerState       = 1;
        m_powerTargetState = 1;

        setupProp( m_indiP_blacklevel, "blacklevel" );
        setupProp( m_indiP_emGain, "emgain" );
        setupProp( m_indiP_temp, "temp_ccd" );
        setupProp( m_indiP_tempcont, "temp_controller" );
        setupProp( m_indiP_roi_set, "roi_set" );
    }

    /// Destructor, frees the image buffer allocated by configureAcquisition.
    ~asiCtrl_test() noexcept
    {
        releaseImageBuffer();
    }

    /// Set the device and name of a property owned by this app.
    void setupProp( pcf::IndiProperty &prop, /**< [out] the property to set up */
                    const std::string &name  /**< [in] the INDI property name */
    )
    {
        prop.setDevice( m_configName );
        prop.setName( name );
    }

    /// Free the image buffer allocated by configureAcquisition (with new[]).
    void releaseImageBuffer()
    {
        delete[] m_imgBuff;
        m_imgBuff = nullptr;
        m_imgSize = 0;
    }

    /// Load an image into the readout buffer, as if read from the camera.
    void setImage( uint32_t                    w, /**< [in] image width */
                   uint32_t                    h, /**< [in] image height */
                   const std::vector<int16_t> &im /**< [in] the pixel values, w*h of them */
    )
    {
        releaseImageBuffer();
        m_width   = w;
        m_height  = h;
        m_imgSize = w * h * sizeof( int16_t );
        m_imgBuff = new unsigned char[m_imgSize];
        memcpy( m_imgBuff, im.data(), m_imgSize );
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

/// Verify the asiCtrl constructor defaults and stdCamera configuration flags.
/**
 * \ingroup asiCtrl_unit_test
 */
TEST_CASE( "asiCtrl constructor defaults", "[asiCtrl]" )
{
    // clang-format off
    #ifdef ASICTRL_TEST_DOXYGEN_REF
    asiCtrl::asiCtrl();
    #endif
    // clang-format on

    resetAsiStub();

    asiCtrl_test app( "camasi" );

    REQUIRE( app.m_powerMgtEnabled == true );

    REQUIRE( app.m_default_x == Approx( 4143.5 ) );
    REQUIRE( app.m_default_y == Approx( 2821.5 ) );
    REQUIRE( app.m_default_w == 2048 );
    REQUIRE( app.m_default_h == 2048 );
    REQUIRE( app.m_default_bin_x == 2 );
    REQUIRE( app.m_default_bin_y == 2 );

    REQUIRE( app.m_full_x == Approx( 4143.5 ) );
    REQUIRE( app.m_full_y == Approx( 2821.5 ) );
    REQUIRE( app.m_full_w == 8288 );
    REQUIRE( app.m_full_h == 5644 );

    REQUIRE( app.m_maxEMGain == Approx( 450 ) );
    REQUIRE( app.m_blacklevel == Approx( 0 ) );
    REQUIRE( app.m_bits == 14 );

    REQUIRE( asiCtrl::c_stdCamera_tempControl == true );
    REQUIRE( asiCtrl::c_stdCamera_temp == true );
    REQUIRE( asiCtrl::c_stdCamera_emGain == true );
    REQUIRE( asiCtrl::c_stdCamera_exptimeCtrl == true );
    REQUIRE( asiCtrl::c_stdCamera_usesROI == true );
    REQUIRE( asiCtrl::c_stdCamera_fpsCtrl == false );
    REQUIRE( asiCtrl::c_stdCamera_usesModes == false );
    REQUIRE( asiCtrl::c_frameGrabber_flippable == true );
}

/// Verify asiCtrl configuration defaults load from an empty config file.
/**
 * \ingroup asiCtrl_unit_test
 */
TEST_CASE( "asiCtrl configuration defaults", "[asiCtrl][config]" )
{
    // clang-format off
    #ifdef ASICTRL_TEST_DOXYGEN_REF
    asiCtrl::setupConfig();
    asiCtrl::loadConfig();
    #endif
    // clang-format on

    resetAsiStub();

    asiCtrl_test app( "camasi" );

    const std::string path = "/tmp/asiCtrl_test_defaults.conf";
    mx::app::writeConfigFile( path, { "none" }, { "nada" }, { "0" } );
    app.loadTestConfig( path );
    std::remove( path.c_str() );

    REQUIRE( app.m_shutdown == 0 );
    REQUIRE( app.m_camName == "" );
    REQUIRE( app.m_maxEMGain == Approx( 450 ) );
    REQUIRE( app.m_startupTemp == Approx( -999 ) );

    // stdCamera starts current and next ROI at the defaults
    REQUIRE( app.m_currentROI.x == Approx( 4143.5 ) );
    REQUIRE( app.m_currentROI.y == Approx( 2821.5 ) );
    REQUIRE( app.m_currentROI.w == 2048 );
    REQUIRE( app.m_currentROI.h == 2048 );
    REQUIRE( app.m_currentROI.bin_x == 2 );
    REQUIRE( app.m_currentROI.bin_y == 2 );
    REQUIRE( app.m_nextROI.w == 2048 );
    REQUIRE( app.m_nextROI.bin_x == 2 );

    // frameGrabber defaults
    REQUIRE( app.m_shmimName == "camasi" );
    REQUIRE( app.m_defaultFlip == dev::frameGrabber<asiCtrl>::fgFlipNone );
}

/// Verify asiCtrl configuration overrides are loaded.
/**
 * \ingroup asiCtrl_unit_test
 */
TEST_CASE( "asiCtrl configuration overrides", "[asiCtrl][config]" )
{
    // clang-format off
    #ifdef ASICTRL_TEST_DOXYGEN_REF
    asiCtrl::setupConfig();
    asiCtrl::loadConfig();
    #endif
    // clang-format on

    resetAsiStub();

    asiCtrl_test app( "camasi" );

    const std::string path = "/tmp/asiCtrl_test_overrides.conf";
    mx::app::writeConfigFile(
        path,
        { "camera", "camera", "camera", "camera", "camera", "camera", "camera", "framegrabber", "framegrabber" },
        { "cameraName",
          "maxEMGain",
          "startupTemp",
          "default_x",
          "default_y",
          "default_w",
          "default_h",
          "shmimName",
          "defaultFlip" },
        { "ZWO_ASI6200MM_Pro", "300", "-15", "1000", "800", "512", "256", "asitest", "flipUD" } );
    app.loadTestConfig( path );
    std::remove( path.c_str() );

    REQUIRE( app.m_shutdown == 0 );
    REQUIRE( app.m_camName == "ZWO_ASI6200MM_Pro" );
    REQUIRE( app.m_maxEMGain == Approx( 300 ) );
    REQUIRE( app.m_startupTemp == Approx( -15 ) );

    REQUIRE( app.m_default_x == Approx( 1000 ) );
    REQUIRE( app.m_default_y == Approx( 800 ) );
    REQUIRE( app.m_default_w == 512 );
    REQUIRE( app.m_default_h == 256 );
    REQUIRE( app.m_default_bin_x == 2 ); // not configured, keeps the constructor value

    REQUIRE( app.m_currentROI.x == Approx( 1000 ) );
    REQUIRE( app.m_currentROI.w == 512 );
    REQUIRE( app.m_nextROI.y == Approx( 800 ) );
    REQUIRE( app.m_nextROI.h == 256 );

    REQUIRE( app.m_shmimName == "asitest" );
    REQUIRE( app.m_defaultFlip == dev::frameGrabber<asiCtrl>::fgFlipUD );
}

/// Verify powerOnDefaults() resets the ROI and gain.
/**
 * \ingroup asiCtrl_unit_test
 */
TEST_CASE( "asiCtrl powerOnDefaults", "[asiCtrl]" )
{
    // clang-format off
    #ifdef ASICTRL_TEST_DOXYGEN_REF
    asiCtrl::powerOnDefaults();
    #endif
    // clang-format on

    resetAsiStub();

    asiCtrl_test app( "camasi" );

    app.m_currentROI.x = 10;
    app.m_currentROI.w = 10;
    app.m_emGainSet    = 77;

    REQUIRE( app.powerOnDefaults() == 0 );

    REQUIRE( app.m_currentROI.x == Approx( 4143.5 ) );
    REQUIRE( app.m_currentROI.y == Approx( 2821.5 ) );
    REQUIRE( app.m_currentROI.w == 2048 );
    REQUIRE( app.m_currentROI.h == 2048 );
    REQUIRE( app.m_currentROI.bin_x == 2 );
    REQUIRE( app.m_currentROI.bin_y == 2 );
    REQUIRE( app.m_emGainSet == Approx( 0 ) );
}

/// Verify connect() finds the configured camera by name, and handles missing cameras and power off.
/**
 * \ingroup asiCtrl_unit_test
 */
TEST_CASE( "asiCtrl connect", "[asiCtrl][connect]" )
{
    // clang-format off
    #ifdef ASICTRL_TEST_DOXYGEN_REF
    asiCtrl::connect();
    #endif
    // clang-format on

    resetAsiStub();

    asiCtrl_test app( "camasi" );
    app.m_camName = "ZWO ASI6200MM Pro";

    SECTION( "no cameras connected" )
    {
        g_asiStub.numCameras = 0;

        REQUIRE( app.connect() == 0 );
        REQUIRE( app.state() == stateCodes::NODEVICE );
        REQUIRE( app.m_camNum == -1 );
        REQUIRE( g_asiStub.openCalls == 0 );
        REQUIRE( g_asiStub.getPropertyCalls == 0 );
    }

    SECTION( "camera found by name" )
    {
        g_asiStub.numCameras  = 2;
        g_asiStub.cameraNames = { "ZWO ASI120MM", "ZWO ASI6200MM Pro" };

        app.m_nextROI.x = 1;
        app.m_nextROI.w = 1;

        REQUIRE( app.connect() == 0 );
        REQUIRE( app.state() == stateCodes::CONNECTED );
        REQUIRE( app.m_camNum == 1 );
        REQUIRE( g_asiStub.openCalls == 1 );
        REQUIRE( g_asiStub.lastOpenID == 1 );
        REQUIRE( g_asiStub.initCalls == 1 );
        REQUIRE( g_asiStub.lastInitID == 1 );
        REQUIRE( std::string( app.m_camInfo.Name ) == "ZWO ASI6200MM Pro" );

        // next ROI is reset to the default
        REQUIRE( app.m_nextROI.x == Approx( app.m_default_x ) );
        REQUIRE( app.m_nextROI.y == Approx( app.m_default_y ) );
        REQUIRE( app.m_nextROI.w == app.m_default_w );
        REQUIRE( app.m_nextROI.h == app.m_default_h );
        REQUIRE( app.m_nextROI.bin_x == app.m_default_bin_x );
        REQUIRE( app.m_nextROI.bin_y == app.m_default_bin_y );
    }

    SECTION( "camera name not found" )
    {
        g_asiStub.numCameras  = 1;
        g_asiStub.cameraNames = { "ZWO ASI120MM" };

        REQUIRE( app.connect() == 0 );
        REQUIRE( app.state() == stateCodes::NODEVICE );
        REQUIRE( app.m_camNum == -1 );
        REQUIRE( g_asiStub.getPropertyCalls == 1 );
        REQUIRE( g_asiStub.openCalls == 0 );
    }

    SECTION( "power off skips the search" )
    {
        g_asiStub.numCameras  = 1;
        g_asiStub.cameraNames = { "ZWO ASI6200MM Pro" };
        app.m_powerState      = 0;

        REQUIRE( app.connect() == 0 );
        REQUIRE( app.state() == stateCodes::UNINITIALIZED );
        REQUIRE( g_asiStub.getPropertyCalls == 0 );
        REQUIRE( g_asiStub.openCalls == 0 );
    }

    SECTION( "an open camera is closed before reconnecting" )
    {
        g_asiStub.numCameras  = 1;
        g_asiStub.cameraNames = { "ZWO ASI6200MM Pro" };
        app.m_camNum          = 3;
        app.m_running         = true;

        REQUIRE( app.connect() == 0 );
        REQUIRE( g_asiStub.stopCalls == 1 );
        REQUIRE( g_asiStub.closeCalls == 1 );
        REQUIRE( g_asiStub.lastCloseID == 3 );
        REQUIRE( app.m_running == false );
        REQUIRE( app.m_camNum == 0 );
        REQUIRE( app.state() == stateCodes::CONNECTED );
    }
}

/// Verify getAcquisitionState() maps the power and running state to the FSM state.
/**
 * \ingroup asiCtrl_unit_test
 */
TEST_CASE( "asiCtrl getAcquisitionState", "[asiCtrl]" )
{
    // clang-format off
    #ifdef ASICTRL_TEST_DOXYGEN_REF
    asiCtrl::getAcquisitionState();
    #endif
    // clang-format on

    resetAsiStub();

    asiCtrl_test app( "camasi" );

    SECTION( "power off returns 0 and leaves the state" )
    {
        app.m_powerState = 0;
        REQUIRE( app.getAcquisitionState() == 0 );
        REQUIRE( app.state() == stateCodes::UNINITIALIZED );
    }

    SECTION( "unknown power is an error" )
    {
        app.m_powerState = -1;
        REQUIRE( app.getAcquisitionState() == -1 );
    }

    SECTION( "target power off is an error" )
    {
        app.m_powerTargetState = 0;
        REQUIRE( app.getAcquisitionState() == -1 );
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

/// Verify the gain is set and read back through the ASI_GAIN control.
/**
 * \ingroup asiCtrl_unit_test
 */
TEST_CASE( "asiCtrl gain control", "[asiCtrl][gain]" )
{
    // clang-format off
    #ifdef ASICTRL_TEST_DOXYGEN_REF
    asiCtrl::setEMGain();
    asiCtrl::getEMGain();
    #endif
    // clang-format on

    resetAsiStub();

    asiCtrl_test app( "camasi" );
    app.m_camNum = 0;

    SECTION( "set gain" )
    {
        app.m_emGainSet = 200;
        REQUIRE( app.setEMGain() == 0 );
        REQUIRE( g_asiStub.lastSet( ASI_GAIN ) == 200 );
        REQUIRE( app.m_emGainSet == Approx( 200 ) );
    }

    SECTION( "set gain reports the camera's actual value" )
    {
        g_asiStub.readbackOverrides[ASI_GAIN] = 450;

        app.m_emGainSet = 500;
        REQUIRE( app.setEMGain() == 0 );
        REQUIRE( g_asiStub.lastSet( ASI_GAIN ) == 500 );
        REQUIRE( app.m_emGainSet == Approx( 450 ) );
    }

    SECTION( "set gain error" )
    {
        g_asiStub.setControlReturns[ASI_GAIN] = c_asiNegativeError;

        app.m_emGainSet = 120;
        REQUIRE( app.setEMGain() == -1 );
        REQUIRE( app.m_emGainSet == Approx( 120 ) );
    }

    SECTION( "get gain" )
    {
        g_asiStub.controlValues[ASI_GAIN] = 321;
        app.m_emGain                      = 0;

        REQUIRE( app.getEMGain() == 0 );
        REQUIRE( app.m_emGain == Approx( 321 ) );
    }
}

/// Verify temperature readout, set point, and cooler control.
/**
 * \ingroup asiCtrl_unit_test
 */
TEST_CASE( "asiCtrl temperature and cooler control", "[asiCtrl][temp]" )
{
    // clang-format off
    #ifdef ASICTRL_TEST_DOXYGEN_REF
    asiCtrl::getTemp();
    asiCtrl::setTempSetPt();
    asiCtrl::setTempControl();
    asiCtrl::getASIParameter(std::declval<long&>(), ASI_TEMPERATURE);
    asiCtrl::setASIParameter(ASI_GAIN, 0, false);
    #endif
    // clang-format on

    resetAsiStub();

    asiCtrl_test app( "camasi" );
    app.m_camNum = 0;

    SECTION( "temperature is read in units of 0.1 C" )
    {
        g_asiStub.controlValues[ASI_TEMPERATURE] = -153;

        REQUIRE( app.getTemp() == 0 );
        REQUIRE( app.m_ccdTemp == Approx( -15.3 ) );
    }

    SECTION( "set point is sent to ASI_TARGET_TEMP" )
    {
        app.m_ccdTempSetpt = -10;

        REQUIRE( app.setTempSetPt() == 0 );
        REQUIRE( g_asiStub.lastSet( ASI_TARGET_TEMP ) == -10 );
    }

    SECTION( "cooler on" )
    {
        app.m_tempControlStatusSet = true;

        REQUIRE( app.setTempControl() == 0 );
        REQUIRE( g_asiStub.lastSet( ASI_COOLER_ON ) == ASI_TRUE );
        REQUIRE( app.m_tempControlStatus == true );
        REQUIRE( app.m_tempControlStatusStr == "COOLING" );
    }

    SECTION( "cooler off" )
    {
        app.m_tempControlStatus    = true;
        app.m_tempControlStatusSet = false;

        REQUIRE( app.setTempControl() == 0 );
        REQUIRE( g_asiStub.lastSet( ASI_COOLER_ON ) == ASI_FALSE );
        REQUIRE( app.m_tempControlStatus == false );
        REQUIRE( app.m_tempControlStatusStr == "OFF" );
    }

    SECTION( "getASIParameter reads the value and flags power off" )
    {
        g_asiStub.controlValues[ASI_TEMPERATURE] = 250;

        long val = 0;
        REQUIRE( app.getASIParameter( val, ASI_TEMPERATURE ) == 0 );
        REQUIRE( val == 250 );

        app.m_powerState = 0;
        val              = 0;
        REQUIRE( app.getASIParameter( val, ASI_TEMPERATURE ) == -1 );
        REQUIRE( val == 250 );
    }

    SECTION( "setASIParameter sends the value" )
    {
        REQUIRE( app.setASIParameter( ASI_GAIN, 42, true ) == 0 );
        REQUIRE( g_asiStub.lastSet( ASI_GAIN ) == 42 );

        REQUIRE( app.setASIParameter( ASI_GAIN, 43, false ) == 0 );
        REQUIRE( g_asiStub.lastSet( ASI_GAIN ) == 43 );
    }

    SECTION( "setReadoutSpeed is a no-op" )
    {
        REQUIRE( app.setReadoutSpeed() == 0 );
        REQUIRE( g_asiStub.setControlCalls.empty() );
    }
}

/// Verify setExpTime() sends the exposure in microseconds and reports the camera's value.
/**
 * Note that setExpTime() sleeps for 1 second on every call.
 *
 * \ingroup asiCtrl_unit_test
 */
TEST_CASE( "asiCtrl setExpTime", "[asiCtrl][exptime]" )
{
    // clang-format off
    #ifdef ASICTRL_TEST_DOXYGEN_REF
    asiCtrl::setExpTime();
    #endif
    // clang-format on

    resetAsiStub();

    asiCtrl_test app( "camasi" );
    app.m_camNum = 0;

    SECTION( "success" )
    {
        g_asiStub.readbackOverrides[ASI_EXPOSURE] = 200000;

        app.m_expTimeSet = 0.25;
        REQUIRE( app.setExpTime() == 0 );
        REQUIRE( g_asiStub.lastSet( ASI_EXPOSURE ) == 250000 );
        REQUIRE( app.m_expTime == Approx( 0.2 ) );
    }

    SECTION( "error" )
    {
        g_asiStub.setControlReturns[ASI_EXPOSURE] = c_asiNegativeError;

        app.m_expTime    = 3;
        app.m_expTimeSet = 0.5;
        REQUIRE( app.setExpTime() == -1 );
        REQUIRE( app.m_expTime == Approx( 3 ) );
    }
}

/// Verify configureAcquisition() sets the ROI, exposure, gain and black level, and allocates the buffer.
/**
 * \ingroup asiCtrl_unit_test
 */
TEST_CASE( "asiCtrl configureAcquisition", "[asiCtrl][roi]" )
{
    // clang-format off
    #ifdef ASICTRL_TEST_DOXYGEN_REF
    asiCtrl::configureAcquisition();
    #endif
    // clang-format on

    resetAsiStub();

    asiCtrl_test app( "camasi" );
    app.m_camNum     = 0;
    app.m_running    = true;
    app.m_expTimeSet = 0.5;
    app.m_emGainSet  = 100;
    app.m_blacklevel = 12;

    SECTION( "unbinned ROI" )
    {
        app.m_nextROI.x     = 100;
        app.m_nextROI.y     = 50;
        app.m_nextROI.w     = 64;
        app.m_nextROI.h     = 32;
        app.m_nextROI.bin_x = 1;
        app.m_nextROI.bin_y = 1;

        REQUIRE( app.configureAcquisition() == 0 );

        REQUIRE( g_asiStub.stopCalls == 1 );
        REQUIRE( app.m_running == false );
        REQUIRE( g_asiStub.lastSet( ASI_HIGH_SPEED_MODE ) == 0 );

        REQUIRE( g_asiStub.roiWidth == 64 );
        REQUIRE( g_asiStub.roiHeight == 32 );
        REQUIRE( g_asiStub.roiBin == 1 );
        REQUIRE( g_asiStub.roiImgType == ASI_IMG_RAW16 );

        // start is center minus half the size
        REQUIRE( g_asiStub.startX == 68 );
        REQUIRE( g_asiStub.startY == 34 );

        REQUIRE( app.m_currentROI.x == Approx( 100 ) );
        REQUIRE( app.m_currentROI.y == Approx( 50 ) );
        REQUIRE( app.m_currentROI.w == 64 );
        REQUIRE( app.m_currentROI.h == 32 );
        REQUIRE( app.m_currentROI.bin_x == 1 );
        REQUIRE( app.m_currentROI.bin_y == 1 );

        REQUIRE( app.m_width == 64u );
        REQUIRE( app.m_height == 32u );
        REQUIRE( app.m_bits == 12 );
        REQUIRE( app.m_bfactor == 16 );

        // configureAcquisition scales the exposure by 1000, unlike setExpTime which uses 1e6
        REQUIRE( g_asiStub.lastSet( ASI_EXPOSURE ) == 500 );
        REQUIRE( app.m_expTime == Approx( 0.5 ) );
        REQUIRE( app.m_expTimeSet == Approx( 0.5 ) );

        REQUIRE( g_asiStub.lastSet( ASI_GAIN ) == 100 );
        REQUIRE( g_asiStub.lastSet( ASI_BRIGHTNESS ) == 12 );

        REQUIRE( app.m_imgBuff != nullptr );
        REQUIRE( app.m_imgSize == 64 * 32 * 2 );
        REQUIRE( app.m_dataType == _DATATYPE_UINT16 );
    }

    SECTION( "binned ROI" )
    {
        app.m_nextROI.x     = 200;
        app.m_nextROI.y     = 100;
        app.m_nextROI.w     = 64;
        app.m_nextROI.h     = 32;
        app.m_nextROI.bin_x = 2;
        app.m_nextROI.bin_y = 2;

        REQUIRE( app.configureAcquisition() == 0 );

        REQUIRE( g_asiStub.roiBin == 2 );
        REQUIRE( g_asiStub.startX == 68 );
        REQUIRE( g_asiStub.startY == 34 );

        REQUIRE( app.m_currentROI.x == Approx( 200 ) );
        REQUIRE( app.m_currentROI.y == Approx( 100 ) );
        REQUIRE( app.m_currentROI.bin_x == 2 );
        REQUIRE( app.m_currentROI.bin_y == 2 );
        REQUIRE( app.m_bits == 14 );
        REQUIRE( app.m_bfactor == 4 );
    }

    SECTION( "the current ROI reports what the camera accepted" )
    {
        app.m_nextROI.x     = 100;
        app.m_nextROI.y     = 50;
        app.m_nextROI.w     = 64;
        app.m_nextROI.h     = 32;
        app.m_nextROI.bin_x = 1;
        app.m_nextROI.bin_y = 1;

        g_asiStub.roiReadbackOverride = true;
        g_asiStub.readWidth           = 48;
        g_asiStub.readHeight          = 24;
        g_asiStub.readBin             = 1;
        g_asiStub.readStartX          = 60;
        g_asiStub.readStartY          = 30;

        REQUIRE( app.configureAcquisition() == 0 );

        REQUIRE( app.m_currentROI.x == Approx( 84 ) );
        REQUIRE( app.m_currentROI.y == Approx( 42 ) );
        REQUIRE( app.m_currentROI.w == 48 );
        REQUIRE( app.m_currentROI.h == 24 );
        REQUIRE( app.m_width == 48u );
        REQUIRE( app.m_height == 24u );

        // the readout buffer is sized from the requested ROI, not the one the camera reports
        REQUIRE( app.m_imgSize == 64 * 32 * 2 );
    }

    SECTION( "ROI format error" )
    {
        app.m_nextROI.w        = 64;
        app.m_nextROI.h        = 32;
        app.m_nextROI.bin_x    = 1;
        g_asiStub.setROIReturn = c_asiNegativeError;

        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( app.m_imgBuff == nullptr );
    }

    SECTION( "start position error" )
    {
        app.m_nextROI.w             = 64;
        app.m_nextROI.h             = 32;
        app.m_nextROI.bin_x         = 1;
        g_asiStub.setStartPosReturn = c_asiNegativeError;

        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( app.m_imgBuff == nullptr );
    }

    SECTION( "exposure error" )
    {
        app.m_nextROI.w                           = 64;
        app.m_nextROI.h                           = 32;
        app.m_nextROI.bin_x                       = 1;
        g_asiStub.setControlReturns[ASI_EXPOSURE] = c_asiNegativeError;

        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( g_asiStub.setCount( ASI_GAIN ) == 0 );
        REQUIRE( app.m_imgBuff == nullptr );
    }

    SECTION( "black level error" )
    {
        app.m_nextROI.w                             = 64;
        app.m_nextROI.h                             = 32;
        app.m_nextROI.bin_x                         = 1;
        g_asiStub.setControlReturns[ASI_BRIGHTNESS] = c_asiNegativeError;

        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( g_asiStub.setCount( ASI_GAIN ) == 1 );
        REQUIRE( app.m_imgBuff == nullptr );
    }
}

/// Verify the framegrabber acquisition hooks drive video capture.
/**
 * \ingroup asiCtrl_unit_test
 */
TEST_CASE( "asiCtrl acquisition hooks", "[asiCtrl][framegrabber]" )
{
    // clang-format off
    #ifdef ASICTRL_TEST_DOXYGEN_REF
    asiCtrl::startAcquisition();
    asiCtrl::acquireAndCheckValid();
    asiCtrl::reconfig();
    asiCtrl::setNextROI();
    asiCtrl::checkNextROI();
    asiCtrl::fps();
    #endif
    // clang-format on

    resetAsiStub();

    asiCtrl_test app( "camasi" );
    app.m_camNum = 0;

    SECTION( "start, acquire, and reconfigure" )
    {
        REQUIRE( app.startAcquisition() == 0 );
        REQUIRE( g_asiStub.startCalls == 1 );
        REQUIRE( app.m_running == true );

        app.setImage( 4, 2, { 0, 0, 0, 0, 0, 0, 0, 0 } );
        g_asiStub.videoFill = 0x5A;

        app.m_currImageTimestamp = { 0, 0 };
        REQUIRE( app.acquireAndCheckValid() == 0 );
        REQUIRE( g_asiStub.videoCalls == 1 );
        REQUIRE( g_asiStub.lastBuffSize == 16 );
        REQUIRE( g_asiStub.lastWaitms == 6000000 );
        REQUIRE( app.m_imgBuff[0] == 0x5A );
        REQUIRE( app.m_imgBuff[15] == 0x5A );
        REQUIRE( app.m_currImageTimestamp.tv_sec > 0 );

        REQUIRE( app.reconfig() == 0 );
        REQUIRE( g_asiStub.stopCalls == 1 );
        REQUIRE( app.m_running == false );
    }

    SECTION( "setNextROI triggers a reconfiguration" )
    {
        app.m_reconfig = false;
        REQUIRE( app.setNextROI() == 0 );
        REQUIRE( app.m_reconfig == true );
    }

    SECTION( "checkNextROI accepts the ROI unchanged" )
    {
        app.m_nextROI.x = 123;
        app.m_nextROI.w = 64;
        REQUIRE( app.checkNextROI() == 0 );
        REQUIRE( app.m_nextROI.x == Approx( 123 ) );
        REQUIRE( app.m_nextROI.w == 64 );
    }

    SECTION( "fps reports m_fps" )
    {
        app.m_fps = 12.5;
        REQUIRE( app.fps() == Approx( 12.5 ) );
    }
}

/// Verify loadImageIntoStream() copies the readout buffer, applying the configured flip.
/**
 * \ingroup asiCtrl_unit_test
 */
TEST_CASE( "asiCtrl loadImageIntoStream", "[asiCtrl][framegrabber]" )
{
    // clang-format off
    #ifdef ASICTRL_TEST_DOXYGEN_REF
    asiCtrl::loadImageIntoStream(nullptr);
    #endif
    // clang-format on

    resetAsiStub();

    asiCtrl_test app( "camasi" );

    std::vector<int16_t> src = { 0, 1, 2, 3, 10, 11, 12, 13, 20, 21, 22, 23 };
    app.setImage( 4, 3, src );

    std::vector<int16_t> dest( src.size(), -1 );

    SECTION( "no flip" )
    {
        app.m_defaultFlip = dev::frameGrabber<asiCtrl>::fgFlipNone;
        REQUIRE( app.loadImageIntoStream( dest.data() ) == 0 );
        REQUIRE( dest == src );
    }

    SECTION( "flip up-down" )
    {
        app.m_defaultFlip = dev::frameGrabber<asiCtrl>::fgFlipUD;
        REQUIRE( app.loadImageIntoStream( dest.data() ) == 0 );

        std::vector<int16_t> expected = { 20, 21, 22, 23, 10, 11, 12, 13, 0, 1, 2, 3 };
        REQUIRE( dest == expected );
    }

    SECTION( "invalid flip is an error" )
    {
        app.m_defaultFlip = 99;
        REQUIRE( app.loadImageIntoStream( dest.data() ) == -1 );
    }
}

/// Verify the power-off and shutdown hooks close an open camera.
/**
 * \ingroup asiCtrl_unit_test
 */
TEST_CASE( "asiCtrl power off and shutdown", "[asiCtrl][power]" )
{
    // clang-format off
    #ifdef ASICTRL_TEST_DOXYGEN_REF
    asiCtrl::onPowerOff();
    asiCtrl::whilePowerOff();
    asiCtrl::appShutdown();
    #endif
    // clang-format on

    resetAsiStub();

    asiCtrl_test app( "camasi" );

    SECTION( "onPowerOff closes an open camera" )
    {
        app.m_camNum  = 2;
        app.m_running = true;

        REQUIRE( app.onPowerOff() == 0 );
        REQUIRE( g_asiStub.stopCalls == 1 );
        REQUIRE( g_asiStub.closeCalls == 1 );
        REQUIRE( g_asiStub.lastCloseID == 2 );
        REQUIRE( app.m_camNum == -1 );
        REQUIRE( app.m_running == false );
    }

    SECTION( "onPowerOff with no camera does nothing" )
    {
        REQUIRE( app.onPowerOff() == 0 );
        REQUIRE( g_asiStub.closeCalls == 0 );
    }

    SECTION( "whilePowerOff" )
    {
        REQUIRE( app.whilePowerOff() == 0 );
        REQUIRE( g_asiStub.closeCalls == 0 );
    }

    SECTION( "appShutdown closes an open camera" )
    {
        app.m_camNum = 1;

        REQUIRE( app.appShutdown() == 0 );
        REQUIRE( g_asiStub.stopCalls == 1 );
        REQUIRE( g_asiStub.closeCalls == 1 );
        REQUIRE( g_asiStub.lastCloseID == 1 );
        REQUIRE( app.m_camNum == -1 );
    }
}

/// Verify the blacklevel INDI callback validates the property and sets ASI_BRIGHTNESS.
/**
 * Note that a successful blacklevel request sleeps for 2 seconds.
 *
 * \ingroup asiCtrl_unit_test
 */
TEST_CASE( "asiCtrl blacklevel callback", "[asiCtrl][indi]" )
{
    // clang-format off
    #ifdef ASICTRL_TEST_DOXYGEN_REF
    asiCtrl::newCallBack_m_indiP_blacklevel(std::declval<const pcf::IndiProperty &>());
    #endif
    // clang-format on

    resetAsiStub();

    asiCtrl_test app( "camasi" );
    app.m_camNum = 0;

    SECTION( "wrong name is rejected" )
    {
        pcf::IndiProperty ip = numberProp( "camasi", "wrong", "target", 42 );
        REQUIRE( app.newCallBack_m_indiP_blacklevel( ip ) == -1 );
        REQUIRE( g_asiStub.setCount( ASI_BRIGHTNESS ) == 0 );
    }

    SECTION( "target sets the black level" )
    {
        pcf::IndiProperty ip = numberProp( "camasi", "blacklevel", "target", 42 );
        REQUIRE( app.newCallBack_m_indiP_blacklevel( ip ) == 0 );
        REQUIRE( app.m_blacklevel == Approx( 42 ) );
        REQUIRE( g_asiStub.lastSet( ASI_BRIGHTNESS ) == 42 );
    }

    SECTION( "current is used without target, and SDK errors are reported" )
    {
        g_asiStub.setControlReturns[ASI_BRIGHTNESS] = c_asiNegativeError;

        pcf::IndiProperty ip = numberProp( "camasi", "blacklevel", "current", 17 );
        REQUIRE( app.newCallBack_m_indiP_blacklevel( ip ) == -1 );
        REQUIRE( app.m_blacklevel == Approx( 17 ) );
        REQUIRE( g_asiStub.lastSet( ASI_BRIGHTNESS ) == 17 );
    }
}

/// Verify the stdCamera INDI callbacks reach the asiCtrl gain, temperature, and ROI hooks.
/**
 * \ingroup asiCtrl_unit_test
 */
TEST_CASE( "asiCtrl stdCamera callbacks drive the camera", "[asiCtrl][indi]" )
{
    // clang-format off
    #ifdef ASICTRL_TEST_DOXYGEN_REF
    dev::stdCamera<asiCtrl>::newCallBack_emgain(std::declval<const pcf::IndiProperty &>());
    dev::stdCamera<asiCtrl>::newCallBack_temp(std::declval<const pcf::IndiProperty &>());
    dev::stdCamera<asiCtrl>::newCallBack_temp_controller(std::declval<const pcf::IndiProperty &>());
    dev::stdCamera<asiCtrl>::newCallBack_roi_set(std::declval<const pcf::IndiProperty &>());
    asiCtrl::setEMGain();
    asiCtrl::setTempSetPt();
    asiCtrl::setTempControl();
    asiCtrl::setNextROI();
    #endif
    // clang-format on

    resetAsiStub();

    asiCtrl_test app( "camasi" );
    app.m_camNum = 0;

    SECTION( "emgain" )
    {
        pcf::IndiProperty ip = numberProp( "camasi", "emgain", "target", 250 );
        REQUIRE( app.newCallBack_emgain( ip ) == 0 );
        REQUIRE( g_asiStub.lastSet( ASI_GAIN ) == 250 );
        REQUIRE( app.m_emGainSet == Approx( 250 ) );
    }

    SECTION( "emgain from the wrong device is rejected" )
    {
        pcf::IndiProperty ip = numberProp( "other", "emgain", "target", 250 );
        REQUIRE( app.newCallBack_emgain( ip ) == -1 );
        REQUIRE( g_asiStub.setCount( ASI_GAIN ) == 0 );
    }

    SECTION( "temperature set point" )
    {
        pcf::IndiProperty ip = numberProp( "camasi", "temp_ccd", "target", -20 );
        REQUIRE( app.newCallBack_temp( ip ) == 0 );
        REQUIRE( app.m_ccdTempSetpt == Approx( -20 ) );
        REQUIRE( g_asiStub.lastSet( ASI_TARGET_TEMP ) == -20 );
    }

    SECTION( "temperature controller on and off" )
    {
        pcf::IndiProperty ipOn = switchProp( "camasi", "temp_controller", "toggle", pcf::IndiElement::On );
        REQUIRE( app.newCallBack_temp_controller( ipOn ) == 0 );
        REQUIRE( app.m_tempControlStatusSet == true );
        REQUIRE( app.m_tempControlStatus == true );
        REQUIRE( g_asiStub.lastSet( ASI_COOLER_ON ) == ASI_TRUE );

        pcf::IndiProperty ipOff = switchProp( "camasi", "temp_controller", "toggle", pcf::IndiElement::Off );
        REQUIRE( app.newCallBack_temp_controller( ipOff ) == 0 );
        REQUIRE( app.m_tempControlStatusSet == false );
        REQUIRE( app.m_tempControlStatus == false );
        REQUIRE( g_asiStub.lastSet( ASI_COOLER_ON ) == ASI_FALSE );
    }

    SECTION( "roi_set request" )
    {
        app.m_currentROI.x = 321;
        app.m_reconfig     = false;

        pcf::IndiProperty ip = switchProp( "camasi", "roi_set", "request", pcf::IndiElement::On );
        REQUIRE( app.newCallBack_roi_set( ip ) == 0 );
        REQUIRE( app.m_reconfig == true );
        REQUIRE( app.m_lastROI.x == Approx( 321 ) );
    }
}

/// Verify the telemeter interface records the camera telemetry.
/**
 * \ingroup asiCtrl_unit_test
 */
TEST_CASE( "asiCtrl telemetry", "[asiCtrl][telem]" )
{
    // clang-format off
    #ifdef ASICTRL_TEST_DOXYGEN_REF
    asiCtrl::recordTelem(nullptr);
    asiCtrl::checkRecordTimes();
    #endif
    // clang-format on

    resetAsiStub();

    asiCtrl_test app( "camasi" );

    REQUIRE( app.recordTelem( static_cast<const telem_stdcam *>( nullptr ) ) == 0 );
    REQUIRE( app.checkRecordTimes() == 0 );
}

} // namespace asiCtrlTest

} // namespace libXWCTest
