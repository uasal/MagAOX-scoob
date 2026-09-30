/** \file hoPredCtrl_test.cpp
 * \brief Catch2 tests for the hoPredCtrl app.
 * \author Claude Code
 *
 * \ingroup hoPredCtrl_files
 *
 * The CUDA headers included by the DDSPC `.cuh` headers are replaced by the stubs in `stubs/`. The DDSPC classes
 * are implemented in `.cu` files, which g++ cannot build, so the members the app uses are defined here as
 * recording stubs.
 */

#include "../../../tests/testXWC.hpp"

#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "../hoPredCtrl.hpp"

using namespace MagAOX::app;

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS

/// Recording state for the DDSPC stubs.
namespace hoPredCtrlStubs
{

/// Call records and controllable results for the DDSPC stubs.
struct hoPredCtrlStubState
{
    /// Number of PredictiveController objects constructed.
    int pcConstructed{ 0 };

    /// Number of PredictiveController objects destroyed.
    int pcDestroyed{ 0 };

    /// History length passed to the PredictiveController c'tor.
    int ctorHistory{ 0 };

    /// Future length passed to the PredictiveController c'tor.
    int ctorFuture{ 0 };

    /// Number of modes passed to the PredictiveController c'tor.
    int ctorModes{ 0 };

    /// Number of measurements passed to the PredictiveController c'tor.
    int ctorMeasurements{ 0 };

    /// Forgetting factor passed to the PredictiveController c'tor.
    float ctorGamma{ 0 };

    /// Regularization passed to the PredictiveController c'tor.
    float ctorLambda{ 0 };

    /// Initial covariance passed to the PredictiveController c'tor.
    float ctorP0{ 0 };

    /// Number of actuators passed to the PredictiveController c'tor.
    int ctorActuators{ 0 };

    /// Copy of the interaction matrix passed to set_interaction_matrix() (modes x measurements).
    std::vector<float> interaction;

    /// Copy of the mapping matrix passed to set_mapping_matrix() (actuators x modes).
    std::vector<float> mapping;

    /// Number of add_measurement() calls.
    int addMeasurementCalls{ 0 };

    /// Copy of the last measurement passed to add_measurement().
    std::vector<float> lastMeasurement;

    /// Number of get_command() calls.
    int getCommandCalls{ 0 };

    /// The clip value passed to the last get_command() call.
    float lastClip{ 0 };

    /// The command returned by get_command().
    std::vector<float> command;

    /// Number of set_zero() calls.
    int setZeroCalls{ 0 };

    /// Number of create_exploration_buffer() calls.
    int explorationCalls{ 0 };

    /// The rms passed to the last create_exploration_buffer() call.
    float explorationRms{ 0 };

    /// The size passed to the last create_exploration_buffer() call.
    int explorationSteps{ 0 };

    /// Number of set_new_regularization() calls.
    int regularizationCalls{ 0 };

    /// The value passed to the last set_new_regularization() call.
    float lastRegularization{ 0 };

    /// Number of PredictiveController::set_new_gamma() calls.
    int gammaCalls{ 0 };

    /// The value passed to the last PredictiveController::set_new_gamma() call.
    float lastGamma{ 0 };

    /// Number of save_state() calls.
    int saveCalls{ 0 };

    /// The path passed to the last save_state() call.
    std::string savePath;

    /// Number of load_state() calls.
    int loadCalls{ 0 };

    /// The path passed to the last load_state() call.
    std::string loadPath;

    /// The timestamp passed to the last load_state() call.
    std::string loadTimestamp;

    /// Number of DistributedAutoRegressiveController::reset_data_buffer() calls.
    int resetBufferCalls{ 0 };

    /// Number of DistributedAutoRegressiveController::reset_controller() calls.
    int resetControllerCalls{ 0 };

    /// Number of DistributedAutoRegressiveController::update_predictor() calls.
    int updatePredictorCalls{ 0 };

    /// Number of DistributedAutoRegressiveController::update_controller() calls.
    int updateControllerCalls{ 0 };
};

/// The global stub state, reset at the start of each test.
hoPredCtrlStubState g_stub;

} // namespace hoPredCtrlStubs

// ---- DDSPC::Matrix (new_matrix.cu): only the c'tor and d'tor, host storage only ----

DDSPC::Matrix::Matrix( float initialization_value, int num_rows, int num_columns, int num_batches )
{
    handle        = nullptr;
    nrows_        = num_rows;
    ncols_        = num_columns;
    batch_size_   = num_batches;
    size_         = num_rows * num_columns;
    element_size_ = sizeof( float );
    total_size_   = size_ * num_batches;

    cpu_data    = new float *[1];
    cpu_data[0] = new float[total_size_];
    for( int n = 0; n < total_size_; ++n )
    {
        cpu_data[0][n] = initialization_value;
    }

    gpu_data     = nullptr;
    dev_gpu_data = nullptr;
    info         = nullptr;
}

DDSPC::Matrix::~Matrix()
{
    delete[] cpu_data[0];
    delete[] cpu_data;
}

// ---- DDSPC::DistributedAutoRegressiveController (distributed_ar_controller.cu) ----

DDSPC::DistributedAutoRegressiveController::DistributedAutoRegressiveController( cublasHandle_t *new_handle,
                                                                                 int             num_history,
                                                                                 int             num_future,
                                                                                 int             num_modes,
                                                                                 float           new_gamma,
                                                                                 float          *new_lambda,
                                                                                 float           P0 )
{
    static_cast<void>( new_lambda );
    static_cast<void>( P0 );

    rls                = nullptr;
    measurement_buffer = nullptr;
    command_buffer     = nullptr;
    phi                = nullptr;
    xf                 = nullptr;
    newest_measurement = nullptr;
    command            = nullptr;
    delta_command      = nullptr;
    H                  = nullptr;
    H11                = nullptr;
    H12                = nullptr;
    invH11             = nullptr;
    invH11sub          = nullptr;
    controller         = nullptr;
    full_controller    = nullptr;
    condition_matrix   = nullptr;
    wp                 = nullptr;
    lambda             = nullptr;

    handle       = new_handle;
    nhistory     = num_history;
    nfuture      = num_future;
    nmodes       = num_modes;
    nfeatures    = 0;
    buffer_size  = 0;
    buffer_index = 0;
    gamma        = new_gamma;

    use_predictor = false;
    int_gain      = 0;
    int_leakage   = 0;
}

DDSPC::DistributedAutoRegressiveController::~DistributedAutoRegressiveController()
{
}

void DDSPC::DistributedAutoRegressiveController::reset_data_buffer()
{
    ++hoPredCtrlStubs::g_stub.resetBufferCalls;
}

void DDSPC::DistributedAutoRegressiveController::reset_controller()
{
    ++hoPredCtrlStubs::g_stub.resetControllerCalls;
}

void DDSPC::DistributedAutoRegressiveController::update_predictor()
{
    ++hoPredCtrlStubs::g_stub.updatePredictorCalls;
}

void DDSPC::DistributedAutoRegressiveController::update_controller()
{
    ++hoPredCtrlStubs::g_stub.updateControllerCalls;
}

// ---- DDSPC::PredictiveController (predictive_controller.cu) ----

DDSPC::PredictiveController::PredictiveController( int   num_history,
                                                   int   num_future,
                                                   int   num_modes,
                                                   int   num_measurements,
                                                   float gamma,
                                                   float lambda,
                                                   float P0,
                                                   int   num_actuators )
{
    handle                    = nullptr;
    m_num_history             = num_history;
    m_num_future              = num_future;
    m_num_modes               = num_modes;
    m_num_measurements        = num_measurements;
    m_num_actuators           = num_actuators;
    m_gamma                   = gamma;
    m_lambda                  = nullptr;
    m_P0                      = P0;
    m_exploration_buffer_size = 0;
    m_exploration_index       = 0;
    m_wfs_measurement         = nullptr;
    m_exploration_signal      = nullptr;
    m_command                 = nullptr;
    m_voltages                = nullptr;
    m_interaction_matrix      = nullptr;
    m_mode_mapping_matrix     = nullptr;
    m_exploration_buffer      = nullptr;
    m_measurement             = nullptr;

    controller =
        new DistributedAutoRegressiveController( &handle, num_history, num_future, num_modes, gamma, nullptr, P0 );

    hoPredCtrlStubs::hoPredCtrlStubState &g = hoPredCtrlStubs::g_stub;
    ++g.pcConstructed;
    g.ctorHistory      = num_history;
    g.ctorFuture       = num_future;
    g.ctorModes        = num_modes;
    g.ctorMeasurements = num_measurements;
    g.ctorGamma        = gamma;
    g.ctorLambda       = lambda;
    g.ctorP0           = P0;
    g.ctorActuators    = num_actuators;
}

DDSPC::PredictiveController::~PredictiveController()
{
    delete controller;
    ++hoPredCtrlStubs::g_stub.pcDestroyed;
}

void DDSPC::PredictiveController::set_zero()
{
    ++hoPredCtrlStubs::g_stub.setZeroCalls;
}

void DDSPC::PredictiveController::create_exploration_buffer( float rms, int exploration_buffer_size )
{
    ++hoPredCtrlStubs::g_stub.explorationCalls;
    hoPredCtrlStubs::g_stub.explorationRms   = rms;
    hoPredCtrlStubs::g_stub.explorationSteps = exploration_buffer_size;
}

void DDSPC::PredictiveController::set_new_regularization( float new_lambda )
{
    ++hoPredCtrlStubs::g_stub.regularizationCalls;
    hoPredCtrlStubs::g_stub.lastRegularization = new_lambda;
}

void DDSPC::PredictiveController::set_new_gamma( float new_gamma )
{
    ++hoPredCtrlStubs::g_stub.gammaCalls;
    hoPredCtrlStubs::g_stub.lastGamma = new_gamma;
}

void DDSPC::PredictiveController::set_interaction_matrix( float *interaction_matrix )
{
    hoPredCtrlStubs::g_stub.interaction.assign( interaction_matrix,
                                                interaction_matrix + m_num_modes * m_num_measurements );
}

void DDSPC::PredictiveController::set_mapping_matrix( float *mapping_matrix )
{
    hoPredCtrlStubs::g_stub.mapping.assign( mapping_matrix, mapping_matrix + m_num_actuators * m_num_modes );
}

void DDSPC::PredictiveController::add_measurement( float *new_wfs_measurement )
{
    ++hoPredCtrlStubs::g_stub.addMeasurementCalls;
    hoPredCtrlStubs::g_stub.lastMeasurement.assign( new_wfs_measurement, new_wfs_measurement + m_num_measurements );
}

