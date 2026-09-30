/** \file dmRecon_test.cpp
 * \brief Catch2 tests for the dmRecon app.
 * \author Claude Code
 *
 * The callback bodies are live in this translation unit (testMacrosINDI.hpp is not included), so the device/name
 * validation of each callback is tested explicitly.
 *
 * \ingroup dmRecon_files
 */

#include "../../../tests/testXWC.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "../dmRecon.hpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup dmRecon_unit_test dmRecon Unit Tests
 * \brief Unit tests for the dmRecon application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `dmRecon` unit tests.
/** \ingroup dmRecon_unit_test
 */
namespace dmReconTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Directory used as `MILK_SHM_DIR` for the tests that create shared-memory streams.
constexpr const char *c_shmDir = "/tmp/dmRecon_test_shm";

/// Working directory for the tests, since `processImage()` writes `PInv.fits` and `wmodes.fits` to the cwd.
constexpr const char *c_workDir = "/tmp/dmRecon_test_work";

/// The DM modes shmimMonitor base of dmRecon.
typedef dev::shmimMonitor<dmRecon, dmModesShmimT> modesSMT;

/// The DM mask shmimMonitor base of dmRecon.
typedef dev::shmimMonitor<dmRecon, dmMaskShmimT> maskSMT;

/// The DM command shmimMonitor base of dmRecon.
typedef dev::shmimMonitor<dmRecon, dmCommandShmimT> commandSMT;

/// The frameGrabber base of dmRecon.
typedef dev::frameGrabber<dmRecon> fgT;

/// The 3x3 DM mask used by the tests (column-major raw data, symmetric so row/column order does not matter).
/** The good pixels are at raw indices 0, 1, 3, 4, 5 and 7.
 */
const std::vector<float> c_mask = { 1, 1, 0, 1, 1, 1, 0, 1, 0 };

/// The raw indices of the good pixels in `c_mask`.
const std::vector<size_t> c_maskIDX = { 0, 1, 3, 4, 5, 7 };

/// Value of mode B at each good pixel of `c_mask`, in `c_maskIDX` order.
const std::vector<float> c_modeB = { 1, -1, 1, -1, 1, -1 };

/// Build a 3x3 frame with `goodVals` at the good pixels of `c_mask` and `badVal` elsewhere.
/**
 * \returns the raw 9-pixel frame
 */
std::vector<float> maskedFrame( const std::vector<float> &goodVals, /**< [in] the values at the good pixels */
                                float                     badVal    /**< [in] the value at the masked-out pixels */
)
{
    std::vector<float> frame( 9, badVal );
    for( size_t n = 0; n < c_maskIDX.size(); ++n )
    {
        frame[c_maskIDX[n]] = goodVals[n];
    }
    return frame;
}

/// Build the two-mode cube: mode A is 2 at every good pixel, mode B is +/-1; masked pixels are 100.
/**
 * \returns the raw 3x3x2 cube
 */
std::vector<float> twoModeCube()
{
    std::vector<float> cube = maskedFrame( std::vector<float>( 6, 2.0f ), 100 );
    std::vector<float> b    = maskedFrame( c_modeB, 100 );
    cube.insert( cube.end(), b.begin(), b.end() );
    return cube;
}

/// Change into a working directory for the lifetime of the object.
struct cwdGuard
{
    /// The directory to restore.
    std::filesystem::path m_old;

    /// Create and change into `dir`.
    explicit cwdGuard( const std::string &dir /**< [in] the working directory */ )
    {
        m_old = std::filesystem::current_path();
        std::filesystem::create_directories( dir );
        std::filesystem::current_path( dir );
    }

    /// Change back to the original directory.
    ~cwdGuard()
    {
        std::filesystem::current_path( m_old );
    }
};

/// Point ImageStreamIO at a private shared-memory directory.
void useTestShmDir()
{
    std::filesystem::create_directories( c_shmDir );
    setenv( "MILK_SHM_DIR", c_shmDir, 1 );
}

