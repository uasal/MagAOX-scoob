/** \file scpiPowerCtrl.hpp
  * \brief The MagAO-X SCPI-standard DC Power Supply controller.
  *
  * \author Adam A. Schilperoort (adamschilperoort@gmail.com)
  *
  * \ingroup scpiPowerCtrl_files
  */

#ifndef scpiPowerCtrl_hpp
#define scpiPowerCtrl_hpp


#include "../../libMagAOX/libMagAOX.hpp" //Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>
#include <atomic>
#include <sys/select.h>

/** \defgroup scpiPowerCtrl SCPI Power Supply
  * \brief Control of MagAO-X SCPI-standard DC Power Supplies.
  *
  * <a href="../handbook/operating/software/apps/scpiPowerCtrl.html">Application Documentation</a>
  *
  * \ingroup apps
  *
  */

/** \defgroup scpiPowerCtrl_files SCPI Power Supply Files
  * \ingroup scpiPowerCtrl
  */

namespace MagAOX
{
namespace app
{

/// How \ref scpiPowerCtrl reaches the instrument (matches `device.protocol` in config).
enum class scpiPowerDeviceTransport
{
   Usb,    ///< `device.address` is a USB-TMC path (e.g. `/dev/usbtmc0`).
   Ipv4,   ///< `device.address` is a host IPv4 address; SCPI is raw TCP on `device.port` (default 5025).
   Invalid ///< `device.protocol` was not recognized after load.
};

/// MagAO-X application to control any DC power supply that supports the SCPI standard.
/** The available outputs are organized into channels.  See \ref dev::outletController for details of configuring the channels.
  *
  * Each active channel's amp and volts are monitored 
  * 
  * \todo begin logging freq/volt/amps telemetry
  * \todo segfaults if device can not be reached on network -- make this an issue
  * 
  * \ingroup scpiPowerCtrl
  */
class scpiPowerCtrl : public MagAOXApp<>, public dev::outletController<scpiPowerCtrl>, public dev::ioDevice
{

protected:

   std::string m_deviceAddr; ///< USB: path (e.g. `/dev/usbtmc0`). IPv4: host address only.
   scpiPowerDeviceTransport m_deviceTransport {scpiPowerDeviceTransport::Usb};
   int m_port {5025}; ///< TCP port when `m_deviceTransport == Ipv4` (typical raw SCPI: 5025).
   int m_socketFd {-1}; ///< Connected TCP socket when using IPv4 transport.
   int m_connectTimeoutMs {5000}; ///< TCP connect timeout [ms], same idea as \ref scpiCtrl.

   // arrays for all high and low limits for volts and amps because defined independently for each channel

    struct ChannelLimits {
        float voltHighLimit = 240.0f;
        float voltLowLimit = 0.0f;
        float currHighLimit = 50.0f;
        float currLowLimit = 0.0f;
    };

    std::vector<ChannelLimits> m_channelLimits;
    std::vector<float> m_channelVoltages;        ///< Measured channel voltages (MEAS:VOLT?).
    std::vector<float> m_channelCurrents;        ///< Measured channel currents (MEAS:CURR?).
    std::vector<float> m_channelSetVoltages;     ///< Active device voltage setpoints (VOLT?).
    std::vector<float> m_channelSetCurrents;     ///< Active device current setpoints (CURR?).
    std::vector<float> m_channelTargetVoltages;
    std::vector<float> m_channelTargetCurrents;
    int m_numChannels = 4; ///< The number of channels on the device -- abandoning dynamic, hard-coding 3 for now
    int m_currentChannel = 0; ///< The current channel being monitored

    int maxChannels = 4; // define maximum number of power channels

    int fd {-1}; ///< USB-TMC file descriptor when using USB transport
    int m_pollRateHz {100};  ///< The polling rate for measurements [Hz].

    // array for voltages with length numChannels when it gets set
    // array for amps with length numChannels when it gets set

   std::string m_status; ///< The device status 

   // Telemetry logging
   bool m_telemetryEnabled {false}; ///< Whether telemetry logging is enabled
   std::string m_telemetryPath {"/opt/MagAOX/telem"}; ///< Path for telemetry files
   std::ofstream m_telemetryFile; ///< File stream for telemetry logging
   std::string m_telemetryFilename; ///< Current telemetry filename
   std::chrono::steady_clock::time_point m_lastTelemetryTime; ///< Last telemetry write time
   std::chrono::milliseconds m_telemetryInterval {10}; ///< Telemetry interval in milliseconds (100Hz = 10ms)

   // Polling control
   std::chrono::steady_clock::time_point m_lastPollTime; ///< Last poll time
   std::chrono::milliseconds m_pollInterval {10}; ///< Polling interval derived from m_pollRateHz

   // High-rate polling thread
   std::thread m_pollThread;
   std::atomic<bool> m_polling {false};
   bool m_pollThreadStarted {false};

   // array for statuses on each channel (On Off) ?

//    std::vector<pcf::IndiProperty> m_indiP_blockVolts;
//    std::vector<pcf::IndiProperty> m_indiP_blockGains;

//    pcf::IndiProperty m_indiP_singleVolt;
//    pcf::IndiProperty m_indiP_singleCurr;

public:

    INDI_NEWCALLBACK_DECL(scpiPowerCtrl, m_indiP_outlet1volt);
    INDI_NEWCALLBACK_DECL(scpiPowerCtrl, m_indiP_outlet2volt);
    INDI_NEWCALLBACK_DECL(scpiPowerCtrl, m_indiP_outlet3volt);
    INDI_NEWCALLBACK_DECL(scpiPowerCtrl, m_indiP_outlet4volt);

    INDI_NEWCALLBACK_DECL(scpiPowerCtrl, m_indiP_outlet1curr);
    INDI_NEWCALLBACK_DECL(scpiPowerCtrl, m_indiP_outlet2curr);
    INDI_NEWCALLBACK_DECL(scpiPowerCtrl, m_indiP_outlet3curr); 
    INDI_NEWCALLBACK_DECL(scpiPowerCtrl, m_indiP_outlet4curr);

    // Telemetry control
    INDI_NEWCALLBACK_DECL(scpiPowerCtrl, m_indiP_telemetryToggle);
    
    // Power control toggles (using outletController framework)
    // These are handled by the outletController base class

    //INDI_NEWCALLBACK_DECL(scpiPowerCtrl, m_indiP_load_channels);

    //std::vector<pcf::IndiProperty> m_indiP_load_channels;   

    // INDI_NEWCALLBACK_DECL(scpiPowerCtrl, m_indiP_singleVolt);
    // INDI_NEWCALLBACK_DECL(scpiPowerCtrl, m_indiP_singleCurr);

    /// Default c'tor.
    scpiPowerCtrl();
 
    /// D'tor, declared and defined for noexcept.
    ~scpiPowerCtrl() noexcept
    {
       stopPollThread();
       devDisconnect();
    }
 
    /** \name MagAOXApp Interface
      *
      * @{ 
      */

    /// Setup the configuration system (called by MagAOXApp::setup())
    virtual void setupConfig();
 
    /// load the configuration system results (called by MagAOXApp::setup())
    virtual void loadConfig();
 
    /// Startup functions
    /** Setsup the INDI vars.
      * Checks if the device was found during loadConfig.
      */
    virtual int appStartup();
 
    /// Implementation of the FSM for the tripp lite PDU.
    virtual int appLogic();
 
    /// Do any needed shutdown tasks.  Currently nothing in this app.
    virtual int appShutdown();
 
    ///@}

    /** \name outletController Interface
      *
      * @{ 
      */

    /// Update a single outlet state
    /** For the scpiPowerCtrl this isn't possible for a single outlet, so this calls updateOutletStates.
      *
      * \returns 0 on success
      * \returns -1 on error
      */
    virtual int updateOutletState( int outletNum /**< [in] the outlet number to update */);
 
    /// Queries the device to find the state of each outlet, as well as other parameters.
    /** Sends `devstatus` to the device, and parses the result.
      *
      * \returns 0 on success
      * \returns -1 on error
      */
    virtual int updateOutletStates();
 
    /// Turn on an outlet.
    /**
      * \returns 0 on success
      * \returns -1 on error
      */
    virtual int turnOutletOn( int outletNum /**< [in] the outlet number to turn on */);
 