float *DDSPC::PredictiveController::get_command( float clip_val )
{
    ++hoPredCtrlStubs::g_stub.getCommandCalls;
    hoPredCtrlStubs::g_stub.lastClip = clip_val;
    return hoPredCtrlStubs::g_stub.command.data();
}

void DDSPC::PredictiveController::save_state( std::string path )
{
    ++hoPredCtrlStubs::g_stub.saveCalls;
    hoPredCtrlStubs::g_stub.savePath = path;
}

void DDSPC::PredictiveController::load_state( std::string path, std::string timestamp )
{
    ++hoPredCtrlStubs::g_stub.loadCalls;
    hoPredCtrlStubs::g_stub.loadPath      = path;
    hoPredCtrlStubs::g_stub.loadTimestamp = timestamp;
}

/// \endcond

namespace libXWCTest
{

/** \defgroup hoPredCtrl_unit_test hoPredCtrl Unit Tests
 * \brief Unit tests for the hoPredCtrl application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `hoPredCtrl` unit tests.
/** \ingroup hoPredCtrl_unit_test
 */
namespace hoPredCtrlTest
{

using hoPredCtrlStubs::g_stub;

/// Directory used as `MILK_SHM_DIR` for the DM stream tests.
constexpr const char *c_shmDir = "/tmp/hoPredCtrl_test_shm";

/// Name of the DM channel stream.
constexpr const char *c_dmName = "hoPredCtrl_test_dm";

/// Pupil mask FITS file.
constexpr const char *c_maskFile = "/tmp/hoPredCtrl_test_mask.fits";

/// Interaction matrix FITS file.
constexpr const char *c_imFile = "/tmp/hoPredCtrl_test_im.fits";

/// Reference wavefront FITS file.
constexpr const char *c_refFile = "/tmp/hoPredCtrl_test_ref.fits";

/// Mapping matrix FITS file.
constexpr const char *c_mapFile = "/tmp/hoPredCtrl_test_map.fits";

/// Size of the (square) WFS image used by the tests.
constexpr int c_wfs = 4;

/// Number of modes in the test interaction and mapping matrices.
constexpr int c_modes = 2;

/// Size of the (square) DM; send_dm_command() always writes 50x50 floats.
constexpr int c_dm = 50;

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness exposing hoPredCtrl internals.
class hoPredCtrl_test : public hoPredCtrl
{
  public:
    /// The WFS shmimMonitor base.
    typedef dev::shmimMonitor<hoPredCtrl> wfsMonitorT;

    /// The dark shmimMonitor base.
    typedef dev::shmimMonitor<hoPredCtrl, darkShmimT> darkMonT;

    using hoPredCtrl::newCallBack_m_indiP_clipval;
    using hoPredCtrl::newCallBack_m_indiP_controlToggle;
    using hoPredCtrl::newCallBack_m_indiP_explorationRms;
    using hoPredCtrl::newCallBack_m_indiP_explorationSteps;
    using hoPredCtrl::newCallBack_m_indiP_gamma;
    using hoPredCtrl::newCallBack_m_indiP_intgain;
    using hoPredCtrl::newCallBack_m_indiP_intleak;
    using hoPredCtrl::newCallBack_m_indiP_lambda;
    using hoPredCtrl::newCallBack_m_indiP_learningIterations;
    using hoPredCtrl::newCallBack_m_indiP_learningSteps;
    using hoPredCtrl::newCallBack_m_indiP_loadRequest;
    using hoPredCtrl::newCallBack_m_indiP_predictorToggle;
    using hoPredCtrl::newCallBack_m_indiP_reset_bufferRequest;
    using hoPredCtrl::newCallBack_m_indiP_reset_cleanRequest;
    using hoPredCtrl::newCallBack_m_indiP_reset_exploreRequest;
    using hoPredCtrl::newCallBack_m_indiP_reset_modelRequest;
    using hoPredCtrl::newCallBack_m_indiP_saveRequest;
    using hoPredCtrl::newCallBack_m_indiP_timestamp;
    using hoPredCtrl::newCallBack_m_indiP_updateControllerRequest;
    using hoPredCtrl::newCallBack_m_indiP_zeroRequest;

    using hoPredCtrl::average_pupil_intensity;
    using hoPredCtrl::controller;
    using hoPredCtrl::dark_pixget;
    using hoPredCtrl::duration;
    using hoPredCtrl::iterations;
    using hoPredCtrl::loading_timestamp;
    using hoPredCtrl::m_clip_val;
    using hoPredCtrl::m_command;
    using hoPredCtrl::m_darkImage;
    using hoPredCtrl::m_darkSet;
    using hoPredCtrl::m_dmChannel;
    using hoPredCtrl::m_dmDataType;
    using hoPredCtrl::m_dmHeight;
    using hoPredCtrl::m_dmOpened;
    using hoPredCtrl::m_dmTypeSize;
    using hoPredCtrl::m_dmWidth;
    using hoPredCtrl::m_exploration_rms;
    using hoPredCtrl::m_exploration_steps;
    using hoPredCtrl::m_gamma;
    using hoPredCtrl::m_illuminatedPixels;
    using hoPredCtrl::m_interaction_matrix;
    using hoPredCtrl::m_interaction_matrix_filename;
    using hoPredCtrl::m_intgain;
    using hoPredCtrl::m_intleak;
    using hoPredCtrl::m_inv_covariance;
    using hoPredCtrl::m_is_closed_loop;
    using hoPredCtrl::m_lambda;
    using hoPredCtrl::m_learning_counter;
    using hoPredCtrl::m_learning_iterations;
    using hoPredCtrl::m_learning_steps;
    using hoPredCtrl::m_mapping_matrix;
    using hoPredCtrl::m_mapping_matrix_filename;
    using hoPredCtrl::m_measurement_size;
    using hoPredCtrl::m_measurementVector;
    using hoPredCtrl::m_numFut;
    using hoPredCtrl::m_numHist;
    using hoPredCtrl::m_numModes;
    using hoPredCtrl::m_numVoltages;
    using hoPredCtrl::m_pupilMask;
    using hoPredCtrl::m_pupilMaskFilename;
    using hoPredCtrl::m_pwfsHeight;
    using hoPredCtrl::m_pwfsWidth;
    using hoPredCtrl::m_quadHeight;
    using hoPredCtrl::m_quadWidth;
    using hoPredCtrl::m_refWavefront;
    using hoPredCtrl::m_refWavefront_filename;
    using hoPredCtrl::m_shaped_command;
    using hoPredCtrl::m_temp_command;
    using hoPredCtrl::m_use_predictive_control;
    using hoPredCtrl::savepath;
    using hoPredCtrl::use_actuators;
    using hoPredCtrl::use_full_image_reconstructor;

    /// Construct a harness with the given device name.
    /** Initializes the members the app leaves uninitialized, and names the INDI properties as appStartup() does.
     */
    explicit hoPredCtrl_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        controller                   = nullptr;
        m_command                    = nullptr;
        m_temp_command               = nullptr;
        m_is_closed_loop             = false;
        m_use_predictive_control     = false;
        use_actuators                = true;
        use_full_image_reconstructor = true;
        loading_timestamp            = 0;
        duration                     = 0;
        iterations                   = 0;
        m_illuminatedPixels          = 0;
        m_measurement_size           = 0;
        m_numModes                   = 0;
        m_numVoltages                = 0;
        m_numHist                    = 0;
        m_numFut                     = 0;
        m_gamma                      = 0;
        m_inv_covariance             = 0;
        m_lambda                     = 0;
        m_clip_val                   = 0;
        m_intgain                    = 0;
        m_intleak                    = 0;
        m_exploration_steps          = 0;
        m_exploration_rms            = 0;
        m_learning_counter           = 0;
        m_learning_steps             = 0;
        m_learning_iterations        = 0;
        average_pupil_intensity      = 0;

        nameProp( m_indiP_controlToggle, "control" );
        nameProp( m_indiP_reset_bufferRequest, "reset_buffer" );
        nameProp( m_indiP_reset_modelRequest, "reset_model" );
        nameProp( m_indiP_reset_cleanRequest, "clean" );
        nameProp( m_indiP_updateControllerRequest, "calc_controller" );
        nameProp( m_indiP_reset_exploreRequest, "reset_exploration" );
        nameProp( m_indiP_zeroRequest, "zero" );
        nameProp( m_indiP_saveRequest, "save" );
        nameProp( m_indiP_loadRequest, "load" );
        nameProp( m_indiP_timestamp, "timestamp" );
        nameProp( m_indiP_learningSteps, "learning_steps" );
        nameProp( m_indiP_learningIterations, "learning_iterations" );
        nameProp( m_indiP_explorationRms, "exploration_rms" );
        nameProp( m_indiP_explorationSteps, "exploration_steps" );
        nameProp( m_indiP_gamma, "gamma" );
        nameProp( m_indiP_lambda, "lambda" );
        nameProp( m_indiP_clipval, "clipval" );
        nameProp( m_indiP_predictorToggle, "use_predictor" );
        nameProp( m_indiP_intgain, "intgain" );
        nameProp( m_indiP_intleak, "intleak" );
    }

    /// Free what the app allocated (the app only does so in appShutdown()) and close the DM stream.
    ~hoPredCtrl_test() noexcept
    {
        if( controller )
        {
            delete controller;
            controller = nullptr;
        }

        if( m_temp_command )
        {
            delete[] m_temp_command;
            m_temp_command = nullptr;
        }

        if( m_dmOpened )
        {
            ImageStreamIO_closeIm( &m_dmStream );
            m_dmOpened = false;
        }
    }

    /// Set the device and name of an INDI property.
    void nameProp( pcf::IndiProperty &prop /**< [out] the property */,
                   const std::string &name /**< [in] the property name */ )
    {
        prop.setDevice( m_configName );
        prop.setName( name );
    }

    /// Read a config file and run loadConfig().
    void loadConfigFromFile( const std::string &fname /**< [in] config file path */ )
    {
        config.readConfig( fname );
        loadConfig();
    }

    /// The WFS input shmim name.
    std::string wfsShmimName()
    {
        return wfsMonitorT::m_shmimName;
    }

    /// The dark input shmim name.
    std::string darkShmimName()
    {
        return darkMonT::m_shmimName;
    }

    /// The dark monitor's getExistingFirst flag.
    bool darkGetExistingFirst()
    {
        return darkMonT::m_getExistingFirst;
    }

