/** \file dmRecon.hpp
 * \brief The MagAO-X DM shape reconstructor
 *
 * \ingroup dmRecon_files
 */

#ifndef dmRecon_hpp
#define dmRecon_hpp

#include <limits>

#include <mx/improc/eigenCube.hpp>
#include <mx/improc/eigenImage.hpp>
#include <mx/sigproc/gramSchmidt.hpp>
#include <mx/math/templateBLAS.hpp>

#include "../../libMagAOX/libMagAOX.hpp" //Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"

#include <mx/math/cuda/cudaPtr.hpp>
#include <mx/math/cuda/cublasHandle.hpp>
#include <mx/math/cuda/templateCublas.hpp>

namespace MagAOX
{
namespace app
{

/** \defgroup dmRecon DM Shape Reconstructor
 * \brief Reconstruct the wavefront corresponding to a DM shape
 *
 * <a href="../handbook/operating/software/apps/dmRecon.html">Application Documentation</a>
 *
 * \ingroup apps
 *
 */

/** \defgroup dmRecon_files DM Shape Reconstructor Files
 * \ingroup dmRecon
 */

struct dmModesShmimT
{
    static std::string configSection()
    {
        return "dmModes";
    };

    static std::string indiPrefix()
    {
        return "dmModes";
    };
};

struct dmMaskShmimT
{
    static std::string configSection()
    {
        return "dmMask";
    };

    static std::string indiPrefix()
    {
        return "dmMask";
    };
};

struct dmCommandShmimT
{
    static std::string configSection()
    {
        return "dmCommand";
    };