    /// Turn off an outlet.
    /**
      * \returns 0 on success
      * \returns -1 on error
      */
    virtual int turnOutletOff( int outletNum /**< [in] the outlet number to turn off */);
 
    ///@}

    /** \name Device Interface 
      *
      */
    int devConnect();

    /// Close USB and/or TCP resources (safe to call multiple times).
    int devDisconnect();

    int devStatus();
    
    int updateChannels();
    
    int updateChannel(int channel);
    
    int setPollRate();
    
    int setChannelVolts(int channel, double volts);

    int setChannelAmps(int channel, double amps);

    int setChannelHighVolt(int channel, double highVolt);
    
    int setChannelLowVolt(int channel, double lowVolt);
    
    int setChannelHighCurr(int channel, double highCurr);
    
    int setChannelLowCurr(int channel, double lowCurr);

    int applyChannelSetpoint(int channel, bool isCurrent, double requestedValue);
    int handleSetpointCallback(pcf::IndiProperty &localProp,
                               const pcf::IndiProperty &ipRecv,
                               int channel,
                               bool isCurrent,
                               const std::string &desc);
    int forceAllOutputsOff();
    int applyConfiguredSetpoints();
    
    void updateAlarmsAndWarnings();

    bool send_scpi(const std::string& cmd, std::string& response);
    
    // High-rate polling helpers
    void startPollThread();
    void stopPollThread();
    void pollLoop();
    
    // Telemetry logging methods
    int startTelemetryLogging();
    int stopTelemetryLogging();
    int writeTelemetryData();
    std::string generateTelemetryFilename();
    
    // Power control methods
    int toggleChannelPower(int channel, bool enable);

    ///@}

protected:

   //declare our properties
   pcf::IndiProperty m_indiP_status; ///< The device's status string
   
   // could not get dynamic-length vector of these to work with callbacks so here they are hard-coded..
   pcf::IndiProperty m_indiP_outlet1volt;
   pcf::IndiProperty m_indiP_outlet2volt;
   pcf::IndiProperty m_indiP_outlet3volt;
   pcf::IndiProperty m_indiP_outlet4volt;
   
   pcf::IndiProperty m_indiP_outlet1curr;
   pcf::IndiProperty m_indiP_outlet2curr;
   pcf::IndiProperty m_indiP_outlet3curr; 
   pcf::IndiProperty m_indiP_outlet4curr;
   pcf::IndiProperty m_indiP_outlet1volt_meas;
   pcf::IndiProperty m_indiP_outlet2volt_meas;
   pcf::IndiProperty m_indiP_outlet3volt_meas;
   pcf::IndiProperty m_indiP_outlet4volt_meas;
   pcf::IndiProperty m_indiP_outlet1curr_meas;
   pcf::IndiProperty m_indiP_outlet2curr_meas;
   pcf::IndiProperty m_indiP_outlet3curr_meas;
   pcf::IndiProperty m_indiP_outlet4curr_meas;

   // Telemetry control
   pcf::IndiProperty m_indiP_telemetryToggle;
   
   // Power control toggles are handled by outletController framework

