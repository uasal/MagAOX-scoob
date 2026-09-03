/** \file llowfscSimCpp.hpp
  * \brief MagAO-X C++ host for the Python esc_llowfsc_sim optical model.
  *
  * INDI, ImageStreamIO, and loop cadence live here. Fraunhofer snaps stay in
  * Python (optical_bridge.py → M.snap_camsci / M.snap_camlo).
  */

#ifndef llowfscSimCpp_hpp
#define llowfscSimCpp_hpp

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include <ImageStreamIO/ImageStreamIO.h>
#include <mx/sys/timeUtils.hpp>

#include "../../libMagAOX/libMagAOX.hpp"
#include "../../magaox_git_version.h"

#include "optical_model_embed.hpp"

namespace MagAOX
{
namespace app
{

namespace
{

bool parseIndiCurrentNumber( const pcf::IndiProperty &ip, double &out )
{
    try
    {
        if( !ip.find( "current" ) )
            return false;
        const std::string s = ip["current"].getValue();
        if( s.empty() )
            return false;
        char *end = nullptr;
        const double v = std::strtod( s.c_str(), &end );
        if( end == s.c_str() )
            return false;
        while( *end != '\0' && std::isspace( static_cast<unsigned char>( *end ) ) )
            ++end;
        if( *end != '\0' )
            return false;
        if( !std::isfinite( v ) )
            return false;
        out = v;
        return true;
    }
    catch( ... )
    {
        return false;
    }
}

} // namespace

class llowfscSimCpp : public MagAOXApp<true>
{
  protected:
    std::string m_camName{ "nsvsim" };
    std::string m_fsmName{ "fsm_sim" };
    std::string m_shmOutput{ "camsci_sim" };
    std::string m_shmCamloOutput; ///< empty = do not publish camlo
    std::string m_shmDmTotal{ "dm01disp" };
    std::string m_shmDmFlat{ "dm01disp00" };
    std::string m_shmOpd{ "opdsim" };
    std::string m_bridgeDir;
    std::string m_pythonPrefix;
    double m_dmScale{ 1e-6 };

    llowfscsim::ModelConfig m_modelCfg;

    std::atomic<bool> m_streaming{ false };
    std::atomic<bool> m_shutterClosed{ false };
    std::atomic<bool> m_useVortex{ true };
    double m_magnitude{ 0.0 }; ///< Vega mag; flux_scale_factor = 2.512**(-m)
    std::atomic<bool> m_modelReady{ false };
    std::atomic<bool> m_workerShutdown{ false };
    std::atomic<bool> m_snapFault{ false };

    std::mutex m_liveMutex;
    double m_camExp{ 0.01 };
    double m_camGain{ 120.0 };
    double m_camBlacklevel{ 10.0 };
    double m_camFps{ 200.0 };
    int m_camBitdepth{ 16 };
    int m_camRoiW{ 512 };
    int m_camRoiH{ 512 };
    double m_fsmX_nm{ 0.0 };
    double m_fsmY_nm{ 0.0 };

    int m_nact{ 0 };
    int m_ncamsci{ 512 };

    std::thread m_worker;

    pcf::IndiProperty m_indiP_streaming;
    INDI_NEWCALLBACK_DECL( llowfscSimCpp, m_indiP_streaming );
    pcf::IndiProperty m_indiP_shutter;
    INDI_NEWCALLBACK_DECL( llowfscSimCpp, m_indiP_shutter );
    pcf::IndiProperty m_indiP_usevortex;
    INDI_NEWCALLBACK_DECL( llowfscSimCpp, m_indiP_usevortex );
    pcf::IndiProperty m_indiP_magnitude;
    INDI_NEWCALLBACK_DECL( llowfscSimCpp, m_indiP_magnitude );
    pcf::IndiProperty m_indiP_camName;
    INDI_NEWCALLBACK_DECL( llowfscSimCpp, m_indiP_camName );
    pcf::IndiProperty m_indiP_fsmName;
    INDI_NEWCALLBACK_DECL( llowfscSimCpp, m_indiP_fsmName );
    pcf::IndiProperty m_indiP_shmOutput;
    INDI_NEWCALLBACK_DECL( llowfscSimCpp, m_indiP_shmOutput );

    pcf::IndiProperty m_indiP_exptime;
    pcf::IndiProperty m_indiP_emgain;
    pcf::IndiProperty m_indiP_blacklevel;
    pcf::IndiProperty m_indiP_fps;
    pcf::IndiProperty m_indiP_bitDepth;

    pcf::IndiProperty m_indiP_remoteExptime;
    INDI_SETCALLBACK_DECL( llowfscSimCpp, m_indiP_remoteExptime );
    pcf::IndiProperty m_indiP_remoteEmgain;
    INDI_SETCALLBACK_DECL( llowfscSimCpp, m_indiP_remoteEmgain );
    pcf::IndiProperty m_indiP_remoteBlacklevel;
    INDI_SETCALLBACK_DECL( llowfscSimCpp, m_indiP_remoteBlacklevel );
    pcf::IndiProperty m_indiP_remoteFps;
    INDI_SETCALLBACK_DECL( llowfscSimCpp, m_indiP_remoteFps );
    pcf::IndiProperty m_indiP_remoteBitDepth;
    INDI_SETCALLBACK_DECL( llowfscSimCpp, m_indiP_remoteBitDepth );
    pcf::IndiProperty m_indiP_remoteRoiW;
    INDI_SETCALLBACK_DECL( llowfscSimCpp, m_indiP_remoteRoiW );
    pcf::IndiProperty m_indiP_remoteRoiH;
    INDI_SETCALLBACK_DECL( llowfscSimCpp, m_indiP_remoteRoiH );
    pcf::IndiProperty m_indiP_remoteFsmVal1;
    INDI_SETCALLBACK_DECL( llowfscSimCpp, m_indiP_remoteFsmVal1 );
    pcf::IndiProperty m_indiP_remoteFsmVal2;
    INDI_SETCALLBACK_DECL( llowfscSimCpp, m_indiP_remoteFsmVal2 );

