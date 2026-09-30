/** \file loPredCtrl_test.cpp
 * \brief Catch2 tests for the loPredCtrl app and its DDSPC predictive-control sources.
 * \author Jared R. Males (jaredmales@gmail.com)
 * \author Claude Code
 */

#include "../../../tests/testXWC.hpp"

#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "../loPredCtrl.hpp"

// The DDSPC sources are built as OTHER_OBJS for the app; include them so this test is a single translation unit.
#include "../utils.cpp"
#include "../recursive_least_squares.cpp"
#include "../ar_controller.cpp"

using namespace MagAOX::app;

namespace libXWCTest
{

/** \defgroup loPredCtrl_unit_test loPredCtrl Unit Tests
 * \brief Unit tests for the loPredCtrl application.
 *
 * \ingroup application_unit_test
 */

/// Namespace for `loPredCtrl` unit tests.
/** \ingroup loPredCtrl_unit_test
 */
namespace loPredCtrlTest
{

/// \cond DOXYGEN_SUPPRESS_TEST_HARNESS
/// Test harness exposing loPredCtrl internals.
class loPredCtrl_test : public loPredCtrl
{
  public:
    /// The shmimMonitor base type.
    typedef dev::shmimMonitor<loPredCtrl> smBaseT;

    /// Construct a harness with the given device name.
    explicit loPredCtrl_test( const std::string &device /**< [in] INDI device name */ )
    {
        m_configName = device;

        m_indiP_exploration.setDevice( device );
        m_indiP_exploration.setName( "exploration_sequence" );
        m_indiP_learningToggle.setDevice( device );
        m_indiP_learningToggle.setName( "learn" );
        m_indiP_predictingToggle.setDevice( device );
        m_indiP_predictingToggle.setName( "predict" );
        m_indiP_resetToggle.setDevice( device );
        m_indiP_resetToggle.setName( "reset_model" );
    }

    /// Destroy the harness, deleting any controller allocate() created (the app only does so in appShutdown()).
    ~loPredCtrl_test() noexcept
    {
        if( controller )
        {
            delete controller;
            controller = nullptr;
        }
    }

    /// Register the configuration options.
    void setupConfigForTest()
    {
        setupConfig();
    }

    /// Read a config file and run loadConfig().
    void loadConfigFromFile( const std::string &fname /**< [in] config file path */ )
    {
        config.readConfig( fname );
        loadConfig();
    }

    /// Set the controller parameters used by allocate().
    void setParams( int   nModes /**< [in] number of controlled modes */,
                    int   history /**< [in] history length */,
                    int   future /**< [in] future length */,
                    float gain /**< [in] integrator gain */ )
    {
        m_num_modes = nModes;
        m_history   = history;
        m_future    = future;
        m_gainCtrl  = gain;
    }

    /// Set the input stream geometry and call allocate().
    int allocateForTest( uint32_t w /**< [in] width */, uint32_t h /**< [in] height */ )
    {
        smBaseT::m_width  = w;
        smBaseT::m_height = h;

        // allocate() leaks any existing controller; delete it first.
        if( controller )
        {
            delete controller;
            controller = nullptr;
        }

        return allocate( dev::shmimT() );
    }

    /// Call processImage().
    int processImageForTest( std::vector<float> &im /**< [in] modeval frame */ )
    {
        return processImage( im.data(), dev::shmimT() );
    }

    /// The controller.
    DDSPC::PredictiveController *ctrl()
    {
        return controller;
    }

    /// The full output command.
    DDSPC::Matrix &fullCommand()
    {
        return full_command;
    }

    /// The new command.
    DDSPC::Matrix &newCommand()
    {
        return new_command;
    }

    /// The new measurement.
    DDSPC::Matrix &newMeasurement()
    {
        return new_measurement;
    }

    /// Modeval width.
    uint32_t modevalWidth()
    {
        return m_modevalWidth;
    }

    /// Modeval height.
    uint32_t modevalHeight()
    {
        return m_modevalHeight;
    }

    /// Modeval type size.
    uint32_t modevalTypeSize()
    {
        return m_modevalTypeSize;
    }

    /// Frame counter.
    long long frameCounter()
    {
        return frame_counter;
    }

    /// Gain.
    float gainCtrl()
    {
        return m_gainCtrl;
    }

    /// Regularization.
    float regularizationCtrl()
    {
        return m_regularizationCtrl;
    }

    /// Forgetting factor.
    float gammaCtrl()
    {
        return m_gammaCtrl;
    }

    /// Initial covariance.
    float covarianceCtrl()
    {
        return m_covarianceCtrl;
    }

    /// Number of modes.
    int numModes()
    {
        return m_num_modes;
    }

    /// History length.
    int history()
    {
        return m_history;
    }

    /// Future length.
    int future()
    {
        return m_future;
    }

    /// Output shmim name.
    std::string outputName()
    {
        return m_outputName;
    }

    /// Input shmim name.
    std::string shmimName()
    {
        return smBaseT::m_shmimName;
    }

    /// Learning flag.
    bool &learning()
    {
        return is_learning;
    }

    /// Predictive control flag.
    bool &predicting()
    {
        return is_predictive_control;
    }

    /// Reset-model request flag.
    bool &resetModel()
    {
        return do_reset_model;
    }

    /// Switch-exploration flag.
    bool &switchExploration()
    {
        return switch_exploration;
    }

    /// Use exploration set 01 flag.
    bool &useSet01()
    {
        return use_set_01;
    }

    /// The current exploration sequence string.
    std::string explorationSequence()
    {
        return m_exploration_sequence;
    }

    /// Exploration steps, set 01.
    std::vector<int> &steps01()
    {
        return m_exploration_steps_01;
    }