    /// Set the WFS stream geometry on the shmimMonitor base.
    void setWfsSize( uint32_t w /**< [in] width */, uint32_t h /**< [in] height */ )
    {
        wfsMonitorT::m_width  = w;
        wfsMonitorT::m_height = h;
    }

    /// Set the dark stream geometry and data type on the dark shmimMonitor base.
    void setDarkStream( uint32_t w /**< [in] width */,
                        uint32_t h /**< [in] height */,
                        uint8_t  type /**< [in] ImageStreamIO type code */ )
    {
        darkMonT::m_width    = w;
        darkMonT::m_height   = h;
        darkMonT::m_dataType = type;
    }

    /// Create a controller directly, as allocate() does.
    void makeController()
    {
        controller = new DDSPC::PredictiveController( 1, 1, 1, 1, 0.9, 0.1, 1.0, c_dm * c_dm );
    }

    /// Open the DM stream as allocate() does, sizing and zeroing the shaped command.
    int openDmForTest( const std::string &name /**< [in] DM stream name */ )
    {
        m_dmChannel = name;

        if( ImageStreamIO_openIm( &m_dmStream, name.c_str() ) != 0 )
        {
            return -1;
        }

        m_dmOpened = true;
        m_dmWidth  = m_dmStream.md->size[0];
        m_dmHeight = m_dmStream.md->size[1];
        m_shaped_command.resize( m_dmWidth, m_dmHeight );
        m_shaped_command.setZero();

        return 0;
    }

    /// Run appShutdown(), then reset the pointers it deleted.
    int shutdownForTest()
    {
        int rv         = appShutdown();
        controller     = nullptr;
        m_temp_command = nullptr;
        m_command      = nullptr;
        return rv;
    }
};

/// A float shared-memory stream created for a test and destroyed with the object.
class testStream
{
  public:
    /// The creator's image.
    IMAGE m_image{};

    /// True if creation succeeded.
    bool m_created{ false };

    /// Create a w x h float stream named \p name with \p nsem semaphores.
    testStream( const std::string &name /**< [in] stream name */,
                uint32_t           w /**< [in] stream width */,
                uint32_t           h /**< [in] stream height */,
                int                nsem /**< [in] number of semaphores */ )
    {
        uint32_t imsize[3] = { w, h, 1 };
        m_created          = ( ImageStreamIO_createIm_gpu( &m_image,
                                                  name.c_str(),
                                                  3,
                                                  imsize,
                                                  _DATATYPE_FLOAT,
                                                  -1,
                                                  1,
                                                  nsem,
                                                  0,
                                                  CIRCULAR_BUFFER | ZAXIS_TEMPORAL,
                                                  0 ) == IMAGESTREAMIO_SUCCESS );
    }

    /// Destroy the stream.
    ~testStream()
    {
        if( m_created )
        {
            ImageStreamIO_destroyIm( &m_image );
        }
    }

    /// The stream's current write count.
    uint64_t cnt0()
    {
        return m_image.md[0].cnt0;
    }

