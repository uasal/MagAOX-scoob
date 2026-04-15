/** \file kim101Ctrl.hpp
 * \brief The MagAO-X KIM101 Inertial Motor Controller header file
 *
 * \ingroup kim101Ctrl_files
 */

#ifndef kim101Ctrl_hpp
#define kim101Ctrl_hpp

#include "../../libMagAOX/libMagAOX.hpp" //Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"

// Include the tmcController from kcubeCtrl
#include "../kcubeCtrl/tmcController.hpp"

/** \defgroup kim101Ctrl
 * \brief The KIM101 Inertial Motor Controller application
 *
 * <a href="../handbook/operating/software/apps/kim101Ctrl.html">Application Documentation</a>
 *
 * \ingroup apps
 *
 */

/** \defgroup kim101Ctrl_files
 * \ingroup kim101Ctrl
 */

namespace MagAOX
{
namespace app
{

/// Local derivation of tmcController to implement MagAO-X logging
template<class parentT>
class kimCon : public tmcController 
{
public:

    /// Print a message to MagAO-X logs describing an error from an \libftdi1 function
    virtual void ftdiErrmsg( const std::string & src,
                             const std::string & msg,
                             int rv,
                             const std::string & file,
                             int line
                           )
    {
        std::stringstream logs;
        logs << src << ": " << msg << " [ libftdi1: " << ftdi_get_error_string(m_ftdi) << " ] ";
        uint32_t ln = line;
        parentT::template log<software_error>({file.c_str(), ln, 0, rv, logs.str()});
    }       

    /// Print a message to MagAO-X logs describing an error 
    virtual void otherErrmsg( const std::string & src,
                              const std::string & msg,
                              const std::string & file,
                              int line
                            )
    {
        uint32_t ln = line;
        parentT::template log<software_error>({file.c_str(), ln, src + ": " + msg});
    }

    /// Route transport trace to MagAO-X text logs when enabled.
    virtual void traceMsg( const std::string & msg )
    {
        parentT::template log<text_log>(msg, logPrio::LOG_NOTICE);
    }

};

/// The MagAO-X KIM101 Inertial Motor Controller
/**
  * Controls the Thorlabs KIM101 K-Cube Inertial Motor Controller.
  * The KIM101 has 4 channels for controlling piezo inertial motors.
  * 
  * \ingroup kim101Ctrl
  */
class kim101Ctrl : public MagAOXApp<true>
{

    // Give the test harness access.
    friend class kim101Ctrl_test;

public:
    /// Number of channels on KIM101
    static constexpr int NumChannels = 4;

protected:
    /** \name Configurable Parameters
     *@{
     */
    
    std::string m_serial; ///< USB serial number of the device
    bool m_traceHex {false}; ///< Enable low-level TX/RX hex tracing for FTDI traffic
    bool m_disableRtsCts {false}; ///< Disable RTS/CTS flow-control setup during connect
    bool m_useKIMEnableMode {false}; ///< Use KIM-specific 0x2B enable mode (experimental)

    /// Drive parameters (can be configured per-channel or globally)
    tmcController::KIMDriveOPParams m_driveParams;
    
    /// Jog parameters
    tmcController::KIMJogParams m_jogParams;
    
    ///@}

    /// The controller connection
    kimCon<kim101Ctrl> m_kcube;

    /// Channel state tracking
    struct ChannelState {
        bool enabled {false};
        int32_t position {0};
        int32_t targetPosition {0};
        bool moving {false};
        bool homed {false};
    };
    ChannelState m_channels[NumChannels];
    uint8_t m_enabledMask {0};               ///< Requested enable mask (bit0..bit3 for ch1..ch4)
    unsigned m_statusConsecutiveFailures {0}; ///< Count consecutive status polling failures

    /// Full device status
    tmcController::KIMStatus m_status;

public:

    /// Default c'tor.
    kim101Ctrl();

    /// D'tor, declared and defined for noexcept.
    ~kim101Ctrl() noexcept
    {
    }

    virtual void setupConfig();

    /// Implementation of loadConfig logic, separated for testing.
    int loadConfigImpl(mx::app::appConfigurator &_config);

    virtual void loadConfig();

    /// Startup function
    virtual int appStartup();

    /// Implementation of the FSM for kim101Ctrl.
    virtual int appLogic();

    /// Shutdown the app.
    virtual int appShutdown();

    /** \name Device Interface
     * @{
     */
    
    /// Initialize the device after connection
    int deviceInitialize();

    /// Update status for all channels
    int updateStatus();

    /// Query each channel individually for position/enable state.
    int queryChannelsIndividually(bool logErrors);

    /// Enable a channel
    int channelEnable(int ch);

    /// Disable a channel  
    int channelDisable(int ch);

    /// Move a channel to absolute position
    int channelMoveAbsolute(int ch, int32_t position);

    /// Jog a channel
    int channelJog(int ch, int direction); ///< direction: 1=forward, -1=reverse

    /// Stop motion on a channel
    int channelStop(int ch);

    /// Zero the position counter for a channel
    int channelZero(int ch);

    ///@}

    /** \name Utility Functions
     * @{
     */

    /// Convert channel number (1-4) to channel identifier bitmask
    static uint16_t channelIdent(int ch)
    {
        if(ch < 1 || ch > 4) return 0;
        return static_cast<uint16_t>(1 << (ch - 1));
    }

    /// Map channel-enable bitmask to KIM 0x2B mode code.
    static bool maskToKIMEnableMode(uint8_t mask, tmcController::KIMChanEnableMode & mode)
    {
        switch(mask)
        {
            case 0x00: mode = tmcController::KIMChanEnableMode::None; return true;
            case 0x01: mode = tmcController::KIMChanEnableMode::Channel1; return true;
            case 0x02: mode = tmcController::KIMChanEnableMode::Channel2; return true;
            case 0x04: mode = tmcController::KIMChanEnableMode::Channel3; return true;
            case 0x08: mode = tmcController::KIMChanEnableMode::Channel4; return true;
            case 0x03: mode = tmcController::KIMChanEnableMode::Pair12; return true;
            case 0x0C: mode = tmcController::KIMChanEnableMode::Pair34; return true;
            default: return false;
        }
    }

    static bool KIMEnableModeToMask(tmcController::KIMChanEnableMode mode, uint8_t & mask)
    {
        switch(mode)
        {
            case tmcController::KIMChanEnableMode::None: mask = 0x00; return true;
            case tmcController::KIMChanEnableMode::Channel1: mask = 0x01; return true;
            case tmcController::KIMChanEnableMode::Channel2: mask = 0x02; return true;
            case tmcController::KIMChanEnableMode::Channel3: mask = 0x04; return true;
            case tmcController::KIMChanEnableMode::Channel4: mask = 0x08; return true;
            case tmcController::KIMChanEnableMode::Pair12: mask = 0x03; return true;
            case tmcController::KIMChanEnableMode::Pair34: mask = 0x0C; return true;
            default: return false;
        }
    }

    static std::string currentChannelString(uint8_t mask)
    {
        switch(mask)
        {
            case 0x01: return "ch_1";
            case 0x02: return "ch_2";
            case 0x04: return "ch_3";
            case 0x08: return "ch_4";
            case 0x03: return "ch_1_2";
            case 0x0C: return "ch_3_4";
            default: return "none";
        }
    }