   int connectUSB();
   int connectTCP();

};

scpiPowerCtrl::scpiPowerCtrl() : MagAOXApp(MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED)
{
   m_firstOne = true;
   //setNumberOfOutlets(m_numChannels); // should be m_numChannels after connecting to the device
   return;
}

void scpiPowerCtrl::setupConfig()
{
    config.add("device.protocol", "", "device.protocol", argType::Required, "device", "protocol", false, "string", "Transport: 'usb' (address is path) or 'ipv4' (address is host, SCPI TCP on device.port).");
    config.add("device.address", "a", "device.address", argType::Required, "device", "address", false, "string", "USB: device path (e.g. /dev/usbtmc0). IPv4: host address only.");
    config.add("device.port", "", "device.port", argType::Optional, "device", "port", false, "int", "TCP port for ipv4 transport (default: 5025, raw SCPI).");
    config.add("device.connectTimeout", "", "device.connectTimeout", argType::Optional, "device", "connectTimeout", false, "int", "TCP connect timeout [ms] for ipv4 (default: 5000).");
    
    // force user to define the number of channels so that limits can be pre-defined
    config.add("device.numChannels", "", "device.numChannels", argType::Required, "device", "numChannels", false, "int", "The number of channels on the device.");
    
    // Polling rate configuration
    config.add("device.pollRateHz", "", "device.pollRateHz", argType::Optional, "device", "pollRateHz", false, "int", "The polling rate for measurements [Hz] (default: 100)");
    
    // Telemetry configuration
    config.add("telemetry.path", "", "telemetry.path", argType::Optional, "telemetry", "path", false, "string", "Path for telemetry files (default: /opt/MagAOX/telem)");
    config.add("telemetry.enabled", "", "telemetry.enabled", argType::Optional, "telemetry", "enabled", false, "bool", "Enable telemetry logging on startup (default: false)");

    dev::ioDevice::setupConfig(config);

    for (int i = 1; i <= maxChannels; ++i) {
        std::string prefix = "channel" + std::to_string(i) + ".limits";
        config.add(prefix + ".highVolt", "", prefix + ".highVolt", argType::Optional,
                   prefix, "highVolt", false, "float", "The high-voltage limit threshold");
        config.add(prefix + ".lowVolt", "", prefix + ".lowVolt", argType::Optional,
                   prefix, "lowVolt", false, "float", "The low-voltage limit threshold");
        config.add(prefix + ".highCurr", "", prefix + ".highCurr", argType::Optional,
                   prefix, "highCurr", false, "float", "The high-current limit threshold");
        config.add(prefix + ".lowCurr", "", prefix + ".lowCurr", argType::Optional,
                   prefix, "lowCurr", false, "float", "The low-current limit threshold");

        std::string defaultsPrefix = "channel" + std::to_string(i) + ".defaults";
        config.add(defaultsPrefix + ".volt", "", defaultsPrefix + ".volt", argType::Optional,
                   defaultsPrefix, "volt", false, "float", "Default target voltage set at startup.");
        config.add(defaultsPrefix + ".curr", "", defaultsPrefix + ".curr", argType::Optional,
                   defaultsPrefix, "curr", false, "float", "Default target current set at startup.");
    }

   dev::outletController<scpiPowerCtrl>::setupConfig(config);
   
}


void scpiPowerCtrl::loadConfig()
{
    config(m_deviceAddr, "device.address");

    std::string protoStr;
    config(protoStr, "device.protocol");
    for (char& c : protoStr) {
       if (c >= 'A' && c <= 'Z') {
          c = static_cast<char>(c - 'A' + 'a');
       }
    }
    if (protoStr == "usb") {
       m_deviceTransport = scpiPowerDeviceTransport::Usb;
    } else if (protoStr == "ipv4") {
       m_deviceTransport = scpiPowerDeviceTransport::Ipv4;
    } else {
       log<software_error>({__FILE__, __LINE__, "device.protocol must be 'usb' or 'ipv4', got: " + protoStr});
       m_deviceTransport = scpiPowerDeviceTransport::Invalid;
    }

    config(m_port, "device.port");
    if (m_port <= 0 || m_port > 65535) {
       log<software_error>({__FILE__, __LINE__, "device.port out of range, using 5025"});
       m_port = 5025;
    }
    config(m_connectTimeoutMs, "device.connectTimeout");
    if (m_connectTimeoutMs <= 0) {
       m_connectTimeoutMs = 5000;
    }

    dev::ioDevice::loadConfig(config);
    config(m_numChannels, "device.numChannels");
    config(m_pollRateHz, "device.pollRateHz");
    
    // Load telemetry configuration
    config(m_telemetryPath, "telemetry.path");
    config(m_telemetryEnabled, "telemetry.enabled");

    if (m_numChannels > maxChannels) {
        log<software_error>({__FILE__, __LINE__, "more channels defined than maximum allowed"});
    }

    setNumberOfOutlets(m_numChannels);
    m_channelLimits.resize(m_numChannels);
    m_channelVoltages.resize(m_numChannels);
    m_channelCurrents.resize(m_numChannels);
    m_channelSetVoltages.resize(m_numChannels, 0.0f);
    m_channelSetCurrents.resize(m_numChannels, 0.0f);
    m_channelTargetVoltages.resize(m_numChannels, 0.0f);
    m_channelTargetCurrents.resize(m_numChannels, 0.0f);

    for (int i = 0; i < m_numChannels; i++) {
        auto& ch = m_channelLimits[i];

        std::string prefix = "channel" + std::to_string(i + 1) + ".limits."; // channel1.limits.highVolt

        config(ch.voltHighLimit, prefix + "highVolt");
        config(ch.voltLowLimit,  prefix + "lowVolt");
        config(ch.currHighLimit, prefix + "highCurr");
        config(ch.currLowLimit,  prefix + "lowCurr");

        std::string defaultsPrefix = "channel" + std::to_string(i + 1) + ".defaults.";
        config(m_channelTargetVoltages[i], defaultsPrefix + "volt");
        config(m_channelTargetCurrents[i], defaultsPrefix + "curr");

        if (m_channelTargetVoltages[i] > ch.voltHighLimit) m_channelTargetVoltages[i] = ch.voltHighLimit;
        if (m_channelTargetVoltages[i] < ch.voltLowLimit) m_channelTargetVoltages[i] = ch.voltLowLimit;
        if (m_channelTargetCurrents[i] > ch.currHighLimit) m_channelTargetCurrents[i] = ch.currHighLimit;
        if (m_channelTargetCurrents[i] < ch.currLowLimit) m_channelTargetCurrents[i] = ch.currLowLimit;

        // Initialize desired and active setpoint mirrors from configured defaults.
        m_channelSetVoltages[i] = m_channelTargetVoltages[i];
        m_channelSetCurrents[i] = m_channelTargetCurrents[i];
        // Measured values come from MEAS:* polling, keep zero until first read.
        m_channelVoltages[i] = 0.0f;
        m_channelCurrents[i] = 0.0f;
    }

    /*  expecting this format

        [channel1.limits]
        highVolt = 230
        lowVolt = 10
        highCurr = 40
        lowCurr = 1

        [channel2.limits]
        highVolt = 220
        lowVolt = 5
        highCurr = 30
        lowCurr = 0.5
    */

   config(m_currentChannel, "device.currentChannel");

   dev::outletController<scpiPowerCtrl>::loadConfig(config);
   
}

int scpiPowerCtrl::appStartup()
{
    // set up the  INDI properties
    REG_INDI_NEWPROP_NOCB(m_indiP_status, "status", pcf::IndiProperty::Text);
    m_indiP_status.add (pcf::IndiElement("value"));

    //m_indiP_load_channels.resize(m_numChannels);

    //if(m_numChannels == 0)
   // {
    //    log<text_log>("0 power channels defined", logPrio::LOG_WARNING);
   // }

    //for (int i = 0; i < m_numChannels; i++) {
    //    std::string name = "load_ch" + std::to_string(i + 1);
    
        // REG_INDI_NEWPROP_NOCB(m_indiP_load_channels[i], name, pcf::IndiProperty::Number);
        // m_indiP_load_channels[i].add(pcf::IndiElement("current_voltage"));
        // m_indiP_load_channels[i].add(pcf::IndiElement("target_voltage"));
        // m_indiP_load_channels[i].add(pcf::IndiElement("current_current"));
        // m_indiP_load_channels[i].add(pcf::IndiElement("target_current"));
    
    //}
    
    createStandardIndiNumber<float>(m_indiP_outlet1volt, "ch_1_volt", -240.0, 240.0, 0.001, "%0.3f");
    m_indiP_outlet1volt["current"] = m_channelSetVoltages[0];
    m_indiP_outlet1volt["target"] = m_channelTargetVoltages[0];
    registerIndiPropertyNew(m_indiP_outlet1volt, INDI_NEWCALLBACK(m_indiP_outlet1volt));

    createStandardIndiNumber<float>(m_indiP_outlet1curr, "ch_1_curr", 0, 1000, 0.001, "%0.3f");
    m_indiP_outlet1curr["current"] = m_channelSetCurrents[0];
    m_indiP_outlet1curr["target"] = m_channelTargetCurrents[0];
    registerIndiPropertyNew(m_indiP_outlet1curr, INDI_NEWCALLBACK(m_indiP_outlet1curr));
    createROIndiNumber(m_indiP_outlet1volt_meas, "ch_1_volt_meas");
    m_indiP_outlet1volt_meas.add(pcf::IndiElement("current"));
    m_indiP_outlet1volt_meas["current"].setMin(-240.0);
    m_indiP_outlet1volt_meas["current"].setMax(240.0);
    m_indiP_outlet1volt_meas["current"].setStep(0.001);
    m_indiP_outlet1volt_meas["current"].setFormat("%0.3f");
    m_indiP_outlet1volt_meas["current"] = m_channelVoltages[0];
    if(registerIndiPropertyReadOnly(m_indiP_outlet1volt_meas) < 0) return log<text_log,-1>("Error registering ch_1_volt_meas", logPrio::LOG_CRITICAL);
    createROIndiNumber(m_indiP_outlet1curr_meas, "ch_1_curr_meas");
    m_indiP_outlet1curr_meas.add(pcf::IndiElement("current"));
    m_indiP_outlet1curr_meas["current"].setMin(0);
    m_indiP_outlet1curr_meas["current"].setMax(1000);
    m_indiP_outlet1curr_meas["current"].setStep(0.001);
    m_indiP_outlet1curr_meas["current"].setFormat("%0.3f");
    m_indiP_outlet1curr_meas["current"] = m_channelCurrents[0];
    if(registerIndiPropertyReadOnly(m_indiP_outlet1curr_meas) < 0) return log<text_log,-1>("Error registering ch_1_curr_meas", logPrio::LOG_CRITICAL);

    createStandardIndiNumber<float>(m_indiP_outlet2volt, "ch_2_volt", -240.0, 240.0, 0.001, "%0.3f");
    m_indiP_outlet2volt["current"] = m_channelSetVoltages[1];
    m_indiP_outlet2volt["target"] = m_channelTargetVoltages[1];
    registerIndiPropertyNew(m_indiP_outlet2volt, INDI_NEWCALLBACK(m_indiP_outlet2volt));

    createStandardIndiNumber<float>(m_indiP_outlet2curr, "ch_2_curr", 0, 1000, 0.001, "%0.3f");
    m_indiP_outlet2curr["current"] = m_channelSetCurrents[1];
    m_indiP_outlet2curr["target"] = m_channelTargetCurrents[1];
    registerIndiPropertyNew(m_indiP_outlet2curr, INDI_NEWCALLBACK(m_indiP_outlet2curr));
    createROIndiNumber(m_indiP_outlet2volt_meas, "ch_2_volt_meas");
    m_indiP_outlet2volt_meas.add(pcf::IndiElement("current"));
    m_indiP_outlet2volt_meas["current"].setMin(-240.0);
    m_indiP_outlet2volt_meas["current"].setMax(240.0);
    m_indiP_outlet2volt_meas["current"].setStep(0.001);
    m_indiP_outlet2volt_meas["current"].setFormat("%0.3f");
    m_indiP_outlet2volt_meas["current"] = m_channelVoltages[1];
    if(registerIndiPropertyReadOnly(m_indiP_outlet2volt_meas) < 0) return log<text_log,-1>("Error registering ch_2_volt_meas", logPrio::LOG_CRITICAL);
    createROIndiNumber(m_indiP_outlet2curr_meas, "ch_2_curr_meas");
    m_indiP_outlet2curr_meas.add(pcf::IndiElement("current"));
    m_indiP_outlet2curr_meas["current"].setMin(0);
    m_indiP_outlet2curr_meas["current"].setMax(1000);
    m_indiP_outlet2curr_meas["current"].setStep(0.001);
    m_indiP_outlet2curr_meas["current"].setFormat("%0.3f");
    m_indiP_outlet2curr_meas["current"] = m_channelCurrents[1];
    if(registerIndiPropertyReadOnly(m_indiP_outlet2curr_meas) < 0) return log<text_log,-1>("Error registering ch_2_curr_meas", logPrio::LOG_CRITICAL);

    createStandardIndiNumber<float>(m_indiP_outlet3volt, "ch_3_volt", -240.0, 240.0, 0.001, "%0.3f");
    m_indiP_outlet3volt["current"] = m_channelSetVoltages[2];
    m_indiP_outlet3volt["target"] = m_channelTargetVoltages[2];
    registerIndiPropertyNew(m_indiP_outlet3volt, INDI_NEWCALLBACK(m_indiP_outlet3volt));

    createStandardIndiNumber<float>(m_indiP_outlet3curr, "ch_3_curr", 0, 1000, 0.001, "%0.3f");
    m_indiP_outlet3curr["current"] = m_channelSetCurrents[2];
    m_indiP_outlet3curr["target"] = m_channelTargetCurrents[2];
    registerIndiPropertyNew(m_indiP_outlet3curr, INDI_NEWCALLBACK(m_indiP_outlet3curr));
    createROIndiNumber(m_indiP_outlet3volt_meas, "ch_3_volt_meas");
    m_indiP_outlet3volt_meas.add(pcf::IndiElement("current"));
    m_indiP_outlet3volt_meas["current"].setMin(-240.0);
    m_indiP_outlet3volt_meas["current"].setMax(240.0);
    m_indiP_outlet3volt_meas["current"].setStep(0.001);
    m_indiP_outlet3volt_meas["current"].setFormat("%0.3f");
    m_indiP_outlet3volt_meas["current"] = m_channelVoltages[2];
    if(registerIndiPropertyReadOnly(m_indiP_outlet3volt_meas) < 0) return log<text_log,-1>("Error registering ch_3_volt_meas", logPrio::LOG_CRITICAL);
    createROIndiNumber(m_indiP_outlet3curr_meas, "ch_3_curr_meas");
    m_indiP_outlet3curr_meas.add(pcf::IndiElement("current"));
    m_indiP_outlet3curr_meas["current"].setMin(0);
    m_indiP_outlet3curr_meas["current"].setMax(1000);
    m_indiP_outlet3curr_meas["current"].setStep(0.001);
    m_indiP_outlet3curr_meas["current"].setFormat("%0.3f");
    m_indiP_outlet3curr_meas["current"] = m_channelCurrents[2];
    if(registerIndiPropertyReadOnly(m_indiP_outlet3curr_meas) < 0) return log<text_log,-1>("Error registering ch_3_curr_meas", logPrio::LOG_CRITICAL);

    createStandardIndiNumber<float>(m_indiP_outlet4volt, "ch_4_volt", -240.0, 240.0, 0.001, "%0.3f");
    m_indiP_outlet4volt["current"] = m_channelSetVoltages[3];
    m_indiP_outlet4volt["target"] = m_channelTargetVoltages[3];
    registerIndiPropertyNew(m_indiP_outlet4volt, INDI_NEWCALLBACK(m_indiP_outlet4volt));

    createStandardIndiNumber<float>(m_indiP_outlet4curr, "ch_4_curr", 0, 1000, 0.001, "%0.3f");
    m_indiP_outlet4curr["current"] = m_channelSetCurrents[3];
    m_indiP_outlet4curr["target"] = m_channelTargetCurrents[3];
    registerIndiPropertyNew(m_indiP_outlet4curr, INDI_NEWCALLBACK(m_indiP_outlet4curr));
    createROIndiNumber(m_indiP_outlet4volt_meas, "ch_4_volt_meas");
    m_indiP_outlet4volt_meas.add(pcf::IndiElement("current"));
    m_indiP_outlet4volt_meas["current"].setMin(-240.0);
    m_indiP_outlet4volt_meas["current"].setMax(240.0);
    m_indiP_outlet4volt_meas["current"].setStep(0.001);
    m_indiP_outlet4volt_meas["current"].setFormat("%0.3f");
    m_indiP_outlet4volt_meas["current"] = m_channelVoltages[3];
    if(registerIndiPropertyReadOnly(m_indiP_outlet4volt_meas) < 0) return log<text_log,-1>("Error registering ch_4_volt_meas", logPrio::LOG_CRITICAL);
    createROIndiNumber(m_indiP_outlet4curr_meas, "ch_4_curr_meas");
    m_indiP_outlet4curr_meas.add(pcf::IndiElement("current"));
    m_indiP_outlet4curr_meas["current"].setMin(0);
    m_indiP_outlet4curr_meas["current"].setMax(1000);
    m_indiP_outlet4curr_meas["current"].setStep(0.001);
    m_indiP_outlet4curr_meas["current"].setFormat("%0.3f");
    m_indiP_outlet4curr_meas["current"] = m_channelCurrents[3];
    if(registerIndiPropertyReadOnly(m_indiP_outlet4curr_meas) < 0) return log<text_log,-1>("Error registering ch_4_curr_meas", logPrio::LOG_CRITICAL);
    
    // Telemetry toggle switch
    m_indiP_telemetryToggle = pcf::IndiProperty(pcf::IndiProperty::Switch);
    m_indiP_telemetryToggle.setDevice(configName());
    m_indiP_telemetryToggle.setName("telemetry");
    m_indiP_telemetryToggle.setPerm(pcf::IndiProperty::ReadWrite);
    m_indiP_telemetryToggle.setState(pcf::IndiProperty::Idle);
    m_indiP_telemetryToggle.setRule(pcf::IndiProperty::AtMostOne);
    m_indiP_telemetryToggle.add(pcf::IndiElement("toggle"));
    m_indiP_telemetryToggle["toggle"].setSwitchState(m_telemetryEnabled ? pcf::IndiElement::On : pcf::IndiElement::Off);
    registerIndiPropertyNew(m_indiP_telemetryToggle, INDI_NEWCALLBACK(m_indiP_telemetryToggle));

    // Initialize timers
    m_lastPollTime = std::chrono::steady_clock::now();
    m_lastTelemetryTime = m_lastPollTime;
    if(m_pollRateHz > 0) {
        m_pollInterval = std::chrono::milliseconds( std::max(1, 1000 / m_pollRateHz) );
    }

    // If enabled by config, begin logging immediately
    if(m_telemetryEnabled) {
        startTelemetryLogging();
    }
    
    if(dev::outletController<scpiPowerCtrl>::setupINDI() < 0)
    {
        return log<text_log,-1>("Error setting up INDI for outlet control.", logPrio::LOG_CRITICAL);
    }

    state(stateCodes::NOTCONNECTED);

    return 0;
}

int scpiPowerCtrl::appLogic()
{
    if( state() == stateCodes::NOTCONNECTED )
    {
        static int lastrv = 0; //Used to handle a change in error within the same state.  Make general?
        static int lasterrno = 0;
         
        int rv = devConnect();

        if(rv == 0)
        {
            state(stateCodes::CONNECTED);

            if(!stateLogged())
            {
                std::string logs = "Connected to " + m_deviceAddr;
                if (m_deviceTransport == scpiPowerDeviceTransport::Ipv4) {
                   logs += ":" + std::to_string(m_port);
                }
                log<text_log>(logs);
            }
            lastrv = rv;
            lasterrno = errno;
        }
        else
        {
            if(!stateLogged())
            {
               std::string detail = m_deviceAddr;
               if (m_deviceTransport == scpiPowerDeviceTransport::Ipv4) {
                  detail += ":" + std::to_string(m_port);
               }
               log<text_log>({"Failed to connect to " + detail}, logPrio::LOG_ERROR);
            }
            if( rv != lastrv )
            {
               if (m_deviceTransport == scpiPowerDeviceTransport::Usb) {
                  log<software_error>( {__FILE__,__LINE__, 0, rv,  tty::ttyErrorString(rv)} );
               } else {
                  log<software_error>( {__FILE__,__LINE__, "devConnect failed (return " + std::to_string(rv) + ")"} );
               }
               lastrv = rv;
            }
            if( errno != lasterrno )
            {
               log<software_error>( {__FILE__,__LINE__, errno, std::string(strerror(errno))});
               lasterrno = errno;
            }
            return 0;
        }
    }
 
    if(state() == stateCodes::CONNECTED)
    {
        {
           std::unique_lock<std::mutex> lock(m_indiMutex);
           // Preserve live instrument output state across reconnects/app restarts.
           // This keeps behavior consistent with other MagAO-X outlet controllers.
           int rv = updateOutletStates();
           if(rv < 0)
           {
              log<software_error>({__FILE__, __LINE__, "Failed to synchronize outlet states after connect."});
              state(stateCodes::NOTCONNECTED);
              return 0;
           }
        }
        // Keep polling in one place (appLogic) so INDI callbacks don't compete with
        // a second polling thread for the same SCPI connection.
        state(stateCodes::READY);
    }
 
    if(state() == stateCodes::READY)
    {
       std::unique_lock<std::mutex> lock(m_indiMutex, std::try_to_lock);
 
       if( !lock.owns_lock())
       {
          return 0;
       }
 
       // Poll at configured rate
       auto now = std::chrono::steady_clock::now();
       if(now - m_lastPollTime >= m_pollInterval)
       {
          m_lastPollTime = now;
          int rv = updateOutletStates();
 
          if(rv < 0) return log<software_error,-1>({__FILE__, __LINE__});
       }
 
       updateAlarmsAndWarnings();

       // Telemetry write at configured interval
        if(m_telemetryEnabled)
        {
            auto tnow = std::chrono::steady_clock::now();
            if(tnow - m_lastTelemetryTime >= m_telemetryInterval)
            {
                m_lastTelemetryTime = tnow;
                writeTelemetryData();
            }
        }
 
       return 0;
    }
 
    state(stateCodes::FAILURE);
    log<text_log>("appLogic fell through", logPrio::LOG_CRITICAL);
    return -1;

}

int scpiPowerCtrl::appShutdown()
{
    stopPollThread();
    devDisconnect();
    return 0;
}

int scpiPowerCtrl::updateOutletState( int outletNum )
{
    int rv = devStatus();
    if(rv < 0)
    {
        log<software_error>({__FILE__, __LINE__, "device status error"});
        state(stateCodes::NOTCONNECTED);
        return 0;
    }

    // Select channel n
    std::string res;
    std::string cmd_sel = "INST:NSEL " + std::to_string(outletNum + 1) + "\n";
    if (!send_scpi(cmd_sel, res)) {
        log<software_error>({__FILE__, __LINE__, "Could not select outlet channel " + std::to_string(outletNum)});
        return -1;
    }

    // Query present-channel output state and measurements
    std::string outp;
    if (send_scpi("OUTP?\n", outp)) {
        int st = 0;
        try { st = std::stoi(outp); } catch(...) { st = 0; }
        m_outletStates[outletNum] = (st == 1 ? OUTLET_STATE_ON : OUTLET_STATE_OFF);
    }

    updateChannel(outletNum);

    updateIfChanged(m_indiP_status, "value", m_status);

    if( outletNum == 0 ) {
        updateIfChanged(m_indiP_outlet1volt, "current", m_channelSetVoltages[0]);
        updateIfChanged(m_indiP_outlet1volt, "target", m_channelTargetVoltages[0]);
        updateIfChanged(m_indiP_outlet1curr, "current", m_channelSetCurrents[0]);
        updateIfChanged(m_indiP_outlet1curr, "target", m_channelTargetCurrents[0]);
        updateIfChanged(m_indiP_outlet1volt_meas, "current", m_channelVoltages[0]);
        updateIfChanged(m_indiP_outlet1curr_meas, "current", m_channelCurrents[0]);
    } else if( outletNum == 1 ) {
        updateIfChanged(m_indiP_outlet2volt, "current", m_channelSetVoltages[1]);
        updateIfChanged(m_indiP_outlet2volt, "target", m_channelTargetVoltages[1]);
        updateIfChanged(m_indiP_outlet2curr, "current", m_channelSetCurrents[1]);
        updateIfChanged(m_indiP_outlet2curr, "target", m_channelTargetCurrents[1]);
        updateIfChanged(m_indiP_outlet2volt_meas, "current", m_channelVoltages[1]);
        updateIfChanged(m_indiP_outlet2curr_meas, "current", m_channelCurrents[1]);
    } else if( outletNum == 2) {
        updateIfChanged(m_indiP_outlet3volt, "current", m_channelSetVoltages[2]);
        updateIfChanged(m_indiP_outlet3volt, "target", m_channelTargetVoltages[2]);
        updateIfChanged(m_indiP_outlet3curr, "current", m_channelSetCurrents[2]);
        updateIfChanged(m_indiP_outlet3curr, "target", m_channelTargetCurrents[2]);
        updateIfChanged(m_indiP_outlet3volt_meas, "current", m_channelVoltages[2]);
        updateIfChanged(m_indiP_outlet3curr_meas, "current", m_channelCurrents[2]);
    } else if( outletNum == 3) {
        updateIfChanged(m_indiP_outlet4volt, "current", m_channelSetVoltages[3]);
        updateIfChanged(m_indiP_outlet4volt, "target", m_channelTargetVoltages[3]);
        updateIfChanged(m_indiP_outlet4curr, "current", m_channelSetCurrents[3]);
        updateIfChanged(m_indiP_outlet4curr, "target", m_channelTargetCurrents[3]);
        updateIfChanged(m_indiP_outlet4volt_meas, "current", m_channelVoltages[3]);
        updateIfChanged(m_indiP_outlet4curr_meas, "current", m_channelCurrents[3]);
    }

    dev::outletController<scpiPowerCtrl>::updateINDI();

    return 0;
}


int scpiPowerCtrl::updateOutletStates()
{
    int rv;

    rv = devStatus();

    if(rv < 0)
    {
        log<software_error>({__FILE__, __LINE__, "device status error"});
        state(stateCodes::NOTCONNECTED);
        return 0;
    }

    // Update each outlet's state and measurements
    for (int i = 0; i < m_numChannels; ++i) {
        updateOutletState(i);
    }

    updateIfChanged(m_indiP_status, "value", m_status);

    /*
    for (int i = 0; i < m_numChannels; i++) {
        std::string propName = "load_ch" + std::to_string(i + 1);
    
        updateIfChanged(m_indiP_load_channels[i], "voltage", m_channelVoltages[i]);
        updateIfChanged(m_indiP_load_channels[i], "current", m_channelCurrents[i]);
    }
    */

    updateIfChanged(m_indiP_outlet1volt, "current", m_channelSetVoltages[0]);
    updateIfChanged(m_indiP_outlet1volt, "target", m_channelTargetVoltages[0]);
    updateIfChanged(m_indiP_outlet1curr, "current", m_channelSetCurrents[0]);
    updateIfChanged(m_indiP_outlet1curr, "target", m_channelTargetCurrents[0]);
    updateIfChanged(m_indiP_outlet2volt, "current", m_channelSetVoltages[1]);
    updateIfChanged(m_indiP_outlet2volt, "target", m_channelTargetVoltages[1]);
    updateIfChanged(m_indiP_outlet2curr, "current", m_channelSetCurrents[1]);
    updateIfChanged(m_indiP_outlet2curr, "target", m_channelTargetCurrents[1]);
    updateIfChanged(m_indiP_outlet3volt, "current", m_channelSetVoltages[2]);
    updateIfChanged(m_indiP_outlet3volt, "target", m_channelTargetVoltages[2]);
    updateIfChanged(m_indiP_outlet3curr, "current", m_channelSetCurrents[2]);
    updateIfChanged(m_indiP_outlet3curr, "target", m_channelTargetCurrents[2]);
    updateIfChanged(m_indiP_outlet4volt, "current", m_channelSetVoltages[3]);
    updateIfChanged(m_indiP_outlet4volt, "target", m_channelTargetVoltages[3]);
    updateIfChanged(m_indiP_outlet4curr, "current", m_channelSetCurrents[3]);
    updateIfChanged(m_indiP_outlet4curr, "target", m_channelTargetCurrents[3]);
    updateIfChanged(m_indiP_outlet1volt_meas, "current", m_channelVoltages[0]);
    updateIfChanged(m_indiP_outlet1curr_meas, "current", m_channelCurrents[0]);
    updateIfChanged(m_indiP_outlet2volt_meas, "current", m_channelVoltages[1]);
    updateIfChanged(m_indiP_outlet2curr_meas, "current", m_channelCurrents[1]);
    updateIfChanged(m_indiP_outlet3volt_meas, "current", m_channelVoltages[2]);
    updateIfChanged(m_indiP_outlet3curr_meas, "current", m_channelCurrents[2]);
    updateIfChanged(m_indiP_outlet4volt_meas, "current", m_channelVoltages[3]);
    updateIfChanged(m_indiP_outlet4curr_meas, "current", m_channelCurrents[3]);

    dev::outletController<scpiPowerCtrl>::updateINDI();

    return 0;
}

int scpiPowerCtrl::turnOutletOn( int outletNum )
{
    std::lock_guard<std::mutex> guard(m_indiMutex);  //Lock the mutex before doing anything

    // Ensure the latest configured target setpoints are in place before enabling output.
    if (applyChannelSetpoint(outletNum, false, m_channelTargetVoltages[outletNum]) < 0) {
        return log<text_log,-1>("Failed applying target voltage for channel " + std::to_string(outletNum), logPrio::LOG_WARNING);
    }
    if (applyChannelSetpoint(outletNum, true, m_channelTargetCurrents[outletNum]) < 0) {
        return log<text_log,-1>("Failed applying target current for channel " + std::to_string(outletNum), logPrio::LOG_WARNING);
    }

    // Select channel, then turn output ON (generic SCPI pattern)
    std::string res;
    std::string cmd_sel = "INST:NSEL " + std::to_string(outletNum + 1) + "\n";
    if (!send_scpi(cmd_sel, res)) {
        return log<text_log,-1>("Could not select outlet channel " + std::to_string(outletNum), logPrio::LOG_WARNING);
    }

    if (!send_scpi("OUTP 1\n", res)) {
        return log<text_log,-1>("Failed to turn output channel " + std::to_string(outletNum) + " on.", logPrio::LOG_WARNING);
    }

    return 0;
}

int scpiPowerCtrl::turnOutletOff( int outletNum )
{
 
    std::lock_guard<std::mutex> guard(m_indiMutex);  //Lock the mutex before doing anything

    // Select channel, then turn output OFF (generic SCPI pattern)
    std::string res;
    std::string cmd_sel = "INST:NSEL " + std::to_string(outletNum + 1) + "\n";
    if (!send_scpi(cmd_sel, res)) {
        return log<text_log,-1>("Could not select outlet channel " + std::to_string(outletNum), logPrio::LOG_WARNING);
    }

    if (!send_scpi("OUTP 0\n", res)) {
        return log<text_log,-1>("Failed to turn output channel " + std::to_string(outletNum) + " off.", logPrio::LOG_WARNING);
    }

    return 0;
}

int scpiPowerCtrl::devConnect()
{
    devDisconnect();

    if (m_deviceTransport == scpiPowerDeviceTransport::Invalid) {
       return log<text_log,-1>("Invalid device.protocol; must be 'usb' or 'ipv4'.", logPrio::LOG_CRITICAL);
    }
    if (m_deviceTransport == scpiPowerDeviceTransport::Usb) {
       return connectUSB();
    }
    return connectTCP();
}

int scpiPowerCtrl::devDisconnect()
{
    int result = 0;
    if (fd >= 0) {
       if (close(fd) < 0) {
          log<software_error>({__FILE__, __LINE__, "Error closing USB TMC: " + std::string(strerror(errno))});
          result = -1;
       }
       fd = -1;
    }
    if (m_socketFd >= 0) {
       if (close(m_socketFd) < 0) {
          log<software_error>({__FILE__, __LINE__, "Error closing TCP socket: " + std::string(strerror(errno))});
          result = -1;
       }
       m_socketFd = -1;
    }
    return result;
}

int scpiPowerCtrl::connectUSB()
{
    fd = open(m_deviceAddr.c_str(), O_RDWR);
    if (fd < 0) {
       return log<text_log,-1>(std::string("Error opening USB TMC device ") + m_deviceAddr + ": " + strerror(errno), logPrio::LOG_CRITICAL);
    }
    log<text_log>("Opened USB TMC: " + m_deviceAddr);
    return 0;
}

int scpiPowerCtrl::connectTCP()
{
    m_socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socketFd < 0) {
       return log<text_log,-1>("Failed to create TCP socket: " + std::string(strerror(errno)), logPrio::LOG_CRITICAL);
    }
    int opt = 1;
    setsockopt(m_socketFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(m_socketFd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));