    /// The value of pixel \p n.
    float pix( size_t n /**< [in] linear pixel index */ )
    {
        return m_image.array.F[n];
    }
};
/// \endcond

/// Create the private shared-memory directory and point ImageStreamIO at it.
/** ImageStreamIO caches the directory on first use, so every test uses the same one.
 */
static void setupShmDir()
{
    std::filesystem::create_directories( c_shmDir );
    setenv( "MILK_SHM_DIR", c_shmDir, 1 );
}

/// The test pupil mask value: all pixels illuminated except (0,0).
static float maskVal( int r /**< [in] row */, int c /**< [in] column */ )
{
    return ( r == 0 && c == 0 ) ? 0.0f : 1.0f;
}

/// The test interaction matrix value of pixel (r,c) in mode \p p.
static float imVal( int p /**< [in] mode */, int r /**< [in] row */, int c /**< [in] column */ )
{
    return 1.0f + p + 0.1f * ( r + c_wfs * c );
}

/// The test reference wavefront value.
static float refVal( int r /**< [in] row */, int c /**< [in] column */ )
{
    return 0.001f * ( r + c );
}

/// Write a square pupil mask of size \p n with \p lit pixels above 0.5 (the first \p lit in column-major order).
static void writeMask( const std::string &fname /**< [in] file name */,
                       int                n /**< [in] mask size */,
                       int                lit /**< [in] number of illuminated pixels, or -1 for maskVal() */ )
{
    eigenImage<float> mask( n, n );
    for( int c = 0; c < n; ++c )
    {
        for( int r = 0; r < n; ++r )
        {
            if( lit < 0 )
            {
                mask( r, c ) = maskVal( r, c );
            }
            else
            {
                mask( r, c ) = ( r + n * c < lit ) ? 0.9f : 0.1f;
            }
        }
    }

    std::filesystem::remove( fname );
    mx::fits::fitsFile<float> ff;
    REQUIRE( ff.write( fname, mask ) == mx::error_t::noerror );
}

/// Write the calibration files read by allocate().
static void writeCalibration()
{
    writeMask( c_maskFile, c_wfs, -1 );

    mx::fits::fitsFile<float> ff;

    eigenCube<float> im( c_wfs, c_wfs, c_modes );
    for( int p = 0; p < c_modes; ++p )
    {
        for( int c = 0; c < c_wfs; ++c )
        {
            for( int r = 0; r < c_wfs; ++r )
            {
                im.image( p )( r, c ) = imVal( p, r, c );
            }
        }
    }
    std::filesystem::remove( c_imFile );
    REQUIRE( ff.write( c_imFile, im ) == mx::error_t::noerror );

    eigenImage<float> ref( c_wfs, c_wfs );
    for( int c = 0; c < c_wfs; ++c )
    {
        for( int r = 0; r < c_wfs; ++r )
        {
            ref( r, c ) = refVal( r, c );
        }
    }
    std::filesystem::remove( c_refFile );
    REQUIRE( ff.write( c_refFile, ref ) == mx::error_t::noerror );

    eigenCube<float> mapCube( c_dm, c_dm, c_modes );
    for( int p = 0; p < c_modes; ++p )
    {
        for( int c = 0; c < c_dm; ++c )
        {
            for( int r = 0; r < c_dm; ++r )
            {
                mapCube.image( p )( r, c ) = p + 0.0001f * ( r + c_dm * c );
            }
        }
    }
    std::filesystem::remove( c_mapFile );
    REQUIRE( ff.write( c_mapFile, mapCube ) == mx::error_t::noerror );
}

/// Remove the calibration files.
static void removeCalibration()
{
    std::filesystem::remove( c_maskFile );
    std::filesystem::remove( c_imFile );
    std::filesystem::remove( c_refFile );
    std::filesystem::remove( c_mapFile );
}

/// Set the parameters and file names used by allocate().
static void configureForAllocate( hoPredCtrl_test &app /**< [in] the harness */ )
{
    app.setWfsSize( c_wfs, c_wfs );
    app.m_pupilMaskFilename           = c_maskFile;
    app.m_interaction_matrix_filename = c_imFile;
    app.m_refWavefront_filename       = c_refFile;
    app.m_mapping_matrix_filename     = c_mapFile;
    app.m_dmChannel                   = c_dmName;
    app.m_numHist                     = 3;
    app.m_numFut                      = 2;
    app.m_gamma                       = 0.98;
    app.m_lambda                      = 1.0;
    app.m_inv_covariance              = 50;
    app.m_clip_val                    = 0.3;
    app.m_exploration_rms             = 0.02;
    app.m_exploration_steps           = 7;
    app.m_intgain                     = 0.4;
    app.m_intleak                     = 0.01;
}

/// Build a Number property with one element.
static pcf::IndiProperty numberProp( const std::string &name /**< [in] property name */,
                                     const std::string &el /**< [in] element name, or "" for none */,
                                     const std::string &value /**< [in] element value */ )
{
    pcf::IndiProperty ip( pcf::IndiProperty::Number );
    ip.setDevice( "hopc" );
    ip.setName( name );
    if( el != "" )
    {
        ip.add( pcf::IndiElement( el, value ) );
    }
    return ip;
}

/// Build a Switch property with one element.
static pcf::IndiProperty switchProp( const std::string &name /**< [in] property name */,
                                     const std::string &el /**< [in] element name, or "" for none */,
                                     bool               on /**< [in] switch state */ )
{
    pcf::IndiProperty ip( pcf::IndiProperty::Switch );
    ip.setDevice( "hopc" );
    ip.setName( name );
    if( el != "" )
    {
        ip.add( pcf::IndiElement( el, on ? pcf::IndiElement::On : pcf::IndiElement::Off ) );
    }
    return ip;
}

/// Verify DDSPC::find_next_power_of_2() returns the bit length of its argument (at least 1).
/**
 * \ingroup hoPredCtrl_unit_test
 */
TEST_CASE( "hoPredCtrl DDSPC find_next_power_of_2", "[hoPredCtrl]" )
{
    // clang-format off
    #ifdef HOPREDCTRL_TEST_DOXYGEN_REF
    DDSPC::find_next_power_of_2(int);
    #endif
    // clang-format on

    // Despite the name, this returns the number of bits, i.e. the exponent of the next power of 2.
    CHECK( DDSPC::find_next_power_of_2( 0 ) == 1 );
    CHECK( DDSPC::find_next_power_of_2( 1 ) == 1 );
    CHECK( DDSPC::find_next_power_of_2( 2 ) == 2 );
    CHECK( DDSPC::find_next_power_of_2( 3 ) == 2 );
    CHECK( DDSPC::find_next_power_of_2( 4 ) == 3 );
    CHECK( DDSPC::find_next_power_of_2( 7 ) == 3 );
    CHECK( DDSPC::find_next_power_of_2( 8 ) == 4 );
    CHECK( DDSPC::find_next_power_of_2( 1023 ) == 10 );
    CHECK( DDSPC::find_next_power_of_2( 1024 ) == 11 );
}

/// Verify the IDX2C and BIDX2C column-major index macros.
/**
 * \ingroup hoPredCtrl_unit_test
 */
TEST_CASE( "hoPredCtrl DDSPC index macros", "[hoPredCtrl]" )
{
    // clang-format off
    #ifdef HOPREDCTRL_TEST_DOXYGEN_REF
    IDX2C(i, j, nrow);
    BIDX2C(i, j, k, nrow, ncol);
    #endif
    // clang-format on

    // IDX2C(i, j, nrow): row i, column j, nrow rows
    CHECK( IDX2C( 0, 0, 3 ) == 0 );
    CHECK( IDX2C( 2, 0, 3 ) == 2 );
    CHECK( IDX2C( 0, 1, 3 ) == 3 );
    CHECK( IDX2C( 1, 2, 3 ) == 7 );

    // BIDX2C(i, j, k, nrow, ncol): batch k is offset by nrow*ncol
    CHECK( BIDX2C( 1, 2, 0, 3, 4 ) == 7 );
    CHECK( BIDX2C( 1, 2, 1, 3, 4 ) == 19 );
    CHECK( BIDX2C( 0, 0, 2, 3, 4 ) == 24 );
}

/// Verify the inline DDSPC::Matrix accessors.
/**
 * \ingroup hoPredCtrl_unit_test
 */
TEST_CASE( "hoPredCtrl DDSPC Matrix inline accessors", "[hoPredCtrl]" )
{
    // clang-format off
    #ifdef HOPREDCTRL_TEST_DOXYGEN_REF
    DDSPC::Matrix::num_elements();
    DDSPC::Matrix::set(float, int, int, int);
    DDSPC::Matrix::get(int, int, int);
    DDSPC::Matrix::get_data_ptr();
    #endif
    // clang-format on

    // The c'tor is a host-only test stub; the accessors are the real inline code.
    DDSPC::Matrix m( 0.5f, 2, 3, 2 );

    CHECK( m.num_elements() == 6 );
    REQUIRE( m.get_data_ptr() == m.cpu_data[0] );
    CHECK( m.get( 1, 2, 0 ) == 0.5f );

    m.set( 7.0f, 1, 2, 0 );
    CHECK( m.get_data_ptr()[5] == 7.0f );
    CHECK( m.get( 1, 2, 0 ) == 7.0f );

    // The default batch index is 1 (the second batch), not 0.
    m.set( 9.0f, 0, 1 );
    CHECK( m.get_data_ptr()[2 + 6] == 9.0f );
    CHECK( m.get( 0, 1 ) == 9.0f );
    CHECK( m.get( 0, 1, 0 ) == 0.5f );
}

/// Verify the inline DDSPC::DistributedAutoRegressiveController setters.
/**
 * \ingroup hoPredCtrl_unit_test
 */
TEST_CASE( "hoPredCtrl DDSPC controller inline setters", "[hoPredCtrl]" )
{
    // clang-format off
    #ifdef HOPREDCTRL_TEST_DOXYGEN_REF
    DDSPC::DistributedAutoRegressiveController::set_integrator(bool, float, float);
    DDSPC::DistributedAutoRegressiveController::set_new_gamma(float);
    DDSPC::DistributedAutoRegressiveController::get_command();
    #endif
    // clang-format on

    cublasHandle_t                             h = nullptr;
    DDSPC::DistributedAutoRegressiveController darc( &h, 2, 1, 3, 0.9, nullptr, 1.0 );

    darc.set_integrator( true, 0.25, 0.05 );
    CHECK( darc.use_predictor == true );
    CHECK( darc.int_gain == Approx( 0.25 ) );
    CHECK( darc.int_leakage == Approx( 0.05 ) );

    darc.set_new_gamma( 0.95 );
    CHECK( darc.gamma == Approx( 0.95 ) );

    CHECK( darc.get_command() == darc.command );
}

/// Verify the configuration defaults.
/**
 * \ingroup hoPredCtrl_unit_test
 */
TEST_CASE( "hoPredCtrl configuration defaults", "[hoPredCtrl]" )
{
    // clang-format off
    #ifdef HOPREDCTRL_TEST_DOXYGEN_REF
    hoPredCtrl::hoPredCtrl();
    hoPredCtrl::setupConfig();
    hoPredCtrl::loadConfig();
    hoPredCtrl::loadConfigImpl(mx::app::appConfigurator&);
    #endif
    // clang-format on

    hoPredCtrl_test app( "hopc" );

    // The c'tor asks the dark monitor to load an existing dark first.
    CHECK( app.darkGetExistingFirst() == true );

    app.setupConfig();

    mx::app::writeConfigFile( "/tmp/hoPredCtrl_test_defaults.conf", { "none" }, { "nada" }, { "0" } );
    app.loadConfigFromFile( "/tmp/hoPredCtrl_test_defaults.conf" );

    CHECK( app.m_pupilMaskFilename == "" );
    CHECK( app.m_interaction_matrix_filename == "" );
    CHECK( app.m_mapping_matrix_filename == "" );
    CHECK( app.m_refWavefront_filename == "" );
    CHECK( app.m_dmChannel == "" );
    CHECK( app.wfsShmimName() == "hopc" );
    CHECK( app.darkShmimName() == "hopc" );

    // The app has no numeric defaults; unset keys leave the (harness-initialized) values alone.
    CHECK( app.m_numHist == 0 );
    CHECK( app.m_numFut == 0 );
    CHECK( app.m_learning_steps == 0 );
    CHECK( app.m_learning_counter == 0 );
    CHECK( app.m_intgain == 0 );

    std::remove( "/tmp/hoPredCtrl_test_defaults.conf" );
}

/// Verify the configuration overrides, including the calibration directory prefix.
/**
 * \ingroup hoPredCtrl_unit_test
 */
TEST_CASE( "hoPredCtrl configuration overrides", "[hoPredCtrl]" )
{
    // clang-format off
    #ifdef HOPREDCTRL_TEST_DOXYGEN_REF
    hoPredCtrl::setupConfig();
    hoPredCtrl::loadConfig();
    hoPredCtrl::loadConfigImpl(mx::app::appConfigurator&);
    #endif
    // clang-format on

    hoPredCtrl_test app( "hopc" );
    app.setupConfig();

    mx::app::writeConfigFile( "/tmp/hoPredCtrl_test_overrides.conf",
                              { "parameters", "parameters", "parameters", "parameters",   "parameters",
                                "parameters", "parameters", "parameters", "parameters",   "parameters",
                                "parameters", "parameters", "parameters", "parameters",   "parameters",
                                "parameters", "integrator", "integrator", "shmimMonitor", "darkShmim" },
                              { "calib_directory",
                                "pupil_mask",
                                "interaction_matrix",
                                "mapping_matrix",
                                "reference_image",
                                "Nhist",
                                "Nfut",
                                "gamma",
                                "inv_covariance",
                                "lambda",
                                "clip_val",
                                "learning_steps",
                                "learning_iterations",
                                "exploration_steps",
                                "exploration_rms",
                                "channel",
                                "gain",
                                "leakage",
                                "shmimName",
                                "shmimName" },
                              { "/calib/", "mask.fits",  "im.fits", "map.fits", "ref.fits", "5",          "2",
                                "0.99",    "100",        "0.01",    "0.5",      "1000",     "4",          "500",
                                "0.02",    "dm00disp07", "0.3",     "0.015",    "camwfs",   "camwfs_dark" } );
    app.loadConfigFromFile( "/tmp/hoPredCtrl_test_overrides.conf" );

    CHECK( app.m_pupilMaskFilename == "/calib/mask.fits" );
    CHECK( app.m_interaction_matrix_filename == "/calib/im.fits" );
    CHECK( app.m_mapping_matrix_filename == "/calib/map.fits" );
    CHECK( app.m_refWavefront_filename == "/calib/ref.fits" );
    CHECK( app.m_numHist == 5 );
    CHECK( app.m_numFut == 2 );
    CHECK( app.m_gamma == Approx( 0.99 ) );
    CHECK( app.m_inv_covariance == Approx( 100 ) );
    CHECK( app.m_lambda == Approx( 0.01 ) );
    CHECK( app.m_clip_val == Approx( 0.5 ) );
    CHECK( app.m_learning_steps == 1000 );
    CHECK( app.m_learning_counter == 1000 );
    CHECK( app.m_learning_iterations == 4 );
    CHECK( app.m_exploration_steps == 500 );
    CHECK( app.m_exploration_rms == Approx( 0.02 ) );
    CHECK( app.m_dmChannel == "dm00disp07" );
    CHECK( app.m_intgain == Approx( 0.3 ) );
    CHECK( app.m_intleak == Approx( 0.015 ) );
    CHECK( app.wfsShmimName() == "camwfs" );
    CHECK( app.darkShmimName() == "camwfs_dark" );

    std::remove( "/tmp/hoPredCtrl_test_overrides.conf" );
}

/// Verify set_pupil_mask() sizes the measurement vector for the full-image and illuminated-pixel reconstructors.
/**
 * \ingroup hoPredCtrl_unit_test
 */
TEST_CASE( "hoPredCtrl set_pupil_mask", "[hoPredCtrl]" )
{
    // clang-format off
    #ifdef HOPREDCTRL_TEST_DOXYGEN_REF
    hoPredCtrl::set_pupil_mask(std::string);
    #endif
    // clang-format on

    const std::string fname = "/tmp/hoPredCtrl_test_setmask.fits";
    writeMask( fname, 6, 10 );

    hoPredCtrl_test app( "hopc" );

    SECTION( "full image" )
    {
        app.use_full_image_reconstructor = true;
        REQUIRE( app.set_pupil_mask( fname ) == 0 );

        CHECK( app.m_pupilMask.rows() == 6 );
        CHECK( app.m_pupilMask.cols() == 6 );
        CHECK( app.m_illuminatedPixels == 0 );
        CHECK( app.m_measurement_size == 36 );
        CHECK( app.m_measurementVector.rows() == 36 );
        CHECK( app.m_measurementVector.cols() == 1 );
        CHECK( app.m_measurementVector.abs().maxCoeff() == 0 );
    }

    SECTION( "illuminated pixels" )
    {
        app.use_full_image_reconstructor = false;
        REQUIRE( app.set_pupil_mask( fname ) == 0 );

        CHECK( app.m_illuminatedPixels == 10 );
        CHECK( app.m_measurement_size == 30 );
        CHECK( app.m_measurementVector.rows() == 30 );
        CHECK( app.m_measurementVector.cols() == 1 );
    }

    std::filesystem::remove( fname );
}

/// Verify the dark allocate() and processImage() store the dark frame.
/**
 * \ingroup hoPredCtrl_unit_test
 */
TEST_CASE( "hoPredCtrl dark frame handling", "[hoPredCtrl]" )
{
    // clang-format off
    #ifdef HOPREDCTRL_TEST_DOXYGEN_REF
    hoPredCtrl::allocate(const darkShmimT&);
    hoPredCtrl::processImage(void*, const darkShmimT&);
    #endif
    // clang-format on

    hoPredCtrl_test app( "hopc" );

    SECTION( "uint16 dark" )
    {
        app.setDarkStream( 3, 3, IMAGESTRUCT_UINT16 );
        app.m_darkSet = true;

        REQUIRE( app.allocate( darkShmimT() ) == 0 );
        CHECK( app.m_darkSet == false );
        CHECK( app.dark_pixget != nullptr );
        CHECK( app.m_darkImage.rows() == 3 );
        CHECK( app.m_darkImage.cols() == 3 );

        std::vector<uint16_t> dark( 9 );
        for( size_t n = 0; n < dark.size(); ++n )
        {
            dark[n] = 100 + 5 * n;
        }

        REQUIRE( app.processImage( dark.data(), darkShmimT() ) == 0 );
        CHECK( app.m_darkSet == true );

        for( size_t n = 0; n < dark.size(); ++n )
        {
            CHECK( app.m_darkImage.data()[n] == Approx( dark[n] ) );
        }
    }

    SECTION( "unsupported data type" )
    {
        app.setDarkStream( 3, 3, 255 );

        CHECK( app.allocate( darkShmimT() ) == -1 );
        CHECK( app.dark_pixget == nullptr );
        CHECK( app.m_darkSet == false );
    }
}

/// Verify allocate() reads the calibration, opens the DM and creates the controller.
/**
 * \ingroup hoPredCtrl_unit_test
 */
TEST_CASE( "hoPredCtrl allocate", "[hoPredCtrl]" )
{
    // clang-format off
    #ifdef HOPREDCTRL_TEST_DOXYGEN_REF
    hoPredCtrl::allocate(const dev::shmimT&);
    hoPredCtrl::send_dm_command();
    #endif
    // clang-format on

    g_stub = hoPredCtrlStubs::hoPredCtrlStubState();
    setupShmDir();
    writeCalibration();

    testStream dm( c_dmName, c_dm, c_dm, 10 );
    REQUIRE( dm.m_created );
    uint64_t cnt0 = dm.cnt0();

    {
        hoPredCtrl_test app( "hopc" );
        configureForAllocate( app );

        REQUIRE( app.allocate( dev::shmimT() ) == 0 );

        // WFS geometry and measurement vector
        CHECK( app.m_pwfsWidth == c_wfs );
        CHECK( app.m_pwfsHeight == c_wfs );
        CHECK( app.m_quadWidth == c_wfs / 2 );
        CHECK( app.m_quadHeight == c_wfs / 2 );
        CHECK( app.use_full_image_reconstructor == true );
        CHECK( app.m_measurement_size == c_wfs * c_wfs );
        CHECK( app.m_measurementVector.rows() == c_wfs * c_wfs );

        // Interaction matrix: one row per mode, each divided by its sum of squares
        REQUIRE( app.m_numModes == c_modes );
        REQUIRE( app.m_interaction_matrix.rows() == c_modes );
        REQUIRE( app.m_interaction_matrix.cols() == c_wfs * c_wfs );
        for( int p = 0; p < c_modes; ++p )
        {
            float sumsq = 0;
            for( int c = 0; c < c_wfs; ++c )
            {
                for( int r = 0; r < c_wfs; ++r )
                {
                    sumsq += imVal( p, r, c ) * imVal( p, r, c );
                }
            }

            for( int c = 0; c < c_wfs; ++c )
            {
                for( int r = 0; r < c_wfs; ++r )
                {
                    CHECK( app.m_interaction_matrix( p, r + c_wfs * c ) == Approx( imVal( p, r, c ) / sumsq ) );
                }
            }
        }

        // Reference and mapping matrix
        CHECK( app.m_refWavefront.rows() == c_wfs );
        CHECK( app.m_refWavefront( 2, 1 ) == Approx( refVal( 2, 1 ) ) );
        REQUIRE( app.m_numVoltages == c_dm * c_dm );
        CHECK( app.m_mapping_matrix.cols() == c_modes );
        CHECK( app.m_mapping_matrix( 51, 1 ) == Approx( 1 + 0.0001 * 51 ) );

        // Temporary command
        REQUIRE( app.m_temp_command != nullptr );
        CHECK( app.m_command == app.m_temp_command );
        CHECK( app.m_temp_command[0] == Approx( 0.001 ) );
        CHECK( app.m_temp_command[c_dm * c_dm - 1] == Approx( 0.001 ) );

        // DM
        CHECK( app.m_dmOpened == true );
        CHECK( app.m_dmWidth == c_dm );
        CHECK( app.m_dmHeight == c_dm );
        CHECK( app.m_dmDataType == _DATATYPE_FLOAT );
        CHECK( app.m_dmTypeSize == sizeof( float ) );
        CHECK( app.m_shaped_command.rows() == c_dm );
        CHECK( app.m_shaped_command.cols() == c_dm );
        CHECK( dm.cnt0() == cnt0 + 1 );
        CHECK( dm.pix( 0 ) == 0 );
        CHECK( dm.pix( c_dm * c_dm - 1 ) == 0 );

        // Controller
        REQUIRE( app.controller != nullptr );
        CHECK( g_stub.pcConstructed == 1 );
        CHECK( g_stub.ctorHistory == 3 );
        CHECK( g_stub.ctorFuture == 2 );
        CHECK( g_stub.ctorModes == c_modes );
        CHECK( g_stub.ctorMeasurements == c_wfs * c_wfs );
        CHECK( g_stub.ctorGamma == Approx( 0.98 ) );
        CHECK( g_stub.ctorLambda == Approx( 1.0 ) );
        CHECK( g_stub.ctorP0 == Approx( 50 ) );
        CHECK( g_stub.ctorActuators == c_dm * c_dm );

        REQUIRE( g_stub.interaction.size() == static_cast<size_t>( c_modes * c_wfs * c_wfs ) );
        for( size_t n = 0; n < g_stub.interaction.size(); ++n )
        {
            CHECK( g_stub.interaction[n] == app.m_interaction_matrix.data()[n] );
        }

        REQUIRE( g_stub.mapping.size() == static_cast<size_t>( c_modes * c_dm * c_dm ) );
        CHECK( g_stub.mapping[c_dm * c_dm + 51] == app.m_mapping_matrix( 51, 1 ) );

        CHECK( g_stub.explorationCalls == 1 );
        CHECK( g_stub.explorationRms == Approx( 0.02 ) );
        CHECK( g_stub.explorationSteps == 7 );

        CHECK( app.controller->controller->use_predictor == false );
        CHECK( app.controller->controller->int_gain == Approx( 0.4 ) );
        CHECK( app.controller->controller->int_leakage == Approx( 0.01 ) );

        // Dark and loop state
        CHECK( app.m_darkImage.rows() == c_wfs );
        CHECK( app.m_darkImage.cols() == c_wfs );
        CHECK( app.m_darkImage.abs().maxCoeff() == 0 );
        CHECK( app.m_darkSet == false );
        CHECK( app.duration == 0 );
        CHECK( app.iterations == 0 );
        CHECK( app.m_is_closed_loop == false );
        CHECK( app.m_use_predictive_control == false );
        CHECK( app.average_pupil_intensity == Approx( -100000.0 ) );
        CHECK( app.savepath == "/data/users/xsup/PredCtrlData/" );
    }

    removeCalibration();
    std::filesystem::remove_all( c_shmDir );
}

/// Verify allocate() fails when the DM stream is missing or has too few semaphores.
/**
 * \ingroup hoPredCtrl_unit_test
 */
TEST_CASE( "hoPredCtrl allocate DM failures", "[hoPredCtrl]" )
{
    // clang-format off
    #ifdef HOPREDCTRL_TEST_DOXYGEN_REF
    hoPredCtrl::allocate(const dev::shmimT&);
    #endif
    // clang-format on

    g_stub = hoPredCtrlStubs::hoPredCtrlStubState();
    setupShmDir();
    writeCalibration();

    SECTION( "DM stream missing" )
    {
        hoPredCtrl_test app( "hopc" );
        configureForAllocate( app );
        app.m_dmChannel = "hoPredCtrl_test_missing_dm";

        CHECK( app.allocate( dev::shmimT() ) == -1 );
        CHECK( app.m_dmOpened == false );
        CHECK( app.controller == nullptr );
        CHECK( g_stub.pcConstructed == 0 );
    }

    SECTION( "DM stream with too few semaphores" )
    {
        testStream dm( c_dmName, c_dm, c_dm, 5 );
        REQUIRE( dm.m_created );

        {
            hoPredCtrl_test app( "hopc" );
            configureForAllocate( app );

            CHECK( app.allocate( dev::shmimT() ) == -1 );
            CHECK( app.m_dmOpened == false );
            CHECK( app.controller == nullptr );
            CHECK( g_stub.pcConstructed == 0 );
        }
    }

    removeCalibration();
    std::filesystem::remove_all( c_shmDir );
}

/// Verify processImage() in open loop computes the normalized, dark- and reference-subtracted measurement.
/**
 * \ingroup hoPredCtrl_unit_test
 */
TEST_CASE( "hoPredCtrl processImage open loop measurement", "[hoPredCtrl]" )
{
    // clang-format off
    #ifdef HOPREDCTRL_TEST_DOXYGEN_REF
    hoPredCtrl::processImage(void*, const dev::shmimT&);
    #endif
    // clang-format on

    g_stub = hoPredCtrlStubs::hoPredCtrlStubState();
    setupShmDir();
    writeCalibration();

    testStream dm( c_dmName, c_dm, c_dm, 10 );
    REQUIRE( dm.m_created );

    {
        hoPredCtrl_test app( "hopc" );
        configureForAllocate( app );
        REQUIRE( app.allocate( dev::shmimT() ) == 0 );

        // A non-zero dark.
        for( int n = 0; n < c_wfs * c_wfs; ++n )
        {
            app.m_darkImage.data()[n] = 2 + ( n % 3 );
        }

        std::vector<uint16_t> frame( c_wfs * c_wfs );
        for( size_t n = 0; n < frame.size(); ++n )
        {
            frame[n] = 100 + 3 * n;
        }

        uint64_t cnt0 = dm.cnt0();

        REQUIRE( app.processImage( frame.data(), dev::shmimT() ) == 0 );

        float norm = 0;
        for( int c = 0; c < c_wfs; ++c )
        {
            for( int r = 0; r < c_wfs; ++r )
            {
                int k = r + c_wfs * c;
                norm += maskVal( r, c ) * ( frame[k] - app.m_darkImage( r, c ) );
            }
        }

        REQUIRE( g_stub.addMeasurementCalls == 1 );
        REQUIRE( g_stub.lastMeasurement.size() == static_cast<size_t>( c_wfs * c_wfs ) );

        for( int c = 0; c < c_wfs; ++c )
        {
            for( int r = 0; r < c_wfs; ++r )
            {
                int   k        = r + c_wfs * c;
                float expected = maskVal( r, c ) * ( ( frame[k] - app.m_darkImage( r, c ) ) / norm - refVal( r, c ) );
                CHECK( app.m_measurementVector( k, 0 ) == Approx( expected ).margin( 1e-7 ) );
                CHECK( g_stub.lastMeasurement[k] == Approx( expected ).margin( 1e-7 ) );
            }
        }

        // Open loop: no command is computed or sent.
        CHECK( g_stub.getCommandCalls == 0 );
        CHECK( dm.cnt0() == cnt0 );
        CHECK( app.iterations == 1 );

        REQUIRE( app.processImage( frame.data(), dev::shmimT() ) == 0 );
        CHECK( g_stub.addMeasurementCalls == 2 );
        CHECK( app.iterations == 2 );
    }

    removeCalibration();
    std::filesystem::remove_all( c_shmDir );
}

/// Verify processImage() in closed loop sends the command and runs the learning schedule.
/**
 * \ingroup hoPredCtrl_unit_test
 */
TEST_CASE( "hoPredCtrl processImage closed loop", "[hoPredCtrl]" )
{
    // clang-format off
    #ifdef HOPREDCTRL_TEST_DOXYGEN_REF
    hoPredCtrl::processImage(void*, const dev::shmimT&);
    hoPredCtrl::map_command_vector_to_dmshmim();
    hoPredCtrl::send_dm_command();
    hoPredCtrl::zero();
    #endif
    // clang-format on

    g_stub = hoPredCtrlStubs::hoPredCtrlStubState();
    setupShmDir();
    writeCalibration();

    g_stub.command.resize( c_dm * c_dm );
    for( size_t n = 0; n < g_stub.command.size(); ++n )
    {
        g_stub.command[n] = 0.001f * n;
    }

    std::vector<uint16_t> frame( c_wfs * c_wfs, 1000 );

    testStream dm( c_dmName, c_dm, c_dm, 10 );
    REQUIRE( dm.m_created );

    {
        hoPredCtrl_test app( "hopc" );
        configureForAllocate( app );
        REQUIRE( app.allocate( dev::shmimT() ) == 0 );

        g_stub.explorationCalls = 0;
        app.m_is_closed_loop    = true;

        SECTION( "integrator only: command sent, no learning" )
        {
            uint64_t cnt0 = dm.cnt0();

            REQUIRE( app.processImage( frame.data(), dev::shmimT() ) == 0 );

            CHECK( g_stub.getCommandCalls == 1 );
            CHECK( g_stub.lastClip == Approx( 0.3 ) );
            CHECK( app.m_command == g_stub.command.data() );
            CHECK( dm.cnt0() == cnt0 + 1 );
            CHECK( dm.pix( 0 ) == g_stub.command[0] );
            CHECK( dm.pix( 1234 ) == g_stub.command[1234] );
            CHECK( dm.pix( c_dm * c_dm - 1 ) == g_stub.command[c_dm * c_dm - 1] );
            CHECK( app.m_shaped_command( 3, 2 ) == g_stub.command[3 + c_dm * 2] );
            CHECK( g_stub.updatePredictorCalls == 0 );
            CHECK( g_stub.updateControllerCalls == 0 );
        }

        SECTION( "predictor learns for a fixed number of steps, then resets" )
        {
            app.m_use_predictive_control = true;
            app.m_learning_steps         = 2;
            app.m_learning_counter       = 2;
            app.m_learning_iterations    = 1;
            app.m_lambda                 = 1.0;

            uint64_t cnt0 = dm.cnt0();

            REQUIRE( app.processImage( frame.data(), dev::shmimT() ) == 0 );
            CHECK( g_stub.updatePredictorCalls == 1 );
            CHECK( g_stub.updateControllerCalls == 1 );
            CHECK( app.m_learning_counter == 1 );
            CHECK( dm.cnt0() == cnt0 + 1 );

            // The second step ends the learning cycle: reset for the next iteration with less regularization.
            REQUIRE( app.processImage( frame.data(), dev::shmimT() ) == 0 );
            CHECK( g_stub.updatePredictorCalls == 2 );
            CHECK( g_stub.updateControllerCalls == 2 );
            CHECK( app.m_learning_iterations == 0 );
            CHECK( app.m_learning_counter == 2 );
            CHECK( app.m_lambda == Approx( 0.1 ) );
            CHECK( g_stub.setZeroCalls == 1 );
            CHECK( g_stub.explorationCalls == 1 );
            CHECK( g_stub.regularizationCalls == 1 );
            CHECK( g_stub.lastRegularization == Approx( 0.1 ) );
            CHECK( g_stub.resetBufferCalls == 1 );
            CHECK( dm.cnt0() == cnt0 + 3 ); // the command, then zero()
            CHECK( dm.pix( 1234 ) == 0 );

            // With no learning iterations left, the next cycle does not reset.
            REQUIRE( app.processImage( frame.data(), dev::shmimT() ) == 0 );
            REQUIRE( app.processImage( frame.data(), dev::shmimT() ) == 0 );
            CHECK( g_stub.updatePredictorCalls == 4 );
            CHECK( app.m_learning_counter == 0 );
            CHECK( g_stub.setZeroCalls == 1 );

            REQUIRE( app.processImage( frame.data(), dev::shmimT() ) == 0 );
            CHECK( g_stub.updatePredictorCalls == 4 );
            CHECK( app.m_learning_counter == 0 );
            CHECK( dm.pix( 1234 ) == g_stub.command[1234] );
        }

        SECTION( "a learning counter of -1 always learns" )
        {
            app.m_use_predictive_control = true;
            app.m_learning_counter       = -1;
            app.m_learning_iterations    = 3;

            for( int n = 0; n < 3; ++n )
            {
                REQUIRE( app.processImage( frame.data(), dev::shmimT() ) == 0 );
            }

            CHECK( g_stub.updatePredictorCalls == 3 );
            CHECK( g_stub.updateControllerCalls == 3 );
            CHECK( app.m_learning_counter == -1 );
            CHECK( app.m_learning_iterations == 3 );
            CHECK( g_stub.setZeroCalls == 0 );
        }

        SECTION( "without actuator mapping the shaped command is not updated" )
        {
            app.use_actuators = false;
            app.m_shaped_command.setConstant( 0.5 );

            REQUIRE( app.processImage( frame.data(), dev::shmimT() ) == 0 );
            CHECK( g_stub.getCommandCalls == 1 );
            CHECK( dm.pix( 1234 ) == 0.5 );
        }
    }

    removeCalibration();
    std::filesystem::remove_all( c_shmDir );
}

/// Verify zero(), send_dm_command() and map_command_vector_to_dmshmim() directly.
/**
 * \ingroup hoPredCtrl_unit_test
 */
TEST_CASE( "hoPredCtrl DM command helpers", "[hoPredCtrl]" )
{
    // clang-format off
    #ifdef HOPREDCTRL_TEST_DOXYGEN_REF
    hoPredCtrl::map_command_vector_to_dmshmim();
    hoPredCtrl::send_dm_command();
    hoPredCtrl::zero();
    #endif
    // clang-format on

    setupShmDir();

    testStream dm( c_dmName, c_dm, c_dm, 10 );
    REQUIRE( dm.m_created );

    {
        hoPredCtrl_test app( "hopc" );
        REQUIRE( app.openDmForTest( c_dmName ) == 0 );
        REQUIRE( app.m_dmWidth == c_dm );
        REQUIRE( app.m_dmHeight == c_dm );

        std::vector<float> cmd( c_dm * c_dm );
        for( size_t n = 0; n < cmd.size(); ++n )
        {
            cmd[n] = -1.0f + 0.0005f * n;
        }
        app.m_command = cmd.data();

        REQUIRE( app.map_command_vector_to_dmshmim() == 0 );
        CHECK( app.m_shaped_command( 0, 0 ) == cmd[0] );
        CHECK( app.m_shaped_command( 7, 3 ) == cmd[7 + c_dm * 3] );
        CHECK( app.m_shaped_command( c_dm - 1, c_dm - 1 ) == cmd[c_dm * c_dm - 1] );

        uint64_t cnt0 = dm.cnt0();
        REQUIRE( app.send_dm_command() == 0 );
        CHECK( dm.cnt0() == cnt0 + 1 );
        CHECK( dm.m_image.md[0].write == 0 );
        CHECK( dm.pix( 7 + c_dm * 3 ) == cmd[7 + c_dm * 3] );

        REQUIRE( app.zero() == 0 );
        CHECK( dm.cnt0() == cnt0 + 2 );
        CHECK( app.m_shaped_command.abs().maxCoeff() == 0 );
        CHECK( dm.pix( 7 + c_dm * 3 ) == 0 );

        app.m_command = nullptr;
    }

    std::filesystem::remove_all( c_shmDir );
}

/// Verify the learning-parameter and timestamp number callbacks.
/**
 * \ingroup hoPredCtrl_unit_test
 */
TEST_CASE( "hoPredCtrl learning parameter INDI callbacks", "[hoPredCtrl]" )
{
    // clang-format off
    #ifdef HOPREDCTRL_TEST_DOXYGEN_REF
    hoPredCtrl::newCallBack_m_indiP_learningSteps(const pcf::IndiProperty&);
    hoPredCtrl::newCallBack_m_indiP_learningIterations(const pcf::IndiProperty&);
    hoPredCtrl::newCallBack_m_indiP_explorationRms(const pcf::IndiProperty&);
    hoPredCtrl::newCallBack_m_indiP_explorationSteps(const pcf::IndiProperty&);
    hoPredCtrl::newCallBack_m_indiP_timestamp(const pcf::IndiProperty&);
    #endif
    // clang-format on

    hoPredCtrl_test app( "hopc" );

    SECTION( "learning_steps" )
    {
        CHECK( app.newCallBack_m_indiP_learningSteps( numberProp( "wrong", "target", "5" ) ) == -1 );
        CHECK( app.m_learning_steps == 0 );

        CHECK( app.newCallBack_m_indiP_learningSteps( numberProp( "learning_steps", "", "" ) ) == 0 );
        CHECK( app.m_learning_steps == 0 );

        CHECK( app.newCallBack_m_indiP_learningSteps( numberProp( "learning_steps", "target", "250" ) ) == 0 );
        CHECK( app.m_learning_steps == 250 );
        CHECK( app.m_learning_counter == 250 );

        CHECK( app.newCallBack_m_indiP_learningSteps( numberProp( "learning_steps", "current", "40" ) ) == 0 );
        CHECK( app.m_learning_steps == 40 );
        CHECK( app.m_learning_counter == 40 );
    }

    SECTION( "learning_iterations" )
    {
        CHECK( app.newCallBack_m_indiP_learningIterations( numberProp( "wrong", "target", "5" ) ) == -1 );
        CHECK( app.newCallBack_m_indiP_learningIterations( numberProp( "learning_iterations", "", "" ) ) == 0 );
        CHECK( app.m_learning_iterations == 0 );

        CHECK( app.newCallBack_m_indiP_learningIterations( numberProp( "learning_iterations", "target", "6" ) ) == 0 );
        CHECK( app.m_learning_iterations == 6 );
    }

    SECTION( "exploration_rms" )
    {
        CHECK( app.newCallBack_m_indiP_explorationRms( numberProp( "wrong", "target", "0.1" ) ) == -1 );
        CHECK( app.newCallBack_m_indiP_explorationRms( numberProp( "exploration_rms", "", "" ) ) == 0 );
        CHECK( app.m_exploration_rms == 0 );

        CHECK( app.newCallBack_m_indiP_explorationRms( numberProp( "exploration_rms", "target", "0.035" ) ) == 0 );
        CHECK( app.m_exploration_rms == Approx( 0.035 ) );
    }

    SECTION( "exploration_steps" )
    {
        CHECK( app.newCallBack_m_indiP_explorationSteps( numberProp( "wrong", "target", "5" ) ) == -1 );
        CHECK( app.newCallBack_m_indiP_explorationSteps( numberProp( "exploration_steps", "", "" ) ) == 0 );
        CHECK( app.m_exploration_steps == 0 );

        CHECK( app.newCallBack_m_indiP_explorationSteps( numberProp( "exploration_steps", "target", "3000" ) ) == 0 );
        CHECK( app.m_exploration_steps == 3000 );
    }

    SECTION( "timestamp" )
    {
        CHECK( app.newCallBack_m_indiP_timestamp( numberProp( "wrong", "target", "5" ) ) == -1 );
        CHECK( app.newCallBack_m_indiP_timestamp( numberProp( "timestamp", "", "" ) ) == 0 );
        CHECK( app.loading_timestamp == 0 );

        CHECK( app.newCallBack_m_indiP_timestamp( numberProp( "timestamp", "target", "20240131123456" ) ) == 0 );
        CHECK( app.loading_timestamp == 20240131123456ULL );
    }
}

/// Verify the controller-parameter callbacks, which lambda, clipval and gamma refuse in closed loop.
/**
 * \ingroup hoPredCtrl_unit_test
 */
TEST_CASE( "hoPredCtrl controller parameter INDI callbacks", "[hoPredCtrl]" )
{
    // clang-format off
    #ifdef HOPREDCTRL_TEST_DOXYGEN_REF
    hoPredCtrl::newCallBack_m_indiP_lambda(const pcf::IndiProperty&);
    hoPredCtrl::newCallBack_m_indiP_clipval(const pcf::IndiProperty&);
    hoPredCtrl::newCallBack_m_indiP_gamma(const pcf::IndiProperty&);
    hoPredCtrl::newCallBack_m_indiP_intgain(const pcf::IndiProperty&);
    hoPredCtrl::newCallBack_m_indiP_intleak(const pcf::IndiProperty&);
    #endif
    // clang-format on

    g_stub = hoPredCtrlStubs::hoPredCtrlStubState();

    hoPredCtrl_test app( "hopc" );
    app.makeController();

    SECTION( "lambda" )
    {
        CHECK( app.newCallBack_m_indiP_lambda( numberProp( "wrong", "target", "0.5" ) ) == -1 );
        CHECK( app.newCallBack_m_indiP_lambda( numberProp( "lambda", "", "" ) ) == 0 );
        CHECK( g_stub.regularizationCalls == 0 );

        CHECK( app.newCallBack_m_indiP_lambda( numberProp( "lambda", "target", "0.5" ) ) == 0 );
        CHECK( app.m_lambda == Approx( 0.5 ) );
        CHECK( g_stub.regularizationCalls == 1 );
        CHECK( g_stub.lastRegularization == Approx( 0.5 ) );

        app.m_is_closed_loop = true;
        CHECK( app.newCallBack_m_indiP_lambda( numberProp( "lambda", "target", "2.0" ) ) == 0 );
        CHECK( app.m_lambda == Approx( 0.5 ) );
        CHECK( g_stub.regularizationCalls == 1 );
    }

    SECTION( "clipval" )
    {
        CHECK( app.newCallBack_m_indiP_clipval( numberProp( "wrong", "target", "0.5" ) ) == -1 );
        CHECK( app.newCallBack_m_indiP_clipval( numberProp( "clipval", "target", "0.25" ) ) == 0 );
        CHECK( app.m_clip_val == Approx( 0.25 ) );

        app.m_is_closed_loop = true;
        CHECK( app.newCallBack_m_indiP_clipval( numberProp( "clipval", "target", "0.75" ) ) == 0 );
        CHECK( app.m_clip_val == Approx( 0.25 ) );
    }

    SECTION( "gamma" )
    {
        CHECK( app.newCallBack_m_indiP_gamma( numberProp( "wrong", "target", "0.5" ) ) == -1 );
        CHECK( app.newCallBack_m_indiP_gamma( numberProp( "gamma", "current", "0.97" ) ) == 0 );
        CHECK( app.m_gamma == Approx( 0.97 ) );
        CHECK( g_stub.gammaCalls == 1 );
        CHECK( g_stub.lastGamma == Approx( 0.97 ) );

        app.m_is_closed_loop = true;
        CHECK( app.newCallBack_m_indiP_gamma( numberProp( "gamma", "target", "0.9" ) ) == 0 );
        CHECK( app.m_gamma == Approx( 0.97 ) );
        CHECK( g_stub.gammaCalls == 1 );
    }

    SECTION( "intgain and intleak, also in closed loop" )
    {
        CHECK( app.newCallBack_m_indiP_intgain( numberProp( "wrong", "target", "0.5" ) ) == -1 );
        CHECK( app.newCallBack_m_indiP_intleak( numberProp( "wrong", "target", "0.5" ) ) == -1 );

        app.m_is_closed_loop = true;

        CHECK( app.newCallBack_m_indiP_intgain( numberProp( "intgain", "target", "0.45" ) ) == 0 );
        CHECK( app.m_intgain == Approx( 0.45 ) );
        CHECK( app.controller->controller->int_gain == Approx( 0.45 ) );

        CHECK( app.newCallBack_m_indiP_intleak( numberProp( "intleak", "target", "0.02" ) ) == 0 );
        CHECK( app.m_intleak == Approx( 0.02 ) );
        CHECK( app.controller->controller->int_gain == Approx( 0.45 ) );
        CHECK( app.controller->controller->int_leakage == Approx( 0.02 ) );
        CHECK( app.controller->controller->use_predictor == false );

        CHECK( app.newCallBack_m_indiP_intleak( numberProp( "intleak", "", "" ) ) == 0 );
        CHECK( app.m_intleak == Approx( 0.02 ) );
    }
}

/// Verify the control and predictor toggle callbacks.
/**
 * \ingroup hoPredCtrl_unit_test
 */
TEST_CASE( "hoPredCtrl toggle INDI callbacks", "[hoPredCtrl]" )
{
    // clang-format off
    #ifdef HOPREDCTRL_TEST_DOXYGEN_REF
    hoPredCtrl::newCallBack_m_indiP_controlToggle(const pcf::IndiProperty&);
    hoPredCtrl::newCallBack_m_indiP_predictorToggle(const pcf::IndiProperty&);
    #endif
    // clang-format on

    g_stub = hoPredCtrlStubs::hoPredCtrlStubState();

    hoPredCtrl_test app( "hopc" );

    SECTION( "control" )
    {
        CHECK( app.newCallBack_m_indiP_controlToggle( switchProp( "wrong", "toggle", true ) ) == -1 );
        CHECK( app.m_is_closed_loop == false );

        REQUIRE_THROWS_AS( app.newCallBack_m_indiP_controlToggle( switchProp( "control", "", true ) ),
                           std::runtime_error );

        CHECK( app.newCallBack_m_indiP_controlToggle( switchProp( "control", "toggle", false ) ) == 0 );
        CHECK( app.m_is_closed_loop == false );

        CHECK( app.newCallBack_m_indiP_controlToggle( switchProp( "control", "toggle", true ) ) == 0 );
        CHECK( app.m_is_closed_loop == true );

        CHECK( app.newCallBack_m_indiP_controlToggle( switchProp( "control", "toggle", true ) ) == 0 );
        CHECK( app.m_is_closed_loop == true );

        CHECK( app.newCallBack_m_indiP_controlToggle( switchProp( "control", "toggle", false ) ) == 0 );
        CHECK( app.m_is_closed_loop == false );
    }

    SECTION( "use_predictor" )
    {
        app.makeController();
        app.m_intgain = 0.3;
        app.m_intleak = 0.01;

        CHECK( app.newCallBack_m_indiP_predictorToggle( switchProp( "wrong", "toggle", true ) ) == -1 );
        CHECK( app.m_use_predictive_control == false );

        REQUIRE_THROWS_AS( app.newCallBack_m_indiP_predictorToggle( switchProp( "use_predictor", "", true ) ),
                           std::runtime_error );

        CHECK( app.newCallBack_m_indiP_predictorToggle( switchProp( "use_predictor", "toggle", true ) ) == 0 );
        CHECK( app.m_use_predictive_control == true );
        CHECK( app.controller->controller->use_predictor == true );
        CHECK( app.controller->controller->int_gain == Approx( 0.3 ) );
        CHECK( app.controller->controller->int_leakage == Approx( 0.01 ) );

        // Switching back to the integrator is refused in closed loop.
        app.m_is_closed_loop = true;
        CHECK( app.newCallBack_m_indiP_predictorToggle( switchProp( "use_predictor", "toggle", false ) ) == 0 );
        CHECK( app.m_use_predictive_control == true );
        CHECK( app.controller->controller->use_predictor == true );

        app.m_is_closed_loop = false;
        CHECK( app.newCallBack_m_indiP_predictorToggle( switchProp( "use_predictor", "toggle", false ) ) == 0 );
        CHECK( app.m_use_predictive_control == false );
        CHECK( app.controller->controller->use_predictor == false );
    }
}

/// Verify the request callbacks that act only on the controller.
/**
 * \ingroup hoPredCtrl_unit_test
 */
TEST_CASE( "hoPredCtrl controller request INDI callbacks", "[hoPredCtrl]" )
{
    // clang-format off
    #ifdef HOPREDCTRL_TEST_DOXYGEN_REF
    hoPredCtrl::newCallBack_m_indiP_reset_bufferRequest(const pcf::IndiProperty&);
    hoPredCtrl::newCallBack_m_indiP_reset_exploreRequest(const pcf::IndiProperty&);
    hoPredCtrl::newCallBack_m_indiP_reset_modelRequest(const pcf::IndiProperty&);
    hoPredCtrl::newCallBack_m_indiP_updateControllerRequest(const pcf::IndiProperty&);
    hoPredCtrl::newCallBack_m_indiP_saveRequest(const pcf::IndiProperty&);
    hoPredCtrl::newCallBack_m_indiP_loadRequest(const pcf::IndiProperty&);
    #endif
    // clang-format on

    g_stub = hoPredCtrlStubs::hoPredCtrlStubState();

    hoPredCtrl_test app( "hopc" );
    app.makeController();

    SECTION( "reset_buffer" )
    {
        CHECK( app.newCallBack_m_indiP_reset_bufferRequest( switchProp( "wrong", "request", true ) ) == -1 );
        CHECK( app.newCallBack_m_indiP_reset_bufferRequest( switchProp( "reset_buffer", "", true ) ) == 0 );
        CHECK( app.newCallBack_m_indiP_reset_bufferRequest( switchProp( "reset_buffer", "request", false ) ) == 0 );
        CHECK( g_stub.resetBufferCalls == 0 );

        CHECK( app.newCallBack_m_indiP_reset_bufferRequest( switchProp( "reset_buffer", "request", true ) ) == 0 );
        CHECK( g_stub.resetBufferCalls == 1 );
    }

    SECTION( "reset_exploration" )
    {
        app.m_exploration_rms   = 0.05;
        app.m_exploration_steps = 123;

        CHECK( app.newCallBack_m_indiP_reset_exploreRequest( switchProp( "wrong", "request", true ) ) == -1 );
        CHECK( app.newCallBack_m_indiP_reset_exploreRequest( switchProp( "reset_exploration", "", true ) ) == 0 );
        CHECK( g_stub.explorationCalls == 0 );

        CHECK( app.newCallBack_m_indiP_reset_exploreRequest( switchProp( "reset_exploration", "request", true ) ) ==
               0 );
        CHECK( g_stub.explorationCalls == 1 );
        CHECK( g_stub.explorationRms == Approx( 0.05 ) );
        CHECK( g_stub.explorationSteps == 123 );
    }

    SECTION( "reset_model" )
    {
        CHECK( app.newCallBack_m_indiP_reset_modelRequest( switchProp( "wrong", "request", true ) ) == -1 );
        CHECK( app.newCallBack_m_indiP_reset_modelRequest( switchProp( "reset_model", "request", false ) ) == 0 );
        CHECK( g_stub.resetControllerCalls == 0 );

        CHECK( app.newCallBack_m_indiP_reset_modelRequest( switchProp( "reset_model", "request", true ) ) == 0 );
        CHECK( g_stub.resetControllerCalls == 1 );
    }

    SECTION( "calc_controller" )
    {
        CHECK( app.newCallBack_m_indiP_updateControllerRequest( switchProp( "wrong", "request", true ) ) == -1 );
        CHECK( app.newCallBack_m_indiP_updateControllerRequest( switchProp( "calc_controller", "", true ) ) == 0 );
        CHECK( g_stub.updateControllerCalls == 0 );

        CHECK( app.newCallBack_m_indiP_updateControllerRequest( switchProp( "calc_controller", "request", true ) ) ==
               0 );
        CHECK( g_stub.updateControllerCalls == 1 );
        CHECK( g_stub.updatePredictorCalls == 0 );
    }

    SECTION( "save and load" )
    {
        app.savepath          = "/tmp/hoPredCtrl_test_state/";
        app.loading_timestamp = 20240131;

        CHECK( app.newCallBack_m_indiP_saveRequest( switchProp( "wrong", "request", true ) ) == -1 );
        CHECK( app.newCallBack_m_indiP_loadRequest( switchProp( "wrong", "request", true ) ) == -1 );
        CHECK( app.newCallBack_m_indiP_saveRequest( switchProp( "save", "request", false ) ) == 0 );
        CHECK( app.newCallBack_m_indiP_loadRequest( switchProp( "load", "", true ) ) == 0 );
        CHECK( g_stub.saveCalls == 0 );
        CHECK( g_stub.loadCalls == 0 );

        CHECK( app.newCallBack_m_indiP_saveRequest( switchProp( "save", "request", true ) ) == 0 );
        CHECK( g_stub.saveCalls == 1 );
        CHECK( g_stub.savePath == "/tmp/hoPredCtrl_test_state/" );

        CHECK( app.newCallBack_m_indiP_loadRequest( switchProp( "load", "request", true ) ) == 0 );
        CHECK( g_stub.loadCalls == 1 );
        CHECK( g_stub.loadPath == "/tmp/hoPredCtrl_test_state/" );
        CHECK( g_stub.loadTimestamp == "20240131" );
    }
}

/// Verify the clean and zero request callbacks, which also write the DM.
/**
 * \ingroup hoPredCtrl_unit_test
 */
TEST_CASE( "hoPredCtrl DM request INDI callbacks", "[hoPredCtrl]" )
{
    // clang-format off
    #ifdef HOPREDCTRL_TEST_DOXYGEN_REF
    hoPredCtrl::newCallBack_m_indiP_reset_cleanRequest(const pcf::IndiProperty&);
    hoPredCtrl::newCallBack_m_indiP_zeroRequest(const pcf::IndiProperty&);
    #endif
    // clang-format on

    g_stub = hoPredCtrlStubs::hoPredCtrlStubState();
    setupShmDir();

    testStream dm( c_dmName, c_dm, c_dm, 10 );
    REQUIRE( dm.m_created );

    {
        hoPredCtrl_test app( "hopc" );
        app.makeController();
        REQUIRE( app.openDmForTest( c_dmName ) == 0 );
        app.m_shaped_command.setConstant( 0.25 );

        uint64_t cnt0 = dm.cnt0();

        SECTION( "clean" )
        {
            app.m_exploration_rms   = 0.01;
            app.m_exploration_steps = 50;

            CHECK( app.newCallBack_m_indiP_reset_cleanRequest( switchProp( "wrong", "request", true ) ) == -1 );
            CHECK( app.newCallBack_m_indiP_reset_cleanRequest( switchProp( "clean", "request", false ) ) == 0 );
            CHECK( g_stub.resetControllerCalls == 0 );
            CHECK( dm.cnt0() == cnt0 );

            CHECK( app.newCallBack_m_indiP_reset_cleanRequest( switchProp( "clean", "request", true ) ) == 0 );
            CHECK( g_stub.explorationCalls == 1 );
            CHECK( g_stub.explorationRms == Approx( 0.01 ) );
            CHECK( g_stub.explorationSteps == 50 );
            CHECK( g_stub.resetControllerCalls == 1 );
            CHECK( g_stub.resetBufferCalls == 1 );
            CHECK( g_stub.setZeroCalls == 1 );
            CHECK( app.m_shaped_command.abs().maxCoeff() == 0 );
            CHECK( dm.cnt0() == cnt0 + 1 );
        }

        SECTION( "zero in open loop" )
        {
            CHECK( app.newCallBack_m_indiP_zeroRequest( switchProp( "wrong", "request", true ) ) == -1 );
            CHECK( app.newCallBack_m_indiP_zeroRequest( switchProp( "zero", "", true ) ) == 0 );
            CHECK( app.m_shaped_command( 0, 0 ) == 0.25 );

            CHECK( app.newCallBack_m_indiP_zeroRequest( switchProp( "zero", "request", true ) ) == 0 );
            CHECK( app.m_shaped_command.abs().maxCoeff() == 0 );
            CHECK( g_stub.setZeroCalls == 1 );
            CHECK( dm.cnt0() == cnt0 + 1 );
        }

        SECTION( "zero in closed loop" )
        {
            app.m_is_closed_loop = true;

            CHECK( app.newCallBack_m_indiP_zeroRequest( switchProp( "zero", "request", true ) ) == 0 );
            CHECK( app.m_shaped_command.abs().maxCoeff() == 0 );
            CHECK( g_stub.setZeroCalls == 0 );
            CHECK( dm.cnt0() == cnt0 );
        }
    }

    std::filesystem::remove_all( c_shmDir );
}

/// Verify appShutdown() zeroes the DM and deletes the controller.
/**
 * \ingroup hoPredCtrl_unit_test
 */
TEST_CASE( "hoPredCtrl appShutdown", "[hoPredCtrl]" )
{
    // clang-format off
    #ifdef HOPREDCTRL_TEST_DOXYGEN_REF
    hoPredCtrl::appShutdown();
    #endif
    // clang-format on

    g_stub = hoPredCtrlStubs::hoPredCtrlStubState();
    setupShmDir();
    writeCalibration();

    testStream dm( c_dmName, c_dm, c_dm, 10 );
    REQUIRE( dm.m_created );

    {
        hoPredCtrl_test app( "hopc" );
        configureForAllocate( app );
        REQUIRE( app.allocate( dev::shmimT() ) == 0 );

        app.m_shaped_command.setConstant( 0.5 );
        REQUIRE( app.send_dm_command() == 0 );
        REQUIRE( dm.pix( 100 ) == 0.5 );

        uint64_t cnt0 = dm.cnt0();

        CHECK( app.shutdownForTest() == 0 );
        CHECK( dm.cnt0() == cnt0 + 1 );
        CHECK( dm.pix( 100 ) == 0 );
        CHECK( g_stub.pcDestroyed == 1 );
    }

    removeCalibration();
    std::filesystem::remove_all( c_shmDir );
}

} // namespace hoPredCtrlTest

} // namespace libXWCTest