    ///@}

    /** \name INDI
     * @{
     */

    pcf::IndiProperty m_indiP_identify;
    INDI_NEWCALLBACK_DECL(kim101Ctrl, m_indiP_identify);
    pcf::IndiProperty m_indiP_current_channel;

    // Per-channel properties
    pcf::IndiProperty m_indiP_ch1_enable;
    pcf::IndiProperty m_indiP_ch1_position;
    pcf::IndiProperty m_indiP_ch1_stop;
    pcf::IndiProperty m_indiP_ch1_zero;

    pcf::IndiProperty m_indiP_ch2_enable;
    pcf::IndiProperty m_indiP_ch2_position;
    pcf::IndiProperty m_indiP_ch2_stop;
    pcf::IndiProperty m_indiP_ch2_zero;

    pcf::IndiProperty m_indiP_ch3_enable;
    pcf::IndiProperty m_indiP_ch3_position;
    pcf::IndiProperty m_indiP_ch3_stop;
    pcf::IndiProperty m_indiP_ch3_zero;

    pcf::IndiProperty m_indiP_ch4_enable;
    pcf::IndiProperty m_indiP_ch4_position;
    pcf::IndiProperty m_indiP_ch4_stop;
    pcf::IndiProperty m_indiP_ch4_zero;

    INDI_NEWCALLBACK_DECL(kim101Ctrl, m_indiP_ch1_enable);
    INDI_NEWCALLBACK_DECL(kim101Ctrl, m_indiP_ch1_position);
    INDI_NEWCALLBACK_DECL(kim101Ctrl, m_indiP_ch1_stop);
    INDI_NEWCALLBACK_DECL(kim101Ctrl, m_indiP_ch1_zero);

    INDI_NEWCALLBACK_DECL(kim101Ctrl, m_indiP_ch2_enable);
    INDI_NEWCALLBACK_DECL(kim101Ctrl, m_indiP_ch2_position);
    INDI_NEWCALLBACK_DECL(kim101Ctrl, m_indiP_ch2_stop);
    INDI_NEWCALLBACK_DECL(kim101Ctrl, m_indiP_ch2_zero);

    INDI_NEWCALLBACK_DECL(kim101Ctrl, m_indiP_ch3_enable);
    INDI_NEWCALLBACK_DECL(kim101Ctrl, m_indiP_ch3_position);
    INDI_NEWCALLBACK_DECL(kim101Ctrl, m_indiP_ch3_stop);
    INDI_NEWCALLBACK_DECL(kim101Ctrl, m_indiP_ch3_zero);

    INDI_NEWCALLBACK_DECL(kim101Ctrl, m_indiP_ch4_enable);
    INDI_NEWCALLBACK_DECL(kim101Ctrl, m_indiP_ch4_position);
    INDI_NEWCALLBACK_DECL(kim101Ctrl, m_indiP_ch4_stop);
    INDI_NEWCALLBACK_DECL(kim101Ctrl, m_indiP_ch4_zero);