  public:
    llowfscSimCpp();
    ~llowfscSimCpp() noexcept;

    virtual void setupConfig();
    virtual void loadConfig();
    virtual int appStartup();
    virtual int appLogic();
    virtual int appShutdown();

  protected:
    static void workerStart( llowfscSimCpp *s );
    void workerExec();
    void copyLive( llowfscsim::SnapRequest &req, double &fps );
    bool grabShmim2D( const std::string &name, std::vector<double> &out, uint32_t &n0,
                      uint32_t &n1, bool magpyxTranspose );
    bool ensureShmimOpen( IMAGE &im, bool &open, const std::string &name );
    bool copyOpenShmim2D( IMAGE &im, std::vector<double> &out, uint32_t &n0, uint32_t &n1,
                          bool magpyxTranspose );
    bool grabOpen2D( IMAGE &im, bool &open, const std::string &name, std::vector<double> &out,
                     uint32_t &n0, uint32_t &n1, bool magpyxTranspose );
    bool grabShmimScalar( const std::string &name, double &out );
    int ensureOutput( IMAGE &im, bool &open, const std::string &name, uint32_t w, uint32_t h );
    int publish( IMAGE &im, const std::vector<float> &pix, uint32_t w, uint32_t h );
    void mirrorCamIndi();
    std::string pickBridgeDir() const;
    std::string pickPythonPrefix() const;
};

llowfscSimCpp::llowfscSimCpp() : MagAOXApp( MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED )
{
    m_loopPause = 100000000; // 100 ms — FSM/state; snaps run on the worker
}

llowfscSimCpp::~llowfscSimCpp() noexcept
{
}

void llowfscSimCpp::setupConfig()
{
    config.add( "sim.cam_name", "", "sim.cam_name", argType::Required, "sim", "cam_name", false,
                "string", "INDI camera device (exptime/emgain/fps/...)." );
    config.add( "sim.fsm_name", "", "sim.fsm_name", argType::Required, "sim", "fsm_name", false,
                "string", "INDI FSM device (val_1/val_2 current [nm]). Default fsm_sim." );
    config.add( "sim.shm_output", "", "sim.shm_output", argType::Required, "sim", "shm_output",
                false, "string", "Output ImageStreamIO for snap_camsci." );
    config.add( "sim.shm_camlo_output", "", "sim.shm_camlo_output", argType::Required, "sim",
                "shm_camlo_output", false, "string",
                "Optional output ImageStreamIO for snap_camlo (empty=off)." );
    config.add( "sim.shm_dm_total", "", "sim.shm_dm_total", argType::Required, "sim",
                "shm_dm_total", false, "string", "Cacao total DM shmim [µm]." );
    config.add( "sim.shm_dm_flat_channel", "", "sim.shm_dm_flat_channel", argType::Required, "sim",
                "shm_dm_flat_channel", false, "string", "DM channel 00 shmim [µm] → M.dm_flat." );
    config.add( "sim.magnitude", "", "sim.magnitude", argType::Required, "sim", "magnitude", false,
                "double", "Vega magnitude (flux_scale_factor = 2.512**(-mag); INDI magnitude)." );
    config.add( "sim.shm_vmag", "", "sim.shm_vmag", argType::Required, "sim", "shm_vmag", false,
                "string", "Deprecated unused. Use INDI magnitude instead." );
    config.add( "sim.shm_opdsim", "", "sim.shm_opdsim", argType::Required, "sim", "shm_opdsim",
                false, "string", "10-Zernike OPD shmim." );
    config.add( "sim.dm_scale", "", "sim.dm_scale", argType::Required, "sim", "dm_scale", false,
                "double", "Multiply milk DM commands by this to get meters." );
    config.add( "sim.bridge_dir", "", "sim.bridge_dir", argType::Required, "sim", "bridge_dir",
                false, "string", "Directory containing optical_bridge.py." );
    config.add( "sim.python_prefix", "", "sim.python_prefix", argType::Required, "sim",
                "python_prefix", false, "string",
                "Python prefix (MagAO-X base conda, default /opt/conda)." );
    config.add( "sim.model_pack", "", "sim.model_pack", argType::Required, "sim", "model_pack",
                false, "string", "esc_llowfsc_sim data pack name." );
    config.add( "sim.cuda_device", "", "sim.cuda_device", argType::Required, "sim", "cuda_device",
                false, "string", "CUDA_VISIBLE_DEVICES." );
    config.add( "sim.wavelength_c", "", "sim.wavelength_c", argType::Required, "sim",
                "wavelength_c", false, "double", "Central wavelength [m]." );
    config.add( "sim.ncamsci", "", "sim.ncamsci", argType::Required, "sim", "ncamsci", false,
                "int", "Optical-model science-camera array size." );
    config.add( "sim.nwaves", "", "sim.nwaves", argType::Required, "sim", "nwaves", false, "int",
                "Bandpass wavelength samples." );
    config.add( "sim.bw", "", "sim.bw", argType::Required, "sim", "bw", false, "double",
                "Fractional bandpass." );
    config.add( "sim.dark_current", "", "sim.dark_current", argType::Required, "sim",
                "dark_current", false, "double", "Detector dark current [e-/pix/s]." );
    config.add( "sim.nbits", "", "sim.nbits", argType::Required, "sim", "nbits", false, "int",
                "Internal ADC bits before rebin." );
    config.add( "sim.qe", "", "sim.qe", argType::Required, "sim", "qe", false, "double",
                "Quantum efficiency." );
    config.add( "sim.default_fps", "", "sim.default_fps", argType::Required, "sim", "default_fps",
                false, "double", "Fallback loop rate [Hz]." );
    config.add( "sim.default_exp_time", "", "sim.default_exp_time", argType::Required, "sim",
                "default_exp_time", false, "double", "Fallback exposure [s] before camera INDI." );
    config.add( "sim.default_gain", "", "sim.default_gain", argType::Required, "sim",
                "default_gain", false, "double", "Fallback emgain before camera INDI." );
    config.add( "sim.default_blacklevel", "", "sim.default_blacklevel", argType::Required, "sim",
                "default_blacklevel", false, "double", "Fallback blacklevel before camera INDI." );
    config.add( "sim.default_bit_depth", "", "sim.default_bit_depth", argType::Required, "sim",
                "default_bit_depth", false, "int", "Fallback rebin / output bit depth." );
    config.add( "sim.usevortex", "", "sim.usevortex", argType::Required, "sim", "usevortex", false,
                "bool", "Default vortex in." );
}

void llowfscSimCpp::loadConfig()
{
    config( m_camName, "sim.cam_name" );
    config( m_fsmName, "sim.fsm_name" );
    config( m_shmOutput, "sim.shm_output" );
    config( m_shmCamloOutput, "sim.shm_camlo_output" );
    config( m_shmDmTotal, "sim.shm_dm_total" );
    config( m_shmDmFlat, "sim.shm_dm_flat_channel" );
    config( m_magnitude, "sim.magnitude" );
    config( m_shmOpd, "sim.shm_opdsim" );
    config( m_dmScale, "sim.dm_scale" );
    config( m_bridgeDir, "sim.bridge_dir" );
    config( m_pythonPrefix, "sim.python_prefix" );
    config( m_modelCfg.model_pack, "sim.model_pack" );
    config( m_modelCfg.cuda_device, "sim.cuda_device" );
    config( m_modelCfg.wavelength_c, "sim.wavelength_c" );
    config( m_modelCfg.ncamsci, "sim.ncamsci" );
    config( m_modelCfg.nwaves, "sim.nwaves" );
    config( m_modelCfg.bw, "sim.bw" );
    config( m_modelCfg.dark_current, "sim.dark_current" );
    config( m_modelCfg.nbits, "sim.nbits" );
    config( m_modelCfg.qe, "sim.qe" );
    config( m_camFps, "sim.default_fps" );
    config( m_camExp, "sim.default_exp_time" );
    config( m_camGain, "sim.default_gain" );
    config( m_camBlacklevel, "sim.default_blacklevel" );
    config( m_camBitdepth, "sim.default_bit_depth" );
    m_modelCfg.default_exp_time = m_camExp;
    m_modelCfg.default_gain = m_camGain;
    m_modelCfg.default_blacklevel = m_camBlacklevel;
    m_modelCfg.default_bit_depth = m_camBitdepth;
    {
        bool uv = true;
        config( uv, "sim.usevortex" );
        m_useVortex = uv;
        m_modelCfg.usevortex = uv;
    }
    m_camRoiW = m_modelCfg.ncamsci;
    m_camRoiH = m_modelCfg.ncamsci;
}

int llowfscSimCpp::appStartup()
{
    CREATE_REG_INDI_NEW_TOGGLESWITCH( m_indiP_streaming, "streaming" );
    CREATE_REG_INDI_NEW_TOGGLESWITCH( m_indiP_shutter, "shutter" );
    if( createStandardIndiToggleSw( m_indiP_usevortex, "usevortex" ) < 0 )
        return log<software_error, -1>( { __FILE__, __LINE__, "createStandardIndiToggleSw usevortex" } );
    if( m_useVortex.load() )
        m_indiP_usevortex["toggle"].setSwitchState( pcf::IndiElement::On );
    if( registerIndiPropertyNew( m_indiP_usevortex, INDI_NEWCALLBACK( m_indiP_usevortex ) ) < 0 )
        return log<software_error, -1>( { __FILE__, __LINE__, "registerIndiPropertyNew usevortex" } );

    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_magnitude, "magnitude", -5, 30, 0.1, "%0.2f",
                                 "Vega magnitude (star brightness)", "sim" );
    m_indiP_magnitude["current"].setValue( m_magnitude );
    m_indiP_magnitude["target"].setValue( m_magnitude );