/// Test harness exposing dmRecon internals.
class dmRecon_test : public dmRecon
{
  public:
    /// Construct a harness with the given device name.
    explicit dmRecon_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;
        sem_init( &m_smSemaphore, 0, 0 );
    }

    /// Destroy the semaphore and close any frameGrabber stream opened by `configureAcquisition()`.
    ~dmRecon_test() noexcept
    {
        if( fgT::m_imageStream != nullptr )
        {
            ImageStreamIO_closeIm( fgT::m_imageStream );
            free( fgT::m_imageStream );
            fgT::m_imageStream = nullptr;
        }

        sem_destroy( &m_smSemaphore );
    }

    using dmRecon::m_command;
    using dmRecon::m_commandReady;
    using dmRecon::m_depth;
    using dmRecon::m_dmMaskReady;
    using dmRecon::m_dmModesReady;
    using dmRecon::m_fgWaiting;
    using dmRecon::m_fps;
    using dmRecon::m_fpsSource;
    using dmRecon::m_gpuIndex;
    using dmRecon::m_height;
    using dmRecon::m_inverseNumModes;
    using dmRecon::m_loopNumber;
    using dmRecon::m_mask;
    using dmRecon::m_maskIDX;
    using dmRecon::m_modeval;
    using dmRecon::m_modevalDiff;
    using dmRecon::m_modevalMon;
    using dmRecon::m_modevals;
    using dmRecon::m_monShmimName;
    using dmRecon::m_numModes;
    using dmRecon::m_PInv;
    using dmRecon::m_respM;
    using dmRecon::m_respMPath;
    using dmRecon::m_smSemaphore;
    using dmRecon::m_updated;
    using dmRecon::m_useGPU;
    using dmRecon::m_width;
    using dmRecon::m_writeDMf;

    using dmRecon::m_indiP_fps;
    using dmRecon::m_indiP_fpsSource;
    using dmRecon::m_indiP_writeDMf;

    using dmRecon::newCallBack_m_indiP_writeDMf;
    using dmRecon::setCallBack_m_indiP_fpsSource;

    /// Access the width of the DM modes stream.
    /**
     * \returns a reference to the modes shmimMonitor width
     */
    uint32_t &modesWidth()
    {
        return modesSMT::m_width;
    }

    /// Access the height of the DM modes stream.
    /**
     * \returns a reference to the modes shmimMonitor height
     */
    uint32_t &modesHeight()
    {
        return modesSMT::m_height;
    }

    /// Access the depth (number of modes) of the DM modes stream.
    /**
     * \returns a reference to the modes shmimMonitor depth
     */
    uint32_t &modesDepth()
    {
        return modesSMT::m_depth;
    }

    /// Access the width of the DM mask stream.
    /**
     * \returns a reference to the mask shmimMonitor width
     */
    uint32_t &maskWidth()
    {
        return maskSMT::m_width;
    }

    /// Access the height of the DM mask stream.
    /**
     * \returns a reference to the mask shmimMonitor height
     */
    uint32_t &maskHeight()
    {
        return maskSMT::m_height;
    }

    /// Access the width of the DM command stream.
    /**
     * \returns a reference to the command shmimMonitor width
     */
    uint32_t &commandWidth()
    {
        return commandSMT::m_width;
    }

    /// Access the height of the DM command stream.
    /**
     * \returns a reference to the command shmimMonitor height
     */
    uint32_t &commandHeight()
    {
        return commandSMT::m_height;
    }

    /// Access the DM modes restart flag.
    /**
     * \returns a reference to the modes shmimMonitor restart flag
     */
    bool &modesRestart()
    {
        return modesSMT::m_restart;
    }

    /// Access the DM mask restart flag.
    /**
     * \returns a reference to the mask shmimMonitor restart flag
     */
    bool &maskRestart()
    {
        return maskSMT::m_restart;
    }

    /// Access the DM command restart flag.
    /**
     * \returns a reference to the command shmimMonitor restart flag
     */
    bool &commandRestart()
    {
        return commandSMT::m_restart;
    }

    /// Access the DM modes shmim name.
    /**
     * \returns the modes shmim name
     */
    std::string modesShmimName()
    {
        return modesSMT::m_shmimName;
    }

    /// Access the DM mask shmim name.
    /**
     * \returns the mask shmim name
     */
    std::string maskShmimName()
    {
        return maskSMT::m_shmimName;
    }

    /// Access the DM command shmim name.
    /**
     * \returns the command shmim name
     */
    std::string commandShmimName()
    {
        return commandSMT::m_shmimName;
    }

    /// Check that all three shmimMonitors get the existing image first.
    /**
     * \returns true if `m_getExistingFirst` is set for the modes, mask and command monitors
     */
    bool allGetExistingFirst()
    {
        return modesSMT::m_getExistingFirst && maskSMT::m_getExistingFirst && commandSMT::m_getExistingFirst;
    }

    /// Access the frameGrabber shmim name.
    /**
     * \returns a reference to the frameGrabber shmim name
     */
    std::string &fgShmimName()
    {
        return fgT::m_shmimName;
    }

    /// Access the frameGrabber image width.
    /**
     * \returns the frameGrabber width
     */
    uint32_t fgWidth()
    {
        return fgT::m_width;
    }

    /// Access the frameGrabber image height.
    /**
     * \returns the frameGrabber height
     */
    uint32_t fgHeight()
    {
        return fgT::m_height;
    }

    /// Access the frameGrabber data type.
    /**
     * \returns the frameGrabber ImageStreamIO data type
     */
    uint8_t fgDataType()
    {
        return fgT::m_dataType;
    }

    /// Access the frameGrabber reconfigure flag.
    /**
     * \returns a reference to the frameGrabber reconfigure flag
     */
    bool &fgReconfig()
    {
        return fgT::m_reconfig;
    }

    /// Access the frameGrabber owned-shmim flag.
    /**
     * \returns the frameGrabber owned-shmim flag
     */
    bool fgOwnShmim()
    {
        return fgT::m_ownShmim;
    }

    /// Access the frameGrabber image stream pointer.
    /**
     * \returns the frameGrabber image stream (nullptr if not open)
     */
    IMAGE *fgImageStream()
    {
        return fgT::m_imageStream;
    }

    /// Run `setupConfig()`, read a configuration file, and run `loadConfig()`.
    void configure( const std::string &fname /**< [in] path of the configuration file to read */ )
    {
        setupConfig();
        config.readConfig( fname );
        loadConfig();
    }

    /// Create the INDI properties the way `appStartup()` does, without registering them or starting threads.
    void setupProperties()
    {
        m_indiP_fpsSource.setDevice( m_fpsSource );
        m_indiP_fpsSource.setName( "fps" );

        createROIndiNumber( m_indiP_fps, "fps" );
        m_indiP_fps.add( pcf::IndiElement( "current" ) );

        createStandardIndiToggleSw( m_indiP_writeDMf, "writeDMf" );
    }

    /// Load the 3x3 `c_mask` through the mask `processImage()`.
    /**
     * \returns the processImage return value
     */
    int loadMask()
    {
        maskWidth()           = 3;
        maskHeight()          = 3;
        std::vector<float> mk = c_mask;
        return processImage( mk.data(), dmMaskShmimT() );
    }

    /// Allocate and load the two-mode cube through the modes `allocate()` and `processImage()`.
    /**
     * \returns the processImage return value
     */
    int loadTwoModes()
    {
        modesWidth()  = 3;
        modesHeight() = 3;
        modesDepth()  = 2;

        if( allocate( dmModesShmimT() ) < 0 )
        {
            return -1;
        }

        std::vector<float> cube = twoModeCube();

        cwdGuard cwd( c_workDir );
        return processImage( cube.data(), dmModesShmimT() );
    }

    /// Set up the command stream sizes and allocate the command buffers and output streams.
    /** Requires `useTestShmDir()`, and an existing `aol<loop>_modevalDM` stream with one row per mode.
     *
     * \returns the allocate return value
     */
    int allocateCommand()
    {
        commandWidth()  = 3;
        commandHeight() = 3;
        m_fgWaiting     = true;
        return allocate( dmCommandShmimT() );
    }
};

