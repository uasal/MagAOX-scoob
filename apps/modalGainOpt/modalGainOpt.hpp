/** \file modalGainOpt.hpp
 * \brief The MagAO-X PSD-based gain optimizer header file
 *
 * \ingroup modalGainOpt_files
 */

#ifndef modalGainOpt_hpp
#define modalGainOpt_hpp

#include <mx/ao/analysis/clGainOpt.hpp>
#include <mx/ao/analysis/clAOLinearPredictor.hpp>

#include "../../libMagAOX/libMagAOX.hpp" //Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"

/** \defgroup modalGainOpt
 * \brief The MagAO-X application to perform PSD-based gain optimization
 *
 * <a href="../handbook/operating/software/apps/modalGainOpt.html">Application Documentation</a>
 *
 * \ingroup apps
 *
 */

/** \defgroup modalGainOpt_files
 * \ingroup modalGainOpt
 */

namespace MagAOX
{
namespace app
{

struct psdShmimT
{
    static std::string configSection()
    {
        return "psdShmim";
    };

    static std::string indiPrefix()
    {
        return "psd";
    };
};

struct freqShmimT
{
    static std::string configSection()
    {
        return "freqShmim";
    };

    static std::string indiPrefix()
    {
        return "freq";
    };
};

struct gainFactShmimT
{
    static std::string configSection()
    {
        return "gainFactShmim";
    };

    static std::string indiPrefix()
    {
        return "gainFact";
    };
};

struct multFactShmimT
{
    static std::string configSection()
    {
        return "multFactShmim";
    };

    static std::string indiPrefix()
    {
        return "multFact";
    };
};

struct pcGainFactShmimT
{
    static std::string configSection()
    {
        return "pcGainFactShmim";
    };

    static std::string indiPrefix()
    {
        return "pcGainFact";
    };
};

struct pcMultFactShmimT
{
    static std::string configSection()
    {
        return "pcMultFactShmim";
    };

    static std::string indiPrefix()
    {
        return "pcMultFact";
    };
};

struct numpccoeffShmimT
{
    static std::string configSection()
    {
        return "numpccoeffShmim";
    };

    static std::string indiPrefix()
    {
        return "numpccoeff";
    };
};

struct acoeffShmimT
{
    static std::string configSection()
    {
        return "acoeffShmim";
    };

    static std::string indiPrefix()
    {
        return "acoeff";
    };
};

struct bcoeffShmimT
{
    static std::string configSection()
    {
        return "bcoeffShmim";
    };

    static std::string indiPrefix()
    {
        return "bcoeff";
    };
};

struct gainCalShmimT
{
    static std::string configSection()
    {
        return "gainCalShmim";
    };

    static std::string indiPrefix()
    {
        return "gainCal";
    };
};

struct gainCalFactShmimT
{
    static std::string configSection()
    {
        return "gainCalFactShmim";
    };

    static std::string indiPrefix()
    {
        return "gainCalFact";
    };
};

struct tauShmimT
{
    static std::string configSection()
    {
        return "tauShmim";
    };

    static std::string indiPrefix()
    {
        return "tau";
    };
};

struct noiseShmimT
{
    static std::string configSection()
    {
        return "noiseShmim";
    };

    static std::string indiPrefix()
    {
        return "noise";
    };
};

struct wfsavgShmimT
{
    static std::string configSection()
    {
        return "wfsavgShmim";
    };

    static std::string indiPrefix()
    {
        return "wfsavg";
    };
};

struct wfsmaskShmimT
{
    static std::string configSection()
    {
        return "wfsmaskShmim";
    };