    int flags = fcntl(m_socketFd, F_GETFL, 0);
    if (flags < 0 || fcntl(m_socketFd, F_SETFL, flags | O_NONBLOCK) < 0) {
       close(m_socketFd);
       m_socketFd = -1;
       return log<text_log,-1>("Failed to set socket non-blocking: " + std::string(strerror(errno)), logPrio::LOG_CRITICAL);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(static_cast<uint16_t>(m_port));
    if (inet_pton(AF_INET, m_deviceAddr.c_str(), &server_addr.sin_addr) <= 0) {
       close(m_socketFd);
       m_socketFd = -1;
       return log<text_log,-1>("Invalid IPv4 address: " + m_deviceAddr, logPrio::LOG_CRITICAL);
    }

    int connect_result = connect(m_socketFd, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr));
    if (connect_result < 0 && errno != EINPROGRESS) {
       close(m_socketFd);
       m_socketFd = -1;
       return log<text_log,-1>("Failed to connect to " + m_deviceAddr + ":" + std::to_string(m_port) + " (" + std::string(strerror(errno)) + ")", logPrio::LOG_CRITICAL);
    }
    if (connect_result < 0 && errno == EINPROGRESS) {
       fd_set write_fds, except_fds;
       struct timeval timeout;
       FD_ZERO(&write_fds);
       FD_ZERO(&except_fds);
       FD_SET(m_socketFd, &write_fds);
       FD_SET(m_socketFd, &except_fds);
       timeout.tv_sec = m_connectTimeoutMs / 1000;
       timeout.tv_usec = (m_connectTimeoutMs % 1000) * 1000;
       int select_result = select(m_socketFd + 1, nullptr, &write_fds, &except_fds, &timeout);
       if (select_result <= 0) {
          close(m_socketFd);
          m_socketFd = -1;
          if (select_result == 0) {
             return log<text_log,-1>("Connection timeout to " + m_deviceAddr + ":" + std::to_string(m_port), logPrio::LOG_CRITICAL);
          }
          return log<text_log,-1>("Connection select error: " + std::string(strerror(errno)), logPrio::LOG_CRITICAL);
       }
       if (FD_ISSET(m_socketFd, &except_fds)) {
          close(m_socketFd);
          m_socketFd = -1;
          return log<text_log,-1>("Connection failed to " + m_deviceAddr + ":" + std::to_string(m_port), logPrio::LOG_CRITICAL);
       }
       int soerr = 0;
       socklen_t len = sizeof(soerr);
       if (getsockopt(m_socketFd, SOL_SOCKET, SO_ERROR, &soerr, &len) < 0) {
          close(m_socketFd);
          m_socketFd = -1;
          return log<text_log,-1>("getsockopt SO_ERROR failed: " + std::string(strerror(errno)), logPrio::LOG_CRITICAL);
       }
       if (soerr != 0) {
          close(m_socketFd);
          m_socketFd = -1;
          return log<text_log,-1>("Connection failed to " + m_deviceAddr + ":" + std::to_string(m_port) + " (" + std::string(strerror(soerr)) + ")", logPrio::LOG_CRITICAL);
       }
    }