/// Build a Number property with a single element.
/**
 * \returns the INDI property
 */
pcf::IndiProperty numberProp( const std::string &device, /**< [in] INDI device name */
                              const std::string &name,   /**< [in] INDI property name */
                              const std::string &el,     /**< [in] element name */
                              float              value   /**< [in] element value */
)
{
    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( device );
    ip.setName( name );
    ip.add( pcf::IndiElement( el, value ) );
    return ip;
}

/// Build a Switch property with a single element.
/**
 * \returns the INDI property
 */
pcf::IndiProperty switchProp( const std::string                &device, /**< [in] INDI device name */
                              const std::string                &name,   /**< [in] INDI property name */
                              const std::string                &el,     /**< [in] element name */
                              pcf::IndiElement::SwitchStateType state   /**< [in] the switch state */
)
{
    pcf::IndiProperty ip( pcf::IndiProperty::Switch );
    ip.setDevice( device );
    ip.setName( name );
    ip.add( pcf::IndiElement( el, state ) );
    return ip;
}

/// \endcond

/// Verify the dmRecon configuration defaults and overrides, including the derived stream names.
/**
 * \ingroup dmRecon_unit_test
 */
TEST_CASE( "dmRecon configuration", "[dmRecon]" )
{
    // clang-format off
    #ifdef DMRECON_TEST_DOXYGEN_REF
    dmRecon::setupConfig();
    dmRecon::loadConfig();
    dmRecon::loadConfigImpl( mx::app::appConfigurator() );
    #endif
    // clang-format on

    const std::string fname = "/tmp/dmRecon_test_config.conf";

    SECTION( "defaults" )
    {
        mx::app::writeConfigFile( fname, { "none" }, { "nada" }, { "0" } );

        dmRecon_test app( "dmrecon" );
        app.configure( fname );

        REQUIRE( app.shutdown() == 0 );
        REQUIRE( app.fgOwnShmim() == false );
        REQUIRE( app.m_loopNumber == 1 );
        REQUIRE( app.m_respMPath == "" );
        REQUIRE( app.m_fpsSource == "camwfs" );
        REQUIRE( app.m_numModes == 0 );
        REQUIRE( app.m_inverseNumModes == 0 );
        REQUIRE( app.m_gpuIndex == 0 );
        REQUIRE( app.m_useGPU == false );

        REQUIRE( app.modesShmimName() == "aol1_CMmodesDM" );
        REQUIRE( app.commandShmimName() == "dm01disp_delta" );
        REQUIRE( app.maskShmimName() == "dm01disp_actmask" );
        REQUIRE( app.fgShmimName() == "aol1_modevalDMf" );
        REQUIRE( app.m_monShmimName == "aol1_modevalDMf_mon" );
        REQUIRE( app.allGetExistingFirst() == true );
    }

    SECTION( "loop number sets the stream names" )
    {
        mx::app::writeConfigFile(
            fname,
            { "recon", "recon", "recon", "recon", "recon", "recon", "recon" },
            { "loopNumber", "respMPath", "numModes", "inverseNumModes", "fpsSource", "gpuIndex", "useGPU" },
            { "12", "/tmp/respM.fits", "40", "35", "camlowfs", "2", "true" } );

        dmRecon_test app( "dmrecon" );
        app.configure( fname );

        REQUIRE( app.shutdown() == 0 );
        REQUIRE( app.m_loopNumber == 12 );
        REQUIRE( app.m_respMPath == "/tmp/respM.fits" );
        REQUIRE( app.m_numModes == 40 );
        REQUIRE( app.m_inverseNumModes == 35 );
        REQUIRE( app.m_fpsSource == "camlowfs" );
        REQUIRE( app.m_gpuIndex == 2 );
        REQUIRE( app.m_useGPU == true );

        REQUIRE( app.modesShmimName() == "aol12_CMmodesDM" );
        REQUIRE( app.commandShmimName() == "dm12disp_delta" );
        REQUIRE( app.maskShmimName() == "dm12disp_actmask" );
        REQUIRE( app.fgShmimName() == "aol12_modevalDMf" );
        REQUIRE( app.m_monShmimName == "aol12_modevalDMf_mon" );
    }

    SECTION( "stream names can be overridden" )
    {
        mx::app::writeConfigFile( fname,
                                  { "recon", "dmModes", "dmMask", "dmCommand", "framegrabber" },
                                  { "loopNumber", "shmimName", "shmimName", "shmimName", "shmimName" },
                                  { "2", "modes_x", "mask_x", "cmd_x", "fg_x" } );

        dmRecon_test app( "dmrecon" );
        app.configure( fname );

        REQUIRE( app.shutdown() == 0 );
        REQUIRE( app.modesShmimName() == "modes_x" );
        REQUIRE( app.maskShmimName() == "mask_x" );
        REQUIRE( app.commandShmimName() == "cmd_x" );
        REQUIRE( app.fgShmimName() == "fg_x" );
        REQUIRE( app.m_monShmimName == "fg_x_mon" );
    }

    remove( fname.c_str() );
}