    static std::string indiPrefix()
    {
        return "wfsmask";
    };
};

/// The MagAO-X PSD-based gain optimizer
/**
 * \ingroup modalGainOpt
 */
class modalGainOpt : public MagAOXApp<true>,
                     dev::shmimMonitor<modalGainOpt, psdShmimT>,
                     dev::shmimMonitor<modalGainOpt, freqShmimT>,
                     dev::shmimMonitor<modalGainOpt, gainFactShmimT>,
                     dev::shmimMonitor<modalGainOpt, multFactShmimT>,
                     dev::shmimMonitor<modalGainOpt, pcGainFactShmimT>,
                     dev::shmimMonitor<modalGainOpt, pcMultFactShmimT>,
                     dev::shmimMonitor<modalGainOpt, numpccoeffShmimT>,
                     dev::shmimMonitor<modalGainOpt, acoeffShmimT>,
                     dev::shmimMonitor<modalGainOpt, bcoeffShmimT>,
                     dev::shmimMonitor<modalGainOpt, gainCalShmimT>,
                     dev::shmimMonitor<modalGainOpt, gainCalFactShmimT>,
                     dev::shmimMonitor<modalGainOpt, tauShmimT>,
                     dev::shmimMonitor<modalGainOpt, noiseShmimT>,
                     dev::shmimMonitor<modalGainOpt, wfsavgShmimT>,
                     dev::shmimMonitor<modalGainOpt, wfsmaskShmimT>

{

    // Give the test harness access.
    friend class modalGainOpt_test;

    friend class dev::shmimMonitor<modalGainOpt, psdShmimT>;
    friend class dev::shmimMonitor<modalGainOpt, freqShmimT>;
    friend class dev::shmimMonitor<modalGainOpt, gainFactShmimT>;
    friend class dev::shmimMonitor<modalGainOpt, multFactShmimT>;
    friend class dev::shmimMonitor<modalGainOpt, pcGainFactShmimT>;
    friend class dev::shmimMonitor<modalGainOpt, pcMultFactShmimT>;
    friend class dev::shmimMonitor<modalGainOpt, numpccoeffShmimT>;
    friend class dev::shmimMonitor<modalGainOpt, acoeffShmimT>;
    friend class dev::shmimMonitor<modalGainOpt, bcoeffShmimT>;
    friend class dev::shmimMonitor<modalGainOpt, gainCalShmimT>;
    friend class dev::shmimMonitor<modalGainOpt, gainCalFactShmimT>;
    friend class dev::shmimMonitor<modalGainOpt, tauShmimT>;
    friend class dev::shmimMonitor<modalGainOpt, noiseShmimT>;
    friend class dev::shmimMonitor<modalGainOpt, wfsavgShmimT>;
    friend class dev::shmimMonitor<modalGainOpt, wfsmaskShmimT>;

  public:
    typedef dev::shmimMonitor<modalGainOpt, psdShmimT>         psdShmimMonitorT;
    typedef dev::shmimMonitor<modalGainOpt, freqShmimT>        freqShmimMonitorT;
    typedef dev::shmimMonitor<modalGainOpt, gainFactShmimT>    gainFactShmimMonitorT;
    typedef dev::shmimMonitor<modalGainOpt, multFactShmimT>    multFactShmimMonitorT;
    typedef dev::shmimMonitor<modalGainOpt, pcGainFactShmimT>  pcGainFactShmimMonitorT;
    typedef dev::shmimMonitor<modalGainOpt, pcMultFactShmimT>  pcMultFactShmimMonitorT;
    typedef dev::shmimMonitor<modalGainOpt, numpccoeffShmimT>  numpccoeffShmimMonitorT;
    typedef dev::shmimMonitor<modalGainOpt, acoeffShmimT>      acoeffShmimMonitorT;
    typedef dev::shmimMonitor<modalGainOpt, bcoeffShmimT>      bcoeffShmimMonitorT;
    typedef dev::shmimMonitor<modalGainOpt, gainCalShmimT>     gainCalShmimMonitorT;
    typedef dev::shmimMonitor<modalGainOpt, gainCalFactShmimT> gainCalFactShmimMonitorT;
    typedef dev::shmimMonitor<modalGainOpt, tauShmimT>         tauShmimMonitorT;
    typedef dev::shmimMonitor<modalGainOpt, noiseShmimT>       noiseShmimMonitorT;
    typedef dev::shmimMonitor<modalGainOpt, wfsavgShmimT>      wfsavgShmimMonitorT;
    typedef dev::shmimMonitor<modalGainOpt, wfsmaskShmimT>     wfsmaskShmimMonitorT;

    typedef std::chrono::time_point<std::chrono::steady_clock> timePointT;
    typedef std::chrono::duration<double>                      durationT;

  protected:
    /** \name Configurable Parameters
     *@{
     */

    int m_loopNum{ 1 }; ///< The number of the loop. Used to set shmim names, as in aolN_mgainfact.

    std::string m_loopName; ///< The name of the loop control INDI device name.

    std::string m_wfsDevice{ "camwfs" };

    std::string m_psdDevice; /**< The INDI device name of the PSD calculator.  Defaults to aolN_modevalPSDs
                                  where N is m_loopNum.*/

    bool m_autoUpdate{ false }; ///< Flag controlling whether gains are automatically updated

    float m_gainGain{ 0.1 }; ///< The gain to use for closed-loop gain updates.  Default is 0.1.

    ///@}

    bool m_updateOnce{ false }; ///< Flag to trigger a single update with gain.

    bool m_dump{ false }; ///< Flag to trigger a single update with no gain.

    float m_fps{ 0 };

    /// Each mode gets its own gain optimizer
    std::vector<mx::AO::analysis::clGainOpt<float>>           m_goptCurrent;
    std::vector<mx::AO::analysis::clGainOpt<float>>           m_goptSI;
    std::vector<mx::AO::analysis::clGainOpt<float>>           m_goptLP;
    std::vector<mx::AO::analysis::clAOLinearPredictor<float>> m_linPred;

    bool m_goptUpdated{ true };   ///< Tracks if a parameter has updated requiring updates to the m_gopt entries.
    bool m_pcgoptUpdated{ true }; ///< Tracks if a parameter has updated requiring updates to the m_gopt entries.

    bool m_freqUpdated{ true }; /**< Tracks if the frequency scale has updated, which necessitates additional calcs.
                                     If true, implies m_goptUpdate == true.*/
    float m_psdTime{ 1 };
    float m_psdAvgTime{ 10 };
    float m_psdOverlapFraction{ 0.5 };

    std::vector<float> m_freq;

    mx::improc::eigenImage<float> m_clPSDs;
    mx::improc::eigenImage<float> m_clXferCurrent;
    mx::improc::eigenImage<float> m_clXferSI;
    mx::improc::eigenImage<float> m_clXferLP;

    std::vector<std::vector<float>> m_olPSDs;
    std::vector<std::vector<float>> m_nPSDs;

    std::vector<float> m_modeVarOL;

    std::vector<float> m_optGainSI;
    std::vector<float> m_modeVarSI;

    std::vector<float> m_optGainLP;
    std::vector<float> m_modeVarLP;

    bool m_loop{ false };

    float m_opticalGain{ 1 };

    float m_gain{ 0 };

    float m_mult{ 1 };

    float m_siGain{ 0 };

    float m_siMult{ 1 };

    float m_pcGain{ 0 };

    float m_pcMult{ 0 };

    bool m_pcOn{ false };

    std::vector<float> m_gainFacts;

    std::vector<float> m_multFacts;

    std::vector<float> m_pcGainFacts;

    std::vector<float> m_pcMultFacts;

    std::vector<int> m_Na;

    std::vector<int> m_NaCurrent;

    std::vector<int> m_Nb;

    std::vector<int> m_NbCurrent;

    eigenImage<float> m_as;

    eigenImage<float> m_bs;

    std::vector<float> m_gmaxLP; ///< The previously calculated maximum gains for LP

    int m_nRegCycles{ 60 }; ///< How often to regularize each mode

    std::vector<int> m_regCounter; ///< Counters to track when this mode was last regularized

    std::vector<float> m_regScale; ///< The regularization scale factors for each mode

    std::vector<float> m_gainCals;

    std::vector<float> m_gainCalFacts;

    std::vector<float> m_taus;

    eigenImage<float> m_noiseParams;

    eigenImage<float> m_wfsavg;
    eigenImage<float> m_wfsmask;
    float             m_counts{ 0 };
    float             m_emg{ 1 };
    int               m_npix{ 0 };

    int m_sinceChange{ -1 };

    std::string m_olPSDShmimName;
    std::string m_noisePSDShmimName;
    std::string m_clXferCurrentShmimName;
    std::string m_clXferSIShmimName;
    std::string m_clXferLPShmimName;

    std::string m_optGainShmimName;
    std::string m_optGainSIShmimName;
    std::string m_optGainLPShmimName;

    std::string m_modevarShmimName;

    IMAGE *m_olPSDStream{ nullptr }; ///< The ImageStreamIO shared memory buffer to publish the open loop PSDs
    IMAGE *m_noisePSDStream{
        nullptr }; ///< The ImageStreamIO shared memory buffer to publish the noise PSDs (single value)
    IMAGE *m_clXferCurrentStream{ nullptr }; ///< The ImageStreamIO shared memory buffer to publish the SI ETF
    IMAGE *m_clXferSIStream{ nullptr };      ///< The ImageStreamIO shared memory buffer to publish the SI ETF
    IMAGE *m_clXferLPStream{ nullptr };      ///< The ImageStreamIO shared memory buffer to publish the LP ETF
    IMAGE *m_optGainStream{ nullptr };       ///< The ImageStreamIO shared memory buffer to publish the optimal gains
    IMAGE *m_optGainSIStream{ nullptr };     ///< The ImageStreamIO shared memory buffer to publish the SI optimal gains
    IMAGE *m_optGainLPStream{ nullptr };     ///< The ImageStreamIO shared memory buffer to publish the LP optimal gains

    IMAGE *m_modevarStream{ nullptr }; ///< The ImageStreamIO shared memory buffer to publish the LP optimal gains

  public:
    /// Default c'tor.
    modalGainOpt();

    /// D'tor, declared and defined for noexcept.
    ~modalGainOpt() noexcept
    {
    }

    virtual void setupConfig();

    /// Implementation of loadConfig logic, separated for testing.
    /** This is called by loadConfig().
     */
    int loadConfigImpl( mx::app::appConfigurator &_config /**< [in] an application configuration
                                                                    from which to load values*/ );

    virtual void loadConfig();

    /// Startup function
    /**
     *
     */
    virtual int appStartup();

    /// Implementation of the FSM for modalGainOpt.
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

    int allocate( const psdShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int processImage( void *curr_src,   ///< [in] pointer to the start of the current frame
                      const psdShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int allocate( const freqShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int processImage( void *curr_src,    ///< [in] pointer to the start of the current frame
                      const freqShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int allocate( const gainFactShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int processImage( void *curr_src,        ///< [in] pointer to the start of the current frame
                      const gainFactShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int allocate( const multFactShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int processImage( void *curr_src,        ///< [in] pointer to the start of the current frame
                      const multFactShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int allocate( const pcGainFactShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int processImage( void *curr_src,          ///< [in] pointer to the start of the current frame
                      const pcGainFactShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int allocate( const pcMultFactShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int processImage( void *curr_src,          ///< [in] pointer to the start of the current frame
                      const pcMultFactShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int allocate( const numpccoeffShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int processImage( void *curr_src,          ///< [in] pointer to the start of the current frame
                      const numpccoeffShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int allocate( const acoeffShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int processImage( void *curr_src,      ///< [in] pointer to the start of the current frame
                      const acoeffShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int allocate( const bcoeffShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int processImage( void *curr_src,      ///< [in] pointer to the start of the current frame
                      const bcoeffShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int allocate( const gainCalShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int processImage( void *curr_src,       ///< [in] pointer to the start of the current frame
                      const gainCalShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int allocate( const gainCalFactShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int processImage( void *curr_src,           ///< [in] pointer to the start of the current frame
                      const gainCalFactShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int allocate( const tauShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int processImage( void *curr_src,   ///< [in] pointer to the start of the current frame
                      const tauShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int allocate( const noiseShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int processImage( void *curr_src,     ///< [in] pointer to the start of the current frame
                      const noiseShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int allocate( const wfsavgShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int processImage( void *curr_src,      ///< [in] pointer to the start of the current frame
                      const wfsavgShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int allocate( const wfsmaskShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

    int processImage( void *curr_src,       ///< [in] pointer to the start of the current frame
                      const wfsmaskShmimT & ///< [in] tag to differentiate shmimMonitor parents.
    );

  protected:
    /// Mutex for synchronizing updates.
    std::mutex m_goptMutex;

    /// Flag used to indicate to the goptThread that it should stop calculations ASAP
    bool m_updating{ false };

    /** \name Gain Optimization Thread
     *
     * @{
     */
    int m_goptThreadPrio{ 0 }; ///< Priority of the gain optimization thread.

    std::string m_goptThreadCpuset; ///< The cpuset to use for the gain optimization thread.

    std::thread m_goptThread; ///< The gain optimization thread.

    bool m_goptThreadInit{ true }; ///< Initialization flag for the gain optimization thread.

    pid_t m_goptThreadID{ 0 }; ///< gain optimization thread PID.

    pcf::IndiProperty m_goptThreadProp; ///< The property to hold the gain optimization thread details.

    sem_t m_goptSemaphore; ///< Semaphore used to synchronize the psdShmim thread and the gopt thread.

    float noisePSD( int n );

    /// Gain Optimization thread starter function
    static void goptThreadStart( modalGainOpt *p /**< [in] pointer to this */ );

    /// Gain optimization thread function
    /** Runs until m_shutdown is true.
     */
    void goptThreadExec();

    ///@}

  public:
    /** \name INDI
     * @{
     */

    pcf::IndiProperty m_indiP_autoUpdate;
    pcf::IndiProperty m_indiP_updateOnce;
    pcf::IndiProperty m_indiP_dump;

    pcf::IndiProperty m_indiP_opticalGain;

    pcf::IndiProperty m_indiP_gainGain;

    pcf::IndiProperty m_indiP_emg;
    pcf::IndiProperty m_indiP_psdTime;
    pcf::IndiProperty m_indiP_psdAvgTime;
    pcf::IndiProperty m_indiP_loop;
    pcf::IndiProperty m_indiP_siGain;
    pcf::IndiProperty m_indiP_siMult;
    pcf::IndiProperty m_indiP_pcGain;
    pcf::IndiProperty m_indiP_pcMult;
    pcf::IndiProperty m_indiP_pcOn;

    INDI_NEWCALLBACK_DECL( modalGainOpt, m_indiP_autoUpdate );
    INDI_NEWCALLBACK_DECL( modalGainOpt, m_indiP_updateOnce );
    INDI_NEWCALLBACK_DECL( modalGainOpt, m_indiP_dump );
    INDI_NEWCALLBACK_DECL( modalGainOpt, m_indiP_opticalGain );
    INDI_NEWCALLBACK_DECL( modalGainOpt, m_indiP_gainGain );
    INDI_SETCALLBACK_DECL( modalGainOpt, m_indiP_emg );
    INDI_SETCALLBACK_DECL( modalGainOpt, m_indiP_psdTime );
    INDI_SETCALLBACK_DECL( modalGainOpt, m_indiP_psdAvgTime );
    INDI_SETCALLBACK_DECL( modalGainOpt, m_indiP_loop );
    INDI_SETCALLBACK_DECL( modalGainOpt, m_indiP_siGain );
    INDI_SETCALLBACK_DECL( modalGainOpt, m_indiP_siMult );
    INDI_SETCALLBACK_DECL( modalGainOpt, m_indiP_pcGain );
    INDI_SETCALLBACK_DECL( modalGainOpt, m_indiP_pcMult );
    INDI_SETCALLBACK_DECL( modalGainOpt, m_indiP_pcOn );

    ///@}
};

modalGainOpt::modalGainOpt() : MagAOXApp( MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED )
{
    psdShmimMonitorT::m_getExistingFirst         = true;
    freqShmimMonitorT::m_getExistingFirst        = true;
    gainFactShmimMonitorT::m_getExistingFirst    = true;
    multFactShmimMonitorT::m_getExistingFirst    = true;
    pcGainFactShmimMonitorT::m_getExistingFirst  = true;
    pcMultFactShmimMonitorT::m_getExistingFirst  = true;
    numpccoeffShmimMonitorT::m_getExistingFirst  = true;
    acoeffShmimMonitorT::m_getExistingFirst      = true;
    bcoeffShmimMonitorT::m_getExistingFirst      = true;
    gainCalShmimMonitorT::m_getExistingFirst     = true;
    gainCalFactShmimMonitorT::m_getExistingFirst = true;
    tauShmimMonitorT::m_getExistingFirst         = true;
    noiseShmimMonitorT::m_getExistingFirst       = true;
    wfsavgShmimMonitorT::m_getExistingFirst      = true;
    wfsmaskShmimMonitorT::m_getExistingFirst     = true;

    return;
}

void modalGainOpt::setupConfig()
{
    config.add( "loop.number",
                "",
                "loop.number",
                argType::Required,
                "loop",
                "number",
                false,
                "int",
                "The number of the loop. Used to set shmim names, as in aolN_mgainfact." );

    config.add( "loop.name",
                "",
                "loop.name",
                argType::Required,
                "loop",
                "name",
                false,
                "string",
                "The name of the loop control INDI device name." );

    config.add( "loop.psdDev",
                "",
                "loop.psdDev",
                argType::Required,
                "loop",
                "psdDev",
                false,
                "string",
                "The INDI device name of the PSD calculator.  Defaults to aolN_modevalPSDs where N is loop.number." );

    config.add( "loop.autoUpdate",
                "",
                "loop.autoUpdate",
                argType::Required,
                "loop",
                "autoUpdate",
                false,
                "bool",
                "Flag controlling whether the gains are auto updated.  Also settable via INDI." );

    config.add( "loop.gainGain",
                "",
                "loop.gainGain",
                argType::Required,
                "loop",
                "gainGain",
                false,
                "float",
                "The gain to use for closed-loop gain updates.  Default is 0.1" );

    SHMIMMONITORT_SETUP_CONFIG( psdShmimMonitorT, config );
    SHMIMMONITORT_SETUP_CONFIG( freqShmimMonitorT, config );
    SHMIMMONITORT_SETUP_CONFIG( gainFactShmimMonitorT, config );
    SHMIMMONITORT_SETUP_CONFIG( multFactShmimMonitorT, config );
    SHMIMMONITORT_SETUP_CONFIG( pcGainFactShmimMonitorT, config );
    SHMIMMONITORT_SETUP_CONFIG( pcMultFactShmimMonitorT, config );
    SHMIMMONITORT_SETUP_CONFIG( numpccoeffShmimMonitorT, config );
    SHMIMMONITORT_SETUP_CONFIG( acoeffShmimMonitorT, config );
    SHMIMMONITORT_SETUP_CONFIG( bcoeffShmimMonitorT, config );
    SHMIMMONITORT_SETUP_CONFIG( gainCalShmimMonitorT, config );
    SHMIMMONITORT_SETUP_CONFIG( gainCalFactShmimMonitorT, config );
    SHMIMMONITORT_SETUP_CONFIG( tauShmimMonitorT, config );
    SHMIMMONITORT_SETUP_CONFIG( noiseShmimMonitorT, config );
    SHMIMMONITORT_SETUP_CONFIG( wfsavgShmimMonitorT, config );
    SHMIMMONITORT_SETUP_CONFIG( wfsmaskShmimMonitorT, config );
}

int modalGainOpt::loadConfigImpl( mx::app::appConfigurator &_config )
{
    _config( m_loopNum, "loop.number" );
    _config( m_loopName, "loop.name" );
    _config( m_autoUpdate, "loop.audoUpdate" );
    _config( m_autoUpdate, "loop.gainGain" );

    char shmim[1024];

    snprintf( shmim, sizeof( shmim ), "aol%d_modevalPSDs", m_loopNum );
    m_psdDevice = shmim;
    _config( m_psdDevice, "loop.psdDev" );

    psdShmimMonitorT::m_shmimName = m_psdDevice + "_psds";
    SHMIMMONITORT_LOAD_CONFIG( psdShmimMonitorT, _config );

    freqShmimMonitorT::m_shmimName = m_psdDevice + "_freq";
    SHMIMMONITORT_LOAD_CONFIG( freqShmimMonitorT, _config );

    snprintf( shmim, sizeof( shmim ), "aol%d_mgainfact", m_loopNum );
    gainFactShmimMonitorT::m_shmimName = shmim;
    SHMIMMONITORT_LOAD_CONFIG( gainFactShmimMonitorT, _config );

    snprintf( shmim, sizeof( shmim ), "aol%d_mmultfact", m_loopNum );
    multFactShmimMonitorT::m_shmimName = shmim;
    SHMIMMONITORT_LOAD_CONFIG( multFactShmimMonitorT, _config );

    snprintf( shmim, sizeof( shmim ), "aol%d_mpcgainfact", m_loopNum );
    pcGainFactShmimMonitorT::m_shmimName = shmim;
    SHMIMMONITORT_LOAD_CONFIG( pcGainFactShmimMonitorT, _config );

    snprintf( shmim, sizeof( shmim ), "aol%d_mpcmultfact", m_loopNum );
    pcMultFactShmimMonitorT::m_shmimName = shmim;
    SHMIMMONITORT_LOAD_CONFIG( pcMultFactShmimMonitorT, _config );

    snprintf( shmim, sizeof( shmim ), "aol%d_numpccoeff", m_loopNum );
    numpccoeffShmimMonitorT::m_shmimName = shmim;
    SHMIMMONITORT_LOAD_CONFIG( numpccoeffShmimMonitorT, _config );

    snprintf( shmim, sizeof( shmim ), "aol%d_acoeff", m_loopNum );
    acoeffShmimMonitorT::m_shmimName = shmim;
    SHMIMMONITORT_LOAD_CONFIG( acoeffShmimMonitorT, _config );

    snprintf( shmim, sizeof( shmim ), "aol%d_bcoeff", m_loopNum );
    bcoeffShmimMonitorT::m_shmimName = shmim;
    SHMIMMONITORT_LOAD_CONFIG( bcoeffShmimMonitorT, _config );

    snprintf( shmim, sizeof( shmim ), "aol%d_mgaincal", m_loopNum );
    gainCalShmimMonitorT::m_shmimName = shmim;
    SHMIMMONITORT_LOAD_CONFIG( gainCalShmimMonitorT, _config );

    snprintf( shmim, sizeof( shmim ), "aol%d_mgaincalfact", m_loopNum );
    gainCalFactShmimMonitorT::m_shmimName = shmim;
    SHMIMMONITORT_LOAD_CONFIG( gainCalFactShmimMonitorT, _config );

    snprintf( shmim, sizeof( shmim ), "aol%d_looptau", m_loopNum );
    tauShmimMonitorT::m_shmimName = shmim;
    SHMIMMONITORT_LOAD_CONFIG( tauShmimMonitorT, _config );

    snprintf( shmim, sizeof( shmim ), "aol%d_mnoiseparam", m_loopNum );
    noiseShmimMonitorT::m_shmimName = shmim;
    SHMIMMONITORT_LOAD_CONFIG( noiseShmimMonitorT, _config );

    snprintf( shmim, sizeof( shmim ), "aol%d_wfsavg", m_loopNum );
    wfsavgShmimMonitorT::m_shmimName = shmim;
    SHMIMMONITORT_LOAD_CONFIG( wfsavgShmimMonitorT, _config );

    snprintf( shmim, sizeof( shmim ), "aol%d_wfsmask", m_loopNum );
    wfsmaskShmimMonitorT::m_shmimName = shmim;
    SHMIMMONITORT_LOAD_CONFIG( wfsmaskShmimMonitorT, _config );

    snprintf( shmim, sizeof( shmim ), "aol%d_olpsds", m_loopNum );
    m_olPSDShmimName = shmim;

    snprintf( shmim, sizeof( shmim ), "aol%d_noisepsds", m_loopNum );
    m_noisePSDShmimName = shmim;

    snprintf( shmim, sizeof( shmim ), "aol%d_clxferCurrent", m_loopNum );
    m_clXferCurrentShmimName = shmim;

    snprintf( shmim, sizeof( shmim ), "aol%d_clxferSI", m_loopNum );
    m_clXferSIShmimName = shmim;

    snprintf( shmim, sizeof( shmim ), "aol%d_clxferLP", m_loopNum );
    m_clXferLPShmimName = shmim;

    snprintf( shmim, sizeof( shmim ), "aol%d_mgainoptimal", m_loopNum );
    m_optGainShmimName = shmim;

    snprintf( shmim, sizeof( shmim ), "aol%d_mgainoptimalSI", m_loopNum );
    m_optGainSIShmimName = shmim;

    snprintf( shmim, sizeof( shmim ), "aol%d_mgainoptimalLP", m_loopNum );
    m_optGainLPShmimName = shmim;

    snprintf( shmim, sizeof( shmim ), "aol%d_mmodevar", m_loopNum );
    m_modevarShmimName = shmim;

    return 0;
}

void modalGainOpt::loadConfig()
{
    loadConfigImpl( config );
}

int modalGainOpt::appStartup()
{
    SHMIMMONITORT_APP_STARTUP( psdShmimMonitorT );
    SHMIMMONITORT_APP_STARTUP( freqShmimMonitorT );
    SHMIMMONITORT_APP_STARTUP( gainFactShmimMonitorT );
    SHMIMMONITORT_APP_STARTUP( multFactShmimMonitorT );
    SHMIMMONITORT_APP_STARTUP( pcGainFactShmimMonitorT );
    SHMIMMONITORT_APP_STARTUP( pcMultFactShmimMonitorT );
    SHMIMMONITORT_APP_STARTUP( numpccoeffShmimMonitorT );
    SHMIMMONITORT_APP_STARTUP( acoeffShmimMonitorT );
    SHMIMMONITORT_APP_STARTUP( bcoeffShmimMonitorT );
    SHMIMMONITORT_APP_STARTUP( gainCalShmimMonitorT );
    SHMIMMONITORT_APP_STARTUP( gainCalFactShmimMonitorT );
    SHMIMMONITORT_APP_STARTUP( tauShmimMonitorT );
    SHMIMMONITORT_APP_STARTUP( noiseShmimMonitorT );
    SHMIMMONITORT_APP_STARTUP( wfsavgShmimMonitorT );
    SHMIMMONITORT_APP_STARTUP( wfsmaskShmimMonitorT );

    CREATE_REG_INDI_NEW_TOGGLESWITCH( m_indiP_autoUpdate, "update_auto" );
    CREATE_REG_INDI_NEW_REQUESTSWITCH( m_indiP_updateOnce, "update_once" );
    CREATE_REG_INDI_NEW_REQUESTSWITCH( m_indiP_dump, "update_dump" );

    CREATE_REG_INDI_NEW_NUMBERF(
        m_indiP_opticalGain, "opticalGain", 0, 1, 0.01, "%0.01f", "Optical Gain", "Gain Opt." );
    CREATE_REG_INDI_NEW_NUMBERF( m_indiP_gainGain, "gainGain", 0, 1, 0.01, "%0.01f", "Gain Gain", "Gain Opt." );

    REG_INDI_SETPROP( m_indiP_emg, m_wfsDevice, "emgain" );
    REG_INDI_SETPROP( m_indiP_psdTime, m_psdDevice, "psdTime" );
    REG_INDI_SETPROP( m_indiP_psdAvgTime, m_psdDevice, "psdAvgTime" );
    REG_INDI_SETPROP( m_indiP_loop, m_loopName, "loop_state" );
    REG_INDI_SETPROP( m_indiP_siGain, m_loopName, "loop_gain" );
    REG_INDI_SETPROP( m_indiP_siMult, m_loopName, "loop_multcoeff" );
    REG_INDI_SETPROP( m_indiP_pcGain, m_loopName, "loop_pcgain" );
    REG_INDI_SETPROP( m_indiP_pcMult, m_loopName, "loop_pcmultcoeff" );
    REG_INDI_SETPROP( m_indiP_pcOn, m_loopName, "loop_pcOn" );

    if( sem_init( &m_goptSemaphore, 0, 0 ) < 0 )
    {
        return log<software_critical, -1>( { __FILE__, __LINE__, errno, 0, "Initializing gopt semaphore" } );
    }

    XWCAPP_THREAD_START( m_goptThread,
                         m_goptThreadInit,
                         m_goptThreadID,
                         m_goptThreadProp,
                         m_goptThreadPrio,
                         m_goptThreadCpuset,
                         "gainopt",
                         goptThreadStart );

    state( stateCodes::OPERATING );
    return 0;
}

int modalGainOpt::appLogic()
{
    SHMIMMONITORT_APP_LOGIC( psdShmimMonitorT );
    SHMIMMONITORT_APP_LOGIC( freqShmimMonitorT );
    SHMIMMONITORT_APP_LOGIC( gainFactShmimMonitorT );
    SHMIMMONITORT_APP_LOGIC( multFactShmimMonitorT );
    SHMIMMONITORT_APP_LOGIC( pcGainFactShmimMonitorT );
    SHMIMMONITORT_APP_LOGIC( pcMultFactShmimMonitorT );
    SHMIMMONITORT_APP_LOGIC( numpccoeffShmimMonitorT );
    SHMIMMONITORT_APP_LOGIC( acoeffShmimMonitorT );
    SHMIMMONITORT_APP_LOGIC( bcoeffShmimMonitorT );
    SHMIMMONITORT_APP_LOGIC( gainCalShmimMonitorT );
    SHMIMMONITORT_APP_LOGIC( gainCalFactShmimMonitorT );
    SHMIMMONITORT_APP_LOGIC( tauShmimMonitorT );
    SHMIMMONITORT_APP_LOGIC( noiseShmimMonitorT );
    SHMIMMONITORT_APP_LOGIC( wfsavgShmimMonitorT );
    SHMIMMONITORT_APP_LOGIC( wfsmaskShmimMonitorT );

    XWCAPP_THREAD_CHECK( m_goptThread, "gainopt" );

    SHMIMMONITORT_UPDATE_INDI( psdShmimMonitorT );
    SHMIMMONITORT_UPDATE_INDI( freqShmimMonitorT );
    SHMIMMONITORT_UPDATE_INDI( gainFactShmimMonitorT );
    SHMIMMONITORT_UPDATE_INDI( multFactShmimMonitorT );
    SHMIMMONITORT_UPDATE_INDI( pcGainFactShmimMonitorT );
    SHMIMMONITORT_UPDATE_INDI( pcMultFactShmimMonitorT );
    SHMIMMONITORT_UPDATE_INDI( numpccoeffShmimMonitorT );
    SHMIMMONITORT_UPDATE_INDI( acoeffShmimMonitorT );
    SHMIMMONITORT_UPDATE_INDI( bcoeffShmimMonitorT );
    SHMIMMONITORT_UPDATE_INDI( gainCalShmimMonitorT );
    SHMIMMONITORT_UPDATE_INDI( gainCalFactShmimMonitorT );
    SHMIMMONITORT_UPDATE_INDI( tauShmimMonitorT );
    SHMIMMONITORT_UPDATE_INDI( noiseShmimMonitorT );
    SHMIMMONITORT_UPDATE_INDI( wfsavgShmimMonitorT );
    SHMIMMONITORT_UPDATE_INDI( wfsmaskShmimMonitorT );

    if( m_autoUpdate )
    {
        updateSwitchIfChanged( m_indiP_autoUpdate, "toggle", pcf::IndiElement::On, INDI_OK );
    }
    else
    {
        updateSwitchIfChanged( m_indiP_autoUpdate, "toggle", pcf::IndiElement::Off, INDI_IDLE );
    }

    if( m_updateOnce )
    {
        updateSwitchIfChanged( m_indiP_updateOnce, "request", pcf::IndiElement::On, INDI_OK );
    }
    else
    {
        updateSwitchIfChanged( m_indiP_updateOnce, "request", pcf::IndiElement::Off, INDI_IDLE );
    }

    if( m_dump )
    {
        updateSwitchIfChanged( m_indiP_dump, "request", pcf::IndiElement::On, INDI_OK );
    }
    else
    {
        updateSwitchIfChanged( m_indiP_dump, "request", pcf::IndiElement::Off, INDI_IDLE );
    }

    updatesIfChanged<float>( m_indiP_opticalGain, { "current", "target" }, { m_opticalGain, m_opticalGain } );

    updatesIfChanged<float>( m_indiP_gainGain, { "current", "target" }, { m_gainGain, m_gainGain } );

    return 0;
}

int modalGainOpt::appShutdown()
{
    XWCAPP_THREAD_STOP( m_goptThread );

    SHMIMMONITORT_APP_SHUTDOWN( psdShmimMonitorT );
    SHMIMMONITORT_APP_SHUTDOWN( freqShmimMonitorT );
    SHMIMMONITORT_APP_SHUTDOWN( gainFactShmimMonitorT );
    SHMIMMONITORT_APP_SHUTDOWN( multFactShmimMonitorT );
    SHMIMMONITORT_APP_SHUTDOWN( pcGainFactShmimMonitorT );
    SHMIMMONITORT_APP_SHUTDOWN( pcMultFactShmimMonitorT );
    SHMIMMONITORT_APP_SHUTDOWN( numpccoeffShmimMonitorT );
    SHMIMMONITORT_APP_SHUTDOWN( acoeffShmimMonitorT );
    SHMIMMONITORT_APP_SHUTDOWN( bcoeffShmimMonitorT );
    SHMIMMONITORT_APP_SHUTDOWN( gainCalShmimMonitorT );
    SHMIMMONITORT_APP_SHUTDOWN( gainCalFactShmimMonitorT );
    SHMIMMONITORT_APP_SHUTDOWN( tauShmimMonitorT );
    SHMIMMONITORT_APP_SHUTDOWN( noiseShmimMonitorT );
    SHMIMMONITORT_APP_SHUTDOWN( wfsavgShmimMonitorT );
    SHMIMMONITORT_APP_SHUTDOWN( wfsmaskShmimMonitorT );

    if( m_olPSDStream != nullptr )
    {
        ImageStreamIO_destroyIm( m_olPSDStream );
        free( m_olPSDStream );
        m_olPSDStream = nullptr;

        ImageStreamIO_destroyIm( m_noisePSDStream );
        free( m_noisePSDStream );
        m_noisePSDStream = nullptr;

        ImageStreamIO_destroyIm( m_clXferCurrentStream );
        free( m_clXferCurrentStream );
        m_clXferCurrentStream = nullptr;

        ImageStreamIO_destroyIm( m_clXferSIStream );
        free( m_clXferSIStream );
        m_clXferSIStream = nullptr;

        ImageStreamIO_destroyIm( m_clXferLPStream );
        free( m_clXferLPStream );
        m_clXferLPStream = nullptr;
    }

    if( m_optGainStream != nullptr )
    {
        ImageStreamIO_destroyIm( m_optGainStream );
        free( m_optGainStream );
        m_optGainStream = nullptr;

        ImageStreamIO_destroyIm( m_optGainSIStream );
        free( m_optGainSIStream );
        m_optGainSIStream = nullptr;

        ImageStreamIO_destroyIm( m_optGainLPStream );
        free( m_optGainLPStream );
        m_optGainLPStream = nullptr;

        ImageStreamIO_destroyIm( m_modevarStream );
        free( m_modevarStream );
        m_modevarStream = nullptr;
    }

    return 0;
}

int modalGainOpt::allocate( const psdShmimT &dummy )
{
    static_cast<void>( dummy );

    m_updating = true;
    std::lock_guard<std::mutex> lock( m_goptMutex );
    m_updating = true;

    m_clPSDs.resize( psdShmimMonitorT::m_width, psdShmimMonitorT::m_height );
    m_clXferCurrent.resize( psdShmimMonitorT::m_width, psdShmimMonitorT::m_height );
    m_clXferSI.resize( psdShmimMonitorT::m_width, psdShmimMonitorT::m_height );
    m_clXferLP.resize( psdShmimMonitorT::m_width, psdShmimMonitorT::m_height );

    m_olPSDs.resize( psdShmimMonitorT::m_height );
    m_nPSDs.resize( psdShmimMonitorT::m_height );

    for( size_t n = 0; n < m_olPSDs.size(); ++n )
    {
        m_olPSDs[n].resize( psdShmimMonitorT::m_width );
        m_nPSDs[n].resize( psdShmimMonitorT::m_width );
    }

    m_modeVarOL.resize( psdShmimMonitorT::m_height );

    m_optGainSI.resize( psdShmimMonitorT::m_height );
    m_modeVarSI.resize( psdShmimMonitorT::m_height );

    m_optGainLP.resize( psdShmimMonitorT::m_height );
    m_modeVarLP.resize( psdShmimMonitorT::m_height );

    if( m_olPSDStream != nullptr && ( m_olPSDStream->md->size[0] != psdShmimMonitorT::m_width ||
                                      m_olPSDStream->md->size[1] != psdShmimMonitorT::m_height ) )
    {
        ImageStreamIO_destroyIm( m_olPSDStream );
        free( m_olPSDStream );
        m_olPSDStream = nullptr;

        ImageStreamIO_destroyIm( m_noisePSDStream );
        free( m_noisePSDStream );
        m_noisePSDStream = nullptr;

        ImageStreamIO_destroyIm( m_clXferCurrentStream );
        free( m_clXferCurrentStream );
        m_clXferCurrentStream = nullptr;

        ImageStreamIO_destroyIm( m_clXferSIStream );
        free( m_clXferSIStream );
        m_clXferSIStream = nullptr;

        ImageStreamIO_destroyIm( m_clXferLPStream );
        free( m_clXferLPStream );
        m_clXferLPStream = nullptr;
    }

    if( m_olPSDStream == nullptr )
    {
        m_olPSDStream = static_cast<IMAGE *>( malloc( sizeof( IMAGE ) ) );
        uint32_t imsize[3];

        imsize[0] = psdShmimMonitorT::m_width;
        imsize[1] = psdShmimMonitorT::m_height;
        imsize[2] = 1;
        ImageStreamIO_createIm_gpu( m_olPSDStream,
                                    m_olPSDShmimName.c_str(),
                                    3,
                                    imsize,
                                    psdShmimMonitorT::m_dataType,
                                    -1,
                                    1,
                                    IMAGE_NB_SEMAPHORE,
                                    0,
                                    CIRCULAR_BUFFER | ZAXIS_TEMPORAL,
                                    0 );

        m_olPSDStream->md->cnt0 = 0;
        m_olPSDStream->md->cnt1 = 0;

        m_noisePSDStream = static_cast<IMAGE *>( malloc( sizeof( IMAGE ) ) );

        imsize[0] = 1;
        imsize[1] = psdShmimMonitorT::m_height;
        imsize[2] = 1;
        ImageStreamIO_createIm_gpu( m_noisePSDStream,
                                    m_noisePSDShmimName.c_str(),
                                    3,
                                    imsize,
                                    psdShmimMonitorT::m_dataType,
                                    -1,
                                    1,
                                    IMAGE_NB_SEMAPHORE,
                                    0,
                                    CIRCULAR_BUFFER | ZAXIS_TEMPORAL,
                                    0 );

        m_noisePSDStream->md->cnt0 = 0;
        m_noisePSDStream->md->cnt1 = 0;

        m_clXferCurrentStream = static_cast<IMAGE *>( malloc( sizeof( IMAGE ) ) );

        imsize[0] = psdShmimMonitorT::m_width;
        imsize[1] = psdShmimMonitorT::m_height;
        imsize[2] = 1;
        ImageStreamIO_createIm_gpu( m_clXferCurrentStream,
                                    m_clXferCurrentShmimName.c_str(),
                                    3,
                                    imsize,
                                    psdShmimMonitorT::m_dataType,
                                    -1,
                                    1,
                                    IMAGE_NB_SEMAPHORE,
                                    0,
                                    CIRCULAR_BUFFER | ZAXIS_TEMPORAL,
                                    0 );

        m_clXferCurrentStream->md->cnt0 = 0;
        m_clXferCurrentStream->md->cnt1 = 0;

        m_clXferSIStream = static_cast<IMAGE *>( malloc( sizeof( IMAGE ) ) );

        ImageStreamIO_createIm_gpu( m_clXferSIStream,
                                    m_clXferSIShmimName.c_str(),
                                    3,
                                    imsize,
                                    psdShmimMonitorT::m_dataType,
                                    -1,
                                    1,
                                    IMAGE_NB_SEMAPHORE,
                                    0,
                                    CIRCULAR_BUFFER | ZAXIS_TEMPORAL,
                                    0 );

        m_clXferSIStream->md->cnt0 = 0;
        m_clXferSIStream->md->cnt1 = 0;

        m_clXferLPStream = static_cast<IMAGE *>( malloc( sizeof( IMAGE ) ) );

        ImageStreamIO_createIm_gpu( m_clXferLPStream,
                                    m_clXferLPShmimName.c_str(),
                                    3,
                                    imsize,
                                    psdShmimMonitorT::m_dataType,
                                    -1,
                                    1,
                                    IMAGE_NB_SEMAPHORE,
                                    0,
                                    CIRCULAR_BUFFER | ZAXIS_TEMPORAL,
                                    0 );

        m_clXferLPStream->md->cnt0 = 0;
        m_clXferLPStream->md->cnt1 = 0;
    }

    if( m_optGainStream != nullptr &&
        ( m_optGainStream->md->size[0] != psdShmimMonitorT::m_height || m_optGainStream->md->size[1] != 1 ) )
    {
        ImageStreamIO_destroyIm( m_optGainStream );
        free( m_optGainStream );
        m_optGainStream = nullptr;

        ImageStreamIO_destroyIm( m_optGainSIStream );
        free( m_optGainSIStream );
        m_optGainSIStream = nullptr;

        ImageStreamIO_destroyIm( m_optGainSIStream );
        free( m_optGainSIStream );
        m_optGainSIStream = nullptr;
    }

    if( m_optGainStream == nullptr )
    {
        m_optGainStream = static_cast<IMAGE *>( malloc( sizeof( IMAGE ) ) );
        uint32_t imsize[3];

        imsize[0] = psdShmimMonitorT::m_height;
        imsize[1] = 1;
        imsize[2] = 1;
        ImageStreamIO_createIm_gpu( m_optGainStream,
                                    m_optGainShmimName.c_str(),
                                    3,
                                    imsize,
                                    psdShmimMonitorT::m_dataType,
                                    -1,
                                    1,
                                    IMAGE_NB_SEMAPHORE,
                                    0,
                                    CIRCULAR_BUFFER | ZAXIS_TEMPORAL,
                                    0 );

        m_optGainStream->md->cnt0 = 0;
        m_optGainStream->md->cnt1 = 0;

        m_optGainSIStream = static_cast<IMAGE *>( malloc( sizeof( IMAGE ) ) );

        ImageStreamIO_createIm_gpu( m_optGainSIStream,
                                    m_optGainSIShmimName.c_str(),
                                    3,
                                    imsize,
                                    psdShmimMonitorT::m_dataType,
                                    -1,
                                    1,
                                    IMAGE_NB_SEMAPHORE,
                                    0,
                                    CIRCULAR_BUFFER | ZAXIS_TEMPORAL,
                                    0 );

        m_optGainSIStream->md->cnt0 = 0;
        m_optGainSIStream->md->cnt1 = 0;

        m_optGainLPStream = static_cast<IMAGE *>( malloc( sizeof( IMAGE ) ) );

        ImageStreamIO_createIm_gpu( m_optGainLPStream,
                                    m_optGainLPShmimName.c_str(),
                                    3,
                                    imsize,
                                    psdShmimMonitorT::m_dataType,
                                    -1,
                                    1,
                                    IMAGE_NB_SEMAPHORE,
                                    0,
                                    CIRCULAR_BUFFER | ZAXIS_TEMPORAL,
                                    0 );

        m_optGainLPStream->md->cnt0 = 0;
        m_optGainLPStream->md->cnt1 = 0;

        m_modevarStream = static_cast<IMAGE *>( malloc( sizeof( IMAGE ) ) );

        imsize[0] = 3;
        imsize[1] = psdShmimMonitorT::m_height;
        imsize[2] = 1;

        ImageStreamIO_createIm_gpu( m_modevarStream,
                                    m_modevarShmimName.c_str(),
                                    3,
                                    imsize,
                                    psdShmimMonitorT::m_dataType,
                                    -1,
                                    1,
                                    IMAGE_NB_SEMAPHORE,
                                    0,
                                    CIRCULAR_BUFFER | ZAXIS_TEMPORAL,
                                    0 );

        m_modevarStream->md->cnt0 = 0;
        m_modevarStream->md->cnt1 = 0;
    }

    m_sinceChange = -1;

    m_updating = false;
    return 0;
}

int modalGainOpt::processImage( void *curr_src, const psdShmimT &dummy )
{
    static_cast<void>( dummy );

    ++m_sinceChange;

    if( m_psdAvgTime <= 0 || m_psdTime <= 0 ) // Safety check, shouldn't happen but means we need to wait.
    {
        return 0;
    }

    int deadTime = ( m_psdAvgTime / m_psdTime ) / m_psdOverlapFraction;

    if( m_sinceChange < deadTime )
    {
        return 0;
    }

    // Here we would update psds, but don't do that if we're in the middle of calculating
    std::unique_lock<std::mutex> lock( m_goptMutex, std::try_to_lock );
    if( !lock.owns_lock() )
    {
        ///\todo update a frame-missed counter
        return 0;
    }

    m_updating = true;

    m_clPSDs = Eigen::Map<Eigen::Array<float, -1, -1>>(
        static_cast<float *>( curr_src ), psdShmimMonitorT::m_width, psdShmimMonitorT::m_height );

    m_updating = false;

    lock.unlock();

    if( sem_post( &m_goptSemaphore ) < 0 )
    {
        return log<software_critical, -1>( { __FILE__, __LINE__, errno, 0, "Error posting to semaphore" } );
    }

    return 0;
}

int modalGainOpt::allocate( const freqShmimT &dummy )
{
    static_cast<void>( dummy );

    return 0;
}

int modalGainOpt::processImage( void *curr_src, const freqShmimT &dummy )
{
    static_cast<void>( dummy );

    if( freqShmimMonitorT::m_width != 1 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "got freq with width not 1" } );
    }

    bool change = false;

    float *f = static_cast<float *>( curr_src );

    size_t sz = freqShmimMonitorT::m_height;

    if( sz != m_freq.size() )
    {
        change = true;
    }

    if( !change ) // f is same size
    {
        for( size_t n = 0; n < sz; ++n )
        {
            if( f[n] != m_freq[n] )
            {
                change = true;
                break;
            }
        }
    }

    if( change )
    {
        m_updating = true;
        std::lock_guard<std::mutex> lock( m_goptMutex );
        m_updating = true; // Make sure it didn't get set to false by thread that had the lock

        m_freq.resize( sz );

        for( size_t n = 0; n < sz; ++n )
        {
            m_freq[n] = f[n];
        }

        m_fps = 2 * m_freq.back();

        m_sinceChange = -1;
        m_goptUpdated = true;
        m_freqUpdated = true;

        m_updating = false;
        std::cerr << "got freq: " << sz << '\n';
        std::cerr << "     fps: " << m_fps << '\n';
    }

    return 0;
}

int modalGainOpt::allocate( const gainFactShmimT &dummy )
{
    static_cast<void>( dummy );

    if( gainFactShmimMonitorT::m_height != 1 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "got gains with height not 1" } );
    }

    return 0;
}

int modalGainOpt::processImage( void *curr_src, const gainFactShmimT &dummy )
{
    static_cast<void>( dummy );

    bool change = false;

    uint32_t w = gainFactShmimMonitorT::m_width;

    std::unique_lock<std::mutex> lock( m_goptMutex, std::defer_lock );

    if( w != m_gainFacts.size() )
    {
        m_updating = true;
        lock.lock();
        m_updating = true; // Make sure it didn't get set to false by thread that had the lock

        change = true;
        m_gainFacts.resize( w );
    }

    float *g = static_cast<float *>( curr_src );

    for( uint32_t n = 0; n < w; ++n )
    {
        if( change || m_gainFacts[n] != g[n] )
        {
            if( !change )
            {
                m_updating = true;
                lock.lock();
                m_updating = true; // Make sure it didn't get set to false by thread that had the lock
                change     = true;
            }

            m_gainFacts[n] = g[n];
        }
    }

    if( change )
    {
        if( m_loop )
        {
            m_sinceChange = -1;
        }

        m_updating = false;
        lock.unlock();
        std::cerr << "got gains: " << m_gainFacts.size() << "\n";
    }

    return 0;
}

int modalGainOpt::allocate( const multFactShmimT &dummy )
{
    static_cast<void>( dummy );

    if( multFactShmimMonitorT::m_height != 1 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "got multcoeffs with height not 1" } );
    }

    return 0;
}

int modalGainOpt::processImage( void *curr_src, const multFactShmimT &dummy )
{
    static_cast<void>( dummy );

    bool change = false;

    uint32_t w = multFactShmimMonitorT::m_width;

    std::unique_lock<std::mutex> lock( m_goptMutex, std::defer_lock );

    if( w != m_multFacts.size() )
    {
        m_updating = true;
        lock.lock();
        m_updating = true; // Make sure it didn't get set to false by thread that had the lock

        change = true;
        m_multFacts.resize( w );
    }

    float *m = static_cast<float *>( curr_src );

    for( uint32_t n = 0; n < w; ++n )
    {
        if( change || m_multFacts[n] != m[n] )
        {
            if( !change )
            {
                m_updating = true;
                lock.lock();
                m_updating = true; // Make sure it didn't get set to false by thread that had the lock

                change = true;
            }

            m_multFacts[n] = m[n];
        }
    }

    if( change )
    {
        if( m_loop )
        {
            m_sinceChange = -1;
        }

        m_updating    = false;
        m_goptUpdated = true;

        lock.unlock();
        std::cerr << "got mcs: " << m_multFacts.size() << "\n";
    }

    return 0;
}

int modalGainOpt::allocate( const pcGainFactShmimT &dummy )
{
    static_cast<void>( dummy );

    if( pcGainFactShmimMonitorT::m_height != 1 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "got pc gains with height not 1" } );
    }

    return 0;
}

int modalGainOpt::processImage( void *curr_src, const pcGainFactShmimT &dummy )
{
    static_cast<void>( dummy );

    bool change = false;

    uint32_t w = pcGainFactShmimMonitorT::m_width;

    std::unique_lock<std::mutex> lock( m_goptMutex, std::defer_lock );

    if( w != m_pcGainFacts.size() )
    {
        m_updating = true;
        lock.lock();
        m_updating = true; // Make sure it didn't get set to false by thread that had the lock

        change = true;
        m_pcGainFacts.resize( w );
    }

    float *g = static_cast<float *>( curr_src );

    for( uint32_t n = 0; n < w; ++n )
    {
        if( change || m_pcGainFacts[n] != g[n] )
        {
            if( !change )
            {
                m_updating = true;
                lock.lock();
                m_updating = true; // Make sure it didn't get set to false by thread that had the lock
                change     = true;
            }

            m_pcGainFacts[n] = g[n];
        }
    }

    if( change )
    {
        if( m_loop )
        {
            m_sinceChange = -1;
        }

        m_updating = false;
        lock.unlock();
        std::cerr << "got pc gains: " << m_pcGainFacts.size() << "\n";
    }

    return 0;
}

int modalGainOpt::allocate( const pcMultFactShmimT &dummy )
{
    static_cast<void>( dummy );

    if( pcMultFactShmimMonitorT::m_height != 1 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "got pcMultcoeffs with height not 1" } );
    }

    return 0;
}

int modalGainOpt::processImage( void *curr_src, const pcMultFactShmimT &dummy )
{
    static_cast<void>( dummy );

    bool change = false;

    uint32_t w = pcMultFactShmimMonitorT::m_width;

    std::unique_lock<std::mutex> lock( m_goptMutex, std::defer_lock );

    if( w != m_pcMultFacts.size() )
    {
        m_updating = true;
        lock.lock();
        m_updating = true; // Make sure it didn't get set to false by thread that had the lock

        change = true;
        m_pcMultFacts.resize( w );
    }

    float *m = static_cast<float *>( curr_src );

    for( uint32_t n = 0; n < w; ++n )
    {
        if( change || m_pcMultFacts[n] != m[n] )
        {
            if( !change )
            {
                m_updating = true;
                lock.lock();
                m_updating = true; // Make sure it didn't get set to false by thread that had the lock

                change = true;
            }

            m_pcMultFacts[n] = m[n];
        }
    }

    if( change )
    {
        if( m_loop )
        {
            m_sinceChange = -1;
        }

        m_updating      = false;
        m_pcgoptUpdated = true;

        lock.unlock();
        std::cerr << "got mcs: " << m_multFacts.size() << "\n";
    }

    return 0;
}

int modalGainOpt::allocate( const numpccoeffShmimT &dummy )
{
    static_cast<void>( dummy );

    if( numpccoeffShmimMonitorT::m_height != 2 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "got numpccoeff's with height not 2" } );
    }

    return 0;
}

int modalGainOpt::processImage( void *curr_src, const numpccoeffShmimT &dummy )
{
    static_cast<void>( dummy );

    bool change = false;

    uint32_t w = numpccoeffShmimMonitorT::m_width;

    std::unique_lock<std::mutex> lock( m_goptMutex, std::defer_lock );

    if( w != m_Na.size() || w != m_Nb.size() || w != m_regCounter.size() || w != m_regScale.size() )
    {
        m_updating = true;
        lock.lock();
        m_updating = true; // Make sure it didn't get set to false by thread that had the lock

        change = true;
        m_Na.resize( w );
        m_Nb.resize( w );

        m_regCounter.resize( w );

        // Initialize the regCounter
        int nc = 0;

        for( size_t n = 0; n < m_regCounter.size(); ++n )
        {
            m_regCounter[n] = nc;
            ++nc;

            if( nc >= m_nRegCycles )
            {
                nc = 0;
            }
        }

        m_regScale.resize( w, -999 );
        m_gmaxLP.resize( w, 0 );
    }

    mx::improc::eigenMap<int> Npc( reinterpret_cast<int *>( curr_src ), w, 2 );

    for( uint32_t n = 0; n < w; ++n )
    {
        if( change || m_Na[n] != Npc( n, 0 ) || m_Nb[n] != Npc( n, 1 ) )
        {
            if( !change )
            {
                m_updating = true;
                lock.lock();
                m_updating = true; // Make sure it didn't get set to false by thread that had the lock

                change = true;
            }

            m_Na[n] = Npc( n, 0 );
            m_Nb[n] = Npc( n, 1 );
        }
    }

    if( change )
    {
        if( m_loop )
        {
            m_sinceChange = -1;
        }

        m_updating    = false;
        m_goptUpdated = true;

        lock.unlock();
        std::cerr << "got num pc coeffs: " << m_Na.size() << "\n";
    }

    return 0;
    return 0;
}

int modalGainOpt::allocate( const acoeffShmimT &dummy )
{
    static_cast<void>( dummy );

    return 0;
}

int modalGainOpt::processImage( void *curr_src, const acoeffShmimT &dummy )
{
    static_cast<void>( dummy );

    bool change = false;

    std::unique_lock<std::mutex> lock( m_goptMutex, std::defer_lock );

    uint32_t w = acoeffShmimMonitorT::m_width;
    uint32_t h = acoeffShmimMonitorT::m_height;

    // If there's a size change we lock
    if( w - 1 != m_as.rows() || h != m_as.cols() || h != m_NaCurrent.size() )
    {
        m_updating = true;
        lock.lock();
        m_updating = true; // Make sure it didn't get set to false by thread that had the lock

        change = true;
        m_NaCurrent.resize( h );
        m_as.resize( w - 1, h );
    }

    eigenMap<float> ac( reinterpret_cast<float *>( curr_src ), w, h );

    for( uint32_t cc = 0; cc < h; ++cc )
    {
        if( change || m_NaCurrent[cc] != ac( 0, cc ) )
        {
            if( !change )
            {
                m_updating = true;
                lock.lock();
                m_updating = true; // Make sure it didn't get set to false by thread that had the lock

                change = true;
            }

            m_NaCurrent[cc] = ac( 0, cc );
        }

        for( uint32_t rr = 1; rr < w; ++rr )
        {
            if( change || m_as( rr - 1, cc ) != ac( rr, cc ) )
            {
                if( !change )
                {
                    m_updating = true;
                    lock.lock();
                    m_updating = true; // Make sure it didn't get set to false by thread that had the lock

                    change = true;
                }

                m_as( rr - 1, cc ) = ac( rr, cc );
            }
        }
    }

    if( change )
    {
        if( m_loop && m_pcOn )
        {
            m_sinceChange = -1;
        }

        m_updating      = false;
        m_pcgoptUpdated = true;

        lock.unlock();
        std::cerr << "got a coeffs: " << w << ' ' << h << ' ' << m_NaCurrent.size() << "\n";
    }

    return 0;
}

int modalGainOpt::allocate( const bcoeffShmimT &dummy )
{
    static_cast<void>( dummy );

    return 0;
}

int modalGainOpt::processImage( void *curr_src, const bcoeffShmimT &dummy )
{
    static_cast<void>( dummy );

    bool change = false;

    std::unique_lock<std::mutex> lock( m_goptMutex, std::defer_lock );

    uint32_t w = bcoeffShmimMonitorT::m_width;
    uint32_t h = bcoeffShmimMonitorT::m_height;

    // If there's a size change we lock
    if( w - 1 != m_bs.rows() || h != m_bs.cols() || h != m_NbCurrent.size() )
    {
        m_updating = true;
        lock.lock();
        m_updating = true; // Make sure it didn't get set to false by thread that had the lock

        change = true;

        m_NbCurrent.resize( h );
        m_bs.resize( w - 1, h );
    }

    eigenMap<float> bc( reinterpret_cast<float *>( curr_src ), w, h );

    for( uint32_t cc = 0; cc < h; ++cc )
    {
        if( change || m_NbCurrent[cc] != bc( 0, cc ) )
        {
            if( !change )
            {
                m_updating = true;
                lock.lock();
                m_updating = true; // Make sure it didn't get set to false by thread that had the lock

                change = true;
            }

            m_NbCurrent[cc] = bc( 0, cc );
        }

        for( uint32_t rr = 1; rr < w; ++rr )
        {
            if( change || m_bs( rr - 1, cc ) != bc( rr, cc ) )
            {
                if( !change )
                {
                    m_updating = true;
                    lock.lock();
                    m_updating = true; // Make sure it didn't get set to false by thread that had the lock

                    change = true;
                }

                m_bs( rr - 1, cc ) = bc( rr, cc );
            }
        }
    }

    if( change )
    {
        if( m_loop && m_pcOn )
        {
            m_sinceChange = -1;
        }

        m_updating      = false;
        m_pcgoptUpdated = true;

        lock.unlock();
        std::cerr << "got b coeffs: " << m_NbCurrent.size() << "\n";
    }

    return 0;
}

int modalGainOpt::allocate( const gainCalShmimT &dummy )
{
    static_cast<void>( dummy );

    return 0;
}

int modalGainOpt::processImage( void *curr_src, const gainCalShmimT &dummy )
{
    static_cast<void>( dummy );

    if( gainCalShmimMonitorT::m_height != 1 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "got gainCals with height not 1" } );
    }

    bool change = false;

    uint32_t w = gainCalShmimMonitorT::m_width;

    std::unique_lock<std::mutex> lock( m_goptMutex, std::defer_lock );

    if( w != m_gainCals.size() )
    {
        m_updating = true;
        lock.lock();
        m_updating = true; // Make sure it didn't get set to false by thread that had the lock

        change = true;
        m_gainCals.resize( w );
    }

    float *g = static_cast<float *>( curr_src );

    for( uint32_t n = 0; n < w; ++n )
    {
        if( change || m_gainCals[n] != g[n] )
        {
            if( !change )
            {
                m_updating = true;
                lock.lock();
                m_updating = true; // Make sure it didn't get set to false by thread that had the lock

                change = true;
            }

            m_gainCals[n] = g[n];
        }
    }

    if( change )
    {
        m_sinceChange = -1;
        m_updating    = false;
        lock.unlock();
        std::cerr << "got gainCals: " << m_gainCals.size() << "\n";
    }

    return 0;
}

int modalGainOpt::allocate( const gainCalFactShmimT &dummy )
{
    static_cast<void>( dummy );

    return 0;
}

int modalGainOpt::processImage( void *curr_src, const gainCalFactShmimT &dummy )
{
    static_cast<void>( dummy );

    if( gainCalFactShmimMonitorT::m_height != 1 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "got gainCalFacts with height not 1" } );
    }

    bool change = false;

    uint32_t w = gainCalFactShmimMonitorT::m_width;

    std::unique_lock<std::mutex> lock( m_goptMutex, std::defer_lock );

    if( w != m_gainCalFacts.size() )
    {
        m_updating = true;
        lock.lock();
        m_updating = true; // Make sure it didn't get set to false by thread that had the lock

        change = true;
        m_gainCalFacts.resize( w );
    }

    float *g = static_cast<float *>( curr_src );

    for( uint32_t n = 0; n < w; ++n )
    {
        if( change || m_gainCalFacts[n] != g[n] )
        {
            if( !change )
            {
                m_updating = true;
                lock.lock();
                m_updating = true; // Make sure it didn't get set to false by thread that had the lock

                change = true;
            }

            m_gainCalFacts[n] = g[n];
        }
    }

    if( change )
    {
        m_sinceChange = -1;
        m_updating    = false;
        lock.unlock();
        std::cerr << "got gainCalsFacts: " << m_gainCalFacts.size() << "\n";
    }

    return 0;
}

int modalGainOpt::allocate( const tauShmimT &dummy )
{
    static_cast<void>( dummy );

    return 0;
}

int modalGainOpt::processImage( void *curr_src, const tauShmimT &dummy )
{
    static_cast<void>( dummy );

    if( tauShmimMonitorT::m_height != 1 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "got tau with height not 1" } );
    }

    bool change = false;

    uint32_t w = tauShmimMonitorT::m_width;

    std::unique_lock<std::mutex> lock( m_goptMutex, std::defer_lock );

    if( w != m_taus.size() )
    {
        m_updating = true;
        lock.lock();
        m_updating = true; // Make sure it didn't get set to false by thread that had the lock

        change = true;
        m_taus.resize( w );
    }

    float *t = static_cast<float *>( curr_src );

    for( uint32_t n = 0; n < w; ++n )
    {
        if( change || m_taus[n] != t[n] )
        {
            if( !change )
            {
                m_updating = true;
                lock.lock();
                m_updating = true; // Make sure it didn't get set to false by thread that had the lock

                change = true;
            }

            m_taus[n] = t[n];
        }
    }

    if( change )
    {
        m_sinceChange = -1;
        m_updating    = false;
        m_goptUpdated = true;
        lock.unlock();
        std::cerr << "got taus: " << m_taus.size() << "\n";
    }
    return 0;
}

int modalGainOpt::allocate( const noiseShmimT &dummy )
{
    static_cast<void>( dummy );

    return 0;
}

int modalGainOpt::processImage( void *curr_src, const noiseShmimT &dummy )
{
    static_cast<void>( dummy );

    if( noiseShmimMonitorT::m_width != 3 )
    {
        return log<software_error, -1>( { __FILE__, __LINE__, "got tau with width not 3" } );
    }

    bool change = false;

    uint32_t h = noiseShmimMonitorT::m_height;

    std::unique_lock<std::mutex> lock( m_goptMutex, std::defer_lock );

    if( h != m_noiseParams.cols() )
    {
        m_updating = true;
        lock.lock();
        m_updating = true; // Make sure it didn't get set to false by thread that had the lock

        change = true;
        m_noiseParams.resize( 3, h );
    }

    mx::improc::eigenMap<float> np( static_cast<float *>( curr_src ), 3, h );

    for( uint32_t n = 0; n < h; ++n )
    {
        if( change || m_noiseParams( 0, n ) != np( 0, n ) || m_noiseParams( 1, n ) != np( 1, n ) ||
            m_noiseParams( 2, n ) != np( 2, n ) )
        {
            if( !change )
            {
                m_updating = true;
                lock.lock();
                m_updating = true; // Make sure it didn't get set to false by thread that had the lock

                change = true;
            }

            m_noiseParams.col( n ) = np.col( n );
        }
    }

    if( change )
    {
        m_sinceChange = -1;
        m_updating    = false;
        m_goptUpdated = true;
        lock.unlock();
        std::cerr << "got noise params: " << m_noiseParams.rows() << " x " << m_noiseParams.cols() << "\n";
    }
    return 0;
}

int modalGainOpt::allocate( const wfsavgShmimT &dummy )
{
    static_cast<void>( dummy );

    std::cerr << "Got WFS avg: " << wfsavgShmimMonitorT::m_width << " x " << wfsavgShmimMonitorT::m_height << '\n';
    return 0;
}

int modalGainOpt::processImage( void *curr_src, const wfsavgShmimT &dummy )
{
    static_cast<void>( dummy );

    m_wfsavg = mx::improc::eigenMap<float>(
        reinterpret_cast<float *>( curr_src ), wfsavgShmimMonitorT::m_width, wfsavgShmimMonitorT::m_height );

    if( m_wfsavg.rows() == m_wfsmask.rows() && m_wfsavg.cols() == m_wfsmask.cols() )
    {
        m_counts = ( m_wfsavg * m_wfsmask ).sum();

        std::cerr << "counts: " << m_counts << '\n';
    }

    return 0;
}

int modalGainOpt::allocate( const wfsmaskShmimT &dummy )
{
    static_cast<void>( dummy );

    std::cerr << "Got WFS mask: " << wfsmaskShmimMonitorT::m_width << " x " << wfsmaskShmimMonitorT::m_height << '\n';
    return 0;
}

int modalGainOpt::processImage( void *curr_src, const wfsmaskShmimT &dummy )
{
    static_cast<void>( dummy );

    m_wfsmask = mx::improc::eigenMap<float>(
        reinterpret_cast<float *>( curr_src ), wfsmaskShmimMonitorT::m_width, wfsmaskShmimMonitorT::m_height );

    m_npix = m_wfsmask.sum();

    if( m_wfsavg.rows() == m_wfsmask.rows() && m_wfsavg.cols() == m_wfsmask.cols() )
    {
        m_counts = ( m_wfsavg * m_wfsmask ).sum();
    }

    return 0;
}

float modalGainOpt::noisePSD( int n )
{
    float mc = m_noiseParams( 1, n ) * m_counts / m_emg;

    float snr2 = ( mc * mc ) / ( mc + m_npix * pow( m_noiseParams( 2, n ) / m_emg, 2 ) );

    return 0.05 * m_noiseParams( 0, n ) / snr2;
}

void modalGainOpt::goptThreadStart( modalGainOpt *p )
{
    p->goptThreadExec();
}

void modalGainOpt::goptThreadExec()
{
    m_goptThreadID = syscall( SYS_gettid );

    while( m_goptThreadInit == true && shutdown() == 0 )
    {
        sleep( 1 );
    }

    std::vector<bool> logged( 50, false );

    while( shutdown() == 0 )
    {
        timespec ts;
        XWC_SEM_WAIT_TS_RETVOID( ts, 1, 0 );

        if( sem_timedwait( &m_goptSemaphore, &ts ) == 0 )
        {
            std::lock_guard<std::mutex> lock( m_goptMutex );

            int L = 0;
            if( (size_t)m_clPSDs.rows() == 0 || m_clPSDs.cols() == 0 ) // somehow here without any data
            {
                if( !logged[L] )
                {
                    log<software_error>( { __FILE__, __LINE__, "PSDs have not been updated" } );
                }
                logged[L] = true;
                continue;
            }
            logged[L++] = false;

            if( (size_t)m_clPSDs.rows() != m_freq.size() )
            {
                if( !logged[L] )
                {
                    log<software_error>( { __FILE__, __LINE__, "PSDs and freq size mismatch" } );
                }
                logged[L] = true;
                continue;
            }
            logged[L++] = false;

            if( (size_t)m_clPSDs.cols() != m_gainFacts.size() )
            {
                if( !logged[L] )
                {
                    log<software_error>( { __FILE__, __LINE__, "PSDs and gains number of modes mismatch" } );
                }
                logged[L] = true;
                continue;
            }
            logged[L++] = false;

            if( (size_t)m_clPSDs.cols() != m_multFacts.size() )
            {
                if( !logged[L] )
                {
                    log<software_error>( { __FILE__, __LINE__, "PSDs and mult coeffs number of modes mismatch" } );
                }
                logged[L] = true;
                continue;
            }
            logged[L++] = false;

            if( (size_t)m_clPSDs.cols() != m_gainCals.size() )
            {
                if( !logged[L] )
                {
                    log<software_error>( { __FILE__, __LINE__, "PSDs and gain cals number of modes mismatch" } );
                }
                logged[L] = true;
                continue;
            }
            logged[L++] = false;

            if( (size_t)m_clPSDs.cols() != m_gainCalFacts.size() )
            {
                if( !logged[L] )
                {
                    log<software_error>( { __FILE__, __LINE__, "PSDs and gain cal facts number of modes mismatch" } );
                }
                logged[L] = true;
                continue;
            }
            logged[L++] = false;

            if( (size_t)m_clPSDs.cols() != m_taus.size() )
            {
                if( !logged[L] )
                {
                    log<software_error>( { __FILE__, __LINE__, "Loop taus have not been set" } );
                }
                logged[L] = true;
                continue;
            }
            logged[L++] = false;

            if( m_clPSDs.cols() != m_noiseParams.cols() || m_noiseParams.rows() != 3 )
            {
                if( !logged[L] )
                {
                    log<software_error>( { __FILE__, __LINE__, "noise params have not been set" } );
                }
                logged[L] = true;
                continue;
            }
            logged[L++] = false;

            if( m_fps <= 0 )
            {
                if( !logged[L] )
                {
                    log<software_error>( { __FILE__, __LINE__, "Loop fps has not been set" } );
                }
                logged[L] = true;
                continue;
            }
            logged[L++] = false;

            if( m_olPSDStream == nullptr )
            {
                if( !logged[L] )
                {
                    log<software_error>( { __FILE__, __LINE__, "m_olPSDStream is not allocated" } );
                }
                logged[L] = true;
                continue;
            }
            logged[L++] = false;

            if( m_noisePSDStream == nullptr )
            {
                if( !logged[L] )
                {
                    log<software_error>( { __FILE__, __LINE__, "m_noisePSDStream is not allocated" } );
                }
                logged[L] = true;
                continue;
            }
            logged[L++] = false;

            if( m_clXferCurrentStream == nullptr )
            {
                if( !logged[L] )
                {
                    log<software_error>( { __FILE__, __LINE__, "m_clXferCurrentStream is not allocated" } );
                }
                logged[L] = true;
                continue;
            }
            logged[L++] = false;

            if( m_clXferSIStream == nullptr )
            {
                if( !logged[L] )
                {
                    log<software_error>( { __FILE__, __LINE__, "m_clXferSIStream is not allocated" } );
                }
                logged[L] = true;
                continue;
            }
            logged[L++] = false;

            if( m_optGainStream == nullptr )
            {
                if( !logged[L] )
                {
                    log<software_error>( { __FILE__, __LINE__, "optGainsStream is not allocated" } );
                }
                logged[L] = true;
                continue;
            }
            logged[L++] = false;

            if( m_optGainSIStream == nullptr )
            {
                if( !logged[L] )
                {
                    log<software_error>( { __FILE__, __LINE__, "optGainsStream SI is not allocated" } );
                }
                logged[L] = true;
                continue;
            }
            logged[L++] = false;

            bool doPCCalcs = true;

            if( (size_t)m_clPSDs.cols() != m_Na.size() )
            {
                if( !logged[L] )
                {
                    log<software_error>( { __FILE__, __LINE__, "Na is not allocated" } );
                }
                logged[L] = true;
                doPCCalcs = false;
            }
            else
            {
                logged[L] = false;
            }
            ++L;

            if( (size_t)m_clPSDs.cols() != m_NaCurrent.size() )
            {
                if( !logged[L] )
                {
                    log<software_error>( { __FILE__, __LINE__, "NaCurrent is not allocated" } );
                }
                logged[L] = true;
                doPCCalcs = false;
            }
            else
            {
                logged[L] = false;
            }
            ++L;

            if( m_clPSDs.cols() != m_as.cols() )
            {
                if( !logged[L] )
                {
                    log<software_error>( { __FILE__, __LINE__, "a coefficients are not allocated" } );
                }
                logged[L] = true;
                doPCCalcs = false;
            }
            else
            {
                logged[L] = false;
            }
            ++L;

            if( (size_t)m_clPSDs.cols() != m_Nb.size() )
            {
                if( !logged[L] )
                {
                    log<software_error>( { __FILE__, __LINE__, "Nb is not allocated" } );
                }
                logged[L] = true;
                doPCCalcs = false;
            }
            else
            {
                logged[L] = false;
            }
            ++L;

            if( (size_t)m_clPSDs.cols() != m_NbCurrent.size() )
            {
                if( !logged[L] )
                {
                    log<software_error>( { __FILE__, __LINE__, "NbCurrent is not allocated" } );
                }
                logged[L] = true;
                doPCCalcs = false;
            }
            else
            {
                logged[L] = false;
            }
            ++L;

            if( m_clPSDs.cols() != m_bs.cols() )
            {
                if( !logged[L] )
                {
                    log<software_error>( { __FILE__, __LINE__, "b coefficients are not allocated" } );
                }
                logged[L] = true;
                doPCCalcs = false;
            }
            else
            {
                logged[L] = false;
            }
            ++L;

            if( m_clXferLPStream == nullptr || m_optGainLPStream == nullptr ||
                (size_t)m_clPSDs.cols() != m_optGainLP.size() || (size_t)m_clPSDs.cols() != m_goptLP.size() )
            {
                if( !logged[L] )
                {
                    log<software_error>( { __FILE__, __LINE__, "LP is not allocated" } );
                }
                logged[L] = true;
                doPCCalcs = false;
            }
            logged[L++] = false;

            if( m_goptUpdated || m_pcgoptUpdated || m_freqUpdated || m_goptCurrent.size() != m_gainFacts.size() )
            {
                if( m_goptCurrent.size() != m_gainFacts.size() )
                {
                    m_freqUpdated = true; // force freq update in this case
                }

                std::cerr << "updating gopt structures\n";

                m_goptCurrent.resize( m_gainFacts.size() );
                m_goptSI.resize( m_gainFacts.size() );
                m_goptLP.resize( m_gainFacts.size() );
                m_linPred.resize( m_gainFacts.size() );

                for( size_t n = 0; n < m_goptCurrent.size(); ++n )
                {
                    m_goptCurrent[n].Ti( 1.0 / m_fps );
                    m_goptCurrent[n].tau( m_taus[n] );

                    m_goptSI[n].Ti( 1.0 / m_fps );
                    m_goptSI[n].tau( m_taus[n] );

                    m_goptLP[n].Ti( 1.0 / m_fps );
                    m_goptLP[n].tau( m_taus[n] );

                    if( !m_pcOn )
                    {
                        m_goptCurrent[n].setLeakyIntegrator( m_mult * m_multFacts[n] );
                    }
                    else
                    {
                        std::vector<float> ta( m_as.rows() );
                        for( size_t m = 0; m < ta.size(); ++m )
                        {
                            ta[m] = m_as( m, n );
                        }
                        m_goptCurrent[n].a( ta );

                        std::vector<float> tb( m_bs.rows() );
                        for( size_t m = 0; m < tb.size(); ++m )
                        {
                            tb[m] = m_bs( m, n );
                        }
                        m_goptCurrent[n].b( tb );

                        m_goptCurrent[n].remember( m_pcMult * m_pcMultFacts[n] );
                    }

                    m_goptSI[n].setLeakyIntegrator( m_mult * m_multFacts[n] );

                    if( m_freqUpdated )
                    {
                        m_goptCurrent[n].f( m_freq );
                        m_goptSI[n].f( m_freq );
                        m_goptLP[n].f( m_freq );
                    }
                }

                m_goptUpdated   = false;
                m_pcgoptUpdated = false;
                m_freqUpdated   = false;

                std::cerr << "done\n";
            }

            if( m_updating )
            {
                continue;
            }

            timePointT t0 = std::chrono::steady_clock::now();

#pragma omp parallel for num_threads( 15 )
            for( size_t n = 0; n < m_goptCurrent.size(); ++n )
            {
                if( m_updating || m_shutdown )
                {
                    continue; // don't break b/c of omp
                }

                if( !m_pcOn )
                {
                    for( size_t f = 0; f < m_goptCurrent[n].f_size(); ++f )
                    {
                        m_clXferCurrent( f, n ) =
                            m_goptCurrent[n].clETF2( f, m_gain * m_gainFacts[n] * m_gainCals[n] * m_opticalGain );
                    }
                }
                else
                {
                    for( size_t f = 0; f < m_goptCurrent[n].f_size(); ++f )
                    {
                        m_clXferCurrent( f, n ) =
                            m_goptCurrent[n].clETF2( f, m_pcGain * m_pcGainFacts[n] * m_gainCals[n] * m_opticalGain );
                    }
                }

                // Calculate the Noise PSD
                float npsd = 0; // noisePSD(n);
                int   nnp  = 0;
                for( size_t f = 0.8 * m_goptCurrent[n].f_size(); f < m_goptCurrent[n].f_size(); ++f )
                {
                    npsd += log10( m_clPSDs( f, n ) );
                    ++nnp;
                }
                npsd /= nnp;
                npsd = pow( 10, npsd );

                // Calculate the OL PSD with the current gopt (PC or SI)
                float og2 = m_opticalGain * m_opticalGain;
                if( !m_loop )
                {
                    for( size_t f = 1; f < m_goptCurrent[n].f_size(); ++f )
                    {
                        m_nPSDs[n][f]  = npsd;
                        m_olPSDs[n][f] = m_clPSDs( f, n ) / og2;
                    }
                }
                else
                {
                    for( size_t f = 1; f < m_goptCurrent[n].f_size(); ++f )
                    {
                        m_nPSDs[n][f]  = npsd;
                        m_olPSDs[n][f] = ( m_clPSDs( f, n ) / og2 ) / m_clXferCurrent( f, n );
                    }
                }

                m_olPSDs[n][0] = m_olPSDs[n][1];
                m_nPSDs[n][0]  = m_nPSDs[n][1];

                m_modeVarOL[n] = mx::sigproc::psdVar( m_freq, m_olPSDs[n] );

                m_optGainSI[n] = m_goptSI[n].optGainOpenLoop( m_modeVarSI[n], m_olPSDs[n], m_nPSDs[n], false );

                if( fabs( ( m_modeVarOL[n] - m_modeVarSI[n] ) / m_modeVarOL[n] ) < 0.01 )
                {
                    m_optGainSI[n] = 0;
                    m_modeVarSI[n] = m_modeVarOL[n];

                    for( size_t f = 0; f < m_goptCurrent[n].f_size(); ++f )
                    {
                        m_clXferSI( f, n ) = 1;
                    }
                }
                else
                {

                    for( size_t f = 0; f < m_goptCurrent[n].f_size(); ++f )
                    {
                        m_clXferSI( f, n ) = m_goptSI[n].clETF2( f, m_optGainSI[n] );
                    }
                }

                if( doPCCalcs )
                {

                    if( m_regScale[n] == -999 || m_regCounter[n] >= m_nRegCycles )
                    {
                        float min_sc;
                        float gmax_lp = 0;

                        m_linPred[n].regularizeCoefficients( gmax_lp,
                                                             m_optGainLP[n],
                                                             m_modeVarLP[n],
                                                             min_sc,
                                                             m_goptLP[n],
                                                             m_olPSDs[n],
                                                             m_nPSDs[n],
                                                             m_Na[n] );

                        if( m_regScale[n] == -999 )
                        {
                            m_regCounter[n] = n % m_nRegCycles;
                        }
                        else
                        {
                            m_regCounter[n] = 0;
                        }

                        m_regScale[n] = min_sc;
                        m_gmaxLP[n]   = gmax_lp;
                        // save max-stable gain m_maxLPGain[n]
                    }
                    else
                    {
                        // use new pre-regularized version
                        float psdReg = m_olPSDs[n][0];
                        if( m_linPred[n].calcCoefficients(
                                m_olPSDs[n], m_nPSDs[n], psdReg * pow( 10, -m_regScale[n] / 10 ), m_Na[n] ) < 0 )
                        {
                            log<software_error>( { __FILE__, __LINE__, "error calculating coefficients" } );
                        }

                        m_goptLP[n].a( m_linPred[n].m_lp.m_c );
                        m_goptLP[n].b( m_linPred[n].m_lp.m_c );

                        m_optGainLP[n] =
                            m_goptLP[n].optGainOpenLoop( m_modeVarLP[n], m_olPSDs[n], m_nPSDs[n], m_gmaxLP[n], false );

                        ++m_regCounter[n];
                    }

                    for( size_t f = 0; f < m_goptCurrent[n].f_size(); ++f )
                    {
                        m_clXferLP( f, n ) = m_goptLP[n].clETF2( f, m_optGainLP[n] );
                    }
                }
                else
                {
                    for( size_t f = 0; f < m_goptCurrent[n].f_size(); ++f )
                    {
                        m_clXferLP( f, n ) = 1;
                    }
                }
            }

            timePointT t1 = std::chrono::steady_clock::now();
            durationT  dt = t1 - t0;
            if( m_updating )
            {
                continue; // don't break b/c of omp
            }

            std::cerr << "Optimization took " << dt.count() << " seconds\n";


            float totVar = 0;
            for(size_t n = 0; n < m_modeVarSI.size(); ++n)
            {
                totVar += m_modeVarSI[n];
            }

            std::cerr << "total variance: " << totVar << '\n';

            float *f   = m_optGainStream->array.F;
            float *fSI = m_optGainSIStream->array.F;
            float *fLP = m_optGainLPStream->array.F;

            mx::improc::eigenMap<float> mvs( m_modevarStream->array.F, 3, m_modeVarSI.size() );

            m_optGainStream->md->write   = 1;
            m_optGainSIStream->md->write = 1;
            m_optGainLPStream->md->write = 1;
            m_modevarStream->md->write   = 1;

            for( size_t n = 0; n < m_optGainSI.size(); ++n )
            {
                f[n]   = ( m_gainCalFacts[n] * m_optGainSI[n] / m_gainCals[n] ) / m_opticalGain;
                fSI[n] = f[n];
                fLP[n] = ( m_gainCalFacts[n] * m_optGainLP[n] / m_gainCals[n] ) / m_opticalGain;

                mvs( 0, n ) = m_modeVarOL[n];
                mvs( 1, n ) = m_modeVarSI[n];
                mvs( 2, n ) = m_modeVarLP[n];
            }

            clock_gettime( CLOCK_ISIO, &m_optGainStream->md->writetime );
            m_optGainStream->md->atime = m_optGainStream->md->writetime;

            m_optGainSIStream->md->writetime = m_optGainStream->md->writetime;
            m_optGainSIStream->md->atime     = m_optGainStream->md->writetime;

            m_optGainLPStream->md->writetime = m_optGainStream->md->writetime;
            m_optGainLPStream->md->atime     = m_optGainStream->md->writetime;

            m_modevarStream->md->writetime = m_optGainStream->md->writetime;
            m_modevarStream->md->atime     = m_optGainStream->md->writetime;

            ++m_optGainStream->md->cnt0;
            m_optGainStream->md->write = 0;
            ImageStreamIO_sempost( m_optGainStream, -1 );

            ++m_optGainSIStream->md->cnt0;
            m_optGainSIStream->md->write = 0;
            ImageStreamIO_sempost( m_optGainSIStream, -1 );

            ++m_optGainLPStream->md->cnt0;
            m_optGainLPStream->md->write = 0;
            ImageStreamIO_sempost( m_optGainLPStream, -1 );

            ++m_modevarStream->md->cnt0;
            m_modevarStream->md->write = 0;
            ImageStreamIO_sempost( m_modevarStream, -1 );

            if( m_autoUpdate || m_updateOnce || m_dump )
            {
                float *f = gainFactShmimMonitorT::m_imageStream.array.F;

                gainFactShmimMonitorT::m_imageStream.md->write = 1;

                if( m_dump )
                {
                    for( size_t n = 0; n < m_optGainSI.size(); ++n )
                    {
                        f[n] = ( m_gainCalFacts[n] * m_optGainSI[n] / m_gainCals[n] ) / m_opticalGain;
                    }
                }
                else
                {
                    for( size_t n = 0; n < m_optGainSI.size(); ++n )
                    {
                        f[n] = f[n] +
                               m_gainGain *
                                   ( ( m_gainCalFacts[n] * m_optGainSI[n] / m_gainCals[n] ) / m_opticalGain - f[n] );
                    }
                }

                clock_gettime( CLOCK_ISIO, &gainFactShmimMonitorT::m_imageStream.md->writetime );
                gainFactShmimMonitorT::m_imageStream.md->atime = gainFactShmimMonitorT::m_imageStream.md->writetime;
                ++gainFactShmimMonitorT::m_imageStream.md->cnt0;
                gainFactShmimMonitorT::m_imageStream.md->write = 0;
                ImageStreamIO_sempost( &( gainFactShmimMonitorT::m_imageStream ), -1 );

                if( m_dump )
                {
                    log<text_log>( "gains updated by dump", logPrio::LOG_NOTICE );
                    m_dump = false;
                }
                else if( m_updateOnce && !m_autoUpdate )
                {
                    log<text_log>( "gains updated once", logPrio::LOG_NOTICE );
                }

                m_updateOnce = false;
            }

            if( m_loop & m_autoUpdate )
            {
                m_sinceChange = -1;
            }

            // Update OL PSDs and Transfer Functions
            m_olPSDStream->md->write         = 1;
            m_noisePSDStream->md->write      = 1;
            m_clXferCurrentStream->md->write = 1;
            m_clXferSIStream->md->write      = 1;
            m_clXferLPStream->md->write      = 1;

            for( size_t q = 0; q < m_olPSDs.size(); ++q )
            {
                memcpy( m_olPSDStream->array.F + q * m_olPSDs[0].size(),
                        m_olPSDs[q].data(),
                        m_olPSDs[0].size() * sizeof( float ) );

                m_noisePSDStream->array.F[q] = m_nPSDs[q][0];
            }

            memcpy( m_clXferCurrentStream->array.F,
                    m_clXferCurrent.data(),
                    m_clXferCurrent.rows() * m_clXferCurrent.cols() );
            memcpy( m_clXferSIStream->array.F, m_clXferSI.data(), m_clXferSI.rows() * m_clXferSI.cols() );
            memcpy( m_clXferLPStream->array.F, m_clXferLP.data(), m_clXferLP.rows() * m_clXferLP.cols() );

            clock_gettime( CLOCK_ISIO, &m_olPSDStream->md->writetime );
            m_olPSDStream->md->atime = m_olPSDStream->md->writetime;

            m_noisePSDStream->md->writetime = m_olPSDStream->md->writetime;
            m_noisePSDStream->md->atime     = m_noisePSDStream->md->writetime;

            m_clXferCurrentStream->md->writetime = m_olPSDStream->md->writetime;
            m_clXferCurrentStream->md->atime     = m_clXferCurrentStream->md->writetime;

            m_clXferSIStream->md->writetime = m_olPSDStream->md->writetime;
            m_clXferSIStream->md->atime     = m_clXferSIStream->md->writetime;

            m_clXferLPStream->md->writetime = m_olPSDStream->md->writetime;
            m_clXferLPStream->md->atime     = m_clXferLPStream->md->writetime;

            ++m_olPSDStream->md->cnt0;
            m_olPSDStream->md->write = 0;
            ImageStreamIO_sempost( m_olPSDStream, -1 );

            ++m_noisePSDStream->md->cnt0;
            m_noisePSDStream->md->write = 0;
            ImageStreamIO_sempost( m_noisePSDStream, -1 );

            ++m_clXferCurrentStream->md->cnt0;
            m_clXferCurrentStream->md->write = 0;
            ImageStreamIO_sempost( m_clXferCurrentStream, -1 );

            ++m_clXferSIStream->md->cnt0;
            m_clXferSIStream->md->write = 0;
            ImageStreamIO_sempost( m_clXferSIStream, -1 );

            ++m_clXferLPStream->md->cnt0;
            m_clXferLPStream->md->write = 0;
            ImageStreamIO_sempost( m_clXferLPStream, -1 );
        }
        else
        {
            /* Check for why we timed out */
            /* ETIMEDOUT just means keep waiting */
            if( errno == ETIMEDOUT )
            {
                // Could Update gopts if needed (requires size checks and requires mutex lock)
                // Probably not worth it for pred. control anyway.
                continue;
            }

            /* EINTER probably indicates time to shutdown, loop wil exit if m_shutdown is set */
            if( errno == EINTR )
            {
                continue;
            }

            /*Otherwise, report an error.*/
            log<software_error>( { __FILE__, __LINE__, errno, "sem_timedwait" } );
            break;
        }
    }
}

INDI_NEWCALLBACK_DEFN( modalGainOpt, m_indiP_autoUpdate )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_autoUpdate, ipRecv );

    if( ipRecv.find( "toggle" ) )
    {
        if( ipRecv["toggle"].getSwitchState() == pcf::IndiElement::On )
        {
            if( !m_autoUpdate )
            {
                log<text_log>( "updating gains", logPrio::LOG_NOTICE );
            }
            m_autoUpdate = true;
        }
        else
        {
            if( m_autoUpdate )
            {
                log<text_log>( "stopped updating gains", logPrio::LOG_NOTICE );
            }
            m_autoUpdate = false;
        }
    }

    return 0;
}

INDI_NEWCALLBACK_DEFN( modalGainOpt, m_indiP_updateOnce )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_updateOnce, ipRecv );

    if( ipRecv.find( "request" ) )
    {
        if( ipRecv["request"].getSwitchState() == pcf::IndiElement::On )
        {
            m_updateOnce = true;
        }
        else
        {
            m_updateOnce = false;
        }
    }

    return 0;
}

INDI_NEWCALLBACK_DEFN( modalGainOpt, m_indiP_dump )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_dump, ipRecv );

    if( ipRecv.find( "request" ) )
    {
        if( ipRecv["request"].getSwitchState() == pcf::IndiElement::On )
        {
            m_dump = true;
        }
        else
        {
            m_dump = false;
        }
    }

    return 0;
}

INDI_NEWCALLBACK_DEFN( modalGainOpt, m_indiP_opticalGain )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_opticalGain, ipRecv );

    float target;
    if( indiTargetUpdate( m_indiP_opticalGain, target, ipRecv, true ) < 0 )
    {
        log<software_error>( { __FILE__, __LINE__ } );
        return -1;
    }

    m_opticalGain = target;

    return 0;
}

INDI_NEWCALLBACK_DEFN( modalGainOpt, m_indiP_gainGain )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_gainGain, ipRecv );

    float target;
    if( indiTargetUpdate( m_indiP_gainGain, target, ipRecv, true ) < 0 )
    {
        log<software_error>( { __FILE__, __LINE__ } );
        return -1;
    }

    m_gainGain = target;

    return 0;
}

INDI_SETCALLBACK_DEFN( modalGainOpt, m_indiP_emg )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_emg, ipRecv );