    if (fcntl(m_socketFd, F_SETFL, flags) < 0) {
       close(m_socketFd);
       m_socketFd = -1;
       return log<text_log,-1>("Failed to restore socket blocking mode: " + std::string(strerror(errno)), logPrio::LOG_CRITICAL);
    }

    struct timeval recv_timeout;
    recv_timeout.tv_sec = 2;
    recv_timeout.tv_usec = 0;
    setsockopt(m_socketFd, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));

    log<text_log>("Connected TCP SCPI: " + m_deviceAddr + ":" + std::to_string(m_port));
    return 0;
}

int scpiPowerCtrl::devStatus()
{
    // verify remote status and external control
    // CONF:SETPT? -> should return 3
    // CONT:EXT? -> should return 1
    /*
    std::string cmd_conf = "CONF:SETPT?";
    std::string cmd_ctrl = "CONT:EXT?";

    int conf = write(fd, cmd_conf.c_str(), cmd_conf.size());
    int ctrl = write(fd, cmd_ctrl.c_str(), cmd_ctrl.size());


    if (conf != 3 ||
        ctrl != 1)
    {
        printf("%d, %d\n", conf, ctrl);
        return log<text_log,-1>("Device not in external control and remote control.", logPrio::LOG_CRITICAL);
    }
    */
    return 0;
}