/// Verify the mask processing and the good-pixel index.
/**
 * \ingroup dmRecon_unit_test
 */
TEST_CASE( "dmRecon mask processing", "[dmRecon]" )
{
    // clang-format off
    #ifdef DMRECON_TEST_DOXYGEN_REF
    dmRecon::allocate( dmMaskShmimT() );
    dmRecon::processImage( nullptr, dmMaskShmimT() );
    #endif
    // clang-format on

    dmRecon_test app( "dmrecon" );

    app.commandRestart() = false;
    REQUIRE( app.allocate( dmMaskShmimT() ) == 0 );
    REQUIRE( app.m_dmMaskReady.load() == false );
    REQUIRE( app.commandRestart() == true );

    REQUIRE( app.loadMask() == 0 );
    REQUIRE( app.m_dmMaskReady.load() == true );
    REQUIRE( app.m_mask.rows() == 3 );
    REQUIRE( app.m_mask.cols() == 3 );
    REQUIRE( app.m_maskIDX == c_maskIDX );

    // A new mask image once ready triggers a restart of all three monitors and does not change the mask
    app.modesRestart()   = false;
    app.maskRestart()    = false;
    app.commandRestart() = false;

    std::vector<float> allGood( 9, 1.0f );
    REQUIRE( app.processImage( allGood.data(), dmMaskShmimT() ) == 0 );
    REQUIRE( app.modesRestart() == true );
    REQUIRE( app.maskRestart() == true );
    REQUIRE( app.commandRestart() == true );
    REQUIRE( app.m_maskIDX == c_maskIDX );
}

/// Verify the DM modes allocation size logic.
/**
 * \ingroup dmRecon_unit_test
 */