    CREATE_REG_INDI_NEW_TEXT( m_indiP_camName, "cam_name", "INDI camera device", "camera" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_fsmName, "fsm_name", "INDI FSM device (val_1/val_2)", "fsm" );
    CREATE_REG_INDI_NEW_TEXT( m_indiP_shmOutput, "shm_output", "Output shmim (camsci)", "shmims" );
    m_indiP_camName["current"].setValue( m_camName );
    m_indiP_camName["target"].setValue( m_camName );
    m_indiP_fsmName["current"].setValue( m_fsmName );
    m_indiP_fsmName["target"].setValue( m_fsmName );
    m_indiP_shmOutput["current"].setValue( m_shmOutput );
    m_indiP_shmOutput["target"].setValue( m_shmOutput );

    CREATE_REG_INDI_RO_NUMBER( m_indiP_exptime, "exptime", "Camera exptime (mirrored)", "camera" );
    m_indiP_exptime.add( pcf::IndiElement( "current" ) );
    m_indiP_exptime["current"].set( m_camExp );
    CREATE_REG_INDI_RO_NUMBER( m_indiP_emgain, "emgain", "Camera emgain (mirrored)", "camera" );
    m_indiP_emgain.add( pcf::IndiElement( "current" ) );
    m_indiP_emgain["current"].set( m_camGain );
    CREATE_REG_INDI_RO_NUMBER( m_indiP_blacklevel, "blacklevel", "Camera blacklevel (mirrored)",
                               "camera" );
    m_indiP_blacklevel.add( pcf::IndiElement( "current" ) );
    m_indiP_blacklevel["current"].set( m_camBlacklevel );
    CREATE_REG_INDI_RO_NUMBER( m_indiP_fps, "fps", "Camera fps (mirrored)", "camera" );
    m_indiP_fps.add( pcf::IndiElement( "current" ) );
    m_indiP_fps["current"].set( m_camFps );
    CREATE_REG_INDI_RO_NUMBER( m_indiP_bitDepth, "bitDepth", "Camera bitDepth (mirrored)",
                               "camera" );
    m_indiP_bitDepth.add( pcf::IndiElement( "current" ) );
    m_indiP_bitDepth["current"].set( m_camBitdepth );

