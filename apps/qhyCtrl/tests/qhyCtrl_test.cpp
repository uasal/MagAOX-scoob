/** \file qhyCtrl_test.cpp
 * \brief Catch2 tests for the qhyCtrl app.
 * \author Claude Code
 *
 * \ingroup qhyCtrl_files
 */

#include "../../../tests/testXWC.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#define protected public
#include "../qhyCtrl.hpp"
#undef protected

using namespace MagAOX::app;

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
namespace
{

/// Storage whose address is used as the fake camera handle.
int g_fakeCameraStorage{ 0 };

/// The fake camera handle returned by OpenQHYCCD.
qhyccd_handle *const c_fakeCamera = &g_fakeCameraStorage;

/// Fake QHYCCD SDK state shared by the stub functions below.
struct qhyStubState
{
    uint32_t initReturn{ QHYCCD_SUCCESS }; ///< Value returned by InitQHYCCDResource.

    int initCalls{ 0 }; ///< Number of InitQHYCCDResource calls.

    uint32_t releaseReturn{ QHYCCD_SUCCESS }; ///< Value returned by ReleaseQHYCCDResource.

    int releaseCalls{ 0 }; ///< Number of ReleaseQHYCCDResource calls.

    qhyccd_handle *openReturn{ c_fakeCamera }; ///< Handle returned by OpenQHYCCD.

    bool openThrows{ false }; ///< If true, OpenQHYCCD throws.

    int openCalls{ 0 }; ///< Number of OpenQHYCCD calls.

    std::string lastOpenID; ///< Camera ID passed to the last OpenQHYCCD.

    uint32_t closeReturn{ QHYCCD_SUCCESS }; ///< Value returned by CloseQHYCCD.

    int closeCalls{ 0 }; ///< Number of CloseQHYCCD calls.

    qhyccd_handle *lastCloseHandle{ nullptr }; ///< Handle passed to the last CloseQHYCCD.

    uint32_t statusReturn{ QHYCCD_SUCCESS }; ///< Value returned by GetQHYCCDCameraStatus.

    int statusCalls{ 0 }; ///< Number of GetQHYCCDCameraStatus calls.

    int binCalls{ 0 }; ///< Number of SetQHYCCDBinMode calls.

    uint32_t lastBinW{ 0 }; ///< x binning passed to the last SetQHYCCDBinMode.

    uint32_t lastBinH{ 0 }; ///< y binning passed to the last SetQHYCCDBinMode.

    int resCalls{ 0 }; ///< Number of SetQHYCCDResolution calls.

    uint32_t lastResX{ 0 }; ///< Start x passed to the last SetQHYCCDResolution.

    uint32_t lastResY{ 0 }; ///< Start y passed to the last SetQHYCCDResolution.

    uint32_t lastResW{ 0 }; ///< Width passed to the last SetQHYCCDResolution.

    uint32_t lastResH{ 0 }; ///< Height passed to the last SetQHYCCDResolution.

    uint32_t memLength{ 0 }; ///< Value returned by GetQHYCCDMemLength.

    int cancelCalls{ 0 }; ///< Number of CancelQHYCCDExposing calls.

    std::map<int, double> params; ///< Values returned by GetQHYCCDParam, keyed by CONTROL_ID.

    bool getParamThrows{ false }; ///< If true, GetQHYCCDParam throws.

    bool setParamThrows{ false }; ///< If true, SetQHYCCDParam throws.

    std::vector<std::pair<int, double>> setParamCalls; ///< Every (control, value) pair sent to SetQHYCCDParam.

    qhyccd_handle *lastSetParamHandle{ nullptr }; ///< Handle passed to the last SetQHYCCDParam.

    uint32_t frameW{ 0 }; ///< Frame width reported by GetQHYCCDSingleFrame.

    uint32_t frameH{ 0 }; ///< Frame height reported by GetQHYCCDSingleFrame.

    uint32_t frameBpp{ 16 }; ///< Bits per pixel reported by GetQHYCCDSingleFrame.

    uint32_t frameChannels{ 1 }; ///< Channels reported by GetQHYCCDSingleFrame.

    std::vector<uint16_t> frameData; ///< Pixels written by GetQHYCCDSingleFrame.

    bool singleFrameThrows{ false }; ///< If true, GetQHYCCDSingleFrame throws.

    int singleFrameCalls{ 0 }; ///< Number of GetQHYCCDSingleFrame calls.

    qhyccd_handle *lastSingleFrameHandle{ nullptr }; ///< Handle passed to the last GetQHYCCDSingleFrame.

    uint32_t sdkVersion[4]{ 24, 5, 13, 0 }; ///< Year, month, day, sub-day returned by GetQHYCCDSDKVersion.