int scpiPowerCtrl::updateChannels()
{
    for(int i=0; i<m_numChannels; i++)
    {
        updateChannel(i);
    }

    return 0; 
}

int scpiPowerCtrl::updateChannel(int channel)
{
    // Select channel, then query measurements (generic SCPI pattern)
    std::string res;
    std::string cmd_sel = "INST:NSEL " + std::to_string(channel + 1) + "\n";
    if (!send_scpi(cmd_sel, res)) {
        return log<text_log,-1>("Could not select outlet channel " + std::to_string(channel), logPrio::LOG_WARNING);
    }

    auto trimResponse = [](std::string &s)
    {
        const size_t first = s.find_first_not_of(" \n\r\t");
        if(first == std::string::npos)
        {
            s.clear();
            return;
        }
        const size_t last = s.find_last_not_of(" \n\r\t");
        s = s.substr(first, last - first + 1);
    };

    auto parseResponse = [&](std::string &raw, float &dest) -> bool
    {
        trimResponse(raw);
        if(raw.empty()) return false;
        try
        {
            dest = std::stof(raw);
            return true;
        }
        catch(...)
        {
            return false;
        }
    };

    std::string setVolt, setCurr, measVolt, measCurr;
    float polledSetVolt = m_channelSetVoltages[channel];
    float polledSetCurr = m_channelSetCurrents[channel];
    bool gotSetVolt = send_scpi("VOLT?\n", setVolt) && parseResponse(setVolt, polledSetVolt);
    bool gotSetCurr = send_scpi("CURR?\n", setCurr) && parseResponse(setCurr, polledSetCurr);

    // Some supplies report 0 for VOLT?/CURR? while OUTP is off.
    // Preserve requested setpoints in that state so ch_*_{volt,curr}.current
    // continues reflecting editable setpoints prior to enabling output.
    if(m_outletStates[channel] == OUTLET_STATE_ON)
    {
        if(gotSetVolt) m_channelSetVoltages[channel] = polledSetVolt;
        if(gotSetCurr) m_channelSetCurrents[channel] = polledSetCurr;
    }

    bool ok_v = send_scpi("MEAS:VOLT?\n", measVolt) && parseResponse(measVolt, m_channelVoltages[channel]);
    bool ok_c = send_scpi("MEAS:CURR?\n", measCurr) && parseResponse(measCurr, m_channelCurrents[channel]);

    if (!(ok_v && ok_c)) {
        log<software_error>({__FILE__, __LINE__, "Failed to read voltage/current from channel " + std::to_string(channel + 1)});
        return -1;
    }

    return 0;
}