    REG_INDI_SETPROP( m_indiP_remoteExptime, m_camName, "exptime" );
    REG_INDI_SETPROP( m_indiP_remoteEmgain, m_camName, "emgain" );
    REG_INDI_SETPROP( m_indiP_remoteBlacklevel, m_camName, "blacklevel" );
    REG_INDI_SETPROP( m_indiP_remoteFps, m_camName, "fps" );
    REG_INDI_SETPROP( m_indiP_remoteBitDepth, m_camName, "bitDepth" );
    REG_INDI_SETPROP( m_indiP_remoteRoiW, m_camName, "roi_region_w" );
    REG_INDI_SETPROP( m_indiP_remoteRoiH, m_camName, "roi_region_h" );
    REG_INDI_SETPROP( m_indiP_remoteFsmVal1, m_fsmName, "val_1" );
    REG_INDI_SETPROP( m_indiP_remoteFsmVal2, m_fsmName, "val_2" );

    m_worker = std::thread( workerStart, this );
    state( stateCodes::NOTCONNECTED );
    log<text_log>( "llowfscSimCpp starting optical-model worker" );
    return 0;
}

int llowfscSimCpp::appLogic()
{
    if( m_snapFault.exchange( false ) )
    {
        m_streaming = false;
        updateSwitchIfChanged( m_indiP_streaming, "toggle", pcf::IndiElement::Off, INDI_IDLE );
        log<text_log>( "streaming OFF after snap failure", logPrio::LOG_ERROR );
    }
    if( !m_modelReady.load() )
        state( stateCodes::NOTCONNECTED );
    else if( m_streaming.load() )
        state( stateCodes::OPERATING );
    else
        state( stateCodes::READY );
    mirrorCamIndi();
    return 0;
}

int llowfscSimCpp::appShutdown()
{
    m_workerShutdown = true;
    try
    {
        if( m_worker.joinable() )
            m_worker.join();
    }
    catch( ... )
    {
    }
    return 0;
}

void llowfscSimCpp::workerStart( llowfscSimCpp *s )
{
    s->workerExec();
}

void llowfscSimCpp::copyLive( llowfscsim::SnapRequest &req, double &fps )
{
    std::lock_guard<std::mutex> lock( m_liveMutex );
    req.exp = m_camExp;
    req.gain = m_camGain;
    req.blacklevel = m_camBlacklevel;
    req.bitdepth = m_camBitdepth;
    req.roi_w = m_camRoiW;
    req.roi_h = m_camRoiH;
    req.fsm_x_nm = m_fsmX_nm;
    req.fsm_y_nm = m_fsmY_nm;
    fps = m_camFps > 0 ? m_camFps : 1.0;
    req.shutter_closed = m_shutterClosed.load();
    req.use_vortex = m_useVortex.load();
    req.vmag = m_magnitude;
}

bool llowfscSimCpp::copyOpenShmim2D( IMAGE &im, std::vector<double> &out, uint32_t &n0,
                                     uint32_t &n1, bool magpyxTranspose )
{
    if( !im.md )
        return false;
    while( im.md->write )
        ;
    n0 = im.md->size[0];
    n1 = ( im.md->naxis > 1 ) ? im.md->size[1] : 1;
    const size_t n = static_cast<size_t>( n0 ) * static_cast<size_t>( n1 );
    std::vector<double> src( n, 0.0 );
    if( im.md->datatype == _DATATYPE_FLOAT )
    {
        const float *p = (const float *)im.array.raw;
        for( size_t i = 0; i < n; ++i )
            src[i] = p[i];
    }
    else if( im.md->datatype == _DATATYPE_DOUBLE )
    {
        const double *p = (const double *)im.array.raw;
        for( size_t i = 0; i < n; ++i )
            src[i] = p[i];
    }
    else
        return false;
    out.resize( n );
    if( magpyxTranspose && n1 > 1 )
    {
        // Match magpyx ImageStream / llowfscSim._copy_shmim:
        //   view = np.array(milk, order='F')  # F-contig (n0,n1): view[i,j] = milk[i + j*n0]
        //   dest = view.T.copy()              # C-contig (n1,n0): dest[j,i] = milk[i + j*n0]
        // C-contiguous storage is dest[j, i] at offset j*n0 + i.
        // Writing dest at j + i*n1 (F-order of the transpose) and then memcpy-ing
        // that buffer into a C-order numpy array *undoes* the transpose — the model
        // then sees milk without .T. dm01disp00 is not symmetric, so that destroys
        // the vortex null while FSM (scalars) still looks fine.
        for( uint32_t j = 0; j < n1; ++j )
            for( uint32_t i = 0; i < n0; ++i )
                out[static_cast<size_t>( j ) * n0 + static_cast<size_t>( i )] =
                    src[static_cast<size_t>( i ) + static_cast<size_t>( j ) * n0];
        std::swap( n0, n1 );
    }
    else
        out = std::move( src );
    return true;
}

bool llowfscSimCpp::ensureShmimOpen( IMAGE &im, bool &open, const std::string &name )
{
    if( open )
        return true;
    if( ImageStreamIO_openIm( &im, name.c_str() ) != IMAGESTREAMIO_SUCCESS )
        return false;
    open = true;
    return true;
}

bool llowfscSimCpp::grabOpen2D( IMAGE &im, bool &open, const std::string &name,
                                std::vector<double> &out, uint32_t &n0, uint32_t &n1,
                                bool magpyxTranspose )
{
    if( !ensureShmimOpen( im, open, name ) )
        return false;
    if( !copyOpenShmim2D( im, out, n0, n1, magpyxTranspose ) )
    {
        ImageStreamIO_closeIm( &im );
        open = false;
        return false;
    }
    return true;
}

bool llowfscSimCpp::grabShmim2D( const std::string &name, std::vector<double> &out, uint32_t &n0,
                                 uint32_t &n1, bool magpyxTranspose )
{
    IMAGE im{};
    bool open = false;
    if( !grabOpen2D( im, open, name, out, n0, n1, magpyxTranspose ) )
        return false;
    ImageStreamIO_closeIm( &im );
    return true;
}

bool llowfscSimCpp::grabShmimScalar( const std::string &name, double &out )
{
    uint32_t n0 = 0, n1 = 0;
    std::vector<double> v;
    if( !grabShmim2D( name, v, n0, n1, false ) || v.empty() )
        return false;
    out = v[0];
    return true;
}

int llowfscSimCpp::ensureOutput( IMAGE &im, bool &open, const std::string &name, uint32_t w,
                                 uint32_t h )
{
    if( open && im.md && im.md->size[0] == h && im.md->size[1] == w )
        return 0;
    if( open )
    {
        ImageStreamIO_closeIm( &im );
        open = false;
    }
    uint32_t sizes[2] = { h, w };
    if( ImageStreamIO_createIm( &im, name.c_str(), 2, sizes, _DATATYPE_FLOAT, 1, 8, 0 ) !=
        IMAGESTREAMIO_SUCCESS )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "createIm failed: " + name } );
    }
    open = true;
    log<text_log>( "created shmim " + name + " " + std::to_string( h ) + "x" +
                   std::to_string( w ) + " (numpy shape, magaox F-write)" );
    return 0;
}