    if( ipRecv.find( "current" ) )
    {
        float emg = ipRecv["current"].get<float>();

        if( emg != m_emg )
        {
            m_emg = emg;
            std::cerr << "Got EMG: " << m_emg << '\n';
        }
    }
    return 0;
}

INDI_SETCALLBACK_DEFN( modalGainOpt, m_indiP_psdTime )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_psdTime, ipRecv );

    if( ipRecv.find( "current" ) )
    {
        float psdTime = ipRecv["current"].get<float>();

        if( psdTime != m_psdTime )
        {
            m_updating = true;
            std::lock_guard<std::mutex> lock( m_goptMutex );
            m_updating = true;

            m_psdTime = psdTime;

            m_sinceChange = -1;
            m_updating    = false;

            std::cerr << "Got psdTime: " << m_psdTime << '\n';
        }
    }
    return 0;
}

INDI_SETCALLBACK_DEFN( modalGainOpt, m_indiP_psdAvgTime )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_psdAvgTime, ipRecv );

    if( ipRecv.find( "current" ) )
    {
        float psdAvgTime = ipRecv["current"].get<float>();

        if( psdAvgTime != m_psdAvgTime )
        {
            m_updating = true;
            std::lock_guard<std::mutex> lock( m_goptMutex );
            m_updating = true;

            m_psdAvgTime = psdAvgTime;

            m_sinceChange = -1;
            m_updating    = false;

            std::cerr << "Got psdAvgTime: " << m_psdAvgTime << '\n';
        }
    }

    return 0;
}