int scpiPowerCtrl::setPollRate()
{
    return 0;
}

int scpiPowerCtrl::applyChannelSetpoint(int channel, bool isCurrent, double requestedValue)
{
    if (channel < 0 || channel >= m_numChannels) {
        return log<text_log,-1>("Invalid channel index for setpoint: " + std::to_string(channel), logPrio::LOG_WARNING);
    }

    const auto & lim = m_channelLimits[channel];
    const double low = (isCurrent ? lim.currLowLimit : lim.voltLowLimit);
    const double high = (isCurrent ? lim.currHighLimit : lim.voltHighLimit);
    double value = requestedValue;
    if (value < low) value = low;
    if (value > high) value = high;

    int rv = (isCurrent ? setChannelAmps(channel, value) : setChannelVolts(channel, value));
    if (rv < 0) return rv;

    if (isCurrent) {
        m_channelTargetCurrents[channel] = static_cast<float>(value);
        m_channelSetCurrents[channel] = static_cast<float>(value);
    } else {
        m_channelTargetVoltages[channel] = static_cast<float>(value);
        m_channelSetVoltages[channel] = static_cast<float>(value);
    }
    return 0;
}

int scpiPowerCtrl::applyConfiguredSetpoints()
{
    for(int ch = 0; ch < m_numChannels; ++ch)
    {
        if (applyChannelSetpoint(ch, false, m_channelTargetVoltages[ch]) < 0) return -1;
        if (applyChannelSetpoint(ch, true, m_channelTargetCurrents[ch]) < 0) return -1;
    }
    return 0;
}

int scpiPowerCtrl::handleSetpointCallback(pcf::IndiProperty &localProp,
                                          const pcf::IndiProperty &ipRecv,
                                          int channel,
                                          bool isCurrent,
                                          const std::string &desc)
{
    if(localProp.createUniqueKey() != ipRecv.createUniqueKey())
    {
        log<software_error>({__FILE__, __LINE__, "wrong INDI property received for " + desc});
        return -1;
    }

    double requested = isCurrent ? static_cast<double>(m_channelTargetCurrents[channel])
                                 : static_cast<double>(m_channelTargetVoltages[channel]);
    if(indiTargetUpdate(localProp, requested, ipRecv, true) < 0)
    {
        log<software_error>({__FILE__, __LINE__, "No target/current element provided for " + desc});
        return -1;
    }

    std::unique_lock<std::mutex> lock(m_indiMutex);
    int rv = applyChannelSetpoint(channel, isCurrent, requested);
    if(rv < 0)
    {
        log<software_error>({__FILE__, __LINE__, "Error setting " + desc});
        if(isCurrent)
        {
            updateIfChanged(localProp, "target", m_channelTargetCurrents[channel], INDI_IDLE);
            updateIfChanged(localProp, "current", m_channelSetCurrents[channel], INDI_IDLE);
        }
        else
        {
            updateIfChanged(localProp, "target", m_channelTargetVoltages[channel], INDI_IDLE);
            updateIfChanged(localProp, "current", m_channelSetVoltages[channel], INDI_IDLE);
        }
        return -1;
    }

    if(isCurrent)
    {
        updateIfChanged(localProp, "target", m_channelTargetCurrents[channel], INDI_IDLE);
        updateIfChanged(localProp, "current", m_channelSetCurrents[channel], INDI_IDLE);
    }
    else
    {
        updateIfChanged(localProp, "target", m_channelTargetVoltages[channel], INDI_IDLE);
        updateIfChanged(localProp, "current", m_channelSetVoltages[channel], INDI_IDLE);
    }

    return 0;
}

int scpiPowerCtrl::forceAllOutputsOff()
{
    std::string res;
    for(int ch = 0; ch < m_numChannels; ++ch)
    {
        std::string cmd_sel = "INST:NSEL " + std::to_string(ch + 1) + "\n";
        if (!send_scpi(cmd_sel, res)) {
            return log<text_log,-1>("Could not select outlet channel " + std::to_string(ch) + " while forcing outputs off", logPrio::LOG_WARNING);
        }
        if (!send_scpi("OUTP 0\n", res)) {
            return log<text_log,-1>("Could not force output off for channel " + std::to_string(ch), logPrio::LOG_WARNING);
        }
        m_outletStates[ch] = OUTLET_STATE_OFF;
    }
    return 0;
}

int scpiPowerCtrl::setChannelVolts(int channel, double volts)
{
    std::string cmd_sel = "INST:NSEL " + std::to_string(channel + 1) + "\n";
    std::string res;

    if (!send_scpi(cmd_sel, res)) {
        return log<text_log,-1>("Could not select outlet channel " + std::to_string(channel), logPrio::LOG_WARNING);
    }

    std::string cmd_volts = "VOLT " + std::to_string(volts) + "\n";

    if (!send_scpi(cmd_volts, res)) {
        return log<text_log,-1>("Failed to set volts for channel " + std::to_string(channel), logPrio::LOG_WARNING);
    }

    return 0;
}