int llowfscSimCpp::publish( IMAGE &im, const std::vector<float> &pix, uint32_t w, uint32_t h )
{
    if( !im.md || pix.size() < static_cast<size_t>( w ) * static_cast<size_t>( h ) )
        return -1;
    // magaox.Image.write(): F-contiguous copy of C-order (h,w) into milk size[0]=h.
    // Blocked transpose — a naive y,x loop is ~0.5 ms at 512², a large slice of 412 Hz.
    im.md->write = 1;
    float *dest = (float *)im.array.raw;
    const float *src = pix.data();
    if( im.md->size[0] == h && im.md->size[1] == w )
    {
        constexpr uint32_t B = 32;
        for( uint32_t y0 = 0; y0 < h; y0 += B )
        {
            const uint32_t y1 = std::min( h, y0 + B );
            for( uint32_t x0 = 0; x0 < w; x0 += B )
            {
                const uint32_t x1 = std::min( w, x0 + B );
                for( uint32_t y = y0; y < y1; ++y )
                {
                    const float *row = src + static_cast<size_t>( y ) * w;
                    for( uint32_t x = x0; x < x1; ++x )
                        dest[static_cast<size_t>( y ) + static_cast<size_t>( x ) * h] = row[x];
                }
            }
        }
    }
    else
    {
        std::memcpy( dest, src,
                     static_cast<size_t>( w ) * static_cast<size_t>( h ) * sizeof( float ) );
    }
    if( ImageStreamIO_UpdateIm( &im ) != IMAGESTREAMIO_SUCCESS )
    {
        im.md->cnt0++;
        im.md->write = 0;
        ImageStreamIO_sempost( &im, -1 );
    }
    return 0;
}

void llowfscSimCpp::mirrorCamIndi()
{
    double exp = 0, gain = 0, bl = 0, fps = 0;
    int bit = 0;
    {
        std::lock_guard<std::mutex> lock( m_liveMutex );
        exp = m_camExp;
        gain = m_camGain;
        bl = m_camBlacklevel;
        fps = m_camFps;
        bit = m_camBitdepth;
    }
    updateIfChanged( m_indiP_exptime, "current", exp );
    updateIfChanged( m_indiP_emgain, "current", gain );
    updateIfChanged( m_indiP_blacklevel, "current", bl );
    updateIfChanged( m_indiP_fps, "current", fps );
    updateIfChanged( m_indiP_bitDepth, "current", static_cast<double>( bit ) );
}