    int sdkVersionCalls{ 0 }; ///< Number of GetQHYCCDSDKVersion calls.

    uint32_t fwReturn{ QHYCCD_SUCCESS }; ///< Value returned by GetQHYCCDFWVersion.

    uint8_t fwBytes[2]{ 0x81, 0x12 }; ///< Firmware version bytes returned by GetQHYCCDFWVersion.

    int fwCalls{ 0 }; ///< Number of GetQHYCCDFWVersion calls.

    qhyccd_handle *lastFwHandle{ nullptr }; ///< Handle passed to the last GetQHYCCDFWVersion.
};

/// The global fake QHYCCD SDK state.
qhyStubState g_qhyStub;

/// Reset the fake QHYCCD SDK state before a test.
void resetQhyStub()
{
    g_qhyStub = qhyStubState();
}

} // namespace

extern "C"
{

    uint32_t GetQHYCCDSDKVersion( uint32_t *year, uint32_t *month, uint32_t *day, uint32_t *subday )
    {
        ++g_qhyStub.sdkVersionCalls;
        *year   = g_qhyStub.sdkVersion[0];
        *month  = g_qhyStub.sdkVersion[1];
        *day    = g_qhyStub.sdkVersion[2];
        *subday = g_qhyStub.sdkVersion[3];
        return QHYCCD_SUCCESS;
    }

    uint32_t GetQHYCCDFWVersion( qhyccd_handle *handle, uint8_t *buf )
    {
        ++g_qhyStub.fwCalls;
        g_qhyStub.lastFwHandle = handle;
        buf[0]                 = g_qhyStub.fwBytes[0];
        buf[1]                 = g_qhyStub.fwBytes[1];
        return g_qhyStub.fwReturn;
    }

    uint32_t InitQHYCCDResource()
    {
        ++g_qhyStub.initCalls;
        return g_qhyStub.initReturn;
    }

    uint32_t ReleaseQHYCCDResource()
    {
        ++g_qhyStub.releaseCalls;
        return g_qhyStub.releaseReturn;
    }

    qhyccd_handle *OpenQHYCCD( char *id )
    {
        ++g_qhyStub.openCalls;
        g_qhyStub.lastOpenID = id;

        if( g_qhyStub.openThrows )
        {
            throw std::runtime_error( "OpenQHYCCD stub failure" );
        }

        return g_qhyStub.openReturn;
    }

    uint32_t CloseQHYCCD( qhyccd_handle *handle )
    {
        ++g_qhyStub.closeCalls;
        g_qhyStub.lastCloseHandle = handle;
        return g_qhyStub.closeReturn;
    }

    uint32_t GetQHYCCDCameraStatus( qhyccd_handle *h, uint8_t *buf )
    {
        static_cast<void>( h );
        ++g_qhyStub.statusCalls;
        *buf = 0;
        return g_qhyStub.statusReturn;
    }

    uint32_t SetQHYCCDBinMode( qhyccd_handle *handle, uint32_t wbin, uint32_t hbin )
    {
        static_cast<void>( handle );
        ++g_qhyStub.binCalls;
        g_qhyStub.lastBinW = wbin;
        g_qhyStub.lastBinH = hbin;
        return QHYCCD_SUCCESS;
    }

    uint32_t SetQHYCCDResolution( qhyccd_handle *handle, uint32_t x, uint32_t y, uint32_t xsize, uint32_t ysize )
    {
        static_cast<void>( handle );
        ++g_qhyStub.resCalls;
        g_qhyStub.lastResX = x;
        g_qhyStub.lastResY = y;
        g_qhyStub.lastResW = xsize;
        g_qhyStub.lastResH = ysize;
        return QHYCCD_SUCCESS;
    }

    uint32_t GetQHYCCDMemLength( qhyccd_handle *handle )
    {
        static_cast<void>( handle );
        return g_qhyStub.memLength;
    }

    uint32_t CancelQHYCCDExposing( qhyccd_handle *handle )
    {
        static_cast<void>( handle );
        ++g_qhyStub.cancelCalls;
        return QHYCCD_SUCCESS;
    }

    uint32_t GetQHYCCDSingleFrame(
        qhyccd_handle *handle, uint32_t *w, uint32_t *h, uint32_t *bpp, uint32_t *channels, uint8_t *imgdata )
    {
        ++g_qhyStub.singleFrameCalls;
        g_qhyStub.lastSingleFrameHandle = handle;

        if( g_qhyStub.singleFrameThrows )
        {
            throw std::runtime_error( "GetQHYCCDSingleFrame stub failure" );
        }

        *w        = g_qhyStub.frameW;
        *h        = g_qhyStub.frameH;
        *bpp      = g_qhyStub.frameBpp;
        *channels = g_qhyStub.frameChannels;

        if( imgdata != nullptr && !g_qhyStub.frameData.empty() )
        {
            memcpy( imgdata, g_qhyStub.frameData.data(), g_qhyStub.frameData.size() * sizeof( uint16_t ) );
        }

        return QHYCCD_SUCCESS;
    }

    double GetQHYCCDParam( qhyccd_handle *handle, CONTROL_ID controlId )
    {
        static_cast<void>( handle );

        if( g_qhyStub.getParamThrows )
        {
            throw std::runtime_error( "GetQHYCCDParam stub failure" );
        }

        if( g_qhyStub.params.count( controlId ) > 0 )
        {
            return g_qhyStub.params[controlId];
        }

        return 0;
    }

    uint32_t SetQHYCCDParam( qhyccd_handle *handle, CONTROL_ID controlId, double value )
    {
        g_qhyStub.lastSetParamHandle = handle;
        g_qhyStub.setParamCalls.push_back( { controlId, value } );

        if( g_qhyStub.setParamThrows )
        {
            throw std::runtime_error( "SetQHYCCDParam stub failure" );
        }

        return QHYCCD_SUCCESS;
    }

} // extern "C"
/// \endcond