int scpiPowerCtrl::setChannelAmps(int channel, double amps)
{
    std::string cmd_sel = "INST:NSEL " + std::to_string(channel + 1) + "\n";
    std::string res;

    if (!send_scpi(cmd_sel, res)) {
        return log<text_log,-1>("Could not select outlet channel " + std::to_string(channel), logPrio::LOG_WARNING);
    }

    std::string cmd_amps = "CURR " + std::to_string(amps) + "\n";

    if (!send_scpi(cmd_amps, res)) {
        return log<text_log,-1>("Failed to set current for channel " + std::to_string(channel), logPrio::LOG_WARNING);
    }

    return 0;
}

int scpiPowerCtrl::setChannelHighVolt(int channel, double highVolt)
{
    std::string cmd_sel = "INST:NSEL " + std::to_string(channel + 1) + "\n";
    std::string res;

    if (!send_scpi(cmd_sel, res)) {
        return log<text_log,-1>("Could not select outlet channel " + std::to_string(channel), logPrio::LOG_WARNING);
    }

    std::string cmd = "VOLT:LIM:HIGH " + std::to_string(highVolt) + "\n";
    if (!send_scpi(cmd, res)) {
        return log<text_log,-1>("Failed to set high voltage limit for channel " + std::to_string(channel), logPrio::LOG_WARNING);
    }

    return 0;
}

int scpiPowerCtrl::setChannelLowVolt(int channel, double lowVolt)
{
    std::string cmd_sel = "INST:NSEL " + std::to_string(channel + 1) + "\n";
    std::string res;

    if (!send_scpi(cmd_sel, res)) {
        return log<text_log, -1>("Could not select outlet channel " + std::to_string(channel), logPrio::LOG_WARNING);
    }

    std::string cmd = "VOLT:LIM:LOW " + std::to_string(lowVolt) + "\n";
    if (!send_scpi(cmd, res)) {
        return log<text_log, -1>("Failed to set low voltage limit for channel " + std::to_string(channel), logPrio::LOG_WARNING);
    }

    return 0;
}

int scpiPowerCtrl::setChannelHighCurr(int channel, double highCurr)
{
    std::string cmd_sel = "INST:NSEL " + std::to_string(channel + 1) + "\n";
    std::string res;

    if (!send_scpi(cmd_sel, res)) {
        return log<text_log, -1>("Could not select outlet channel " + std::to_string(channel), logPrio::LOG_WARNING);
    }

    std::string cmd = "CURR:LIM:HIGH " + std::to_string(highCurr) + "\n";
    if (!send_scpi(cmd, res)) {
        return log<text_log, -1>("Failed to set high current limit for channel " + std::to_string(channel), logPrio::LOG_WARNING);
    }

    return 0;
}

int scpiPowerCtrl::setChannelLowCurr(int channel, double lowCurr)
{
    std::string cmd_sel = "INST:NSEL " + std::to_string(channel + 1) + "\n";
    std::string res;

    if (!send_scpi(cmd_sel, res)) {
        return log<text_log, -1>("Could not select outlet channel " + std::to_string(channel), logPrio::LOG_WARNING);
    }

    std::string cmd = "CURR:LIM:LOW " + std::to_string(lowCurr) + "\n";
    if (!send_scpi(cmd, res)) {
        return log<text_log,-1>("Failed to set low current limit for channel " + std::to_string(channel), logPrio::LOG_WARNING);
    }

    return 0;
}

void scpiPowerCtrl::updateAlarmsAndWarnings()
{
    // TODO poll the various alarm statuses from the PDU
}

bool scpiPowerCtrl::send_scpi(const std::string& cmd, std::string& response) {
    const int active_fd = (m_deviceTransport == scpiPowerDeviceTransport::Ipv4) ? m_socketFd : fd;
    if (active_fd < 0) {
       log<software_error>({__FILE__, __LINE__, "SCPI I/O with no open device (fd)"});
       return false;
    }

    const ssize_t nwr = write(active_fd, cmd.c_str(), cmd.size());
    if (nwr < 0) {
       log<software_error>({__FILE__, __LINE__, std::string("SCPI write failed: ") + strerror(errno)});
       return false;
    }

    if (cmd.find('?') == std::string::npos) {
        response.clear();
        return true;
    }

    std::vector<char> buffer(8192);
    response.clear();
    while (true) {
        ssize_t n = read(active_fd, buffer.data(), buffer.size() - 1);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                for (int retry = 0; retry < 5; ++retry) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(20 * (retry + 1)));
                    n = read(active_fd, buffer.data(), buffer.size() - 1);
                    if (n >= 0) {
                        break;
                    }
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        break;
                    }
                }
            }
            if (n < 0) {
                log<software_error>({__FILE__, __LINE__, std::string("SCPI read failed: ") + strerror(errno)});
                return false;
            }
        }
        if (n == 0) {
            break;
        }
        response.append(buffer.data(), static_cast<size_t>(n));
        if (!response.empty() && response.back() == '\n') {
            break;
        }
        if (response.size() > 100000) {
            log<software_error>({__FILE__, __LINE__, "SCPI response exceeded 100KB, truncating"});
            break;
        }
    }
    return true;
}

INDI_NEWCALLBACK_DEFN(scpiPowerCtrl, m_indiP_outlet1volt)(const pcf::IndiProperty &ipRecv)
{
   return handleSetpointCallback(m_indiP_outlet1volt, ipRecv, 0, false, "channel 1 volts");
}

INDI_NEWCALLBACK_DEFN(scpiPowerCtrl, m_indiP_outlet2volt)(const pcf::IndiProperty &ipRecv)
{
   return handleSetpointCallback(m_indiP_outlet2volt, ipRecv, 1, false, "channel 2 volts");
}

INDI_NEWCALLBACK_DEFN(scpiPowerCtrl, m_indiP_outlet3volt)(const pcf::IndiProperty &ipRecv)
{
   return handleSetpointCallback(m_indiP_outlet3volt, ipRecv, 2, false, "channel 3 volts");
}

INDI_NEWCALLBACK_DEFN(scpiPowerCtrl, m_indiP_outlet4volt)(const pcf::IndiProperty &ipRecv)
{
   return handleSetpointCallback(m_indiP_outlet4volt, ipRecv, 3, false, "channel 4 volts");
}

INDI_NEWCALLBACK_DEFN(scpiPowerCtrl, m_indiP_outlet1curr)(const pcf::IndiProperty &ipRecv)
{
   return handleSetpointCallback(m_indiP_outlet1curr, ipRecv, 0, true, "channel 1 current");
}

INDI_NEWCALLBACK_DEFN(scpiPowerCtrl, m_indiP_outlet2curr)(const pcf::IndiProperty &ipRecv)
{
   return handleSetpointCallback(m_indiP_outlet2curr, ipRecv, 1, true, "channel 2 current");
}

INDI_NEWCALLBACK_DEFN(scpiPowerCtrl, m_indiP_outlet3curr)(const pcf::IndiProperty &ipRecv)
{
   return handleSetpointCallback(m_indiP_outlet3curr, ipRecv, 2, true, "channel 3 current");
}

INDI_NEWCALLBACK_DEFN(scpiPowerCtrl, m_indiP_outlet4curr)(const pcf::IndiProperty &ipRecv)
{
   return handleSetpointCallback(m_indiP_outlet4curr, ipRecv, 3, true, "channel 4 current");
}

INDI_NEWCALLBACK_DEFN(scpiPowerCtrl, m_indiP_telemetryToggle)(const pcf::IndiProperty &ipRecv)
{
   if (ipRecv.getName() != m_indiP_telemetryToggle.getName())
   {
      log<software_error>({__FILE__, __LINE__, "wrong INDI property received."});
      return -1;
   }

   bool enable = false;
   if (ipRecv.find("toggle"))
   {
       enable = (ipRecv["toggle"].getSwitchState() == pcf::IndiElement::On);
   }

   std::unique_lock<std::mutex> lock(m_indiMutex);
   m_telemetryEnabled = enable;
   if(m_telemetryEnabled) startTelemetryLogging();
   else stopTelemetryLogging();

   // Reflect new state back to INDI so GUI shows it
   updateSwitchIfChanged(m_indiP_telemetryToggle, "toggle", m_telemetryEnabled ? pcf::IndiElement::On : pcf::IndiElement::Off);

   return 0;
}


}//namespace app
} //namespace MagAOX

#endif //scpiPowerCtrl_hpp