std::string llowfscSimCpp::pickBridgeDir() const
{
    auto hasBridge = []( const std::string &dir ) -> bool {
        if( dir.empty() )
            return false;
        std::string p = dir;
        if( p.back() != '/' )
            p += '/';
        p += "optical_bridge.py";
        return access( p.c_str(), R_OK ) == 0;
    };

    if( hasBridge( m_bridgeDir ) )
        return m_bridgeDir;
    if( hasBridge( "/opt/MagAOX/python/llowfscSimCpp" ) )
        return "/opt/MagAOX/python/llowfscSimCpp";
#ifdef LLOWFSCSIM_BRIDGE_DIR
    if( hasBridge( LLOWFSCSIM_BRIDGE_DIR ) )
        return LLOWFSCSIM_BRIDGE_DIR;
#endif
    if( !m_bridgeDir.empty() )
        return m_bridgeDir;
    return "/opt/MagAOX/python/llowfscSimCpp";
}

std::string llowfscSimCpp::pickPythonPrefix() const
{
    auto ok = []( const std::string &p ) -> bool {
        if( p.empty() )
            return false;
        return access( ( p + "/bin/python" ).c_str(), X_OK ) == 0 ||
               access( ( p + "/bin/python3" ).c_str(), X_OK ) == 0;
    };
    if( ok( m_pythonPrefix ) )
        return m_pythonPrefix;
#ifdef PYTHON_PREFIX
    if( ok( PYTHON_PREFIX ) )
        return PYTHON_PREFIX;
#endif
    // MagAO-X Python packages and apps are installed into base conda.
    if( ok( "/opt/conda" ) )
        return "/opt/conda";
    if( !m_pythonPrefix.empty() )
        return m_pythonPrefix;
#ifdef PYTHON_PREFIX
    return PYTHON_PREFIX;
#else
    return {};
#endif
}