    /// Exploration steps, set 02.
    std::vector<int> &steps02()
    {
        return m_exploration_steps_02;
    }

    /// Exploration noise strengths, set 01.
    std::vector<float> &noise01()
    {
        return m_exploration_noise_strength_01;
    }

    /// Exploration noise strengths, set 02.
    std::vector<float> &noise02()
    {
        return m_exploration_noise_strength_02;
    }

    /// Regularization steps, set 01.
    std::vector<float> &reg01()
    {
        return m_regularization_steps_01;
    }

    /// Regularization steps, set 02.
    std::vector<float> &reg02()
    {
        return m_regularization_steps_02;
    }
};
/// \endcond

/// Make a 1x1 matrix holding \p v.
static DDSPC::Matrix scalarMat( float v /**< [in] the value */ )
{
    DDSPC::Matrix m( 1, 1 );
    m( 0, 0 ) = v;
    return m;
}

/// Build a switch property with one element.
static pcf::IndiProperty switchProp( const std::string &device /**< [in] device name */,
                                     const std::string &name /**< [in] property name */,
                                     const std::string &el /**< [in] element name */,
                                     bool               on /**< [in] switch state */ )
{
    pcf::IndiProperty ip( pcf::IndiProperty::Switch );
    ip.setDevice( device );
    ip.setName( name );
    ip.add( pcf::IndiElement( el, on ? pcf::IndiElement::On : pcf::IndiElement::Off ) );
    return ip;
}

/// Compute the expected controller row from a prediction matrix, following PredictiveController::update_controller().
static Eigen::MatrixXd expectedController( const Eigen::MatrixXd &P /**< [in] prediction matrix */,
                                           int                    nc /**< [in] number of correlations (future*modes) */,
                                           int                    nModes /**< [in] number of modes */,
                                           double                 reg /**< [in] regularization */ )
{
    Eigen::MatrixXd H   = P.transpose() * P;
    Eigen::MatrixXd H11 = H.block( 0, 0, nc, nc );
    Eigen::MatrixXd H21 = H.block( 0, nc, nc, H.cols() - nc );
    Eigen::MatrixXd R   = Eigen::MatrixXd::Identity( nc, nc ) * reg;

    Eigen::MatrixXd full = -1.0 * ( H11 + H11.maxCoeff() * R ).inverse() * H21;
    return full.block( full.rows() - nModes, 0, nModes, full.cols() );
}

/// Verify find_next_power_of_2() returns the next power of 2 strictly greater than its argument.
/**
 * \ingroup loPredCtrl_unit_test
 */
TEST_CASE( "loPredCtrl DDSPC find_next_power_of_2", "[loPredCtrl]" )
{
    // clang-format off
    #ifdef LOPREDCTRL_TEST_DOXYGEN_REF
    DDSPC::find_next_power_of_2(int);
    #endif
    // clang-format on

    CHECK( DDSPC::find_next_power_of_2( 0 ) == 2 );
    CHECK( DDSPC::find_next_power_of_2( 1 ) == 2 );
    CHECK( DDSPC::find_next_power_of_2( 2 ) == 4 );
    CHECK( DDSPC::find_next_power_of_2( 3 ) == 4 );
    CHECK( DDSPC::find_next_power_of_2( 7 ) == 8 );
    CHECK( DDSPC::find_next_power_of_2( 8 ) == 16 );
    CHECK( DDSPC::find_next_power_of_2( 10 ) == 16 );
    CHECK( DDSPC::find_next_power_of_2( 16 ) == 32 );
}

/// Verify save_matrix()/load_matrix() round trip for shapes that are not affected by storage order.
/**
 * \ingroup loPredCtrl_unit_test
 */
TEST_CASE( "loPredCtrl DDSPC save_matrix and load_matrix", "[loPredCtrl]" )
{
    // clang-format off
    #ifdef LOPREDCTRL_TEST_DOXYGEN_REF
    DDSPC::save_matrix(std::string, DDSPC::Matrix);
    DDSPC::load_matrix(std::string);
    #endif
    // clang-format on

    SECTION( "column vector" )
    {
        DDSPC::Matrix v( 4, 1 );
        v << 1.5f, -2.25f, 3.0f, 0.125f;

        DDSPC::save_matrix( "/tmp/loPredCtrl_test_vec.csv", v );
        DDSPC::Matrix l = DDSPC::load_matrix( "/tmp/loPredCtrl_test_vec.csv" );

        REQUIRE( l.rows() == 4 );
        REQUIRE( l.cols() == 1 );
        for( int i = 0; i < 4; ++i )
        {
            CHECK( l( i, 0 ) == Approx( v( i, 0 ) ) );
        }

        std::remove( "/tmp/loPredCtrl_test_vec.csv" );
    }

    SECTION( "symmetric matrix" )
    {
        DDSPC::Matrix s( 3, 3 );
        s << 1, 2, 3, 2, 4, 5, 3, 5, 6;

        DDSPC::save_matrix( "/tmp/loPredCtrl_test_sym.csv", s );
        DDSPC::Matrix l = DDSPC::load_matrix( "/tmp/loPredCtrl_test_sym.csv" );

        REQUIRE( l.rows() == 3 );
        REQUIRE( l.cols() == 3 );
        CHECK( ( l - s ).norm() == Approx( 0 ).margin( 1e-6 ) );

        std::remove( "/tmp/loPredCtrl_test_sym.csv" );
    }
}

/// Verify the RecursiveLeastSquares constructor sizes and initial state.
/**
 * \ingroup loPredCtrl_unit_test
 */
TEST_CASE( "loPredCtrl RLS construction and reset", "[loPredCtrl]" )
{
    // clang-format off
    #ifdef LOPREDCTRL_TEST_DOXYGEN_REF
    DDSPC::RecursiveLeastSquares::RecursiveLeastSquares(int, int, realT, realT);
    DDSPC::RecursiveLeastSquares::reset();
    #endif
    // clang-format on

    DDSPC::RecursiveLeastSquares rls( 2, 3, 0.5, 10 );

    CHECK( rls._gamma == Approx( 0.5 ) );
    CHECK( rls._inverse_gamma == Approx( 2.0 ) );
    CHECK( rls._initial_covariance == Approx( 10 ) );
    CHECK( rls._num_predictors == 2 );
    CHECK( rls._num_features == 3 );

    CHECK( rls.prediction_matrix.rows() == 2 );
    CHECK( rls.prediction_matrix.cols() == 3 );
    CHECK( rls.prediction_matrix.norm() == 0 );
    CHECK( rls.prediction_output.rows() == 2 );
    CHECK( rls.prediction_output.cols() == 1 );
    CHECK( rls.err.rows() == 2 );
    CHECK( rls.K.rows() == 1 );
    CHECK( rls.K.cols() == 3 );

    REQUIRE( rls.inverse_covariance.rows() == 3 );
    REQUIRE( rls.inverse_covariance.cols() == 3 );
    for( int r = 0; r < 3; ++r )
    {
        for( int c = 0; c < 3; ++c )
        {
            CHECK( rls.inverse_covariance( r, c ) == Approx( r == c ? 10 : 0 ) );
        }
    }

    // perturb and reset
    DDSPC::Matrix x( 3, 1 );
    x << 1, 2, 3;
    DDSPC::Matrix y( 2, 1 );
    y << 1, -1;
    rls.update( &x, &y );
    REQUIRE( rls.prediction_matrix.norm() > 0 );

    rls.reset();
    CHECK( rls.prediction_matrix.norm() == 0 );
    CHECK( rls.K.norm() == 0 );
    CHECK( rls.err.norm() == 0 );
    for( int r = 0; r < 3; ++r )
    {
        for( int c = 0; c < 3; ++c )
        {
            CHECK( rls.inverse_covariance( r, c ) == Approx( r == c ? 10 : 0 ) );
        }
    }
}

/// Verify one RLS update against a hand calculation.
/**
 * \ingroup loPredCtrl_unit_test
 */
TEST_CASE( "loPredCtrl RLS single update", "[loPredCtrl]" )
{
    // clang-format off
    #ifdef LOPREDCTRL_TEST_DOXYGEN_REF
    DDSPC::RecursiveLeastSquares::update(Matrix *, Matrix *);
    #endif
    // clang-format on

    DDSPC::RecursiveLeastSquares rls( 1, 2, 1.0, 10 );

    DDSPC::Matrix x( 2, 1 );
    x << 1, 0;
    DDSPC::Matrix y( 1, 1 );
    y << 2;

    rls.update( &x, &y );

    // err = y, K = x^T C / (1 + x^T C x) = [10/11, 0], P = err K, C' = C - K^T x^T C
    CHECK( rls.err( 0, 0 ) == Approx( 2 ) );
    CHECK( rls.K( 0, 0 ) == Approx( 10.0 / 11.0 ) );
    CHECK( rls.K( 0, 1 ) == Approx( 0 ).margin( 1e-7 ) );
    CHECK( rls.prediction_matrix( 0, 0 ) == Approx( 20.0 / 11.0 ) );
    CHECK( rls.prediction_matrix( 0, 1 ) == Approx( 0 ).margin( 1e-7 ) );
    CHECK( rls.inverse_covariance( 0, 0 ) == Approx( 10.0 / 11.0 ) );
    CHECK( rls.inverse_covariance( 1, 1 ) == Approx( 10 ) );
    CHECK( rls.inverse_covariance( 0, 1 ) == Approx( 0 ).margin( 1e-7 ) );
    CHECK( rls.inverse_covariance( 1, 0 ) == Approx( 0 ).margin( 1e-7 ) );
}

/// Verify RLS converges to known linear-model coefficients, with both update overloads.
/**
 * \ingroup loPredCtrl_unit_test
 */
TEST_CASE( "loPredCtrl RLS converges to known coefficients", "[loPredCtrl]" )
{
    // clang-format off
    #ifdef LOPREDCTRL_TEST_DOXYGEN_REF
    DDSPC::RecursiveLeastSquares::update(Matrix *, Matrix *);
    DDSPC::RecursiveLeastSquares::update(eigenImage<realT> *, eigenImage<realT> *);
    #endif
    // clang-format on

    DDSPC::Matrix A( 2, 3 );
    A << 0.5, -1.0, 2.0, 1.5, 0.25, -0.75;

    DDSPC::RecursiveLeastSquares rlsM( 2, 3, 1.0, 100 );
    DDSPC::RecursiveLeastSquares rlsI( 2, 3, 1.0, 100 );

    std::mt19937                          gen( 42 );
    std::uniform_real_distribution<float> U( -1.0f, 1.0f );

    for( int n = 0; n < 300; ++n )
    {
        DDSPC::Matrix x( 3, 1 );
        x << U( gen ), U( gen ), U( gen );
        DDSPC::Matrix y = A * x;

        rlsM.update( &x, &y );

        mx::improc::eigenImage<DDSPC::realT> xi = x.array();
        mx::improc::eigenImage<DDSPC::realT> yi = y.array();
        rlsI.update( &xi, &yi );
    }

    for( int r = 0; r < 2; ++r )
    {
        for( int c = 0; c < 3; ++c )
        {
            CHECK( rlsM.prediction_matrix( r, c ) == Approx( A( r, c ) ).margin( 1e-2 ) );
            CHECK( rlsI.prediction_matrix( r, c ) == Approx( rlsM.prediction_matrix( r, c ) ).margin( 1e-5 ) );
        }
    }

    // a-priori error on the last sample is small
    CHECK( rlsM.err.norm() < 1e-2 );
}

/// Verify the RLS forgetting factor lets the model track a change in the coefficients.
/**
 * \ingroup loPredCtrl_unit_test
 */
TEST_CASE( "loPredCtrl RLS forgetting factor tracks a model change", "[loPredCtrl]" )
{
    // clang-format off
    #ifdef LOPREDCTRL_TEST_DOXYGEN_REF
    DDSPC::RecursiveLeastSquares::update(Matrix *, Matrix *);
    #endif
    // clang-format on

    DDSPC::Matrix A1( 1, 2 );
    A1 << 1.0, -1.0;
    DDSPC::Matrix A2( 1, 2 );
    A2 << -0.5, 2.0;

    DDSPC::RecursiveLeastSquares forget( 1, 2, 0.9, 100 );
    DDSPC::RecursiveLeastSquares remember( 1, 2, 1.0, 100 );

    std::mt19937                          gen( 7 );
    std::uniform_real_distribution<float> U( -1.0f, 1.0f );

    for( int n = 0; n < 400; ++n )
    {
        DDSPC::Matrix x( 2, 1 );
        x << U( gen ), U( gen );
        DDSPC::Matrix y = ( n < 200 ? A1 : A2 ) * x;

        forget.update( &x, &y );
        remember.update( &x, &y );
    }

    CHECK( forget.prediction_matrix( 0, 0 ) == Approx( A2( 0, 0 ) ).margin( 2e-2 ) );
    CHECK( forget.prediction_matrix( 0, 1 ) == Approx( A2( 0, 1 ) ).margin( 2e-2 ) );

    // without forgetting the estimate is a blend of the two models
    CHECK( ( remember.prediction_matrix - A2 ).norm() > 0.1 );
}

/// Verify the PredictiveController initial state and integrator-only command.
/**
 * \ingroup loPredCtrl_unit_test
 */
TEST_CASE( "loPredCtrl PredictiveController integrator command", "[loPredCtrl]" )
{
    // clang-format off
    #ifdef LOPREDCTRL_TEST_DOXYGEN_REF
    DDSPC::PredictiveController::PredictiveController(int, int, int, realT, realT, realT, realT);
    DDSPC::PredictiveController::get_prediction_matrix();
    DDSPC::PredictiveController::calculate_command(Matrix, Matrix);
    #endif
    // clang-format on

    DDSPC::PredictiveController pc( 1, 3, 2, 0.25, 1.0, 0.015, 100 );

    DDSPC::Matrix P = pc.get_prediction_matrix();
    CHECK( P.rows() == 2 );
    CHECK( P.cols() == 7 );
    CHECK( P.norm() == 0 );

    // controller is zero, so the command is -gain * newest measurement + noise
    DDSPC::Matrix c1 = pc.calculate_command( scalarMat( 2.0 ), scalarMat( 0.0 ) );
    REQUIRE( c1.rows() == 1 );
    REQUIRE( c1.cols() == 1 );
    CHECK( c1( 0, 0 ) == Approx( -0.5 ) );

    DDSPC::Matrix c2 = pc.calculate_command( scalarMat( 4.0 ), scalarMat( 0.1 ) );
    CHECK( c2( 0, 0 ) == Approx( -0.9 ) );
}

/// Verify the PredictiveController measurement and command buffer accessors.
/**
 * \ingroup loPredCtrl_unit_test
 */
TEST_CASE( "loPredCtrl PredictiveController buffer accessors", "[loPredCtrl]" )
{
    // clang-format off
    #ifdef LOPREDCTRL_TEST_DOXYGEN_REF
    DDSPC::PredictiveController::get_measurement_future();
    DDSPC::PredictiveController::get_measurement_past();
    DDSPC::PredictiveController::get_command_future(int);
    DDSPC::PredictiveController::get_command_past();
    DDSPC::PredictiveController::get_current_measurement_past(int);
    DDSPC::PredictiveController::get_current_command_past(int);
    #endif
    // clang-format on

    // history 3, future 2, gain 0.5: command k = -0.5 * m_k
    DDSPC::PredictiveController pc( 1, 3, 2, 0.5, 1.0, 0.015, 100 );

    for( int k = 1; k <= 6; ++k )
    {
        pc.calculate_command( scalarMat( k ), scalarMat( 0 ) );
    }

    DDSPC::Matrix cmp = pc.get_current_measurement_past( 3 );
    REQUIRE( cmp.rows() == 3 );
    CHECK( cmp( 0, 0 ) == Approx( 6 ) );
    CHECK( cmp( 1, 0 ) == Approx( 5 ) );
    CHECK( cmp( 2, 0 ) == Approx( 4 ) );

    DDSPC::Matrix ccp = pc.get_current_command_past( 2 );
    REQUIRE( ccp.rows() == 2 );
    CHECK( ccp( 0, 0 ) == Approx( -3.0 ) );
    CHECK( ccp( 1, 0 ) == Approx( -2.5 ) );

    DDSPC::Matrix mf = pc.get_measurement_future();
    REQUIRE( mf.rows() == 2 );
    CHECK( mf( 0, 0 ) == Approx( 6 ) );
    CHECK( mf( 1, 0 ) == Approx( 5 ) );

    DDSPC::Matrix mp = pc.get_measurement_past();
    REQUIRE( mp.rows() == 3 );
    CHECK( mp( 0, 0 ) == Approx( 4 ) );
    CHECK( mp( 1, 0 ) == Approx( 3 ) );
    CHECK( mp( 2, 0 ) == Approx( 2 ) );

    DDSPC::Matrix cf0 = pc.get_command_future( 0 );
    REQUIRE( cf0.rows() == 2 );
    CHECK( cf0( 0, 0 ) == Approx( -3.0 ) );
    CHECK( cf0( 1, 0 ) == Approx( -2.5 ) );

    DDSPC::Matrix cf1 = pc.get_command_future( 1 );
    REQUIRE( cf1.rows() == 1 );
    CHECK( cf1( 0, 0 ) == Approx( -2.5 ) );

    DDSPC::Matrix cpast = pc.get_command_past();
    REQUIRE( cpast.rows() == 3 );
    CHECK( cpast( 0, 0 ) == Approx( -2.0 ) );
    CHECK( cpast( 1, 0 ) == Approx( -1.5 ) );
    CHECK( cpast( 2, 0 ) == Approx( -1.0 ) );
}

/// Verify the PredictiveController learns a known first-order plant and builds the matching controller.
/**
 * \ingroup loPredCtrl_unit_test
 */
TEST_CASE( "loPredCtrl PredictiveController learns a known plant", "[loPredCtrl]" )
{
    // clang-format off
    #ifdef LOPREDCTRL_TEST_DOXYGEN_REF
    DDSPC::PredictiveController::update_system();
    DDSPC::PredictiveController::update_controller();
    DDSPC::PredictiveController::set_regularization(realT);
    DDSPC::PredictiveController::reset();
    DDSPC::PredictiveController::calculate_command(Matrix, Matrix);
    #endif
    // clang-format on

    // plant: m_k = a m_{k-1} + b c_{k-1}; history 2, future 2, 1 mode
    const double a    = 0.8;
    const double b    = 0.5;
    const float  gain = 0.25;
    const float  reg  = 0.015;

    DDSPC::PredictiveController pc( 1, 2, 2, gain, 1.0, reg, 1000 );

    std::mt19937                          gen( 1234 );
    std::uniform_real_distribution<float> U( -1.0f, 1.0f );

    double m = 0;
    double c = 0;
    for( int k = 0; k < 1500; ++k )
    {
        m                 = a * m + b * c;
        DDSPC::Matrix cmd = pc.calculate_command( scalarMat( m ), scalarMat( U( gen ) ) );
        c                 = cmd( 0, 0 );
        pc.update_system();
    }

    // features: [c_{k-1}, c_{k-2}, c_{k-3}, m_{k-2}, m_{k-3}], predictions: [m_k, m_{k-1}]
    Eigen::MatrixXd Ptrue( 2, 5 );
    Ptrue << b, a * b, 0, a * a, 0, 0, b, 0, a, 0;

    DDSPC::Matrix P = pc.get_prediction_matrix();
    REQUIRE( P.rows() == 2 );
    REQUIRE( P.cols() == 5 );
    for( int r = 0; r < 2; ++r )
    {
        for( int cc = 0; cc < 5; ++cc )
        {
            CHECK( P( r, cc ) == Approx( Ptrue( r, cc ) ).margin( 2e-2 ) );
        }
    }

    Eigen::MatrixXd Pd = P.cast<double>();

    SECTION( "update_controller builds the regularized controller" )
    {
        pc.update_controller();

        Eigen::MatrixXd K     = expectedController( Pd, 2, 1, reg );
        Eigen::MatrixXd Ktrue = expectedController( Ptrue, 2, 1, reg );
        REQUIRE( K.rows() == 1 );
        REQUIRE( K.cols() == 3 );
        for( int n = 0; n < 3; ++n )
        {
            CHECK( K( 0, n ) == Approx( Ktrue( 0, n ) ).margin( 5e-2 ) );
        }

        double cprev = pc.get_current_command_past( 1 )( 0, 0 );
        double mprev = pc.get_current_measurement_past( 1 )( 0, 0 );
        double mnew  = a * m + b * c;

        DDSPC::Matrix cmd = pc.calculate_command( scalarMat( mnew ), scalarMat( 0 ) );

        // past vector is [c_latest, m_new, m_prev]; integrator acts on m_new
        double expect = K( 0, 0 ) * cprev + ( K( 0, 1 ) - gain ) * mnew + K( 0, 2 ) * mprev;
        CHECK( cmd( 0, 0 ) == Approx( expect ).margin( 1e-3 ) );
    }

    SECTION( "set_regularization switches the regularization used by update_controller" )
    {
        pc.set_regularization( 1.0 );
        pc.update_controller();

        Eigen::MatrixXd K = expectedController( Pd, 2, 1, 1.0 );

        double cprev = pc.get_current_command_past( 1 )( 0, 0 );
        double mprev = pc.get_current_measurement_past( 1 )( 0, 0 );
        double mnew  = a * m + b * c;

        DDSPC::Matrix cmd = pc.calculate_command( scalarMat( mnew ), scalarMat( 0 ) );

        double expect = K( 0, 0 ) * cprev + ( K( 0, 1 ) - gain ) * mnew + K( 0, 2 ) * mprev;
        CHECK( cmd( 0, 0 ) == Approx( expect ).margin( 1e-3 ) );

        // and the controller differs from the one with the original regularization
        Eigen::MatrixXd K0 = expectedController( Pd, 2, 1, reg );
        CHECK( ( K - K0 ).norm() > 1e-2 );
    }

    SECTION( "reset clears the model and the controller" )
    {
        pc.update_controller();
        pc.reset();

        CHECK( pc.get_prediction_matrix().norm() == 0 );

        DDSPC::Matrix cmd = pc.calculate_command( scalarMat( 2.0 ), scalarMat( 0 ) );
        CHECK( cmd( 0, 0 ) == Approx( -0.5 ) );
    }
}

/// Verify default configuration values.
/**
 * \ingroup loPredCtrl_unit_test
 */
TEST_CASE( "loPredCtrl configuration defaults", "[loPredCtrl]" )
{
    // clang-format off
    #ifdef LOPREDCTRL_TEST_DOXYGEN_REF
    loPredCtrl::loPredCtrl();
    loPredCtrl::setupConfig();
    loPredCtrl::loadConfig();
    loPredCtrl::loadConfigImpl(mx::app::appConfigurator &);
    #endif
    // clang-format on

    loPredCtrl_test app( "lopredctrl" );
    app.setupConfigForTest();

    mx::app::writeConfigFile( "/tmp/loPredCtrl_test_defaults.conf", { "none" }, { "nada" }, { "0" } );
    app.loadConfigFromFile( "/tmp/loPredCtrl_test_defaults.conf" );

    CHECK( app.gainCtrl() == 0 );
    CHECK( app.regularizationCtrl() == 1 );
    CHECK( app.gammaCtrl() == 1 );
    CHECK( app.covarianceCtrl() == Approx( 100000.0 ) );
    CHECK( app.numModes() == 1 );
    CHECK( app.history() == 5 );
    CHECK( app.future() == 3 );
    CHECK( app.outputName() == "" );
    CHECK( app.shmimName() == "lopredctrl" );
    CHECK( app.ctrl() == nullptr );

    std::remove( "/tmp/loPredCtrl_test_defaults.conf" );
}

/// Verify configuration overrides.
/**
 * \ingroup loPredCtrl_unit_test
 */
TEST_CASE( "loPredCtrl configuration overrides", "[loPredCtrl]" )
{
    // clang-format off
    #ifdef LOPREDCTRL_TEST_DOXYGEN_REF
    loPredCtrl::setupConfig();
    loPredCtrl::loadConfigImpl(mx::app::appConfigurator &);
    #endif
    // clang-format on

    loPredCtrl_test app( "lopredctrl" );
    app.setupConfigForTest();

    mx::app::writeConfigFile(
        "/tmp/loPredCtrl_test_override.conf",
        { "parameters",
          "parameters",
          "parameters",
          "parameters",
          "parameters",
          "parameters",
          "parameters",
          "outputShmim",
          "shmimMonitor" },
        { "gain", "regularization", "gamma", "covariance", "num_modes", "history", "future", "shmimName", "shmimName" },
        { "0.3", "0.02", "0.995", "1000", "4", "7", "2", "dm_lo_pc", "aol2_modevalWFS" } );

    app.loadConfigFromFile( "/tmp/loPredCtrl_test_override.conf" );

    CHECK( app.gainCtrl() == Approx( 0.3 ) );
    CHECK( app.regularizationCtrl() == Approx( 0.02 ) );
    CHECK( app.gammaCtrl() == Approx( 0.995 ) );
    CHECK( app.covarianceCtrl() == Approx( 1000 ) );
    CHECK( app.numModes() == 4 );
    CHECK( app.history() == 7 );
    CHECK( app.future() == 2 );
    CHECK( app.outputName() == "dm_lo_pc" );
    CHECK( app.shmimName() == "aol2_modevalWFS" );

    std::remove( "/tmp/loPredCtrl_test_override.conf" );
}

/// Verify allocate() sizes the buffers and creates the controller.
/**
 * \ingroup loPredCtrl_unit_test
 */
TEST_CASE( "loPredCtrl allocate", "[loPredCtrl]" )
{
    // clang-format off
    #ifdef LOPREDCTRL_TEST_DOXYGEN_REF
    loPredCtrl::allocate(const dev::shmimT &);
    loPredCtrl::send_to_shmim();
    #endif
    // clang-format on

    loPredCtrl_test app( "lopredctrl" );
    app.setParams( 1, 2, 2, 0.5 );

    REQUIRE( app.allocateForTest( 4, 1 ) == 0 );

    CHECK( app.modevalWidth() == 4 );
    CHECK( app.modevalHeight() == 1 );
    CHECK( app.modevalTypeSize() == sizeof( float ) );
    CHECK( app.fullCommand().rows() == 4 );
    CHECK( app.fullCommand().cols() == 1 );
    CHECK( app.newCommand().rows() == 1 );
    CHECK( app.newMeasurement().rows() == 1 );

    REQUIRE( app.ctrl() != nullptr );
    DDSPC::Matrix P = app.ctrl()->get_prediction_matrix();
    CHECK( P.rows() == 2 );
    CHECK( P.cols() == 5 );

    CHECK( app.send_to_shmim() == 0 );
}

/// Verify processImage() computes the command for the controlled modes and passes the rest through.
/**
 * \ingroup loPredCtrl_unit_test
 */
TEST_CASE( "loPredCtrl processImage predictive command", "[loPredCtrl]" )
{
    // clang-format off
    #ifdef LOPREDCTRL_TEST_DOXYGEN_REF
    loPredCtrl::processImage(void *, const dev::shmimT &);
    #endif
    // clang-format on

    SECTION( "predictive control on, integrator only" )
    {
        loPredCtrl_test app( "lopredctrl" );
        app.setParams( 1, 2, 2, 0.5 );
        REQUIRE( app.allocateForTest( 3, 1 ) == 0 );
        app.predicting() = true;

        std::vector<float> im = { 2.0f, 7.0f, 9.0f };
        REQUIRE( app.processImageForTest( im ) == 0 );

        CHECK( app.newMeasurement()( 0, 0 ) == Approx( 2.0 ) );
        CHECK( app.fullCommand()( 0, 0 ) == Approx( -1.0 ) );
        CHECK( app.fullCommand()( 1, 0 ) == Approx( 7.0 ) );
        CHECK( app.fullCommand()( 2, 0 ) == Approx( 9.0 ) );
        CHECK( app.frameCounter() == 1 );

        std::vector<float> im2 = { -4.0f, 1.0f, 2.0f };
        REQUIRE( app.processImageForTest( im2 ) == 0 );
        CHECK( app.fullCommand()( 0, 0 ) == Approx( 2.0 ) );
        CHECK( app.fullCommand()( 1, 0 ) == Approx( 1.0 ) );
        CHECK( app.frameCounter() == 2 );
    }

    SECTION( "learning after warm-up updates the model" )
    {
        loPredCtrl_test app( "lopredctrl" );
        app.setParams( 1, 2, 2, 0.5 );
        REQUIRE( app.allocateForTest( 1, 1 ) == 0 );
        app.predicting() = true;

        // warm up the controller buffers so the first learning step has non-zero features
        std::vector<float> im = { 0.0f };
        for( int k = 0; k < 8; ++k )
        {
            im[0] = std::sin( 0.7 * k ) + 0.3f * ( k % 3 );
            REQUIRE( app.processImageForTest( im ) == 0 );
        }
        REQUIRE( app.ctrl()->get_prediction_matrix().norm() == 0 );

        app.learning() = true;
        for( int k = 8; k < 16; ++k )
        {
            im[0] = std::sin( 0.7 * k ) + 0.3f * ( k % 3 );
            REQUIRE( app.processImageForTest( im ) == 0 );
        }

        CHECK( app.ctrl()->get_prediction_matrix().norm() > 0 );
        CHECK( std::fabs( app.fullCommand()( 0, 0 ) ) < 1e6 );
        CHECK( app.frameCounter() == 16 );
    }

    SECTION( "reset request resets the model on the next frame" )
    {
        loPredCtrl_test app( "lopredctrl" );
        app.setParams( 1, 2, 2, 0.5 );
        REQUIRE( app.allocateForTest( 1, 1 ) == 0 );
        app.predicting() = true;

        std::vector<float> im = { 0.0f };
        for( int k = 0; k < 8; ++k )
        {
            im[0] = std::sin( 0.7 * k ) + 0.3f * ( k % 3 );
            app.processImageForTest( im );
        }
        app.learning() = true;
        for( int k = 8; k < 12; ++k )
        {
            im[0] = std::sin( 0.7 * k ) + 0.3f * ( k % 3 );
            app.processImageForTest( im );
        }
        REQUIRE( app.ctrl()->get_prediction_matrix().norm() > 0 );

        app.learning()   = false;
        app.resetModel() = true;
        im[0]            = 1.0f;
        REQUIRE( app.processImageForTest( im ) == 0 );

        CHECK( app.resetModel() == false );
        CHECK( app.ctrl()->get_prediction_matrix().norm() == 0 );
        // controller cleared, so the command is the integrator only
        CHECK( app.fullCommand()( 0, 0 ) == Approx( -0.5 ) );
    }
}

/// Verify the exploration sequence callback parses the sequence and processImage() consumes it.
/**
 * \ingroup loPredCtrl_unit_test
 */
TEST_CASE( "loPredCtrl exploration sequence", "[loPredCtrl]" )
{
    // clang-format off
    #ifdef LOPREDCTRL_TEST_DOXYGEN_REF
    loPredCtrl::newCallBack_m_indiP_exploration(const pcf::IndiProperty &);
    loPredCtrl::processImage(void *, const dev::shmimT &);
    #endif
    // clang-format on

    SECTION( "wrong device or name is rejected" )
    {
        loPredCtrl_test   app( "lopredctrl" );
        pcf::IndiProperty ip( pcf::IndiProperty::Text );
        ip.setDevice( "wrong" );
        ip.setName( "exploration_sequence" );
        ip.add( pcf::IndiElement( "target", std::string( "10,0.5,0.1" ) ) );
        CHECK( app.newCallBack_m_indiP_exploration( ip ) == -1 );

        ip.setDevice( "lopredctrl" );
        ip.setName( "wrong" );
        CHECK( app.newCallBack_m_indiP_exploration( ip ) == -1 );
        CHECK( app.switchExploration() == false );
    }

    SECTION( "missing target is an error" )
    {
        loPredCtrl_test   app( "lopredctrl" );
        pcf::IndiProperty ip( pcf::IndiProperty::Text );
        ip.setDevice( "lopredctrl" );
        ip.setName( "exploration_sequence" );
        CHECK( app.newCallBack_m_indiP_exploration( ip ) == -1 );
    }

    SECTION( "sequence is parsed into the inactive set and consumed" )
    {
        loPredCtrl_test app( "lopredctrl" );
        app.setParams( 1, 2, 2, 0.0 );
        REQUIRE( app.allocateForTest( 1, 1 ) == 0 );

        pcf::IndiProperty ip( pcf::IndiProperty::Text );
        ip.setDevice( "lopredctrl" );
        ip.setName( "exploration_sequence" );
        ip.add( pcf::IndiElement( "target", std::string( "3,0.5,0.1,2,0.25,0.05" ) ) );

        REQUIRE( app.useSet01() == true );
        REQUIRE( app.newCallBack_m_indiP_exploration( ip ) == 0 );

        CHECK( app.explorationSequence() == "3,0.5,0.1,2,0.25,0.05" );
        CHECK( app.switchExploration() == true );
        CHECK( app.steps01().empty() );
        std::vector<int> expSteps = { 3, 2 };
        REQUIRE( app.steps02() == expSteps );
        REQUIRE( app.noise02().size() == 2 );
        CHECK( app.noise02()[0] == Approx( 0.5 ) );
        CHECK( app.noise02()[1] == Approx( 0.25 ) );
        REQUIRE( app.reg02().size() == 2 );
        CHECK( app.reg02()[0] == Approx( 0.1 ) );
        CHECK( app.reg02()[1] == Approx( 0.05 ) );

        // first frame switches to set 02 and uses one step
        app.predicting()      = true;
        std::vector<float> im = { 0.0f };
        REQUIRE( app.processImageForTest( im ) == 0 );
        CHECK( app.useSet01() == false );
        CHECK( app.switchExploration() == false );
        CHECK( app.steps02()[0] == 2 );

        // gain is zero and the measurement is zero, so the command is the exploration noise
        CHECK( app.fullCommand()( 0, 0 ) != 0 );

        // two more frames finish the first block
        REQUIRE( app.processImageForTest( im ) == 0 );
        REQUIRE( app.processImageForTest( im ) == 0 );
        REQUIRE( app.steps02().size() == 1 );
        CHECK( app.steps02()[0] == 2 );
        REQUIRE( app.noise02().size() == 1 );
        CHECK( app.noise02()[0] == Approx( 0.25 ) );
        REQUIRE( app.reg02().size() == 1 );
        CHECK( app.reg02()[0] == Approx( 0.05 ) );

        // two more frames finish the sequence
        REQUIRE( app.processImageForTest( im ) == 0 );
        REQUIRE( app.processImageForTest( im ) == 0 );
        CHECK( app.steps02().empty() );
        CHECK( app.noise02().empty() );
        CHECK( app.reg02().empty() );

        // with no exploration left the command is zero
        REQUIRE( app.processImageForTest( im ) == 0 );
        CHECK( app.fullCommand()( 0, 0 ) == Approx( 0 ).margin( 1e-7 ) );
    }
}

/// Verify the learning and predicting toggle callbacks.
/**
 * \ingroup loPredCtrl_unit_test
 */
TEST_CASE( "loPredCtrl learning and predicting toggles", "[loPredCtrl]" )
{
    // clang-format off
    #ifdef LOPREDCTRL_TEST_DOXYGEN_REF
    loPredCtrl::newCallBack_m_indiP_learningToggle(const pcf::IndiProperty &);
    loPredCtrl::newCallBack_m_indiP_predictingToggle(const pcf::IndiProperty &);
    #endif
    // clang-format on

    SECTION( "wrong name is rejected" )
    {
        loPredCtrl_test app( "lopredctrl" );
        CHECK( app.newCallBack_m_indiP_learningToggle( switchProp( "lopredctrl", "wrong", "toggle", true ) ) == -1 );
        CHECK( app.newCallBack_m_indiP_predictingToggle( switchProp( "lopredctrl", "wrong", "toggle", true ) ) == -1 );
        CHECK( app.learning() == false );
        CHECK( app.predicting() == false );
    }

    SECTION( "learning toggles on and off" )
    {
        loPredCtrl_test app( "lopredctrl" );
        REQUIRE( app.newCallBack_m_indiP_learningToggle( switchProp( "lopredctrl", "learn", "toggle", true ) ) == 0 );
        CHECK( app.learning() == true );
        REQUIRE( app.newCallBack_m_indiP_learningToggle( switchProp( "lopredctrl", "learn", "toggle", false ) ) == 0 );
        CHECK( app.learning() == false );
    }

    SECTION( "predicting toggles on and off" )
    {
        loPredCtrl_test app( "lopredctrl" );
        REQUIRE( app.newCallBack_m_indiP_predictingToggle( switchProp( "lopredctrl", "predict", "toggle", true ) ) ==
                 0 );
        CHECK( app.predicting() == true );
        REQUIRE( app.newCallBack_m_indiP_predictingToggle( switchProp( "lopredctrl", "predict", "toggle", false ) ) ==
                 0 );
        CHECK( app.predicting() == false );
    }
}

/// Verify the reset-model request callback.
/**
 * \ingroup loPredCtrl_unit_test
 */
TEST_CASE( "loPredCtrl reset model request", "[loPredCtrl]" )
{
    // clang-format off
    #ifdef LOPREDCTRL_TEST_DOXYGEN_REF
    loPredCtrl::newCallBack_m_indiP_resetToggle(const pcf::IndiProperty &);
    #endif
    // clang-format on

    SECTION( "wrong name is rejected" )
    {
        loPredCtrl_test app( "lopredctrl" );
        CHECK( app.newCallBack_m_indiP_resetToggle( switchProp( "lopredctrl", "wrong", "request", true ) ) == -1 );
        CHECK( app.resetModel() == false );
    }

    SECTION( "missing request element is ignored" )
    {
        loPredCtrl_test app( "lopredctrl" );
        CHECK( app.newCallBack_m_indiP_resetToggle( switchProp( "lopredctrl", "reset_model", "other", true ) ) == 0 );
        CHECK( app.resetModel() == false );
    }

    SECTION( "request off does nothing" )
    {
        loPredCtrl_test app( "lopredctrl" );
        CHECK( app.newCallBack_m_indiP_resetToggle( switchProp( "lopredctrl", "reset_model", "request", false ) ) ==
               0 );
        CHECK( app.resetModel() == false );
    }

    SECTION( "request on flags a reset" )
    {
        loPredCtrl_test app( "lopredctrl" );
        CHECK( app.newCallBack_m_indiP_resetToggle( switchProp( "lopredctrl", "reset_model", "request", true ) ) == 0 );
        CHECK( app.resetModel() == true );
    }
}

} // namespace loPredCtrlTest

} // namespace libXWCTest