TEST_CASE( "dmRecon modes allocation", "[dmRecon]" )
{
    // clang-format off
    #ifdef DMRECON_TEST_DOXYGEN_REF
    dmRecon::allocate( dmModesShmimT() );
    #endif
    // clang-format on

    dmRecon_test app( "dmrecon" );
    app.modesWidth()  = 3;
    app.modesHeight() = 3;
    app.modesDepth()  = 4;

    SECTION( "all modes by default, ready once the mask matches" )
    {
        REQUIRE( app.loadMask() == 0 );

        app.commandRestart() = false;
        app.modesRestart()   = false;

        REQUIRE( app.allocate( dmModesShmimT() ) == 0 );
        REQUIRE( app.m_width == 3u );
        REQUIRE( app.m_height == 3u );
        REQUIRE( app.m_depth == 4u );
        REQUIRE( app.commandRestart() == true );
        REQUIRE( app.modesRestart() == false );
    }

    SECTION( "numModes limits the number of modes" )
    {
        REQUIRE( app.loadMask() == 0 );

        app.m_numModes = 2;
        REQUIRE( app.allocate( dmModesShmimT() ) == 0 );
        REQUIRE( app.m_depth == 2u );

        // but never more than are in the stream
        app.m_numModes = 10;
        REQUIRE( app.allocate( dmModesShmimT() ) == 0 );
        REQUIRE( app.m_depth == 4u );
    }

    SECTION( "waits for the mask" )
    {
        app.modesRestart() = false;

        REQUIRE( app.allocate( dmModesShmimT() ) == 0 ); // sleeps 1 second
        REQUIRE( app.modesRestart() == true );
        REQUIRE( app.m_dmModesReady.load() == false );
    }
}

/// Verify the pseudo-inverse of the masked DM modes.
/**
 * \ingroup dmRecon_unit_test
 */
TEST_CASE( "dmRecon modes pseudo-inverse", "[dmRecon]" )
{
    // clang-format off
    #ifdef DMRECON_TEST_DOXYGEN_REF
    dmRecon::processImage( nullptr, dmModesShmimT() );
    #endif
    // clang-format on

    SECTION( "all modes" )
    {
        dmRecon_test app( "dmrecon" );
        REQUIRE( app.loadMask() == 0 );
        REQUIRE( app.loadTwoModes() == 0 );

        REQUIRE( app.m_dmModesReady.load() == true );
        REQUIRE( app.m_PInv.rows() == 2 );
        REQUIRE( app.m_PInv.cols() == 6 );

        // The modes are orthogonal on the mask, so the pseudo-inverse rows are mode / |mode|^2.
        // The masked-out pixels (value 100) must not contribute.
        for( int n = 0; n < 6; ++n )
        {
            REQUIRE( app.m_PInv( 0, n ) == Approx( 2.0 / 24.0 ).margin( 1e-5 ) );
            REQUIRE( app.m_PInv( 1, n ) == Approx( c_modeB[n] / 6.0 ).margin( 1e-5 ) );
        }

        // A second modes image once ready triggers a restart of all three monitors
        app.modesRestart()   = false;
        app.maskRestart()    = false;
        app.commandRestart() = false;

        std::vector<float> cube = twoModeCube();
        REQUIRE( app.processImage( cube.data(), dmModesShmimT() ) == 0 );
        REQUIRE( app.modesRestart() == true );
        REQUIRE( app.maskRestart() == true );
        REQUIRE( app.commandRestart() == true );
    }

    SECTION( "inverseNumModes truncates the inverse" )
    {
        dmRecon_test app( "dmrecon" );
        app.m_inverseNumModes = 1;
        REQUIRE( app.loadMask() == 0 );
        REQUIRE( app.loadTwoModes() == 0 );

        REQUIRE( app.m_PInv.rows() == 2 );
        REQUIRE( app.m_PInv.cols() == 6 );

        // Only the mode with the largest singular value (mode A) is kept
        for( int n = 0; n < 6; ++n )
        {
            REQUIRE( app.m_PInv( 0, n ) == Approx( 2.0 / 24.0 ).margin( 1e-5 ) );
            REQUIRE( app.m_PInv( 1, n ) == Approx( 0.0 ).margin( 1e-5 ) );
        }
    }

    SECTION( "numModes uses only the first modes" )
    {
        dmRecon_test app( "dmrecon" );
        app.m_numModes = 1;
        REQUIRE( app.loadMask() == 0 );
        REQUIRE( app.loadTwoModes() == 0 );

        REQUIRE( app.m_depth == 1u );
        REQUIRE( app.m_PInv.rows() == 1 );
        REQUIRE( app.m_PInv.cols() == 6 );
        for( int n = 0; n < 6; ++n )
        {
            REQUIRE( app.m_PInv( 0, n ) == Approx( 2.0 / 24.0 ).margin( 1e-5 ) );
        }
    }

    SECTION( "response matrix converts and renormalizes the modes" )
    {
        dmRecon_test app( "dmrecon" );

        // Identity response: the modes are unchanged except for the renormalization by
        // sqrt(sum^2 / (rows*cols)) / sqrt(sum^2 / nmask) = sqrt(nmask / 9)
        app.m_respM.resize( 9, 9 );
        app.m_respM.setZero();
        for( int n = 0; n < 9; ++n )
        {
            app.m_respM( n, n ) = 1;
        }
        app.m_width  = 3;
        app.m_height = 3;

        REQUIRE( app.loadMask() == 0 );
        REQUIRE( app.loadTwoModes() == 0 );

        double k = sqrt( 6.0 / 9.0 );

        REQUIRE( app.m_PInv.rows() == 2 );
        REQUIRE( app.m_PInv.cols() == 6 );
        for( int n = 0; n < 6; ++n )
        {
            REQUIRE( app.m_PInv( 0, n ) == Approx( 2.0 / 24.0 / k ).margin( 1e-5 ) );
            REQUIRE( app.m_PInv( 1, n ) == Approx( c_modeB[n] / 6.0 / k ).margin( 1e-5 ) );
        }
    }

    std::filesystem::remove_all( c_workDir );
}