namespace libXWCTest
{

/** \defgroup qhyCtrl_unit_test qhyCtrl Unit Tests
 * \brief Unit tests for the qhyCtrl application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `qhyCtrl` unit tests.
/** \ingroup qhyCtrl_unit_test
 */
namespace qhyCtrlTest
{

/// A 31 character camera ID, the longest that fills qhyCtrl::m_camId with a terminating null.
const std::string c_serial31 = "QHY600M-0123456789abcdef0123456";

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness which initializes the qhyCtrl state the constructor leaves unset.
class qhyCtrl_test : public qhyCtrl
{
  public:
    /// Construct a harness with the given device name.
    explicit qhyCtrl_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        // qhyCtrl() leaves these uninitialized
        memset( m_camId, 0, sizeof( m_camId ) );
        m_ccdTemp      = -999;
        m_expTimeSet   = 0;
        m_expTime      = 1;
        m_frame_length = 0;
        m_frame_data   = nullptr;

        m_indiP_exptime.setDevice( device );
        m_indiP_exptime.setName( "exptime" );
    }

    /// Destructor, frees the frame buffer allocated by configureAcquisition.
    ~qhyCtrl_test() noexcept
    {
        releaseFrameBuffer();
    }

    /// Free the frame buffer (allocated with new[]).
    void releaseFrameBuffer()
    {
        delete[] m_frame_data;
        m_frame_data   = nullptr;
        m_frame_length = 0;
    }