void llowfscSimCpp::workerExec()
{
    llowfscsim::OpticalModel model;
    const std::string bridge = pickBridgeDir();
    const std::string prefix = pickPythonPrefix();

    log<text_log>( "Python prefix=" + prefix + " bridge=" + bridge );
    if( !model.initialize( prefix, bridge, m_modelCfg.cuda_device ) )
    {
        log<software_error>( { __FILE__, __LINE__, model.lastError } );
        return;
    }
    log<text_log>( "Python interpreter ready; creating optical model (slow, GPU). "
                   "esc.single is built once; later frames apply live DM/FSM/camera then snap." );
    int nc = 0, na = 0;
    if( !model.create( m_modelCfg, nc, na ) )
    {
        log<software_error>( { __FILE__, __LINE__, model.lastError } );
        return;
    }
    m_ncamsci = nc;
    m_nact = na;
    log<text_log>( "optical model ready ncamsci=" + std::to_string( nc ) +
                   " nact=" + std::to_string( na ) + " " + model.lastInfo );

    uint32_t fn0 = 0, fn1 = 0;
    std::vector<double> flat;
    if( grabShmim2D( m_shmDmFlat, flat, fn0, fn1, true ) &&
        static_cast<int>( fn0 ) == na && static_cast<int>( fn1 ) == na )
    {
        for( double &v : flat )
            v *= m_dmScale;
        if( !model.setDmFlat( flat.data(), na ) )
            log<text_log>( "set_dm_flat failed: " + model.lastError, logPrio::LOG_WARNING );
        else
        {
            double acc = 0.0;
            double maxAsym = 0.0;
            const size_t nact = static_cast<size_t>( na );
            for( double v : flat )
                acc += v * v;
            for( int i = 0; i < na; ++i )
                for( int j = i + 1; j < na; ++j )
                {
                    const double a = flat[static_cast<size_t>( i ) * nact + static_cast<size_t>( j )];
                    const double b = flat[static_cast<size_t>( j ) * nact + static_cast<size_t>( i )];
                    maxAsym = std::max( maxAsym, std::abs( a - b ) );
                }
            const double rms = std::sqrt( acc / static_cast<double>( flat.size() ) );
            log<text_log>( "loaded DM flat from " + m_shmDmFlat + " magpyx F.T rms=" +
                           std::to_string( 1e9 * rms ) + " nm  max|a-a.T|=" +
                           std::to_string( 1e9 * maxAsym ) + " nm" );
        }
    }
    else
        log<text_log>( "no DM flat from " + m_shmDmFlat + " — using model compute_flat",
                       logPrio::LOG_WARNING );

    IMAGE outIm{};
    bool outOpen = false;
    IMAGE camloIm{};
    bool camloOpen = false;
    IMAGE dmIm{};
    bool dmOpen = false;
    IMAGE opdIm{};
    bool opdOpen = false;
    m_modelReady = true;

    double t0 = mx::sys::get_curr_time();
    double timeCounter = 0.0;
    uint64_t nSnap = 0;
    double grabSum_s = 0.0;
    double snapSum_s = 0.0;
    double pubSum_s = 0.0;
    std::vector<float> frame;

    while( !m_workerShutdown.load() )
    {
        double fps = 200.0;
        llowfscsim::SnapRequest req;
        copyLive( req, fps );
        req.nact = m_nact;

        if( !m_streaming.load() )
        {
            mx::sys::milliSleep( 50 );
            continue;
        }

        uint32_t n0 = 0, n1 = 0;
        std::vector<double> dm;
        const double tGrab0 = mx::sys::get_curr_time();
        if( grabOpen2D( dmIm, dmOpen, m_shmDmTotal, dm, n0, n1, true ) &&
            static_cast<int>( n0 ) == m_nact && static_cast<int>( n1 ) == m_nact )
        {
            for( double &v : dm )
                v *= m_dmScale;
            req.dm = dm.data();
        }

        double opd[10]{};
        uint32_t o0 = 0, o1 = 0;
        std::vector<double> opdv;
        if( grabOpen2D( opdIm, opdOpen, m_shmOpd, opdv, o0, o1, false ) && opdv.size() >= 10 )
        {
            for( int i = 0; i < 10; ++i )
                opd[i] = opdv[static_cast<size_t>( i )];
            req.opd = opd;
        }
        const double tSnap0 = mx::sys::get_curr_time();

        uint32_t w = 0, h = 0;
        req.camlo = false;
        if( !model.snap( req, frame, w, h ) )
        {
            log<text_log>( "snap_camsci failed: " + model.lastError, logPrio::LOG_ERROR );
            m_streaming = false;
            m_snapFault = true;
            mx::sys::milliSleep( 50 );
            continue;
        }
        const double tPub0 = mx::sys::get_curr_time();
        if( ensureOutput( outIm, outOpen, m_shmOutput, w, h ) == 0 )
            publish( outIm, frame, w, h );
        const double tEnd = mx::sys::get_curr_time();

        ++nSnap;
        grabSum_s += tSnap0 - tGrab0;
        snapSum_s += tPub0 - tSnap0;
        pubSum_s += tEnd - tPub0;
        if( nSnap % 256 == 0 )
        {
            const double n = static_cast<double>( nSnap );
            const double grab_ms = 1000.0 * grabSum_s / n;
            const double snap_ms = 1000.0 * snapSum_s / n;
            const double pub_ms = 1000.0 * pubSum_s / n;
            const double tot_ms = grab_ms + snap_ms + pub_ms;
            log<text_log>( "frame mean " + std::to_string( tot_ms ) + " ms (~" +
                           std::to_string( 1000.0 / std::max( tot_ms, 1e-6 ) ) +
                           " Hz) grab=" + std::to_string( grab_ms ) + " snap=" +
                           std::to_string( snap_ms ) + " pub=" + std::to_string( pub_ms ) +
                           " over " + std::to_string( nSnap ) );
        }

        if( !m_shmCamloOutput.empty() )
        {
            req.camlo = true;
            std::vector<float> lo;
            uint32_t lw = 0, lh = 0;
            if( model.snap( req, lo, lw, lh ) )
            {
                if( ensureOutput( camloIm, camloOpen, m_shmCamloOutput, lw, lh ) == 0 )
                    publish( camloIm, lo, lw, lh );
            }
        }

        const double period = 1.0 / std::max( fps, 1e-6 );
        timeCounter += period;
        const double now = mx::sys::get_curr_time();
        const double target = t0 + timeCounter;
        const double remain = target - now;
        if( remain > 0.0 )
            mx::sys::microSleep( static_cast<unsigned>( remain * 1e6 ) );
        else if( remain < -1.0 )
        {
            t0 = mx::sys::get_curr_time();
            timeCounter = 0.0;
        }
    }

    if( outOpen )
        ImageStreamIO_closeIm( &outIm );
    if( camloOpen )
        ImageStreamIO_closeIm( &camloIm );
    if( dmOpen )
        ImageStreamIO_closeIm( &dmIm );
    if( opdOpen )
        ImageStreamIO_closeIm( &opdIm );
}

INDI_NEWCALLBACK_DEFN( llowfscSimCpp, m_indiP_streaming )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_streaming, ipRecv );
    if( !ipRecv.find( "toggle" ) )
        return 0;
    const bool on = ipRecv["toggle"].getSwitchState() == pcf::IndiElement::On;
    m_streaming = on;
    updateSwitchIfChanged( m_indiP_streaming, "toggle",
                           on ? pcf::IndiElement::On : pcf::IndiElement::Off,
                           on ? INDI_BUSY : INDI_IDLE );
    log<text_log>( std::string( "streaming " ) + ( on ? "ON" : "OFF" ) );
    return 0;
}

INDI_NEWCALLBACK_DEFN( llowfscSimCpp, m_indiP_shutter )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_shutter, ipRecv );
    if( !ipRecv.find( "toggle" ) )
        return 0;
    const bool closed = ipRecv["toggle"].getSwitchState() == pcf::IndiElement::On;
    m_shutterClosed = closed;
    updateSwitchIfChanged( m_indiP_shutter, "toggle",
                           closed ? pcf::IndiElement::On : pcf::IndiElement::Off, INDI_IDLE );
    log<text_log>( std::string( "shutter " ) + ( closed ? "CLOSED" : "OPEN" ) );
    return 0;
}

INDI_NEWCALLBACK_DEFN( llowfscSimCpp, m_indiP_usevortex )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_usevortex, ipRecv );
    if( !ipRecv.find( "toggle" ) )
        return 0;
    const bool on = ipRecv["toggle"].getSwitchState() == pcf::IndiElement::On;
    m_useVortex = on;
    updateSwitchIfChanged( m_indiP_usevortex, "toggle",
                           on ? pcf::IndiElement::On : pcf::IndiElement::Off, INDI_IDLE );
    log<text_log>( std::string( "usevortex " ) + ( on ? "ON" : "OFF" ) );
    return 0;
}