/// Verify the DM command allocation checks.
/**
 * \ingroup dmRecon_unit_test
 */
TEST_CASE( "dmRecon command allocation waits for its inputs", "[dmRecon]" )
{
    // clang-format off
    #ifdef DMRECON_TEST_DOXYGEN_REF
    dmRecon::allocate( dmCommandShmimT() );
    #endif
    // clang-format on

    SECTION( "modes and mask not ready" )
    {
        dmRecon_test app( "dmrecon" );
        app.commandRestart() = false;
        app.fgReconfig()     = false;

        REQUIRE( app.allocate( dmCommandShmimT() ) == 0 ); // sleeps 1 second
        REQUIRE( app.m_commandReady.load() == false );
        REQUIRE( app.commandRestart() == true );
        REQUIRE( app.fgReconfig() == true );
    }

    SECTION( "frameGrabber not waiting" )
    {
        dmRecon_test app( "dmrecon" );
        REQUIRE( app.loadMask() == 0 );
        REQUIRE( app.loadTwoModes() == 0 );

        app.commandWidth()   = 3;
        app.commandHeight()  = 3;
        app.m_fgWaiting      = false;
        app.commandRestart() = false;
        app.fgReconfig()     = false;

        REQUIRE( app.allocate( dmCommandShmimT() ) == 0 ); // sleeps 1 second
        REQUIRE( app.m_commandReady.load() == false );
        REQUIRE( app.commandRestart() == true );
        REQUIRE( app.fgReconfig() == true );
    }

    std::filesystem::remove_all( c_workDir );
}

/// Verify the full reconstruction of a DM command into mode amplitudes and the output streams.
/**
 * \ingroup dmRecon_unit_test
 */
TEST_CASE( "dmRecon reconstructs a DM command", "[dmRecon]" )
{
    // clang-format off
    #ifdef DMRECON_TEST_DOXYGEN_REF
    dmRecon::allocate( dmCommandShmimT() );
    dmRecon::processImage( nullptr, dmCommandShmimT() );
    dmRecon::acquireAndCheckValid();
    dmRecon::loadImageIntoStream( nullptr );
    #endif
    // clang-format on

    useTestShmDir();

    {
        // The reference mode values, which the diff stream is relative to
        mx::improc::milkImage<float> refModeval;
        refModeval.create( "aol7_modevalDM", 2, 1 );
        refModeval( 0, 0 ) = 1.0f;
        refModeval( 1, 0 ) = 2.0f;

        dmRecon_test app( "dmrecon" );
        app.m_loopNumber   = 7;
        app.m_monShmimName = "dmRecon_test_mon";

        REQUIRE( app.loadMask() == 0 );
        REQUIRE( app.loadTwoModes() == 0 );

        // A command before allocation is ignored and restarts the command monitor
        std::vector<float> cmd = maskedFrame( { 6.5f, 5.5f, 6.5f, 5.5f, 6.5f, 5.5f }, 1000 );
        app.commandRestart()   = false;
        REQUIRE( app.processImage( cmd.data(), dmCommandShmimT() ) == 0 );
        REQUIRE( app.commandRestart() == true );
        REQUIRE( app.m_updated.load() == false );

        REQUIRE( app.allocateCommand() == 0 );
        REQUIRE( app.m_commandReady.load() == true );
        REQUIRE( app.m_updated.load() == false );
        REQUIRE( app.m_command.rows() == 6 );
        REQUIRE( app.m_modevals.rows() == 2 );
        REQUIRE( app.m_modevalMon.rows() == 2u );
        REQUIRE( app.m_modevalDiff.rows() == 2u );

        SECTION( "command is 3 A + 0.5 B" )
        {
            REQUIRE( app.processImage( cmd.data(), dmCommandShmimT() ) == 0 );
            REQUIRE( app.m_updated.load() == true );

            for( size_t n = 0; n < c_maskIDX.size(); ++n )
            {
                REQUIRE( app.m_command( n, 0 ) == cmd[c_maskIDX[n]] );
            }

            REQUIRE( app.m_modevals( 0, 0 ) == Approx( 3.0 ).margin( 1e-4 ) );
            REQUIRE( app.m_modevals( 1, 0 ) == Approx( 0.5 ).margin( 1e-4 ) );

            REQUIRE( app.m_modevalMon( 0, 0 ) == Approx( 3.0 ).margin( 1e-4 ) );
            REQUIRE( app.m_modevalMon( 1, 0 ) == Approx( 0.5 ).margin( 1e-4 ) );

            REQUIRE( app.m_modevalDiff( 0, 0 ) == Approx( 2.0 ).margin( 1e-4 ) );
            REQUIRE( app.m_modevalDiff( 1, 0 ) == Approx( -1.5 ).margin( 1e-4 ) );

            // Not writing DMf, so the frameGrabber is not triggered
            int sval = -1;
            sem_getvalue( &app.m_smSemaphore, &sval );
            REQUIRE( sval == 0 );

            // The frameGrabber gets the latest mode values
            std::vector<float> dest( 2, 0 );
            REQUIRE( app.loadImageIntoStream( dest.data() ) == 0 );
            REQUIRE( dest[0] == Approx( 3.0 ).margin( 1e-4 ) );
            REQUIRE( dest[1] == Approx( 0.5 ).margin( 1e-4 ) );
        }

        SECTION( "writing DMf triggers the frameGrabber" )
        {
            app.m_writeDMf = true;
            REQUIRE( app.processImage( cmd.data(), dmCommandShmimT() ) == 0 );

            int sval = -1;
            sem_getvalue( &app.m_smSemaphore, &sval );
            REQUIRE( sval == 1 );

            REQUIRE( app.acquireAndCheckValid() == 0 );

            sem_getvalue( &app.m_smSemaphore, &sval );
            REQUIRE( sval == 0 );
        }

        SECTION( "a posted frame without an update is not valid" )
        {
            app.m_updated = false;
            sem_post( &app.m_smSemaphore );
            REQUIRE( app.acquireAndCheckValid() == 1 );
        }
    }

    std::filesystem::remove_all( c_shmDir );
    std::filesystem::remove_all( c_workDir );
}