INDI_SETCALLBACK_DEFN( modalGainOpt, m_indiP_loop )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_loop, ipRecv );

    if( ipRecv.find( "toggle" ) )
    {
        bool state;

        if( ipRecv["toggle"].getSwitchState() == pcf::IndiElement::On )
        {
            state = true;
        }
        else
        {
            state = false;
        }

        if( state != m_loop )
        {
            m_updating = true;
            std::lock_guard<std::mutex> lock( m_goptMutex );
            m_updating = true;

            m_loop = state;

            m_sinceChange = -1;
            m_updating    = false;
            std::cerr << "Got loop: " << m_loop << '\n';
        }
    }

    return 0;
}

INDI_SETCALLBACK_DEFN( modalGainOpt, m_indiP_siGain )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_siGain, ipRecv );

    if( ipRecv.find( "current" ) )
    {
        float gain = ipRecv["current"].get<float>();

        if( gain != m_gain && !m_pcOn )
        {
            m_updating = true;
            std::lock_guard<std::mutex> lock( m_goptMutex );
            m_updating = true;

            m_gain = gain;

            if( m_loop )
            {
                m_sinceChange = -1;
            }

            m_updating = false;

            std::cerr << "Got gain: " << m_gain << '\n';
        }
        else
        {
            m_gain = gain; // for the m_pcOn case
        }
    }

    return 0;
}