INDI_NEWCALLBACK_DEFN( llowfscSimCpp, m_indiP_magnitude )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_magnitude, ipRecv );
    double target;
    if( indiTargetUpdate( m_indiP_magnitude, target, ipRecv, false ) < 0 )
        return -1;
    if( !std::isfinite( target ) )
        return 0;
    {
        std::lock_guard<std::mutex> lock( m_liveMutex );
        m_magnitude = target;
    }
    updateIfChanged( m_indiP_magnitude, "current", m_magnitude );
    log<text_log>( "magnitude -> " + std::to_string( target ) );
    return 0;
}

INDI_NEWCALLBACK_DEFN( llowfscSimCpp, m_indiP_camName )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_camName, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_camName, target, ipRecv, false ) < 0 )
        return -1;
    log<text_log>( "cam_name -> " + target + " (restart app to rebind SET subscriptions)",
                   logPrio::LOG_WARNING );
    m_camName = target;
    updateIfChanged( m_indiP_camName, "current", m_camName );
    return 0;
}

INDI_NEWCALLBACK_DEFN( llowfscSimCpp, m_indiP_fsmName )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_fsmName, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_fsmName, target, ipRecv, false ) < 0 )
        return -1;
    log<text_log>( "fsm_name -> " + target + " (restart app to rebind SET subscriptions)",
                   logPrio::LOG_WARNING );
    m_fsmName = target;
    updateIfChanged( m_indiP_fsmName, "current", m_fsmName );
    return 0;
}

INDI_NEWCALLBACK_DEFN( llowfscSimCpp, m_indiP_shmOutput )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_shmOutput, ipRecv );
    std::string target;
    if( indiTargetUpdate( m_indiP_shmOutput, target, ipRecv, false ) < 0 )
        return -1;
    m_shmOutput = target;
    updateIfChanged( m_indiP_shmOutput, "current", m_shmOutput );
    return 0;
}

INDI_SETCALLBACK_DEFN( llowfscSimCpp, m_indiP_remoteExptime )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_remoteExptime, ipRecv );
    double v = 0;
    if( !parseIndiCurrentNumber( ipRecv, v ) )
        return 0;
    std::lock_guard<std::mutex> lock( m_liveMutex );
    m_camExp = v;
    return 0;
}

INDI_SETCALLBACK_DEFN( llowfscSimCpp, m_indiP_remoteEmgain )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_remoteEmgain, ipRecv );
    double v = 0;
    if( !parseIndiCurrentNumber( ipRecv, v ) )
        return 0;
    std::lock_guard<std::mutex> lock( m_liveMutex );
    m_camGain = v;
    return 0;
}

INDI_SETCALLBACK_DEFN( llowfscSimCpp, m_indiP_remoteBlacklevel )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_remoteBlacklevel, ipRecv );
    double v = 0;
    if( !parseIndiCurrentNumber( ipRecv, v ) )
        return 0;
    std::lock_guard<std::mutex> lock( m_liveMutex );
    m_camBlacklevel = v;
    return 0;
}

INDI_SETCALLBACK_DEFN( llowfscSimCpp, m_indiP_remoteFps )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_remoteFps, ipRecv );
    double v = 0;
    if( !parseIndiCurrentNumber( ipRecv, v ) )
        return 0;
    std::lock_guard<std::mutex> lock( m_liveMutex );
    m_camFps = v;
    return 0;
}

INDI_SETCALLBACK_DEFN( llowfscSimCpp, m_indiP_remoteBitDepth )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_remoteBitDepth, ipRecv );
    double v = 0;
    if( !parseIndiCurrentNumber( ipRecv, v ) )
        return 0;
    std::lock_guard<std::mutex> lock( m_liveMutex );
    m_camBitdepth = static_cast<int>( v );
    return 0;
}

INDI_SETCALLBACK_DEFN( llowfscSimCpp, m_indiP_remoteRoiW )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_remoteRoiW, ipRecv );
    double v = 0;
    if( !parseIndiCurrentNumber( ipRecv, v ) )
        return 0;
    std::lock_guard<std::mutex> lock( m_liveMutex );
    m_camRoiW = std::max( 1, static_cast<int>( v ) );
    return 0;
}

INDI_SETCALLBACK_DEFN( llowfscSimCpp, m_indiP_remoteRoiH )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_remoteRoiH, ipRecv );
    double v = 0;
    if( !parseIndiCurrentNumber( ipRecv, v ) )
        return 0;
    std::lock_guard<std::mutex> lock( m_liveMutex );
    m_camRoiH = std::max( 1, static_cast<int>( v ) );
    return 0;
}

INDI_SETCALLBACK_DEFN( llowfscSimCpp, m_indiP_remoteFsmVal1 )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_remoteFsmVal1, ipRecv );
    double v = 0;
    if( !parseIndiCurrentNumber( ipRecv, v ) )
        return 0;
    std::lock_guard<std::mutex> lock( m_liveMutex );
    m_fsmX_nm = v;
    return 0;
}

INDI_SETCALLBACK_DEFN( llowfscSimCpp, m_indiP_remoteFsmVal2 )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_remoteFsmVal2, ipRecv );
    double v = 0;
    if( !parseIndiCurrentNumber( ipRecv, v ) )
        return 0;
    std::lock_guard<std::mutex> lock( m_liveMutex );
    m_fsmY_nm = v;
    return 0;
}

} // namespace app
} // namespace MagAOX

#endif