/// Verify the frameGrabber interface functions.
/**
 * \ingroup dmRecon_unit_test
 */
TEST_CASE( "dmRecon frameGrabber interface", "[dmRecon]" )
{
    // clang-format off
    #ifdef DMRECON_TEST_DOXYGEN_REF
    dmRecon::configureAcquisition();
    dmRecon::fps();
    dmRecon::startAcquisition();
    dmRecon::acquireAndCheckValid();
    dmRecon::reconfig();
    #endif
    // clang-format on

    useTestShmDir();

    SECTION( "configureAcquisition waits for the command" )
    {
        dmRecon_test app( "dmrecon" );
        app.m_fgWaiting = false;

        REQUIRE( app.configureAcquisition() == -1 ); // sleeps 0.1 seconds
        REQUIRE( app.m_fgWaiting.load() == true );
    }

    SECTION( "acquireAndCheckValid is not valid before the command is ready" )
    {
        dmRecon_test app( "dmrecon" );
        REQUIRE( app.acquireAndCheckValid() == 1 );
    }

    SECTION( "configureAcquisition sets the image size and waits for the stream" )
    {
        dmRecon_test app( "dmrecon" );
        app.m_modevals.resize( 5, 1 );
        app.m_commandReady = true;
        app.m_fgWaiting    = true;
        app.fgShmimName()  = "dmRecon_test_nofg";

        REQUIRE( app.configureAcquisition() == 1 );
        REQUIRE( app.m_fgWaiting.load() == false );
        REQUIRE( app.fgWidth() == 5u );
        REQUIRE( app.fgHeight() == 1u );
        REQUIRE( app.fgDataType() == _DATATYPE_FLOAT );
        REQUIRE( app.fgImageStream() == nullptr );
    }

    SECTION( "configureAcquisition opens an existing stream" )
    {
        mx::improc::milkImage<float> fg;
        fg.create( "dmRecon_test_fg", 5, 1 );

        dmRecon_test app( "dmrecon" );
        app.m_modevals.resize( 5, 1 );
        app.m_commandReady = true;
        app.fgShmimName()  = "dmRecon_test_fg";

        REQUIRE( app.configureAcquisition() == 0 );
        REQUIRE( app.fgImageStream() != nullptr );
        REQUIRE( app.fgImageStream()->md[0].size[0] == 5u );
    }

    SECTION( "fps, startAcquisition and reconfig" )
    {
        dmRecon_test app( "dmrecon" );
        app.m_fps = 1234.5f;
        REQUIRE( app.fps() == Approx( 1234.5f ) );
        REQUIRE( app.startAcquisition() == 0 );
        REQUIRE( app.reconfig() == 0 );
    }

    std::filesystem::remove_all( c_shmDir );
}

/// Verify setGPU without GPU support.
/**
 * \ingroup dmRecon_unit_test
 */