    /// Allocate a frame buffer of the given size.
    void allocFrameBuffer( uint32_t len /**< [in] the buffer size in bytes */ )
    {
        releaseFrameBuffer();
        m_frame_length = len;
        m_frame_data   = new uint8_t[len];
        memset( m_frame_data, 0, len );
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

/// Verify the qhyCtrl constructor defaults and stdCamera configuration flags.
/**
 * \ingroup qhyCtrl_unit_test
 */
TEST_CASE( "qhyCtrl constructor defaults", "[qhyCtrl]" )
{
    // clang-format off
    #ifdef QHYCTRL_TEST_DOXYGEN_REF
    qhyCtrl::qhyCtrl();
    #endif
    // clang-format on

    resetQhyStub();

    qhyCtrl_test app( "camqhy" );

    REQUIRE( app.m_powerMgtEnabled == false );
    REQUIRE( app.m_camera == nullptr );
    REQUIRE( app.m_bits == 16u );
    REQUIRE( app.m_retVal == 0u );
    REQUIRE( app.channels == 1u );

    REQUIRE( qhyCtrl::c_stdCamera_tempControl == true );
    REQUIRE( qhyCtrl::c_stdCamera_temp == true );
    REQUIRE( qhyCtrl::c_stdCamera_exptimeCtrl == true );
    REQUIRE( qhyCtrl::c_stdCamera_emGain == false );
    REQUIRE( qhyCtrl::c_stdCamera_usesROI == false );
    REQUIRE( qhyCtrl::c_stdCamera_usesModes == false );
    REQUIRE( qhyCtrl::c_frameGrabber_flippable == false );
}

/// Verify qhyCtrl configuration defaults load from an empty config file.
/**
 * \ingroup qhyCtrl_unit_test
 */
TEST_CASE( "qhyCtrl configuration defaults", "[qhyCtrl][config]" )
{
    // clang-format off
    #ifdef QHYCTRL_TEST_DOXYGEN_REF
    qhyCtrl::setupConfig();
    qhyCtrl::loadConfig();
    #endif
    // clang-format on

    resetQhyStub();

    qhyCtrl_test app( "camqhy" );

    // loadConfig() copies 32 characters of m_serialNumber into m_camId, which reads past the end of a
    // shorter string, so start from a 31 character serial number that the empty config leaves unchanged.
    app.m_serialNumber = c_serial31;

    const std::string path = "/tmp/qhyCtrl_test_defaults.conf";
    mx::app::writeConfigFile( path, { "none" }, { "nada" }, { "0" } );
    app.loadTestConfig( path );
    std::remove( path.c_str() );

    REQUIRE( app.m_shutdown == 0 );
    REQUIRE( app.m_serialNumber == c_serial31 );
    REQUIRE( std::string( app.m_camId ) == c_serial31 );
    REQUIRE( app.m_bits == 16u );
    REQUIRE( app.m_startupTemp == Approx( -999 ) );
    REQUIRE( app.m_shmimName == "camqhy" );
    REQUIRE( app.m_circBuffLength == 1u );
}

/// Verify qhyCtrl configuration overrides are loaded.
/**
 * \ingroup qhyCtrl_unit_test
 */
TEST_CASE( "qhyCtrl configuration overrides", "[qhyCtrl][config]" )
{
    // clang-format off
    #ifdef QHYCTRL_TEST_DOXYGEN_REF
    qhyCtrl::setupConfig();
    qhyCtrl::loadConfig();
    #endif
    // clang-format on

    resetQhyStub();

    qhyCtrl_test app( "camqhy" );

    REQUIRE( c_serial31.size() == 31 );

    const std::string path = "/tmp/qhyCtrl_test_overrides.conf";
    mx::app::writeConfigFile( path,
                              { "camera", "camera", "camera", "framegrabber", "framegrabber" },
                              { "serialNumber", "bits", "startupTemp", "shmimName", "circBuffLength" },
                              { c_serial31, "12", "-5", "qhytest", "4" } );
    app.loadTestConfig( path );
    std::remove( path.c_str() );

    REQUIRE( app.m_shutdown == 0 );
    REQUIRE( app.m_serialNumber == c_serial31 );
    REQUIRE( std::string( app.m_camId ) == c_serial31 );
    REQUIRE( app.m_bits == 12u );
    REQUIRE( app.m_startupTemp == Approx( -5 ) );
    REQUIRE( app.m_shmimName == "qhytest" );
    REQUIRE( app.m_circBuffLength == 4u );
}

/// Verify the free SDK helper functions.
/**
 * \ingroup qhyCtrl_unit_test
 */
TEST_CASE( "qhyCtrl SDK helper functions", "[qhyCtrl][helpers]" )
{
    // clang-format off
    #ifdef QHYCTRL_TEST_DOXYGEN_REF
    MagAOX::app::qhyccdSDKErrorName(CONTROL_EXPOSURE);
    MagAOX::app::SDKVersion();
    MagAOX::app::FirmWareVersion(nullptr);
    #endif
    // clang-format on

    resetQhyStub();

    SECTION( "qhyccdSDKErrorName" )
    {
        REQUIRE( qhyccdSDKErrorName( static_cast<CONTROL_ID>( QHYCCD_SUCCESS ) ) == "QHT_SUCCES" );
        REQUIRE( qhyccdSDKErrorName( CONTROL_EXPOSURE ) == "UNKNOWN: 8" );
    }

    SECTION( "SDKVersion queries the SDK for each version layout" )
    {
        SDKVersion();

        g_qhyStub.sdkVersion[1] = 5;
        g_qhyStub.sdkVersion[2] = 23;
        SDKVersion();

        g_qhyStub.sdkVersion[1] = 11;
        g_qhyStub.sdkVersion[2] = 3;
        SDKVersion();

        g_qhyStub.sdkVersion[1] = 12;
        g_qhyStub.sdkVersion[2] = 25;
        SDKVersion();

        REQUIRE( g_qhyStub.sdkVersionCalls == 4 );
    }

    SECTION( "FirmWareVersion queries the camera" )
    {
        FirmWareVersion( c_fakeCamera );
        REQUIRE( g_qhyStub.fwCalls == 1 );
        REQUIRE( g_qhyStub.lastFwHandle == c_fakeCamera );

        g_qhyStub.fwBytes[0] = 0xA1;
        FirmWareVersion( c_fakeCamera );
        REQUIRE( g_qhyStub.fwCalls == 2 );

        g_qhyStub.fwReturn = QHYCCD_ERROR;
        FirmWareVersion( c_fakeCamera );
        REQUIRE( g_qhyStub.fwCalls == 3 );
    }
}

/// Verify connect() opens the camera by ID, closes an open camera first, and handles SDK exceptions.
/**
 * \ingroup qhyCtrl_unit_test
 */
TEST_CASE( "qhyCtrl connect", "[qhyCtrl][connect]" )
{
    // clang-format off
    #ifdef QHYCTRL_TEST_DOXYGEN_REF
    qhyCtrl::connect();
    #endif
    // clang-format on

    resetQhyStub();

    qhyCtrl_test app( "camqhy" );
    strncpy( app.m_camId, c_serial31.c_str(), sizeof( app.m_camId ) - 1 );

    SECTION( "open by ID" )
    {
        REQUIRE( app.connect() == 0 );
        REQUIRE( g_qhyStub.openCalls == 1 );
        REQUIRE( g_qhyStub.lastOpenID == c_serial31 );
        REQUIRE( g_qhyStub.closeCalls == 0 );
        REQUIRE( app.m_camera == c_fakeCamera );
    }

    SECTION( "an open camera is closed first" )
    {
        int oldCamera = 0;
        app.m_camera  = &oldCamera;

        REQUIRE( app.connect() == 0 );
        REQUIRE( g_qhyStub.closeCalls == 1 );
        REQUIRE( g_qhyStub.lastCloseHandle == static_cast<qhyccd_handle *>( &oldCamera ) );
        REQUIRE( app.m_camera == c_fakeCamera );
    }

    SECTION( "OpenQHYCCD returning null leaves no camera" )
    {
        g_qhyStub.openReturn = nullptr;

        REQUIRE( app.connect() == 0 );
        REQUIRE( app.m_camera == nullptr );
    }

    SECTION( "an SDK exception is NODEVICE" )
    {
        int oldCamera        = 0;
        app.m_camera         = &oldCamera;
        g_qhyStub.openThrows = true;

        REQUIRE( app.connect() == 0 );
        REQUIRE( app.state() == stateCodes::NODEVICE );
        REQUIRE( app.m_camera == nullptr );
        REQUIRE( g_qhyStub.closeCalls == 1 ); // the old camera, before the open
    }
}

/// Verify configureAcquisition() sets binning and resolution, and (re)allocates the frame buffer.
/**
 * \ingroup qhyCtrl_unit_test
 */
TEST_CASE( "qhyCtrl configureAcquisition", "[qhyCtrl][roi]" )
{
    // clang-format off
    #ifdef QHYCTRL_TEST_DOXYGEN_REF
    qhyCtrl::configureAcquisition();
    #endif
    // clang-format on

    resetQhyStub();

    qhyCtrl_test app( "camqhy" );

    app.m_nextROI.x     = 100;
    app.m_nextROI.y     = 50;
    app.m_nextROI.w     = 64;
    app.m_nextROI.h     = 32;
    app.m_nextROI.bin_x = 2;
    app.m_nextROI.bin_y = 2;

    app.m_currentROI.w = 40;
    app.m_currentROI.h = 20;

    SECTION( "no camera" )
    {
        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( g_qhyStub.statusCalls == 0 );
    }

    SECTION( "status error" )
    {
        app.m_camera           = c_fakeCamera;
        g_qhyStub.statusReturn = QHYCCD_ERROR;

        REQUIRE( app.configureAcquisition() == -1 );
        REQUIRE( app.state() == stateCodes::ERROR );
        REQUIRE( g_qhyStub.binCalls == 0 );
        REQUIRE( app.m_frame_data == nullptr );
    }

    SECTION( "success" )
    {
        app.m_camera        = c_fakeCamera;
        g_qhyStub.memLength = 4096;

        REQUIRE( app.configureAcquisition() == 0 );

        REQUIRE( g_qhyStub.statusCalls == 1 );
        REQUIRE( g_qhyStub.binCalls == 1 );
        REQUIRE( g_qhyStub.lastBinW == 2u );
        REQUIRE( g_qhyStub.lastBinH == 2u );

        // the top-left corner, 1-based, from the center and size
        REQUIRE( g_qhyStub.resCalls == 1 );
        REQUIRE( g_qhyStub.lastResX == 69u );
        REQUIRE( g_qhyStub.lastResY == 35u );
        REQUIRE( g_qhyStub.lastResW == 64u );
        REQUIRE( g_qhyStub.lastResH == 32u );

        // the image size comes from the current ROI
        REQUIRE( app.m_width == 40u );
        REQUIRE( app.m_height == 20u );
        REQUIRE( app.m_dataType == _DATATYPE_INT16 );

        REQUIRE( app.m_frame_length == 4096u );
        REQUIRE( app.m_frame_data != nullptr );

        // the same size keeps the buffer
        uint8_t *first = app.m_frame_data;
        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( app.m_frame_data == first );
        REQUIRE( app.m_frame_length == 4096u );

        // a new size reallocates it
        g_qhyStub.memLength = 8192;
        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( app.m_frame_length == 8192u );
        REQUIRE( app.m_frame_data != nullptr );
    }
}

/// Verify the framegrabber acquisition hooks.
/**
 * \ingroup qhyCtrl_unit_test
 */
TEST_CASE( "qhyCtrl acquisition hooks", "[qhyCtrl][framegrabber]" )
{
    // clang-format off
    #ifdef QHYCTRL_TEST_DOXYGEN_REF
    qhyCtrl::startAcquisition();
    qhyCtrl::AbortAcquisition();
    qhyCtrl::reconfig();
    qhyCtrl::getFPS();
    qhyCtrl::fps();
    #endif
    // clang-format on

    resetQhyStub();

    qhyCtrl_test app( "camqhy" );
    app.m_camera = c_fakeCamera;

    SECTION( "startAcquisition is OPERATING" )
    {
        REQUIRE( app.startAcquisition() == 0 );
        REQUIRE( app.state() == stateCodes::OPERATING );
    }

    SECTION( "AbortAcquisition cancels the exposure" )
    {
        REQUIRE( app.AbortAcquisition() == 0 );
        REQUIRE( g_qhyStub.cancelCalls == 1 );
    }

    SECTION( "reconfig is a no-op" )
    {
        REQUIRE( app.reconfig() == 0 );
    }

    SECTION( "getFPS is the inverse of the exposure time" )
    {
        app.m_expTime = 0.5;
        REQUIRE( app.getFPS() == 2 );

        app.m_expTime = 0.25;
        REQUIRE( app.getFPS() == 4 );
    }

    SECTION( "fps reports m_fps" )
    {
        app.m_fps = 7.5;
        REQUIRE( app.fps() == Approx( 7.5 ) );
    }
}

/// Verify loadImageIntoStream() reads a frame from the camera and copies it into the stream.
/**
 * \ingroup qhyCtrl_unit_test
 */
TEST_CASE( "qhyCtrl loadImageIntoStream", "[qhyCtrl][framegrabber]" )
{
    // clang-format off
    #ifdef QHYCTRL_TEST_DOXYGEN_REF
    qhyCtrl::loadImageIntoStream(nullptr);
    #endif
    // clang-format on

    resetQhyStub();

    qhyCtrl_test app( "camqhy" );
    app.m_camera = c_fakeCamera;
    app.allocFrameBuffer( 4 * 3 * sizeof( uint16_t ) );

    g_qhyStub.frameW    = 4;
    g_qhyStub.frameH    = 3;
    g_qhyStub.frameBpp  = 14;
    g_qhyStub.frameData = { 0, 1, 2, 3, 10, 11, 12, 13, 20, 21, 22, 23 };

    std::vector<uint16_t> dest( 12, 0xFFFF );

    REQUIRE( app.loadImageIntoStream( dest.data() ) == 0 );

    REQUIRE( g_qhyStub.singleFrameCalls == 1 );
    REQUIRE( g_qhyStub.lastSingleFrameHandle == c_fakeCamera );
    REQUIRE( app.m_width == 4u );
    REQUIRE( app.m_height == 3u );
    REQUIRE( app.m_bits == 14u );
    REQUIRE( dest == g_qhyStub.frameData );

    SECTION( "an SDK exception is NOTCONNECTED" )
    {
        g_qhyStub.singleFrameThrows = true;

        REQUIRE( app.loadImageIntoStream( dest.data() ) == -1 );
        REQUIRE( app.state() == stateCodes::NOTCONNECTED );
    }
}

/// Verify getTemp() and getExpTime() read the camera parameters, and handle no camera and SDK exceptions.
/**
 * \ingroup qhyCtrl_unit_test
 */
TEST_CASE( "qhyCtrl temperature and exposure readout", "[qhyCtrl][temp][exptime]" )
{
    // clang-format off
    #ifdef QHYCTRL_TEST_DOXYGEN_REF
    qhyCtrl::getTemp();
    qhyCtrl::getExpTime();
    #endif
    // clang-format on

    resetQhyStub();

    qhyCtrl_test app( "camqhy" );

    g_qhyStub.params[CONTROL_CURTEMP]  = -12.5;
    g_qhyStub.params[CONTROL_EXPOSURE] = 250000;

    SECTION( "no camera does nothing" )
    {
        app.m_ccdTemp = 3;
        app.m_expTime = 4;

        REQUIRE( app.getTemp() == 0 );
        REQUIRE( app.getExpTime() == 0 );
        REQUIRE( app.m_ccdTemp == Approx( 3 ) );
        REQUIRE( app.m_expTime == Approx( 4 ) );
    }

    SECTION( "read from the camera" )
    {
        app.m_camera = c_fakeCamera;

        REQUIRE( app.getTemp() == 0 );
        REQUIRE( app.m_ccdTemp == Approx( -12.5 ) );

        // microseconds to seconds
        REQUIRE( app.getExpTime() == 0 );
        REQUIRE( app.m_expTime == Approx( 0.25 ) );
    }

    SECTION( "SDK exceptions are NOTCONNECTED" )
    {
        app.m_camera             = c_fakeCamera;
        g_qhyStub.getParamThrows = true;

        REQUIRE( app.getTemp() == -1 );
        REQUIRE( app.m_ccdTemp == Approx( -999 ) );
        REQUIRE( app.state() == stateCodes::NOTCONNECTED );

        REQUIRE( app.getExpTime() == -1 );
        REQUIRE( app.m_expTime == Approx( -999 ) );
    }
}

/// Verify setExpTime() sends the exposure time in microseconds.
/**
 * \ingroup qhyCtrl_unit_test
 */
TEST_CASE( "qhyCtrl setExpTime", "[qhyCtrl][exptime]" )
{
    // clang-format off
    #ifdef QHYCTRL_TEST_DOXYGEN_REF
    qhyCtrl::setExpTime();
    #endif
    // clang-format on

    resetQhyStub();

    qhyCtrl_test app( "camqhy" );
    app.m_expTimeSet = 0.02;

    SECTION( "no camera does nothing" )
    {
        REQUIRE( app.setExpTime() == 0 );
        REQUIRE( g_qhyStub.setParamCalls.empty() );
    }

    SECTION( "sends microseconds to the camera" )
    {
        app.m_camera = c_fakeCamera;

        REQUIRE( app.setExpTime() == 0 );
        REQUIRE( g_qhyStub.setParamCalls.size() == 1 );
        REQUIRE( g_qhyStub.setParamCalls[0].first == CONTROL_EXPOSURE );
        REQUIRE( g_qhyStub.setParamCalls[0].second == Approx( 20000 ) );
        REQUIRE( g_qhyStub.lastSetParamHandle == c_fakeCamera );
    }

    SECTION( "an SDK exception is an error" )
    {
        app.m_camera             = c_fakeCamera;
        g_qhyStub.setParamThrows = true;

        REQUIRE( app.setExpTime() == -1 );
    }
}

/// Verify the stdCamera exposure callback reaches qhyCtrl::setExpTime().
/**
 * \ingroup qhyCtrl_unit_test
 */
TEST_CASE( "qhyCtrl exptime callback", "[qhyCtrl][indi]" )
{
    // clang-format off
    #ifdef QHYCTRL_TEST_DOXYGEN_REF
    dev::stdCamera<qhyCtrl>::newCallBack_exptime(std::declval<const pcf::IndiProperty &>());
    qhyCtrl::setExpTime();
    #endif
    // clang-format on

    resetQhyStub();

    qhyCtrl_test app( "camqhy" );
    app.m_camera = c_fakeCamera;

    SECTION( "wrong device is rejected" )
    {
        pcf::IndiProperty ip( pcf::IndiProperty::Number );
        ip.setDevice( "other" );
        ip.setName( "exptime" );
        ip.add( pcf::IndiElement( "target", 0.1f ) );

        REQUIRE( app.newCallBack_exptime( ip ) == -1 );
        REQUIRE( g_qhyStub.setParamCalls.empty() );
    }

    SECTION( "a target sets the stdCamera target and calls setExpTime" )
    {
        pcf::IndiProperty ip( pcf::IndiProperty::Number );
        ip.setDevice( "camqhy" );
        ip.setName( "exptime" );
        ip.add( pcf::IndiElement( "target", 0.1f ) );

        REQUIRE( app.newCallBack_exptime( ip ) == 0 );
        REQUIRE( app.dev::stdCamera<qhyCtrl>::m_expTimeSet == Approx( 0.1 ) );
        REQUIRE( g_qhyStub.setParamCalls.size() == 1 );
        REQUIRE( g_qhyStub.setParamCalls[0].first == CONTROL_EXPOSURE );

        // Note: qhyCtrl::m_expTimeSet shadows stdCamera::m_expTimeSet, so the value sent to the camera
        // is qhyCtrl::m_expTimeSet, not the INDI target.  That is not asserted here.
    }
}

/// Verify the ROI and power-on stdCamera hooks.
/**
 * \ingroup qhyCtrl_unit_test
 */
TEST_CASE( "qhyCtrl ROI and power-on hooks", "[qhyCtrl][roi]" )
{
    // clang-format off
    #ifdef QHYCTRL_TEST_DOXYGEN_REF
    qhyCtrl::checkNextROI();
    qhyCtrl::setNextROI();
    qhyCtrl::powerOnDefaults();
    #endif
    // clang-format on

    resetQhyStub();

    qhyCtrl_test app( "camqhy" );
    app.m_camera = c_fakeCamera;

    SECTION( "checkNextROI accepts the ROI unchanged" )
    {
        app.m_nextROI.x = 12;
        app.m_nextROI.w = 34;
        REQUIRE( app.checkNextROI() == 0 );
        REQUIRE( app.m_nextROI.x == Approx( 12 ) );
        REQUIRE( app.m_nextROI.w == 34 );
    }

    SECTION( "setNextROI aborts and triggers a reconfiguration" )
    {
        app.m_modeName = "full";
        app.m_nextMode = "";
        app.m_reconfig = false;

        REQUIRE( app.setNextROI() == 0 );
        REQUIRE( g_qhyStub.cancelCalls == 1 );
        REQUIRE( app.state() == stateCodes::CONFIGURING );
        REQUIRE( app.m_nextMode == "full" );
        REQUIRE( app.m_reconfig == true );
    }

    SECTION( "powerOnDefaults sets the next ROI to the default" )
    {
        app.m_default_x     = 1000;
        app.m_default_y     = 500;
        app.m_default_w     = 256;
        app.m_default_h     = 128;
        app.m_default_bin_x = 2;
        app.m_default_bin_y = 4;

        REQUIRE( app.powerOnDefaults() == 0 );
        REQUIRE( app.m_nextROI.x == Approx( 1000 ) );
        REQUIRE( app.m_nextROI.y == Approx( 500 ) );
        REQUIRE( app.m_nextROI.w == 256 );
        REQUIRE( app.m_nextROI.h == 128 );
        REQUIRE( app.m_nextROI.bin_x == 2 );
        REQUIRE( app.m_nextROI.bin_y == 4 );
    }
}

/// Verify appStartup() fails when the SDK cannot be initialized, and appShutdown() releases the SDK.
/**
 * \ingroup qhyCtrl_unit_test
 */
TEST_CASE( "qhyCtrl SDK startup and shutdown", "[qhyCtrl][lifecycle]" )
{
    // clang-format off
    #ifdef QHYCTRL_TEST_DOXYGEN_REF
    qhyCtrl::appStartup();
    qhyCtrl::appShutdown();
    #endif
    // clang-format on

    resetQhyStub();

    qhyCtrl_test app( "camqhy" );

    SECTION( "appStartup fails if the SDK does not initialize" )
    {
        g_qhyStub.initReturn = QHYCCD_ERROR;

        REQUIRE( app.appStartup() == 1 );
        REQUIRE( g_qhyStub.initCalls == 1 );
        REQUIRE( app.state() == stateCodes::UNINITIALIZED );
    }

    SECTION( "appShutdown closes the camera and releases the SDK" )
    {
        app.m_camera = c_fakeCamera;

        REQUIRE( app.appShutdown() == 0 );
        REQUIRE( g_qhyStub.closeCalls == 1 );
        REQUIRE( g_qhyStub.lastCloseHandle == c_fakeCamera );
        REQUIRE( g_qhyStub.releaseCalls == 1 );
    }

    SECTION( "appShutdown with no camera only releases the SDK" )
    {
        REQUIRE( app.appShutdown() == 0 );
        REQUIRE( g_qhyStub.closeCalls == 0 );
        REQUIRE( g_qhyStub.releaseCalls == 1 );
    }

    SECTION( "appShutdown reports a release failure" )
    {
        g_qhyStub.releaseReturn = QHYCCD_ERROR;

        REQUIRE( app.appShutdown() == 1 );
        REQUIRE( app.m_retVal == QHYCCD_ERROR );
    }
}

/// Verify the telemeter interface records the camera telemetry.
/**
 * \ingroup qhyCtrl_unit_test
 */
TEST_CASE( "qhyCtrl telemetry", "[qhyCtrl][telem]" )
{
    // clang-format off
    #ifdef QHYCTRL_TEST_DOXYGEN_REF
    qhyCtrl::recordTelem(nullptr);
    qhyCtrl::checkRecordTimes();
    #endif
    // clang-format on

    resetQhyStub();

    qhyCtrl_test app( "camqhy" );

    REQUIRE( app.recordTelem( static_cast<const telem_stdcam *>( nullptr ) ) == 0 );
    REQUIRE( app.checkRecordTimes() == 0 );
}

} // namespace qhyCtrlTest

} // namespace libXWCTest