    ///@}
};

kim101Ctrl::kim101Ctrl() : MagAOXApp(MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED)
{
    m_powerMgtEnabled = true;
    // KIM101 can stop responding if USB reset triggers re-enumeration/rebind
    // immediately after connect(); keep FTDI link setup but skip usb_reset.
    m_kcube.usbResetOnConnect(false);
    return;
}

void kim101Ctrl::setupConfig()
{
    config.add("device.serial", "", "device.serial", argType::Required, "device", "serial", false, "string", "USB serial number");
    config.add("device.traceHex", "", "device.traceHex", argType::True, "device", "traceHex", false, "bool", "Enable low-level TX/RX hex tracing");
    config.add("device.disableRtsCts", "", "device.disableRtsCts", argType::True, "device", "disableRtsCts", false, "bool", "Disable RTS/CTS flow-control setup in connect()");
    config.add("device.useKIMEnableMode", "", "device.useKIMEnableMode", argType::True, "device", "useKIMEnableMode", false, "bool", "Use KIM-specific 0x2B channel enable mode (experimental)");
    
    // Drive parameters
    config.add("drive.maxVoltage", "", "drive.maxVoltage", argType::Required, "drive", "maxVoltage", false, "int", "Max drive voltage (85-125V), default 110");
    config.add("drive.stepRate", "", "drive.stepRate", argType::Required, "drive", "stepRate", false, "int", "Step rate (1-2000 steps/sec), default 500");
    config.add("drive.stepAccn", "", "drive.stepAccn", argType::Required, "drive", "stepAccn", false, "int", "Acceleration (1-100000 steps/sec/sec), default 100000");
    
    // Jog parameters
    config.add("jog.mode", "", "jog.mode", argType::Required, "jog", "mode", false, "int", "Jog mode (1=continuous, 2=step), default 2");
    config.add("jog.stepSize", "", "jog.stepSize", argType::Required, "jog", "stepSize", false, "int", "Jog step size in steps, default 100");
    config.add("jog.stepRate", "", "jog.stepRate", argType::Required, "jog", "stepRate", false, "int", "Jog step rate (1-2000), default 500");
    config.add("jog.stepAccn", "", "jog.stepAccn", argType::Required, "jog", "stepAccn", false, "int", "Jog acceleration, default 100000");
}

int kim101Ctrl::loadConfigImpl(mx::app::appConfigurator &_config)
{
    _config(m_serial, "device.serial");
    m_kcube.serial(m_serial);
    _config(m_traceHex, "device.traceHex");
    m_kcube.traceIO(m_traceHex);
    _config(m_disableRtsCts, "device.disableRtsCts");
    m_kcube.useRtsCtsOnConnect(!m_disableRtsCts);
    _config(m_useKIMEnableMode, "device.useKIMEnableMode");
    if(m_traceHex)
    {
        log<text_log>("FTDI hex trace enabled for kim101 transport", logPrio::LOG_WARNING);
    }
    if(m_disableRtsCts)
    {
        log<text_log>("RTS/CTS flow-control disabled for kim101 transport", logPrio::LOG_WARNING);
    }
    if(m_useKIMEnableMode)
    {
        log<text_log>("KIM 0x2B enable mode enabled (experimental)", logPrio::LOG_WARNING);
    }
    
    // Drive parameters
    int maxV = m_driveParams.MaxVoltage;
    _config(maxV, "drive.maxVoltage");
    m_driveParams.MaxVoltage = maxV;
    
    int stepRate = m_driveParams.StepRate;
    _config(stepRate, "drive.stepRate");
    m_driveParams.StepRate = stepRate;
    
    int stepAccn = m_driveParams.StepAccn;
    _config(stepAccn, "drive.stepAccn");
    m_driveParams.StepAccn = stepAccn;
    
    // Jog parameters
    int jogMode = m_jogParams.JogMode;
    _config(jogMode, "jog.mode");
    m_jogParams.JogMode = jogMode;
    
    int jogStep = m_jogParams.JogStepSizeFwd;
    _config(jogStep, "jog.stepSize");
    m_jogParams.JogStepSizeFwd = jogStep;
    m_jogParams.JogStepSizeRev = jogStep;
    
    int jogRate = m_jogParams.JogStepRate;
    _config(jogRate, "jog.stepRate");
    m_jogParams.JogStepRate = jogRate;
    
    int jogAccn = m_jogParams.JogStepAccn;
    _config(jogAccn, "jog.stepAccn");
    m_jogParams.JogStepAccn = jogAccn;

    return 0;
}

void kim101Ctrl::loadConfig()
{
    loadConfigImpl(config);
}

int kim101Ctrl::appStartup()
{
    // Identify button
    createStandardIndiRequestSw(m_indiP_identify, "identify");  
    if(registerIndiPropertyNew(m_indiP_identify, INDI_NEWCALLBACK(m_indiP_identify)) < 0)
    {
        log<software_error>({__FILE__,__LINE__});
        return -1;
    }

    if(createROIndiText(m_indiP_current_channel, "current_channel", "value", "current_channel", "channels", "current_channel") < 0)
    {
        log<software_error>({__FILE__,__LINE__});
        return -1;
    }
    if(registerIndiPropertyReadOnly(m_indiP_current_channel) < 0)
    {
        log<software_error>({__FILE__,__LINE__});
        return -1;
    }
    m_indiP_current_channel["value"] = "none";

    // Channel 1
    createStandardIndiToggleSw(m_indiP_ch1_enable, "ch1_enable");  
    if(registerIndiPropertyNew(m_indiP_ch1_enable, INDI_NEWCALLBACK(m_indiP_ch1_enable)) < 0)
    {
        log<software_error>({__FILE__,__LINE__});
        return -1;
    }
    
    createStandardIndiNumber<int32_t>(m_indiP_ch1_position, "ch1_position", -2147483648, 2147483647, 1, "%d");  
    if(registerIndiPropertyNew(m_indiP_ch1_position, INDI_NEWCALLBACK(m_indiP_ch1_position)) < 0)
    {
        log<software_error>({__FILE__,__LINE__});
        return -1;
    }
    m_indiP_ch1_position["current"] = 0;
    m_indiP_ch1_position["target"] = 0;

    createStandardIndiRequestSw(m_indiP_ch1_stop, "ch1_stop");  
    if(registerIndiPropertyNew(m_indiP_ch1_stop, INDI_NEWCALLBACK(m_indiP_ch1_stop)) < 0)
    {
        log<software_error>({__FILE__,__LINE__});
        return -1;
    }

    createStandardIndiRequestSw(m_indiP_ch1_zero, "ch1_zero");  
    if(registerIndiPropertyNew(m_indiP_ch1_zero, INDI_NEWCALLBACK(m_indiP_ch1_zero)) < 0)
    {
        log<software_error>({__FILE__,__LINE__});
        return -1;
    }

    // Channel 2
    createStandardIndiToggleSw(m_indiP_ch2_enable, "ch2_enable");  
    if(registerIndiPropertyNew(m_indiP_ch2_enable, INDI_NEWCALLBACK(m_indiP_ch2_enable)) < 0)
    {
        log<software_error>({__FILE__,__LINE__});
        return -1;
    }
    
    createStandardIndiNumber<int32_t>(m_indiP_ch2_position, "ch2_position", -2147483648, 2147483647, 1, "%d");  
    if(registerIndiPropertyNew(m_indiP_ch2_position, INDI_NEWCALLBACK(m_indiP_ch2_position)) < 0)
    {
        log<software_error>({__FILE__,__LINE__});
        return -1;
    }
    m_indiP_ch2_position["current"] = 0;
    m_indiP_ch2_position["target"] = 0;

    createStandardIndiRequestSw(m_indiP_ch2_stop, "ch2_stop");  
    if(registerIndiPropertyNew(m_indiP_ch2_stop, INDI_NEWCALLBACK(m_indiP_ch2_stop)) < 0)
    {
        log<software_error>({__FILE__,__LINE__});
        return -1;
    }

    createStandardIndiRequestSw(m_indiP_ch2_zero, "ch2_zero");  
    if(registerIndiPropertyNew(m_indiP_ch2_zero, INDI_NEWCALLBACK(m_indiP_ch2_zero)) < 0)
    {
        log<software_error>({__FILE__,__LINE__});
        return -1;
    }

    // Channel 3
    createStandardIndiToggleSw(m_indiP_ch3_enable, "ch3_enable");  
    if(registerIndiPropertyNew(m_indiP_ch3_enable, INDI_NEWCALLBACK(m_indiP_ch3_enable)) < 0)
    {
        log<software_error>({__FILE__,__LINE__});
        return -1;
    }
    
    createStandardIndiNumber<int32_t>(m_indiP_ch3_position, "ch3_position", -2147483648, 2147483647, 1, "%d");  
    if(registerIndiPropertyNew(m_indiP_ch3_position, INDI_NEWCALLBACK(m_indiP_ch3_position)) < 0)
    {
        log<software_error>({__FILE__,__LINE__});
        return -1;
    }
    m_indiP_ch3_position["current"] = 0;
    m_indiP_ch3_position["target"] = 0;

    createStandardIndiRequestSw(m_indiP_ch3_stop, "ch3_stop");  
    if(registerIndiPropertyNew(m_indiP_ch3_stop, INDI_NEWCALLBACK(m_indiP_ch3_stop)) < 0)
    {
        log<software_error>({__FILE__,__LINE__});
        return -1;
    }

    createStandardIndiRequestSw(m_indiP_ch3_zero, "ch3_zero");  
    if(registerIndiPropertyNew(m_indiP_ch3_zero, INDI_NEWCALLBACK(m_indiP_ch3_zero)) < 0)
    {
        log<software_error>({__FILE__,__LINE__});
        return -1;
    }

    // Channel 4
    createStandardIndiToggleSw(m_indiP_ch4_enable, "ch4_enable");  
    if(registerIndiPropertyNew(m_indiP_ch4_enable, INDI_NEWCALLBACK(m_indiP_ch4_enable)) < 0)
    {
        log<software_error>({__FILE__,__LINE__});
        return -1;
    }
    
    createStandardIndiNumber<int32_t>(m_indiP_ch4_position, "ch4_position", -2147483648, 2147483647, 1, "%d");  
    if(registerIndiPropertyNew(m_indiP_ch4_position, INDI_NEWCALLBACK(m_indiP_ch4_position)) < 0)
    {
        log<software_error>({__FILE__,__LINE__});
        return -1;
    }
    m_indiP_ch4_position["current"] = 0;
    m_indiP_ch4_position["target"] = 0;

    createStandardIndiRequestSw(m_indiP_ch4_stop, "ch4_stop");  
    if(registerIndiPropertyNew(m_indiP_ch4_stop, INDI_NEWCALLBACK(m_indiP_ch4_stop)) < 0)
    {
        log<software_error>({__FILE__,__LINE__});
        return -1;
    }

    createStandardIndiRequestSw(m_indiP_ch4_zero, "ch4_zero");  
    if(registerIndiPropertyNew(m_indiP_ch4_zero, INDI_NEWCALLBACK(m_indiP_ch4_zero)) < 0)
    {
        log<software_error>({__FILE__,__LINE__});
        return -1;
    }

    state(stateCodes::NODEVICE);

    return 0;
}

int kim101Ctrl::appLogic()
{
    if(state() == stateCodes::POWERON || state() == stateCodes::NODEVICE || state() == stateCodes::ERROR)
    {
        int rv;
        {
            elevatedPrivileges elPriv(this);
            rv = m_kcube.open(false);
        }

        if(rv == 0)
        {
            if(!stateLogged())
            {
                std::stringstream logs;
                logs << "USB Device " << m_kcube.vendor() << ":" << m_kcube.product() << ":";
                logs << m_kcube.serial() << " found";
                log<text_log>(logs.str());
            }

            state(stateCodes::NOTCONNECTED);
        }
        else if(rv == -3)
        {
            state(stateCodes::NODEVICE);
            return 0;
        }
        else
        {
            std::stringstream em;
            em << "tmcController::open failed (rv=" << rv << "). Check device.serial matches the 8-digit USB string; ";
            em << "VID:PID " << std::hex << m_kcube.vendor() << ":" << m_kcube.product() << std::dec << "; ";
            em << "on Linux ensure the interface is not bound by ftdi_sio (see lsmod) and udev permits access.";
            log<software_error>({__FILE__, __LINE__, 0, rv, em.str()});
            state(stateCodes::ERROR);
            return 0;
        }        
    }

    if(state() == stateCodes::NOTCONNECTED)
    {
        int rv;
        {
            std::lock_guard<std::mutex> guard(m_indiMutex);
            elevatedPrivileges elPriv(this);
            rv = m_kcube.connect();
        }

        if(rv < 0)
        {
            sleep(1);
            if(m_powerState == 0) return -1;

            {
                std::lock_guard<std::mutex> guard(m_indiMutex);
                m_kcube.close(false);
            }

            std::stringstream em;
            em << "tmcController::connect failed (rv=" << rv << "). USB open succeeded but FTDI init failed; ";
            em << "see APT USB setup (115200 8N1, purge, reset, RTS/CTS, RTS high).";
            log<software_error>({__FILE__, __LINE__, 0, rv, em.str()});
            state(stateCodes::ERROR);
            return 0;
        }

        // state() updates INDI fsm via try_lock(m_indiMutex); must not hold m_indiMutex here
        state(stateCodes::CONNECTED);
    }

    if(state() == stateCodes::CONNECTED)
    {
        m_statusConsecutiveFailures = 0;
        // Give the controller a brief settle period after FTDI connect/reset before
        // first HW queries (observed to help with intermittent zero-byte responses).
        sleep(1);
        if(m_powerState == 0) return -1;

        int diRv;
        {
            std::lock_guard<std::mutex> guard(m_indiMutex);
            diRv = deviceInitialize();
        }

        if(diRv < 0)
        {
            {
                std::lock_guard<std::mutex> guard(m_indiMutex);
                m_kcube.close(false);
            }
            log<software_error>({__FILE__,__LINE__, "error during device initialization"});
            state(stateCodes::ERROR);
            return 0;
        }

        state(stateCodes::READY);
    }

    if(state() == stateCodes::READY || state() == stateCodes::OPERATING)
    {
        // Try to get lock, but don't block
        std::unique_lock<std::mutex> lock(m_indiMutex, std::try_to_lock);
        if(!lock.owns_lock()) return 0;

        // Update status for all channels
        if(updateStatus() < 0)
        {
            ++m_statusConsecutiveFailures;
            log<software_error>({__FILE__, __LINE__, "status update failed (" + std::to_string(m_statusConsecutiveFailures) + " consecutive)"});

            // Tolerate transient FTDI/USB read misses; only force reconnect on persistent failure.
            if(m_statusConsecutiveFailures < 3)
            {
                return 0;
            }

            sleep(1);
            if(m_powerState == 0) return -1;

            m_kcube.close(false);
            lock.unlock();
            state(stateCodes::ERROR);
            return 0;
        }
        m_statusConsecutiveFailures = 0;

        // Update INDI properties based on status
        bool anyMoving = false;

        for(int ch = 0; ch < NumChannels; ch++)
        {
            auto& chs = m_channels[ch];
            auto& ss = m_status.channels[ch];
            
            chs.position = ss.position;
            chs.moving = ss.isMoving();
            chs.homed = ss.homed();
            chs.enabled = (m_enabledMask & channelIdent(ch+1)) != 0;

            if(chs.moving) anyMoving = true;
        }

        // Update channel 1 properties
        if(m_channels[0].enabled)
        {
            updateSwitchIfChanged(m_indiP_ch1_enable, "toggle", pcf::IndiElement::On, INDI_OK);
            updateIfChanged(m_indiP_ch1_position, "current", m_channels[0].position, 
                           m_channels[0].moving ? INDI_BUSY : INDI_OK);
            updateIfChanged(m_indiP_ch1_position, "target", m_channels[0].targetPosition, INDI_OK);
        }
        else
        {
            updateSwitchIfChanged(m_indiP_ch1_enable, "toggle", pcf::IndiElement::Off, INDI_IDLE);
            updateIfChanged(m_indiP_ch1_position, "current", m_channels[0].position, INDI_IDLE);
            updateIfChanged(m_indiP_ch1_position, "target", m_channels[0].targetPosition, INDI_IDLE);
        }

        // Update channel 2 properties
        if(m_channels[1].enabled)
        {
            updateSwitchIfChanged(m_indiP_ch2_enable, "toggle", pcf::IndiElement::On, INDI_OK);
            updateIfChanged(m_indiP_ch2_position, "current", m_channels[1].position,
                           m_channels[1].moving ? INDI_BUSY : INDI_OK);
            updateIfChanged(m_indiP_ch2_position, "target", m_channels[1].targetPosition, INDI_OK);
        }
        else
        {
            updateSwitchIfChanged(m_indiP_ch2_enable, "toggle", pcf::IndiElement::Off, INDI_IDLE);
            updateIfChanged(m_indiP_ch2_position, "current", m_channels[1].position, INDI_IDLE);
            updateIfChanged(m_indiP_ch2_position, "target", m_channels[1].targetPosition, INDI_IDLE);
        }

        // Update channel 3 properties
        if(m_channels[2].enabled)
        {
            updateSwitchIfChanged(m_indiP_ch3_enable, "toggle", pcf::IndiElement::On, INDI_OK);
            updateIfChanged(m_indiP_ch3_position, "current", m_channels[2].position,
                           m_channels[2].moving ? INDI_BUSY : INDI_OK);
            updateIfChanged(m_indiP_ch3_position, "target", m_channels[2].targetPosition, INDI_OK);
        }
        else
        {
            updateSwitchIfChanged(m_indiP_ch3_enable, "toggle", pcf::IndiElement::Off, INDI_IDLE);
            updateIfChanged(m_indiP_ch3_position, "current", m_channels[2].position, INDI_IDLE);
            updateIfChanged(m_indiP_ch3_position, "target", m_channels[2].targetPosition, INDI_IDLE);
        }

        // Update channel 4 properties
        if(m_channels[3].enabled)
        {
            updateSwitchIfChanged(m_indiP_ch4_enable, "toggle", pcf::IndiElement::On, INDI_OK);
            updateIfChanged(m_indiP_ch4_position, "current", m_channels[3].position,
                           m_channels[3].moving ? INDI_BUSY : INDI_OK);
            updateIfChanged(m_indiP_ch4_position, "target", m_channels[3].targetPosition, INDI_OK);
        }
        else
        {
            updateSwitchIfChanged(m_indiP_ch4_enable, "toggle", pcf::IndiElement::Off, INDI_IDLE);
            updateIfChanged(m_indiP_ch4_position, "current", m_channels[3].position, INDI_IDLE);
            updateIfChanged(m_indiP_ch4_position, "target", m_channels[3].targetPosition, INDI_IDLE);
        }

        updateIfChanged(m_indiP_current_channel, "value", currentChannelString(m_enabledMask), INDI_OK);

        // Reset identify and other request buttons
        updateSwitchIfChanged(m_indiP_identify, "request", pcf::IndiElement::Off, INDI_IDLE);
        updateSwitchIfChanged(m_indiP_ch1_stop, "request", pcf::IndiElement::Off, INDI_IDLE);
        updateSwitchIfChanged(m_indiP_ch1_zero, "request", pcf::IndiElement::Off, INDI_IDLE);
        updateSwitchIfChanged(m_indiP_ch2_stop, "request", pcf::IndiElement::Off, INDI_IDLE);
        updateSwitchIfChanged(m_indiP_ch2_zero, "request", pcf::IndiElement::Off, INDI_IDLE);
        updateSwitchIfChanged(m_indiP_ch3_stop, "request", pcf::IndiElement::Off, INDI_IDLE);
        updateSwitchIfChanged(m_indiP_ch3_zero, "request", pcf::IndiElement::Off, INDI_IDLE);
        updateSwitchIfChanged(m_indiP_ch4_stop, "request", pcf::IndiElement::Off, INDI_IDLE);
        updateSwitchIfChanged(m_indiP_ch4_zero, "request", pcf::IndiElement::Off, INDI_IDLE);

        const stateCodes::stateCodeT nextState =
            anyMoving ? stateCodes::OPERATING : stateCodes::READY;
        lock.unlock();
        state(nextState);
    }

    return 0;
}

int kim101Ctrl::appShutdown()
{
    return 0;
}

int kim101Ctrl::deviceInitialize()
{
    int rv;

    // Get hardware info
    tmcController::HWInfo hwi;
    rv = -1;
    for(int attempt = 0; attempt < 4; ++attempt)
    {
        rv = m_kcube.hw_req_info(hwi);
        if(rv >= 0) break;

        sleep(1);
        if(m_powerState == 0) return -1;

        if(attempt < 3)
        {
            log<software_error>({__FILE__, __LINE__, 0, rv, "hw_req_info failed, retrying"});
        }
    }
    std::stringstream logs;
    if(rv < 0)
    {
        // Some KIM units appear to ignore HW_REQ_INFO over this transport.
        // Continue init and fall back to channel-by-channel probes.
        log<text_log>("hw_req_info failed; continuing with per-channel probing", logPrio::LOG_WARNING);
    }
    else
    {
        hwi.dump(logs);
        log<text_log>(logs.str());
    }

    // Stop automatic update messages
    rv = -1;
    for(int attempt = 0; attempt < 3; ++attempt)
    {
        rv = m_kcube.hw_stop_updatemsgs();
        if(rv >= 0) break;

        sleep(1);
        if(m_powerState == 0) return -1;

        if(attempt < 2)
        {
            log<software_error>({__FILE__, __LINE__, 0, rv, "hw_stop_updatemsgs failed, retrying"});
        }
    }
    if(rv < 0)
    {
        // Non-fatal: if periodic updates are not active, this may be unsupported/ignored.
        log<text_log>("hw_stop_updatemsgs failed; continuing initialization", logPrio::LOG_WARNING);
    }

    // Detect currently enabled channel mode at startup and reflect this in INDI toggles.
    // Do not force-disable all channels here.
    {
        tmcController::KIMChanEnableMode mode;
        rv = m_kcube.kim_req_chan_enable_mode(mode, false);
        if(rv >= 0)
        {
            uint8_t mask = 0;
            if(KIMEnableModeToMask(mode, mask))
            {
                m_enabledMask = mask;
            }
        }
        else
        {
            m_enabledMask = 0;
        }

        for(int ch = 0; ch < NumChannels; ++ch)
        {
            m_channels[ch].enabled = (m_enabledMask & channelIdent(ch+1)) != 0;
        }
    }

    // Set drive parameters for all channels
    for(int ch = 1; ch <= NumChannels; ch++)
    {
        rv = m_kcube.kim_set_driveop_params(channelIdent(ch), m_driveParams);
        if(rv < 0)
        {
            sleep(1);
            if(m_powerState == 0) return -1;
            log<software_error>({__FILE__, __LINE__, 0, rv, "failed to set drive params for channel " + std::to_string(ch)});
            return -1;
        }

        rv = m_kcube.kim_set_jog_params(channelIdent(ch), m_jogParams);
        if(rv < 0)
        {
            sleep(1);
            if(m_powerState == 0) return -1;
            log<software_error>({__FILE__, __LINE__, 0, rv, "failed to set jog params for channel " + std::to_string(ch)});
            return -1;
        }
    }

    // Log the configured parameters
    logs.str("");
    m_driveParams.dump(logs);
    log<text_log>(logs.str());

    logs.str("");
    m_jogParams.dump(logs);
    log<text_log>(logs.str());

    // Prime startup values by querying each channel explicitly.
    // This sequence matched the first successful RX runs on KIM101.
    queryChannelsIndividually(false);

    return 0;
}

int kim101Ctrl::updateStatus()
{
    int rv = m_kcube.kim_req_statusupdate(m_status);
    if(rv < 0)
    {
        // Fallback path for controllers that do not return the packed 4-channel status.
        return queryChannelsIndividually(true);
    }

    // Primary fast-path: parse one packed status message containing all channels.
    for(int ch = 0; ch < NumChannels; ++ch)
    {
        m_channels[ch].position = m_status.channels[ch].position;
        m_channels[ch].moving = m_status.channels[ch].isMoving();
    }

    for(int ch = 0; ch < NumChannels; ++ch)
    {
        m_channels[ch].enabled = (m_enabledMask & channelIdent(ch+1)) != 0;
    }

    return 0;
}

int kim101Ctrl::queryChannelsIndividually(bool logErrors)
{
    int failures = 0;
    for(int ch = 1; ch <= NumChannels; ++ch)
    {
        int32_t pos = 0;
        int rv = m_kcube.kim_req_poscounts(channelIdent(ch), pos);
        if(rv < 0)
        {
            ++failures;
            if(logErrors)
            {
                log<software_error>({__FILE__, __LINE__, 0, rv, "kim_req_poscounts failed for channel " + std::to_string(ch)});
            }
            continue;
        }

        m_channels[ch-1].position = pos;
        m_channels[ch-1].moving = false;

        // Do not request 0x0211/0x0212 here; it adds timeout-heavy traffic on KIM.
        // Enabled state is tracked via the requested KIM channel-enable mode.
    }

    if(failures == NumChannels)
    {
        return -1;
    }

    return 0;
}

int kim101Ctrl::channelEnable(int ch)
{
    if(ch < 1 || ch > NumChannels)
    {
        log<software_error>({__FILE__, __LINE__, "invalid channel number: " + std::to_string(ch)});
        return -1;
    }

    if(m_useKIMEnableMode)
    {
        uint8_t newMask = static_cast<uint8_t>(channelIdent(ch));
        tmcController::KIMChanEnableMode mode;
        if(!maskToKIMEnableMode(newMask, mode))
        {
            log<software_error>({__FILE__, __LINE__, "unsupported KIM enable mask " + std::to_string(newMask)});
            return -1;
        }

        int rv = m_kcube.kim_set_chan_enable_mode(mode);
        if(rv < 0)
        {
            sleep(1);
            if(m_powerState == 0) return -1;
            log<software_error>({__FILE__, __LINE__, 0, rv, "failed to enable channel " + std::to_string(ch)});
            return -1;
        }

        m_enabledMask = newMask;
        for(int i = 0; i < NumChannels; ++i)
        {
            m_channels[i].enabled = (m_enabledMask & channelIdent(i+1)) != 0;
        }
        tmcController::KIMChanEnableMode rbMode;
        rv = m_kcube.kim_req_chan_enable_mode(rbMode, false);
        if(rv >= 0 && rbMode != mode)
        {
            log<text_log>("KIM enable-mode readback mismatch after enable request", logPrio::LOG_WARNING);
        }
    }
    else
    {
        int rv = m_kcube.mod_set_chanenablestate(channelIdent(ch), tmcController::EnableState::enabled);
        if(rv < 0)
        {
            sleep(1);
            if(m_powerState == 0) return -1;
            log<software_error>({__FILE__, __LINE__, 0, rv, "failed to enable channel " + std::to_string(ch)});
            return -1;
        }

        // Verify whether the legacy enable command actually latched for KIM.
        // KIM status enable bits are unreliable, so still enforce one-active-channel mode below.
        bool latched = false;
        if(m_kcube.kim_req_statusupdate(m_status, false) >= 0)
        {
            latched = m_status.channels[ch-1].channelEnabled();
        }

        // Force KIM one-channel mode so INDI toggles and hardware selection stay consistent.
        tmcController::KIMChanEnableMode mode;
        if(!maskToKIMEnableMode(static_cast<uint8_t>(channelIdent(ch)), mode))
        {
            log<software_error>({__FILE__, __LINE__, "unable to map channel to KIM enable mode"});
            return -1;
        }

        rv = m_kcube.kim_set_chan_enable_mode(mode, false);
        if(rv < 0)
        {
            log<software_error>({__FILE__, __LINE__, 0, rv, "failed to set KIM one-channel mode for channel " + std::to_string(ch)});
            return -1;
        }

        if(!latched)
        {
            log<text_log>("legacy enable did not latch; used KIM 0x2B mode set for channel " + std::to_string(ch), logPrio::LOG_WARNING);
        }

        m_enabledMask = static_cast<uint8_t>(channelIdent(ch));
        for(int i = 0; i < NumChannels; ++i)
        {
            m_channels[i].enabled = (m_enabledMask & channelIdent(i+1)) != 0;
        }
    }
    log<text_log>("enabled channel " + std::to_string(ch), logPrio::LOG_NOTICE);

    return 0;
}

int kim101Ctrl::channelDisable(int ch)
{
    if(ch < 1 || ch > NumChannels)
    {
        log<software_error>({__FILE__, __LINE__, "invalid channel number: " + std::to_string(ch)});
        return -1;
    }

    if(m_useKIMEnableMode)
    {
        uint8_t newMask = (m_enabledMask & static_cast<uint8_t>(channelIdent(ch))) ? 0x00 : m_enabledMask;
        tmcController::KIMChanEnableMode mode;
        if(!maskToKIMEnableMode(newMask, mode))
        {
            log<software_error>({__FILE__, __LINE__, "unsupported KIM enable mask " + std::to_string(newMask)});
            return -1;
        }

        int rv = m_kcube.kim_set_chan_enable_mode(mode);
        if(rv < 0)
        {
            sleep(1);
            if(m_powerState == 0) return -1;
            log<software_error>({__FILE__, __LINE__, 0, rv, "failed to disable channel " + std::to_string(ch)});
            return -1;
        }

        m_enabledMask = newMask;
        for(int i = 0; i < NumChannels; ++i)
        {
            m_channels[i].enabled = (m_enabledMask & channelIdent(i+1)) != 0;
        }
        tmcController::KIMChanEnableMode rbMode;
        rv = m_kcube.kim_req_chan_enable_mode(rbMode, false);
        if(rv >= 0 && rbMode != mode)
        {
            log<text_log>("KIM enable-mode readback mismatch after disable request", logPrio::LOG_WARNING);
        }
    }
    else
    {
        int rv = m_kcube.mod_set_chanenablestate(channelIdent(ch), tmcController::EnableState::disabled);
        if(rv < 0)
        {
            sleep(1);
            if(m_powerState == 0) return -1;
            log<software_error>({__FILE__, __LINE__, 0, rv, "failed to disable channel " + std::to_string(ch)});
            return -1;
        }

        if(m_enabledMask & static_cast<uint8_t>(channelIdent(ch)))
        {
            rv = m_kcube.kim_set_chan_enable_mode(tmcController::KIMChanEnableMode::None, false);
            if(rv < 0)
            {
                log<software_error>({__FILE__, __LINE__, 0, rv, "failed to clear KIM one-channel mode while disabling channel " + std::to_string(ch)});
                return -1;
            }
            m_enabledMask = 0;
        }

        m_channels[ch-1].enabled = false;
        m_enabledMask &= ~static_cast<uint8_t>(channelIdent(ch));
    }
    log<text_log>("disabled channel " + std::to_string(ch), logPrio::LOG_NOTICE);

    return 0;
}

int kim101Ctrl::channelMoveAbsolute(int ch, int32_t position)
{
    if(ch < 1 || ch > NumChannels)
    {
        log<software_error>({__FILE__, __LINE__, "invalid channel number: " + std::to_string(ch)});
        return -1;
    }

    int rv = m_kcube.kim_move_absolute(channelIdent(ch), position);
    if(rv < 0)
    {
        sleep(1);
        if(m_powerState == 0) return -1;
        log<software_error>({__FILE__, __LINE__, 0, rv, "move absolute failed for channel " + std::to_string(ch)});
        return -1;
    }

    m_channels[ch-1].targetPosition = position;
    switch(ch)
    {
        case 1: updateIfChanged(m_indiP_ch1_position, "target", position, INDI_BUSY); break;
        case 2: updateIfChanged(m_indiP_ch2_position, "target", position, INDI_BUSY); break;
        case 3: updateIfChanged(m_indiP_ch3_position, "target", position, INDI_BUSY); break;
        case 4: updateIfChanged(m_indiP_ch4_position, "target", position, INDI_BUSY); break;
    }
    log<text_log>("channel " + std::to_string(ch) + " moving to " + std::to_string(position), logPrio::LOG_NOTICE);

    return 0;
}

int kim101Ctrl::channelJog(int ch, int direction)
{
    if(ch < 1 || ch > NumChannels)
    {
        log<software_error>({__FILE__, __LINE__, "invalid channel number: " + std::to_string(ch)});
        return -1;
    }

    tmcController::KIMJogDir dir = (direction > 0) ? tmcController::KIMJogDir::Forward : tmcController::KIMJogDir::Reverse;

    int rv = m_kcube.kim_move_jog(channelIdent(ch), dir);
    if(rv < 0)
    {
        sleep(1);
        if(m_powerState == 0) return -1;
        log<software_error>({__FILE__, __LINE__, 0, rv, "jog failed for channel " + std::to_string(ch)});
        return -1;
    }

    log<text_log>("channel " + std::to_string(ch) + " jogging " + (direction > 0 ? "forward" : "reverse"), logPrio::LOG_NOTICE);

    return 0;
}

int kim101Ctrl::channelStop(int ch)
{
    if(ch < 1 || ch > NumChannels)
    {
        log<software_error>({__FILE__, __LINE__, "invalid channel number: " + std::to_string(ch)});
        return -1;
    }

    int rv = m_kcube.kim_move_stop(channelIdent(ch));
    if(rv < 0)
    {
        sleep(1);
        if(m_powerState == 0) return -1;
        log<software_error>({__FILE__, __LINE__, 0, rv, "stop failed for channel " + std::to_string(ch)});
        return -1;
    }

    log<text_log>("channel " + std::to_string(ch) + " stopped", logPrio::LOG_NOTICE);

    return 0;
}

int kim101Ctrl::channelZero(int ch)
{
    if(ch < 1 || ch > NumChannels)
    {
        log<software_error>({__FILE__, __LINE__, "invalid channel number: " + std::to_string(ch)});
        return -1;
    }

    int rv = m_kcube.kim_set_poscounts(channelIdent(ch), 0);
    if(rv < 0)
    {
        sleep(1);
        if(m_powerState == 0) return -1;
        log<software_error>({__FILE__, __LINE__, 0, rv, "zero failed for channel " + std::to_string(ch)});
        return -1;
    }

    m_channels[ch-1].position = 0;
    m_channels[ch-1].targetPosition = 0;
    log<text_log>("channel " + std::to_string(ch) + " position zeroed", logPrio::LOG_NOTICE);

    return 0;
}

//=============================================================================
// INDI Callbacks
//=============================================================================

INDI_NEWCALLBACK_DEFN(kim101Ctrl, m_indiP_identify)(const pcf::IndiProperty &ipRecv)
{
    INDI_VALIDATE_CALLBACK_PROPS(m_indiP_identify, ipRecv);

    if(state() != stateCodes::READY && state() != stateCodes::OPERATING) return 0;

    if(ipRecv["request"].getSwitchState() == pcf::IndiElement::On)
    {
        std::lock_guard<std::mutex> guard(m_indiMutex);
        updateSwitchIfChanged(m_indiP_identify, "request", pcf::IndiElement::On, INDI_BUSY);
        return m_kcube.mod_identify();
    }
   
    return 0;
}

// Channel 1 callbacks
INDI_NEWCALLBACK_DEFN(kim101Ctrl, m_indiP_ch1_enable)(const pcf::IndiProperty &ipRecv)
{
    INDI_VALIDATE_CALLBACK_PROPS(m_indiP_ch1_enable, ipRecv);

    if(!(state() == stateCodes::READY || state() == stateCodes::OPERATING)) return 0;
    
    if(ipRecv["toggle"].getSwitchState() == pcf::IndiElement::On)
    {
        std::lock_guard<std::mutex> guard(m_indiMutex);
        if(channelEnable(1) < 0)
        {
            if(m_powerState == 0) return 0;
            return log<software_error,-1>({__FILE__, __LINE__, "channel 1 enable failed"});
        }
    }
    else
    {
        std::lock_guard<std::mutex> guard(m_indiMutex);
        if(channelDisable(1) < 0)
        {
            if(m_powerState == 0) return 0;
            return log<software_error,-1>({__FILE__, __LINE__, "channel 1 disable failed"});
        }
    }
   
    return 0;
}

INDI_NEWCALLBACK_DEFN(kim101Ctrl, m_indiP_ch1_position)(const pcf::IndiProperty &ipRecv)
{
    INDI_VALIDATE_CALLBACK_PROPS(m_indiP_ch1_position, ipRecv);

    if(!(state() == stateCodes::READY || state() == stateCodes::OPERATING)) return 0;

    int32_t target;
    indiTargetUpdate(m_indiP_ch1_position, target, ipRecv, true);

    std::lock_guard<std::mutex> guard(m_indiMutex);
    if(channelMoveAbsolute(1, target) < 0)
    {
        if(m_powerState == 0) return 0;
        return log<software_error,-1>({__FILE__, __LINE__, "channel 1 move failed"});
    }
    
    return 0;
}

INDI_NEWCALLBACK_DEFN(kim101Ctrl, m_indiP_ch1_stop)(const pcf::IndiProperty &ipRecv)
{
    INDI_VALIDATE_CALLBACK_PROPS(m_indiP_ch1_stop, ipRecv);

    if(!(state() == stateCodes::READY || state() == stateCodes::OPERATING)) return 0;

    if(ipRecv["request"].getSwitchState() == pcf::IndiElement::On)
    {
        std::lock_guard<std::mutex> guard(m_indiMutex);
        return channelStop(1);
    }
   
    return 0;
}

INDI_NEWCALLBACK_DEFN(kim101Ctrl, m_indiP_ch1_zero)(const pcf::IndiProperty &ipRecv)
{
    INDI_VALIDATE_CALLBACK_PROPS(m_indiP_ch1_zero, ipRecv);

    if(!(state() == stateCodes::READY || state() == stateCodes::OPERATING)) return 0;

    if(ipRecv["request"].getSwitchState() == pcf::IndiElement::On)
    {
        std::lock_guard<std::mutex> guard(m_indiMutex);
        return channelZero(1);
    }
   
    return 0;
}

// Channel 2 callbacks
INDI_NEWCALLBACK_DEFN(kim101Ctrl, m_indiP_ch2_enable)(const pcf::IndiProperty &ipRecv)
{
    INDI_VALIDATE_CALLBACK_PROPS(m_indiP_ch2_enable, ipRecv);

    if(!(state() == stateCodes::READY || state() == stateCodes::OPERATING)) return 0;
    
    if(ipRecv["toggle"].getSwitchState() == pcf::IndiElement::On)
    {
        std::lock_guard<std::mutex> guard(m_indiMutex);
        if(channelEnable(2) < 0)
        {
            if(m_powerState == 0) return 0;
            return log<software_error,-1>({__FILE__, __LINE__, "channel 2 enable failed"});
        }
    }
    else
    {
        std::lock_guard<std::mutex> guard(m_indiMutex);
        if(channelDisable(2) < 0)
        {
            if(m_powerState == 0) return 0;
            return log<software_error,-1>({__FILE__, __LINE__, "channel 2 disable failed"});
        }
    }
   
    return 0;
}

INDI_NEWCALLBACK_DEFN(kim101Ctrl, m_indiP_ch2_position)(const pcf::IndiProperty &ipRecv)
{
    INDI_VALIDATE_CALLBACK_PROPS(m_indiP_ch2_position, ipRecv);

    if(!(state() == stateCodes::READY || state() == stateCodes::OPERATING)) return 0;

    int32_t target;
    indiTargetUpdate(m_indiP_ch2_position, target, ipRecv, true);

    std::lock_guard<std::mutex> guard(m_indiMutex);
    if(channelMoveAbsolute(2, target) < 0)
    {
        if(m_powerState == 0) return 0;
        return log<software_error,-1>({__FILE__, __LINE__, "channel 2 move failed"});
    }
    
    return 0;
}

INDI_NEWCALLBACK_DEFN(kim101Ctrl, m_indiP_ch2_stop)(const pcf::IndiProperty &ipRecv)
{
    INDI_VALIDATE_CALLBACK_PROPS(m_indiP_ch2_stop, ipRecv);

    if(!(state() == stateCodes::READY || state() == stateCodes::OPERATING)) return 0;

    if(ipRecv["request"].getSwitchState() == pcf::IndiElement::On)
    {
        std::lock_guard<std::mutex> guard(m_indiMutex);
        return channelStop(2);
    }
   
    return 0;
}

INDI_NEWCALLBACK_DEFN(kim101Ctrl, m_indiP_ch2_zero)(const pcf::IndiProperty &ipRecv)
{
    INDI_VALIDATE_CALLBACK_PROPS(m_indiP_ch2_zero, ipRecv);

    if(!(state() == stateCodes::READY || state() == stateCodes::OPERATING)) return 0;

    if(ipRecv["request"].getSwitchState() == pcf::IndiElement::On)
    {
        std::lock_guard<std::mutex> guard(m_indiMutex);
        return channelZero(2);
    }
   
    return 0;
}

// Channel 3 callbacks
INDI_NEWCALLBACK_DEFN(kim101Ctrl, m_indiP_ch3_enable)(const pcf::IndiProperty &ipRecv)
{
    INDI_VALIDATE_CALLBACK_PROPS(m_indiP_ch3_enable, ipRecv);

    if(!(state() == stateCodes::READY || state() == stateCodes::OPERATING)) return 0;
    
    if(ipRecv["toggle"].getSwitchState() == pcf::IndiElement::On)
    {
        std::lock_guard<std::mutex> guard(m_indiMutex);
        if(channelEnable(3) < 0)
        {
            if(m_powerState == 0) return 0;
            return log<software_error,-1>({__FILE__, __LINE__, "channel 3 enable failed"});
        }
    }
    else
    {
        std::lock_guard<std::mutex> guard(m_indiMutex);
        if(channelDisable(3) < 0)
        {
            if(m_powerState == 0) return 0;
            return log<software_error,-1>({__FILE__, __LINE__, "channel 3 disable failed"});
        }
    }
   
    return 0;
}

INDI_NEWCALLBACK_DEFN(kim101Ctrl, m_indiP_ch3_position)(const pcf::IndiProperty &ipRecv)
{
    INDI_VALIDATE_CALLBACK_PROPS(m_indiP_ch3_position, ipRecv);

    if(!(state() == stateCodes::READY || state() == stateCodes::OPERATING)) return 0;

    int32_t target;
    indiTargetUpdate(m_indiP_ch3_position, target, ipRecv, true);

    std::lock_guard<std::mutex> guard(m_indiMutex);
    if(channelMoveAbsolute(3, target) < 0)
    {
        if(m_powerState == 0) return 0;
        return log<software_error,-1>({__FILE__, __LINE__, "channel 3 move failed"});
    }
    
    return 0;
}

INDI_NEWCALLBACK_DEFN(kim101Ctrl, m_indiP_ch3_stop)(const pcf::IndiProperty &ipRecv)
{
    INDI_VALIDATE_CALLBACK_PROPS(m_indiP_ch3_stop, ipRecv);

    if(!(state() == stateCodes::READY || state() == stateCodes::OPERATING)) return 0;

    if(ipRecv["request"].getSwitchState() == pcf::IndiElement::On)
    {
        std::lock_guard<std::mutex> guard(m_indiMutex);
        return channelStop(3);
    }
   
    return 0;
}

INDI_NEWCALLBACK_DEFN(kim101Ctrl, m_indiP_ch3_zero)(const pcf::IndiProperty &ipRecv)
{
    INDI_VALIDATE_CALLBACK_PROPS(m_indiP_ch3_zero, ipRecv);

    if(!(state() == stateCodes::READY || state() == stateCodes::OPERATING)) return 0;

    if(ipRecv["request"].getSwitchState() == pcf::IndiElement::On)
    {
        std::lock_guard<std::mutex> guard(m_indiMutex);
        return channelZero(3);
    }
   
    return 0;
}

// Channel 4 callbacks
INDI_NEWCALLBACK_DEFN(kim101Ctrl, m_indiP_ch4_enable)(const pcf::IndiProperty &ipRecv)
{
    INDI_VALIDATE_CALLBACK_PROPS(m_indiP_ch4_enable, ipRecv);

    if(!(state() == stateCodes::READY || state() == stateCodes::OPERATING)) return 0;
    
    if(ipRecv["toggle"].getSwitchState() == pcf::IndiElement::On)
    {
        std::lock_guard<std::mutex> guard(m_indiMutex);
        if(channelEnable(4) < 0)
        {
            if(m_powerState == 0) return 0;
            return log<software_error,-1>({__FILE__, __LINE__, "channel 4 enable failed"});
        }
    }
    else
    {
        std::lock_guard<std::mutex> guard(m_indiMutex);
        if(channelDisable(4) < 0)
        {
            if(m_powerState == 0) return 0;
            return log<software_error,-1>({__FILE__, __LINE__, "channel 4 disable failed"});
        }
    }
   
    return 0;
}

INDI_NEWCALLBACK_DEFN(kim101Ctrl, m_indiP_ch4_position)(const pcf::IndiProperty &ipRecv)
{
    INDI_VALIDATE_CALLBACK_PROPS(m_indiP_ch4_position, ipRecv);

    if(!(state() == stateCodes::READY || state() == stateCodes::OPERATING)) return 0;

    int32_t target;
    indiTargetUpdate(m_indiP_ch4_position, target, ipRecv, true);

    std::lock_guard<std::mutex> guard(m_indiMutex);
    if(channelMoveAbsolute(4, target) < 0)
    {
        if(m_powerState == 0) return 0;
        return log<software_error,-1>({__FILE__, __LINE__, "channel 4 move failed"});
    }
    
    return 0;
}

INDI_NEWCALLBACK_DEFN(kim101Ctrl, m_indiP_ch4_stop)(const pcf::IndiProperty &ipRecv)
{
    INDI_VALIDATE_CALLBACK_PROPS(m_indiP_ch4_stop, ipRecv);

    if(!(state() == stateCodes::READY || state() == stateCodes::OPERATING)) return 0;

    if(ipRecv["request"].getSwitchState() == pcf::IndiElement::On)
    {
        std::lock_guard<std::mutex> guard(m_indiMutex);
        return channelStop(4);
    }
   
    return 0;
}

INDI_NEWCALLBACK_DEFN(kim101Ctrl, m_indiP_ch4_zero)(const pcf::IndiProperty &ipRecv)
{
    INDI_VALIDATE_CALLBACK_PROPS(m_indiP_ch4_zero, ipRecv);

    if(!(state() == stateCodes::READY || state() == stateCodes::OPERATING)) return 0;

    if(ipRecv["request"].getSwitchState() == pcf::IndiElement::On)
    {
        std::lock_guard<std::mutex> guard(m_indiMutex);
        return channelZero(4);
    }
   
    return 0;
}

} // namespace app
} // namespace MagAOX

#endif // kim101Ctrl_hpp