TEST_CASE( "dmRecon setGPU", "[dmRecon]" )
{
    // clang-format off
    #ifdef DMRECON_TEST_DOXYGEN_REF
    dmRecon::setGPU();
    #endif
    // clang-format on

    SECTION( "no GPU requested" )
    {
        dmRecon_test app( "dmrecon" );
        app.m_useGPU = false;
        REQUIRE( app.setGPU() == 0 );
        REQUIRE( app.m_useGPU == false );
    }

    // clang-format off
    #ifndef MXLIB_CUDA
    // clang-format on
    SECTION( "GPU requested but mxlib has no CUDA" )
    {
        dmRecon_test app( "dmrecon" );
        app.m_useGPU = true;
        REQUIRE( app.setGPU() == -1 );
        REQUIRE( app.m_useGPU == false );
    }
    // clang-format off
    #endif
    // clang-format on
}

/// Verify the fps source set callback.
/**
 * \ingroup dmRecon_unit_test
 */
TEST_CASE( "dmRecon fps source callback", "[dmRecon]" )
{
    // clang-format off
    #ifdef DMRECON_TEST_DOXYGEN_REF
    dmRecon::setCallBack_m_indiP_fpsSource( pcf::IndiProperty() );
    #endif
    // clang-format on

    dmRecon_test app( "dmrecon" );
    app.setupProperties();

    SECTION( "wrong device is rejected" )
    {
        REQUIRE( app.setCallBack_m_indiP_fpsSource( numberProp( "camlowfs", "fps", "current", 100.0f ) ) == -1 );
        REQUIRE( app.m_fps == 0 );
    }

    SECTION( "wrong name is rejected" )
    {
        REQUIRE( app.setCallBack_m_indiP_fpsSource( numberProp( "camwfs", "wrong", "current", 100.0f ) ) == -1 );
        REQUIRE( app.m_fps == 0 );
    }

    SECTION( "missing current element is rejected" )
    {
        REQUIRE( app.setCallBack_m_indiP_fpsSource( numberProp( "camwfs", "fps", "target", 100.0f ) ) == -1 );
        REQUIRE( app.m_fps == 0 );
    }

    SECTION( "current sets the fps" )
    {
        REQUIRE( app.setCallBack_m_indiP_fpsSource( numberProp( "camwfs", "fps", "current", 1500.0f ) ) == 0 );
        REQUIRE( app.m_fps == Approx( 1500.0f ) );
        REQUIRE( app.fps() == Approx( 1500.0f ) );
    }
}

/// Verify the writeDMf new callback.
/**
 * \ingroup dmRecon_unit_test
 */
TEST_CASE( "dmRecon writeDMf callback", "[dmRecon]" )
{
    // clang-format off
    #ifdef DMRECON_TEST_DOXYGEN_REF
    dmRecon::newCallBack_m_indiP_writeDMf( pcf::IndiProperty() );
    #endif
    // clang-format on

    dmRecon_test app( "dmrecon" );
    app.setupProperties();

    SECTION( "wrong device is rejected" )
    {
        REQUIRE( app.newCallBack_m_indiP_writeDMf(
                     switchProp( "wrong", "writeDMf", "toggle", pcf::IndiElement::On ) ) == -1 );
        REQUIRE( app.m_writeDMf.load() == false );
    }

    SECTION( "wrong name is rejected" )
    {
        REQUIRE( app.newCallBack_m_indiP_writeDMf( switchProp( "dmrecon", "wrong", "toggle", pcf::IndiElement::On ) ) ==
                 -1 );
        REQUIRE( app.m_writeDMf.load() == false );
    }

    SECTION( "missing toggle element is rejected" )
    {
        REQUIRE( app.newCallBack_m_indiP_writeDMf(
                     switchProp( "dmrecon", "writeDMf", "other", pcf::IndiElement::On ) ) == -1 );
        REQUIRE( app.m_writeDMf.load() == false );
    }

    SECTION( "toggle on and off" )
    {
        REQUIRE( app.newCallBack_m_indiP_writeDMf(
                     switchProp( "dmrecon", "writeDMf", "toggle", pcf::IndiElement::On ) ) == 0 );
        REQUIRE( app.m_writeDMf.load() == true );

        REQUIRE( app.newCallBack_m_indiP_writeDMf(
                     switchProp( "dmrecon", "writeDMf", "toggle", pcf::IndiElement::Off ) ) == 0 );
        REQUIRE( app.m_writeDMf.load() == false );
    }
}

/// Verify that appShutdown succeeds when no threads were started.
/**
 * \ingroup dmRecon_unit_test
 */
TEST_CASE( "dmRecon appShutdown without threads", "[dmRecon]" )
{
    // clang-format off
    #ifdef DMRECON_TEST_DOXYGEN_REF
    dmRecon::appShutdown();
    #endif
    // clang-format on

    dmRecon_test app( "dmrecon" );
    REQUIRE( app.appShutdown() == 0 );
}

} // namespace dmReconTest

} // namespace libXWCTest