INDI_SETCALLBACK_DEFN( modalGainOpt, m_indiP_siMult )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_siMult, ipRecv );

    if( ipRecv.find( "current" ) )
    {
        float mc = ipRecv["current"].get<float>();

        if( mc != m_mult && !m_pcOn )
        {
            m_updating = true;
            std::lock_guard<std::mutex> lock( m_goptMutex );
            m_updating = true;

            m_mult = mc;

            if( m_loop )
            {
                m_sinceChange = -1;
            }

            m_goptUpdated = true;
            m_updating    = false;
            std::cerr << "Got mc: " << m_mult << '\n';
        }
        else
        {
            m_mult = mc; // for the m_pcOn case
        }
    }

    return 0;
}

INDI_SETCALLBACK_DEFN( modalGainOpt, m_indiP_pcGain )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_pcGain, ipRecv );

    if( ipRecv.find( "current" ) )
    {
        float gain = ipRecv["current"].get<float>();

        if( gain != m_pcGain && m_pcOn )
        {
            m_updating = true;
            std::lock_guard<std::mutex> lock( m_goptMutex );
            m_updating = true;

            m_pcGain = gain;

            if( m_loop )
            {
                m_sinceChange = -1;
            }

            m_updating = false;

            std::cerr << "Got pc gain: " << m_pcGain << '\n';
        }
        else
        {
            m_pcGain = gain; // for the !m_pcOn case
        }
    }

    return 0;
}