    static std::string indiPrefix()
    {
        return "dmCommand";
    };
};

/** MagAO-X application to perform wavefront reconstruction from a DM surface
 *
 * \ingroup dmRecon
 *
 */
class dmRecon : public MagAOXApp<true>,
                public dev::shmimMonitor<dmRecon, dmModesShmimT>,
                public dev::shmimMonitor<dmRecon, dmMaskShmimT>,
                public dev::shmimMonitor<dmRecon, dmCommandShmimT>,
                public dev::frameGrabber<dmRecon>,
                public dev::telemeter<dmRecon>
{
    // Give the test harness access.
    friend class dmRecon_test;

    friend class dev::shmimMonitor<dmRecon, dmModesShmimT>;
    typedef dev::shmimMonitor<dmRecon, dmModesShmimT> dmModesSMT;

    friend class dev::shmimMonitor<dmRecon, dmMaskShmimT>;
    typedef dev::shmimMonitor<dmRecon, dmMaskShmimT> dmMaskSMT;

    friend class dev::shmimMonitor<dmRecon, dmCommandShmimT>;
    typedef dev::shmimMonitor<dmRecon, dmCommandShmimT> dmCommandSMT;

    friend class dev::frameGrabber<dmRecon>;
    typedef dev::frameGrabber<dmRecon> frameGrabberT;

    static constexpr bool c_frameGrabber_flippable = false;

    friend class dev::telemeter<dmRecon>;
    typedef dev::telemeter<dmRecon> telemeterT;

    /// Floating point type in which to do all calculations.
    typedef float realT;

  protected:
    /** \name Configurable Parameters
     *@{
     */

    std::string m_loopNumber{ "1" }; ///< The loop number.  Default is 1 as in aol1.

    std::string m_fpsSource{ "camwfs" }; /**< Device name for getting fps of the loop.
                                              This device must have *.fps.current.  Default is camwfs*/

    uint16_t m_gpuIndex{ 0 }; /**< Index of the GPU to use for calculations */

    bool m_useGPU{ false }; /**< Flag controlling whether the GPU is used for calculations */

    ///@}

    float m_fps{ 0 }; ///< Current FPS from the FPS source.

    mx::improc::eigenImage<float> m_maskedDMModes;

    // clang-format off
    #ifdef MXLIB_CUDA

    mx::cuda::cudaPtr<float> m_maskedDMModes_GPU;

    #endif
    // clang-format on

    bool m_dmModesReady{ false }; ///< Flag indicating that the DM modes are ready for processing

    mx::improc::eigenImage<float> m_mask;

    std::vector<size_t> m_maskIDX; ///< The index of masked pixels

    bool m_dmMaskReady{ false }; ///< Flag indicating that the DM mask is ready for processing

    bool m_commandReady{ false }; ///< Flag indicating that all sizes match and arrays are ready for processing

    bool m_fgWaiting{ false }; ///< Flag indicating that the FG thread is waiting for the command thread

    mx::improc::eigenImage<float> m_command; ///< The DM command, copied out of the incoming shmim

    mx::improc::eigenImage<float> m_modevals; ///< The calculated mode amplitudes

    // clang-format off
    #ifdef MXLIB_CUDA

    mx::cuda::cudaPtr<float> m_command_GPU;

    mx::cuda::cudaPtr<float> m_modevals_GPU;

    #endif
    // clang-format on

    sem_t m_smSemaphore{ 0 }; ///< Semaphore used to synchronize the fg thread and the dm command thread.

    bool m_updated{ false }; ///< Flag indicating that the mode vals have been updated

    mx::cuda::cublasHandle m_cublas; ///< Handle for the cuBLAS library

  public:
    /// Default c'tor.
    dmRecon();

    /// D'tor, declared and defined for noexcept.
    ~dmRecon() noexcept
    {
    }

    virtual void setupConfig();

    /// Implementation of loadConfig logic, separated for testing.
    /** This is called by loadConfig().
     */
    int loadConfigImpl( mx::app::appConfigurator &_config /**< [in] an application configuration
                        from which to load values*/
    );

    virtual void loadConfig();

    /// Startup function
    /**
     *
     */
    virtual int appStartup();

    /// Implementation of the FSM for dmRecon.
    /**
     * \returns 0 on no critical error
     * \returns -1 on an error requiring shutdown
     */
    virtual int appLogic();

    /// Shutdown the app.
    /**
     *
     */
    virtual int appShutdown();

    /// Set the GPU index
    /** Uses m_gpuIndex.  On errors it sets m_useGPU to false.
     *
     */
    int setGPU();

    /// Allocate method for the dm modes shmimMonitor
    /**
     * \returns 0 on success
     * \returns -1 on an error
     */
    int allocate( const dmModesShmimT & /**< [in] tag to differentiate shmimMonitor parents.*/ );

    /// Process images for the dm modes shmimMonitor
    /**
     * \returns 0 on sucess
     * \returns -1 on an error
     */
    int processImage( void *curr_src,       ///< [in] pointer to start of current frame.
                      const dmModesShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    /// Allocate method for the dm mask shmimMonitor
    /**
     * \returns 0 on success
     * \returns -1 on an error
     */
    int allocate( const dmMaskShmimT & /**< [in] tag to differentiate shmimMonitor parents.*/ );

    /// Process images for the dm mask shmimMonitor
    /**
     * \returns 0 on sucess
     * \returns -1 on an error
     */
    int processImage( void *curr_src,      ///< [in] pointer to start of current frame.
                      const dmMaskShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    /// Allocate method for the dm command shmimMonitor
    /**
     * \returns 0 on success
     * \returns -1 on an error
     */
    int allocate( const dmCommandShmimT & /**< [in] tag to differentiate shmimMonitor parents.*/ );

    /// Process images for the dm command shmimMonitor
    /**
     * \returns 0 on sucess
     * \returns -1 on an error
     */
    int processImage( void *curr_src,         ///< [in] pointer to start of current frame.
                      const dmCommandShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    /** \name Framegrabber Interface */
    /**
     * @{
     */

    int configureAcquisition();

    float fps();

    int startAcquisition();

    int acquireAndCheckValid();

    int loadImageIntoStream( void *dest );

    int reconfig();

    ///@}
  protected:
    /** \name INDI Interface
     *
     * @{
     */
    pcf::IndiProperty m_indiP_fpsSource;
    INDI_SETCALLBACK_DECL( dmRecon, m_indiP_fpsSource );

    pcf::IndiProperty m_indiP_fps;

    ///@}

    /** \name Telemeter Interface
     *
     * @{
     */

    int checkRecordTimes();

    int recordTelem( const telem_fgtimings * );

    ///@}
};

inline dmRecon::dmRecon() : MagAOXApp( MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED )
{
    return;
}

inline void dmRecon::setupConfig()
{
    config.add( "recon.loopNumber",
                "",
                "recon.loopNumber",
                argType::Required,
                "recon",
                "loopNumber",
                false,
                "int",
                "The loop number.  Default is 1 as in aol1." );

    config.add( "recon.fpsSource",
                "",
                "recon.fpsSource",
                argType::Required,
                "recon",
                "fpsSource",
                false,
                "string",
                "Device name for getting fps of the loop.  This device should have *.fps.current.  "
                "Default is camwfs" );

    config.add( "recon.gpuIndex",
                "",
                "recon.gpuIndex",
                argType::Required,
                "recon",
                "gpuIndex",
                false,
                "int",
                "Index of the GPU to use for calculations.  Default is 0." );

    config.add( "recon.useGPU",
                "",
                "recon.useGPU",
                argType::Required,
                "recon",
                "useGPU",
                false,
                "bool",
                "Flag controlling whether the GPU is used for calculations. Default is false." );

    SHMIMMONITORT_SETUP_CONFIG( dmModesSMT, config );

    SHMIMMONITORT_SETUP_CONFIG( dmMaskSMT, config );

    SHMIMMONITORT_SETUP_CONFIG( dmCommandSMT, config );

    FRAMEGRABBER_SETUP_CONFIG( config );

    TELEMETER_SETUP_CONFIG( config );
}

inline int dmRecon::loadConfigImpl( mx::app::appConfigurator &_config )
{

    _config( m_loopNumber, "recon.loopNumber" );
    _config( m_fpsSource, "recon.fpsSource" );
    _config( m_gpuIndex, "recon.gpuIndex" );
    _config( m_useGPU, "recon.useGPU" );

    std::string loopName = "aol" + m_loopNumber;

    dmModesSMT::m_shmimName = loopName + "_CMmodesDM";
    SHMIMMONITORT_LOAD_CONFIG( dmModesSMT, _config );

    dmMaskSMT::m_shmimName = loopName + "_dmmask";
    SHMIMMONITORT_LOAD_CONFIG( dmMaskSMT, _config );

    dmCommandSMT::m_shmimName = "dm01disp_delta";
    SHMIMMONITORT_LOAD_CONFIG( dmCommandSMT, _config );

    frameGrabberT::m_shmimName = loopName + "_modevalDMf";
    FRAMEGRABBER_LOAD_CONFIG( _config );

    TELEMETER_LOAD_CONFIG( _config );

    return 0;
}

inline void dmRecon::loadConfig()
{
    loadConfigImpl( config );
}

inline int dmRecon::appStartup()
{

    REG_INDI_SETPROP( m_indiP_fpsSource, m_fpsSource, std::string( "fps" ) );

    createROIndiNumber( m_indiP_fps, "fps" );
    m_indiP_fps.add( pcf::IndiElement( "current" ) );
    if( registerIndiPropertyReadOnly( m_indiP_fps ) < 0 )
    {
        log<software_error>( { __FILE__, __LINE__ } );
        return -1;
    }

    if( sem_init( &m_smSemaphore, 0, 0 ) < 0 )
    {
        log<software_critical>( { __FILE__, __LINE__, errno, 0, "Initializing S.M. semaphore" } );
        return -1;
    }

    dmModesSMT::m_getExistingFirst = true;
    SHMIMMONITORT_APP_STARTUP( dmModesSMT );
    dmMaskSMT::m_getExistingFirst = true;
    SHMIMMONITORT_APP_STARTUP( dmMaskSMT );
    SHMIMMONITORT_APP_STARTUP( dmCommandSMT );

    FRAMEGRABBER_APP_STARTUP;

    TELEMETER_APP_STARTUP;

    state( stateCodes::OPERATING );

    return 0;
}

int dmRecon::appLogic()
{
    SHMIMMONITORT_APP_LOGIC( dmModesSMT );
    SHMIMMONITORT_APP_LOGIC( dmMaskSMT );
    SHMIMMONITORT_APP_LOGIC( dmCommandSMT );

    FRAMEGRABBER_APP_LOGIC;

    TELEMETER_APP_LOGIC;

    std::unique_lock<std::mutex> lock( m_indiMutex );

    SHMIMMONITORT_UPDATE_INDI( dmModesSMT );
    SHMIMMONITORT_UPDATE_INDI( dmMaskSMT );
    SHMIMMONITORT_UPDATE_INDI( dmCommandSMT );

    FRAMEGRABBER_UPDATE_INDI;

    return 0;
}

inline int dmRecon::appShutdown()
{
    SHMIMMONITORT_APP_SHUTDOWN( dmModesSMT );
    SHMIMMONITORT_APP_SHUTDOWN( dmMaskSMT );
    SHMIMMONITORT_APP_SHUTDOWN( dmCommandSMT );

    FRAMEGRABBER_APP_SHUTDOWN;

    TELEMETER_APP_SHUTDOWN;

    return 0;
}

int dmRecon::setGPU()
{
#ifdef MXLIB_CUDA

    if( !m_useGPU )
    {
        return 0;
    }

    int deviceCount;
    int devicecntMax = 100;

    cudaError_t ce = cudaGetDeviceCount( &deviceCount );

    if( ce != cudaSuccess )
    {

        log<software_error>( { __FILE__,
                               __LINE__,
                               std::format( "cudaGetDeviceCount returned error: "
                                            "[{}] {}\nNOT USING GPU",
                                            cudaGetErrorName( ce ),
                                            cudaGetErrorString( ce ) ) } );

        m_useGPU = false;
        state( state(), true );
        return -1;
    }

    std::string msg = std::format( "CUDA: found {} devices\n", deviceCount );

    if( deviceCount > devicecntMax )
    {
        deviceCount = 0;
        msg += "      greater than devicecntMax\n";
    }
    if( deviceCount < 0 )
    {
        msg += "      less than zero\n";
    }

    if( deviceCount == 0 )
    {
        msg += "      no devices found!\nNOT USING GPU";
        log<software_error>( { __FILE__, __LINE__, msg } );
        m_useGPU = false;
        state( state(), true );
        return -1;
    }

    for( int k = 0; k < deviceCount; k++ )
    {
        cudaDeviceProp deviceProp;
        ce = cudaGetDeviceProperties( &deviceProp, k );

        if( ce != cudaSuccess )
        {
            msg += std::format( "cudaGetDeviceProperties returned error: "
                                "[{}] {}\nNOT USING GPU",
                                cudaGetErrorName( ce ),
                                cudaGetErrorString( ce ) );
            log<software_error>( { __FILE__, __LINE__, msg } );
            m_useGPU = false;
            state( state(), true );
            return -1;
        }

        int clockRate;
        ce = cudaDeviceGetAttribute( &clockRate, cudaDevAttrClockRate, k );

        if( ce != cudaSuccess )
        {
            msg += std::format( "cudaGetDeviceAttribute returned error: "
                                "[{}] {}\nNOT USING GPU",
                                cudaGetErrorName( ce ),
                                cudaGetErrorString( ce ) );
            log<software_error>( { __FILE__, __LINE__, msg } );
            m_useGPU = false;
            state( state(), true );
            return -1;
        }

        msg += std::format( "      Device {} / {} [ {} ] has compute capability {}.{}.\n",
                            k + 1,
                            deviceCount,
                            deviceProp.name,
                            deviceProp.major,
                            deviceProp.minor );

        msg += std::format( "          Total amount of global memory: {} MBytes\n",
                            (float)deviceProp.totalGlobalMem / 1048576.0f );

        msg += std::format( "          Multiprocessors: {}\n", deviceProp.multiProcessorCount );
        msg += std::format( "          Clock rate: {} MHz ({} GHz)\n", clockRate * 1e-3f, clockRate * 1e-6f );
    }

    if( m_gpuIndex >= deviceCount )
    {
        msg += std::format( "gpuIndex = {} is not valid for {} devices\nNOT USING GPU", m_gpuIndex, deviceCount );
        log<software_error>( { __FILE__, __LINE__, msg } );
        m_useGPU = false;
        state( state(), true );
        return -1;
    }

    ce = cudaSetDevice( m_gpuIndex );

    if( ce != cudaSuccess )
    {
        msg += std::format( "cudaSetDevice returned error: "
                            "[{}] {}\nNOT USING GPU",
                            cudaGetErrorName( ce ),
                            cudaGetErrorString( ce ) );
        log<software_error>( { __FILE__, __LINE__, msg } );
        m_useGPU = false;
        state( state(), true );
        return -1;
    }

    msg += std::format( "Set GPU Index to device {} ( {} / {})\n", m_gpuIndex, m_gpuIndex + 1, deviceCount );

    cublasStatus_t cbs = m_cublas.create();

    if( cbs != CUBLAS_STATUS_SUCCESS )
    {
        msg += std::format( "cublasHandle create returned error: "
                            "[{}] {}\nNOT USING GPU",
                            cublasGetStatusName( cbs ),
                            cublasGetStatusString( cbs ) );
        log<software_error>( { __FILE__, __LINE__, msg } );
        m_useGPU = false;
        state( state(), true );
        return -1;
    }

    msg += "      cuBLAS initialized";

    log<text_log>( msg );

    return 0;

#else // MXLIB_CUDA

    if( m_useGPU )
    {
        log<software_error>( { __FILE__, __LINE__, "mxlib was compiled without CUDA support. NOT USING GPU" } );

        m_useGPU = false;
        state( state(), true );
        return -1;
    }
    return 0;

#endif // MXLIB_CUDA
}

int dmRecon::allocate( const dmModesShmimT & )
{
    m_dmModesReady = false;

    dmCommandSMT::m_restart = true;

    // Can't process modes until mask is ready.
    if( dmModesSMT::m_width != dmMaskSMT::m_width || dmModesSMT::m_height != dmMaskSMT::m_height || !m_dmMaskReady )
    {
        mx::sys::milliSleep( 1000 );
        dmModesSMT::m_restart = true;
        return 0; // This won't log an error, but setting m_restart will cause it to loop again until sizes match
    }

    // This will let us go on to processImage

    // Do a type check for float
    return 0;
}

int dmRecon::processImage( void *curr_src, const dmModesShmimT & )
{
    if( m_dmModesReady == true )
    {
        // This means an new image has come in.  We need to reset and restart everything.
        dmModesSMT::m_restart   = true;
        dmMaskSMT::m_restart    = true;
        dmCommandSMT::m_restart = true;
        return 0;
    }

    mx::improc::eigenCube<float> dmModes(dmModesSMT::m_width, dmModesSMT::m_height, dmModesSMT::m_depth);

    for(size_t n =0; n < dmModesSMT::m_width * dmModesSMT::m_height* dmModesSMT::m_depth; ++n)
    {
        dmModes.data()[n] = reinterpret_cast<float *>( curr_src )[n];
    }
        

    // Wait for m_commandReady to become false
    while( m_commandReady == true && !m_shutdown && dmModesSMT::m_restart == false )
    {
        mx::sys::milliSleep( 1000 );
    }

    /*mx::improc::eigenCube<float> dmModes(
        reinterpret_cast<float *>( curr_src ), dmModesSMT::m_width, dmModesSMT::m_height, dmModesSMT::m_depth );
    */

    /*int w = dmModesSMT::m_width;
    int h = dmModesSMT::m_height;
    float * dmModes = reinterpret_cast<float *>( curr_src );*/


    std::cerr << "DM modes: " << dmModesSMT::m_width << ' ' << dmModesSMT::m_height << ' ' << dmModesSMT::m_depth << '\n';
    //std::cerr << dmModes.asVectors().square().sum() << '\n';
    
    std::cerr << "Mask pixels: " << m_maskIDX.size() << '\n';

    m_maskedDMModes.resize( dmModesSMT::m_depth, m_maskIDX.size() );

    // Load only the unmasked pixels
    for( int rr = 0; rr < m_maskedDMModes.rows(); ++rr )
    {
        for( size_t n = 0; n < m_maskIDX.size(); ++n )
        {
            m_maskedDMModes(rr,n) = 0;
        }
    }

    std::cerr << "DM modes masked sum: " << m_maskedDMModes.square().sum() << '\n';
    m_dmModesReady = true;
    return 0;
}

int dmRecon::allocate( const dmMaskShmimT & )
{
    m_dmMaskReady = false;

    dmCommandSMT::m_restart = true;

    // Do a type check for float
    return 0;
}

int dmRecon::processImage( void *curr_src, const dmMaskShmimT & )
{
    if( m_dmMaskReady == true )
    {
        // This means an new image has come in.  We need to reset and restart everything.
        dmModesSMT::m_restart   = true;
        dmMaskSMT::m_restart    = true;
        dmCommandSMT::m_restart = true;

        return 0;
    }

    // Wait for m_commandReady to become false
    while( m_commandReady == true && !m_shutdown && dmMaskSMT::m_restart == false )
    {
        mx::sys::milliSleep( 1000 );
    }

    m_mask =
        mx::improc::eigenMap<float>( reinterpret_cast<float *>( curr_src ), dmMaskSMT::m_width, dmMaskSMT::m_height );

    m_maskIDX.clear();

    size_t n = 0;

    int nmax = 0;
    for( int rr = 0; rr < m_mask.rows(); ++rr )
    {
        for( int cc = 0; cc < m_mask.cols(); ++cc )
        {
            if( m_mask( rr, cc ) == 1 )
            {
                m_maskIDX.push_back( n );
                nmax = n;
            }

            ++n;
        }
    }

    std::cerr << n <<  ' ' << nmax << '\n';

    std::cerr << "Got mask of size " << m_mask.rows() << " x " << m_mask.cols() << " with " << m_maskIDX.size()
              << " good pixels.\n";

    m_dmMaskReady = true;
    return 0;
}

int dmRecon::allocate( const dmCommandShmimT & )
{
    // This is the only place that m_commandReady can be changed
    if( !m_dmModesReady || !m_dmMaskReady || dmCommandSMT::m_width != dmModesSMT::m_width ||
        dmCommandSMT::m_height != dmModesSMT::m_height || dmCommandSMT::m_width != dmMaskSMT::m_width ||
        dmCommandSMT::m_height != dmMaskSMT::m_height )
    {
        m_commandReady = false;

        dmCommandSMT::m_restart   = true;
        frameGrabberT::m_reconfig = true;

        mx::sys::milliSleep( 1000 );

        return 0; // This won't log an error, but setting m_restart will cause it to reconnect again until sizes match
    }

    if( !m_fgWaiting )
    {
        mx::sys::milliSleep( 1000 );

        dmCommandSMT::m_restart = true;
        return 0; // This won't log an error, but setting m_restart will cause it to reconnect again until sizes match
    }

    m_command.resize( m_maskIDX.size(), 1 );

    m_modevals.resize( m_maskedDMModes.rows(), 1 );

#ifdef MXLIB_CUDA
    if( m_useGPU )
    {
        // Do all initializations and uploads here so it's in the right thread on the right device
        if( setGPU() < 0 )
        {
            log<software_error>( { __FILE__, __LINE__, "setting GPU device failed." } );
            m_useGPU = false;
            state( state(), true );
            return -1;
        }

        mx::error_t ec =
            m_maskedDMModes_GPU.upload( m_maskedDMModes.data(), m_maskedDMModes.rows(), m_maskedDMModes.cols() );
        if( ec != mx::error_t::noerror )
        {
            return log<software_error, -1>( { __FILE__,
                                              __LINE__,
                                              std::format( "error uploading modes to GPU: [{}] {}",
                                                           mx::errorName( ec ),
                                                           mx::errorMessage( ec ) ) } );
        }

        ec = m_command_GPU.resize( m_command.rows() * m_command.cols() );
        if( ec != mx::error_t::noerror )
        {
            return log<software_error, -1>( { __FILE__,
                                              __LINE__,
                                              std::format( "error allocating command on GPU: [{}] {}",
                                                           mx::errorName( ec ),
                                                           mx::errorMessage( ec ) ) } );
        }

        ec = m_modevals_GPU.resize( m_modevals.rows() * m_modevals.cols() );
        if( ec != mx::error_t::noerror )
        {
            return log<software_error, -1>( { __FILE__,
                                              __LINE__,
                                              std::format( "error allocating modevals on GPU: [{}] {}",
                                                           mx::errorName( ec ),
                                                           mx::errorMessage( ec ) ) } );
        }
    }
#endif // MXLIB_CUDA

    m_updated      = false;
    m_commandReady = true;

    return 0;
}

int dmRecon::processImage( void *curr_src, const dmCommandShmimT & )
{
    if( !m_commandReady )
    {
        dmCommandSMT::m_restart = true;
        return 0;
    }

    //Set atime to now
    clock_gettime( CLOCK_REALTIME, &m_currImageTimestamp );

    // extract masked pixels
    for( size_t n = 0; n < m_maskIDX.size(); ++n )
    {
        m_command( n, 0 ) = reinterpret_cast<float *>( curr_src )[m_maskIDX[n]];
    }

    // clang-format off
    #ifdef MXLIB_CUDA // clang-format on
    if( !m_useGPU )
    {
        //std::cerr << __LINE__ << '\n';
        // CPU:
        m_modevals = m_maskedDMModes * m_command;
    }
    else
    {
        // GPU:
        mx::error_t ec = m_command_GPU.upload( m_command.data() );
        if( ec != mx::error_t::noerror )
        {
            return log<software_error, -1>( { __FILE__,
                                              __LINE__,
                                              std::format( "error uploading command to GPU: [{}] {}",
                                                           mx::errorName( ec ),
                                                           mx::errorMessage( ec ) ) } );
        }

        float alpha = 1;
        float beta  = 0;

        cublasStatus_t cbs = mx::cuda::cublasTgemv( m_cublas,
                                                    CUBLAS_OP_N,
                                                    m_maskedDMModes_GPU.rows(),
                                                    m_maskedDMModes_GPU.cols(),
                                                    &alpha,
                                                    m_maskedDMModes_GPU.data(),
                                                    m_maskedDMModes_GPU.rows(),
                                                    m_command_GPU.data(),
                                                    1,
                                                    &beta,
                                                    m_modevals_GPU.data(),
                                                    1 );

        if( cbs != CUBLAS_STATUS_SUCCESS )
        {
            return log<software_error, -1>( { __FILE__,
                                              __LINE__,
                                              std::format( "error downloading modevals from GPU: [{}] {}",
                                                           cublasGetStatusName( cbs ),
                                                           cublasGetStatusString( cbs ) ) } );
        }

        ec = m_modevals_GPU.download( m_modevals.data() );
        if( ec != mx::error_t::noerror )
        {
            return log<software_error, -1>( { __FILE__,
                                              __LINE__,
                                              std::format( "error downloading modevals from GPU: [{}] {}",
                                                           mx::errorName( ec ),
                                                           mx::errorMessage( ec ) ) } );
        }
    }

    // clang-format off
    #else // MXLIB_CUDA

    // CPU:
    m_modevals = m_maskedDMModes * m_command;

    #endif // MXLIB_CUDA
    // clang-format on

    m_updated = true;

    //std::cerr << __LINE__ << '\n';

    // trigger framegrabber
    if( sem_post( &m_smSemaphore ) < 0 )
    {
        log<software_critical>( { __FILE__, __LINE__, errno, 0, "Error posting to semaphore" } );
        return -1;
    }

    //std::cerr << __LINE__ << '\n';

    return 0;
}

int dmRecon::configureAcquisition()
{
    if( !m_commandReady )
    {
        m_fgWaiting = true;
        mx::sys::milliSleep( 100 );
        return -1;
    }

    m_fgWaiting = false;

    frameGrabberT::m_width    = m_modevals.rows();
    frameGrabberT::m_height   = m_modevals.cols();
    frameGrabberT::m_dataType = _DATATYPE_FLOAT;

    return 0;
}

float dmRecon::fps()
{
    return m_fps;
}

int dmRecon::startAcquisition()
{
    
    return 0;
}

int dmRecon::acquireAndCheckValid()
{
    timespec ts;

    errno = 0;
    if( clock_gettime( CLOCK_REALTIME, &ts ) < 0 )
    {
        log<software_critical>( { __FILE__, __LINE__, errno, 0, "clock_gettime" } );
        return -1;
    }

    ts.tv_sec += 1;

    if( !m_commandReady )
    {
        return 1;
    }

    //std::cerr << __LINE__ << '\n';
    //mx::sys::microSleep(1000);
    if( sem_timedwait( &m_smSemaphore, &ts ) == 0 )
    {
        //std::cerr << __LINE__ << '\n';

        if( m_updated && m_commandReady )
        {
            return 0;
        }
        else
        {
            //std::cerr << __LINE__ << '\n';
            return 1;
        }
    }
    else
    {
        //std::cerr << __LINE__ << '\n';
        return 1;
    }

    return 0;
}

int dmRecon::loadImageIntoStream( void *dest )
{
    memcpy( dest, m_modevals.data(), m_modevals.rows() * m_modevals.cols() * sizeof( float ) );
    m_updated = false;

    return 0;
}

int dmRecon::reconfig()
{
    return 0;
}

INDI_SETCALLBACK_DEFN( dmRecon, m_indiP_fpsSource )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_fpsSource, ipRecv );

    if( ipRecv.find( "current" ) != true ) // this isn't valid
    {
        return -1;
    }

    std::lock_guard<std::mutex> guard( m_indiMutex );

    realT fps = ipRecv["current"].get<float>();

    if( fps != m_fps )
    {
        m_fps = fps;
        updateIfChanged( m_indiP_fps, "current", m_fps );
    }

    return 0;
}

int dmRecon::checkRecordTimes()
{
    return telemeterT::checkRecordTimes( telem_fgtimings() );
}

int dmRecon::recordTelem( const telem_fgtimings * )
{
    return recordFGTimings( true );
}

} // namespace app
} // namespace MagAOX

#endif // dmRecon_hpp