INDI_SETCALLBACK_DEFN( modalGainOpt, m_indiP_pcMult )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_pcMult, ipRecv );

    if( ipRecv.find( "current" ) )
    {
        float mc = ipRecv["current"].get<float>();

        if( mc != m_pcMult && m_pcOn )
        {
            m_updating = true;
            std::lock_guard<std::mutex> lock( m_goptMutex );
            m_updating = true;

            m_pcMult = mc;

            if( m_loop )
            {
                m_sinceChange = -1;
            }

            m_pcgoptUpdated = true;
            m_updating      = false;
            std::cerr << "Got pc mc: " << m_pcMult << '\n';
        }
        else
        {
            m_pcMult = mc; // for the !m_pcOn case
        }
    }

    return 0;
}

INDI_SETCALLBACK_DEFN( modalGainOpt, m_indiP_pcOn )( const pcf::IndiProperty &ipRecv )
{
    INDI_VALIDATE_CALLBACK_PROPS( m_indiP_pcOn, ipRecv );

    if( ipRecv.find( "toggle" ) )
    {
        bool state;

        if( ipRecv["toggle"].getSwitchState() == pcf::IndiElement::On )
        {
            state = true;
        }
        else
        {
            state = false;
        }

        if( state != m_pcOn )
        {
            m_updating = true;
            std::lock_guard<std::mutex> lock( m_goptMutex );
            m_updating = true;

            m_pcOn = state;

            m_sinceChange = -1;
            m_updating    = false;
            std::cerr << "Got pcOn: " << std::boolalpha << m_pcOn << '\n';
        }
    }

    return 0;
}

} // namespace app
} // namespace MagAOX

#endif // modalGainOpt_hpp
