/** \file scpiCtrl.hpp
  * \brief The MagAO-X SCPI-standard DC Power Supply controller.
  *
  * \author Adam A. Schilperoort (adamschilperoort@gmail.com)
  *
  * \ingroup scpiCtrl_files
  */

#ifndef scpiCtrl_hpp
#define scpiCtrl_hpp


#include "../../libMagAOX/libMagAOX.hpp" //Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <regex>
#include <algorithm>
#include <sstream>

/** \defgroup scpiCtrl SCPI Power Supply
  * \brief Control of MagAO-X SCPI-standard DC Power Supplies.
  *
  * <a href="../handbook/operating/software/apps/scpiCtrl.html">Application Documentation</a>
  *
  * \ingroup apps
  *
  */

/** \defgroup scpiCtrl_files SCPI Power Supply Files
  * \ingroup scpiCtrl
  */

namespace MagAOX
{
namespace app
{

/// Communication protocol types for SCPI devices
enum class SCPIProtocol {
    USB_TMC,    ///< USB Test and Measurement Class (e.g., /dev/usbtmc0)
    TCP_IP      ///< TCP/IP Ethernet connection (e.g., TCPIP0::169.254.159.239::inst0::INSTR)
};

/// MagAO-X application to control any DC power supply that supports the SCPI standard.
/** The available outputs are organized into channels.  See \ref dev::outletController for details of configuring the channels.
  *
  * Each active channel's amp and volts are monitored 
  * 
  * \todo begin logging freq/volt/amps telemetry
  * \todo segfaults if device can not be reached on network -- make this an issue
  * 
  * \ingroup scpiCtrl
  */
class scpiCtrl : public MagAOXApp<>, public dev::outletController<scpiCtrl>, public dev::ioDevice
{

protected:

   std::string m_deviceAddr; ///< The device address  -> /dev/usbtmc0 for USB or TCPIP0::IP::inst0::INSTR for Ethernet
   SCPIProtocol m_protocol; ///< Communication protocol (USB_TMC or TCP_IP)
   
   // Device type detection
   bool m_isPowerSupply {true}; ///< True for power supplies, false for measurement devices
   
   // Measurement mode configuration
   enum class MeasurementMode {
       POLLING,        ///< Individual READ? queries (default, low rate)
       BUFFERED,       ///< Buffer-based acquisition (high rate)
       DIGITIZED       ///< High-speed digitized acquisition
   };
   MeasurementMode m_measurementMode {MeasurementMode::POLLING};

   enum class MeasurementFunction {
       VOLTAGE,        ///< Direct voltage measurement
       CURRENT         ///< Current measurement via voltage conversion
   };
   MeasurementFunction m_measurementFunction {MeasurementFunction::VOLTAGE};
   
   double m_currentConversionFactor {0.1}; ///< V-to-A conversion factor (default: 100mV/A)
   
   // Buffered acquisition parameters
   int m_bufferSize {1000}; ///< Number of samples per buffer
   double m_sampleRateHz {100.0}; ///< Target sampling rate (Hz) for buffered/polling
   double m_sampleInterval {0.01}; ///< Deprecated: interval between samples in seconds (0 = as fast as possible)
   bool m_includeTimestamps {true}; ///< Include relative timestamps
   
   // Buffer state management
   bool m_bufferAcquisitionActive {false}; ///< Whether buffer acquisition is currently running
   std::chrono::steady_clock::time_point m_acquisitionStartTime; ///< When current acquisition started
   double m_maxAcquisitionTime {300.0}; ///< Maximum acquisition time in seconds (prevent overruns)
   std::string m_bufferDataPath {"/opt/MagAOX/data"}; ///< Path for saving buffer data files
   std::string m_telemDir {"default"}; ///< Telemetry directory name
   
   // TCP/IP specific members
   std::string m_ipAddress; ///< IP address for TCP/IP connections
   int m_port {5025}; ///< Port for TCP/IP connections (default SCPI port)
   int m_socketFd {-1}; ///< Socket file descriptor for TCP/IP connections
   int m_connectTimeout {5000}; ///< Connection timeout in milliseconds

   // arrays for all high and low limits for volts and amps because defined independently for each channel

    struct ChannelLimits {
        float voltHighLimit = 240.0f;
        float voltLowLimit = 0.0f;
        float currHighLimit = 50.0f;
        float currLowLimit = 0.0f;
    };

    std::vector<ChannelLimits> m_channelLimits;
    std::vector<float> m_channelVoltages;
    std::vector<float> m_channelCurrents;
    int m_numChannels = 3; ///< The number of channels on the device -- abandoning dynamic, hard-coding 3 for now
    int m_currentChannel = 0; ///< The current channel being monitored

    int maxChannels = 3; // define maximum number of power channels

    int fd; ///< The file descriptor for the device
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

    INDI_NEWCALLBACK_DECL(scpiCtrl, m_indiP_outlet1volt);
    INDI_NEWCALLBACK_DECL(scpiCtrl, m_indiP_outlet2volt);
    INDI_NEWCALLBACK_DECL(scpiCtrl, m_indiP_outlet3volt);
    INDI_NEWCALLBACK_DECL(scpiCtrl, m_indiP_outlet1curr);
    INDI_NEWCALLBACK_DECL(scpiCtrl, m_indiP_outlet2curr);
    INDI_NEWCALLBACK_DECL(scpiCtrl, m_indiP_outlet3curr);
    
    // Telemetry control
    INDI_NEWCALLBACK_DECL(scpiCtrl, m_indiP_telemetryToggle);
    INDI_NEWCALLBACK_DECL(scpiCtrl, m_indiP_resetToggle);
    // Measurement configuration controls
    INDI_NEWCALLBACK_DECL(scpiCtrl, m_indiP_samplingRateHz);
    INDI_NEWCALLBACK_DECL(scpiCtrl, m_indiP_bufferSize);
    INDI_NEWCALLBACK_DECL(scpiCtrl, m_indiP_measurementMode);
    INDI_NEWCALLBACK_DECL(scpiCtrl, m_indiP_measurementFunction);
    INDI_NEWCALLBACK_DECL(scpiCtrl, m_indiP_currentConversionFactor);
    
    // Power control toggles (using outletController framework)
    // These are handled by the outletController base class

    //INDI_NEWCALLBACK_DECL(scpiCtrl, m_indiP_load_channels);

    //std::vector<pcf::IndiProperty> m_indiP_load_channels;   

    // INDI_NEWCALLBACK_DECL(scpiCtrl, m_indiP_singleVolt);
    // INDI_NEWCALLBACK_DECL(scpiCtrl, m_indiP_singleCurr);

    /// Default c'tor.
    scpiCtrl();
 
    /// D'tor, declared and defined for noexcept.
    ~scpiCtrl() noexcept
    {
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
    /** For the scpiCtrl this isn't possible for a single outlet, so this calls updateOutletStates.
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
    
    int devDisconnect();

    int devStatus();
    
    // Protocol detection and parsing
    SCPIProtocol detectProtocol(const std::string& address);
    int parseAddress(const std::string& address);
    
    // Protocol-specific connection methods
    int connectUSB();
    int connectTCP();
    
    int updateChannels();
    
    int updateChannel(int channel);
    
    // Telemetry data collection using selected measurement mode
    int collectTelemetryData(std::vector<float>& voltages, std::vector<double>& timestamps);
    
    // Device parameter readback functions
    int readDeviceBufferSize();
    int readDeviceSamplingRate();
    int readDeviceMeasurementMode();
    int readDeviceMeasurementFunction();
    
    // Buffered acquisition methods
    int setupBufferedAcquisition();
    int startBufferAcquisition();
    int stopBufferAcquisition();
    int checkBufferStatus();
    int getBufferedData(std::vector<float>& voltages, std::vector<double>& timestamps);
    int saveBufferData(const std::vector<float>& voltages, const std::vector<double>& timestamps, const std::string& filename = "");
    int setupDigitizedAcquisition(double sampleRate, double aperture, double voltageRange);
    int getDigitizedData(std::vector<float>& voltages, std::vector<double>& timestamps);
    
    // Buffer management
    bool isBufferOverrunRisk();
    std::string generateBufferFilename();
    
    int setPollRate();
    // Auto-tune target sampling rate in buffered mode (zero-delay SimpleLoop)
    int autoTuneBuffered(double targetRateHz, int maxIterations = 6, double toleranceSeconds = 0.0005);
    int applyBaseSpeedSettings(); // range, autozero, filters, etc.
    int applyNPLC(double nplc);
    
    int setChannelVolts(int channel, double volts);

    int setChannelAmps(int channel, double amps);

    int setChannelHighVolt(int channel, double highVolt);
    
    int setChannelLowVolt(int channel, double lowVolt);
    
    int setChannelHighCurr(int channel, double highCurr);
    
    int setChannelLowCurr(int channel, double lowCurr);
    
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
   pcf::IndiProperty m_indiP_outlet1curr;
   pcf::IndiProperty m_indiP_outlet2curr;
   pcf::IndiProperty m_indiP_outlet3curr;
   
   // Telemetry control
   pcf::IndiProperty m_indiP_telemetryToggle;
    pcf::IndiProperty m_indiP_resetToggle;
   
   // Dynamic measurement configuration via INDI
   pcf::IndiProperty m_indiP_samplingRateHz;        ///< User-selectable sampling rate (Hz)
   pcf::IndiProperty m_indiP_bufferSize;            ///< User-selectable buffer size (samples)
   pcf::IndiProperty m_indiP_bufferSizeCurrent;     ///< Current buffer size from device (read-only)
   pcf::IndiProperty m_indiP_measurementMode;       ///< User-selectable measurement mode
   pcf::IndiProperty m_indiP_measurementFunction;   ///< Voltage/current measurement toggle
   pcf::IndiProperty m_indiP_measurementFunctionCurrent; ///< Current measurement function from device (read-only)
   pcf::IndiProperty m_indiP_currentConversionFactor; ///< V-to-A conversion factor
   pcf::IndiProperty m_indiP_telemDir; ///< Telemetry directory name for data storage
   
   // Power control toggles are handled by outletController framework

   // INDI callback declarations
   int newCallBack_m_indiP_telemDir(const pcf::IndiProperty &ipRecv);
   static int st_newCallBack_m_indiP_telemDir(void * p, const pcf::IndiProperty &ipRecv);

};

scpiCtrl::scpiCtrl() : MagAOXApp(MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED)
{
   m_firstOne = true;
   //setNumberOfOutlets(m_numChannels); // should be m_numChannels after connecting to the device
   return;
}

void scpiCtrl::setupConfig()
{
    config.add("device.address", "a", "device.address", argType::Required, "device", "address", false, "string", "The device address (/dev/usbtmc0 for USB or TCPIP0::IP::inst0::INSTR for Ethernet).");
    
    // Device type configuration
    config.add("device.isPowerSupply", "", "device.isPowerSupply", argType::Optional, "device", "isPowerSupply", false, "bool", "True for power supplies, false for measurement devices (default: true)");
    
    // Measurement mode configuration
    config.add("device.measurementMode", "", "device.measurementMode", argType::Optional, "device", "measurementMode", false, "string", "Measurement mode: polling, buffered, or digitized (default: polling)");
    config.add("device.measurementFunction", "", "device.measurementFunction", argType::Optional, "device", "measurementFunction", false, "string", "Measurement function: voltage or current (default: voltage)");
    config.add("device.currentConversionFactor", "", "device.currentConversionFactor", argType::Optional, "device", "currentConversionFactor", false, "float", "V-to-A conversion factor for current mode (default: 0.1 = 100mV/A)");
    config.add("device.bufferSize", "", "device.bufferSize", argType::Optional, "device", "bufferSize", false, "int", "Buffer size for buffered/digitized modes (default: 1000)");
    config.add("device.sampleInterval", "", "device.sampleInterval", argType::Optional, "device", "sampleInterval", false, "float", "[Deprecated] Sample interval (s). Prefer device.samplingRateHz.");
    config.add("device.samplingRateHz", "", "device.samplingRateHz", argType::Optional, "device", "samplingRateHz", false, "float", "Target sampling rate in Hz for polling/buffered (default: 100)");
    config.add("device.maxAcquisitionTime", "", "device.maxAcquisitionTime", argType::Optional, "device", "maxAcquisitionTime", false, "float", "Maximum acquisition time in seconds to prevent overruns (default: 300)");
    config.add("device.bufferDataPath", "", "device.bufferDataPath", argType::Optional, "device", "bufferDataPath", false, "string", "Path for saving buffer data files (default: /opt/MagAOX/data)");
    config.add("device.telemetryPath", "", "device.telemetryPath", argType::Optional, "device", "telemetryPath", false, "string", "Path for telemetry files (default: /opt/MagAOX/telem)");
    
    // TCP/IP specific configuration
    config.add("device.port", "", "device.port", argType::Optional, "device", "port", false, "int", "TCP port for Ethernet connections (default: 5025)");
    config.add("device.connectTimeout", "", "device.connectTimeout", argType::Optional, "device", "connectTimeout", false, "int", "Connection timeout in milliseconds (default: 5000)");
    
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
    }

   dev::outletController<scpiCtrl>::setupConfig(config);
   
}


void scpiCtrl::loadConfig()
{
    config(m_deviceAddr, "device.address");
    
    // Parse address and detect protocol
    m_protocol = detectProtocol(m_deviceAddr);
    if (parseAddress(m_deviceAddr) < 0) {
        log<software_error>({__FILE__, __LINE__, "Failed to parse device address: " + m_deviceAddr});
    }

    // Load device type and measurement mode configuration
    config(m_isPowerSupply, "device.isPowerSupply");
    
    std::string modeStr = "polling";
    config(modeStr, "device.measurementMode");
    if (modeStr == "buffered") m_measurementMode = MeasurementMode::BUFFERED;
    else if (modeStr == "digitized") m_measurementMode = MeasurementMode::DIGITIZED;
    else m_measurementMode = MeasurementMode::POLLING;
    
    // Load measurement function and conversion factor
    std::string functionStr = "voltage";
    config(functionStr, "device.measurementFunction");
    if (functionStr == "current") m_measurementFunction = MeasurementFunction::CURRENT;
    else m_measurementFunction = MeasurementFunction::VOLTAGE;
    
    config(m_currentConversionFactor, "device.currentConversionFactor");
    
    config(m_bufferSize, "device.bufferSize");
    // Backward compatibility: if sampleInterval present and samplingRateHz missing, map it
    double cfgSampleInterval = m_sampleInterval;
    config(cfgSampleInterval, "device.sampleInterval");
    config(m_sampleRateHz, "device.samplingRateHz");
    if (m_sampleRateHz <= 0.0 && cfgSampleInterval > 0.0) {
        m_sampleRateHz = 1.0 / cfgSampleInterval;
    }
    config(m_maxAcquisitionTime, "device.maxAcquisitionTime");
    config(m_bufferDataPath, "device.bufferDataPath");
    config(m_telemDir, "device.telemDir");
    
    // Load TCP configuration
    config(m_port, "device.port");
    config(m_connectTimeout, "device.connectTimeout");

    dev::ioDevice::loadConfig(config);
    config(m_numChannels, "device.numChannels");
    config(m_pollRateHz, "device.pollRateHz");
    
    // Load telemetry configuration
    config(m_telemetryPath, "device.telemetryPath");
    config(m_telemetryEnabled, "telemetry.enabled");

    if (m_numChannels > maxChannels) {
        log<software_error>({__FILE__, __LINE__, "more channels defined than maximum allowed"});
    }

    setNumberOfOutlets(m_numChannels);
    m_channelLimits.resize(m_numChannels);
    m_channelVoltages.resize(m_numChannels);
    m_channelCurrents.resize(m_numChannels);

    for (int i = 0; i < m_numChannels; i++) {
        auto& ch = m_channelLimits[i];

        std::string prefix = "channel" + std::to_string(i + 1) + ".limits."; // channel1.limits.highVolt

        config(ch.voltHighLimit, prefix + "highVolt");
        config(ch.voltLowLimit,  prefix + "lowVolt");
        config(ch.currHighLimit, prefix + "highCurr");
        config(ch.currLowLimit,  prefix + "lowCurr");
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

   dev::outletController<scpiCtrl>::loadConfig(config);
   
}

int scpiCtrl::appStartup()
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
    
    // Create INDI properties only for configured number of channels
    // Channel 1 (always created if numChannels >= 1)
    if (m_numChannels >= 1) {
        createStandardIndiNumber<float>(m_indiP_outlet1volt, "ch_1_volt", -240.0, 240.0, 0.001, "%.3f");
    m_indiP_outlet1volt["current"] = m_channelVoltages[0];
    m_indiP_outlet1volt["target"] = m_channelVoltages[0];
    registerIndiPropertyNew(m_indiP_outlet1volt, INDI_NEWCALLBACK(m_indiP_outlet1volt));

        createStandardIndiNumber<float>(m_indiP_outlet1curr, "ch_1_curr", 0, 1000, 0.001, "%.3f");
    m_indiP_outlet1curr["current"] = m_channelCurrents[0];
    m_indiP_outlet1curr["target"] = m_channelCurrents[0];
    registerIndiPropertyNew(m_indiP_outlet1curr, INDI_NEWCALLBACK(m_indiP_outlet1curr));
    }

    // Channel 2 (only created if numChannels >= 2)
    if (m_numChannels >= 2) {
        createStandardIndiNumber<float>(m_indiP_outlet2volt, "ch_2_volt", -240.0, 240.0, 0.001, "%.3f");
    m_indiP_outlet2volt["current"] = m_channelVoltages[1];
    m_indiP_outlet2volt["target"] = m_channelVoltages[1];
    registerIndiPropertyNew(m_indiP_outlet2volt, INDI_NEWCALLBACK(m_indiP_outlet2volt));

        createStandardIndiNumber<float>(m_indiP_outlet2curr, "ch_2_curr", 0, 1000, 0.001, "%.3f");
    m_indiP_outlet2curr["current"] = m_channelCurrents[1];
    m_indiP_outlet2curr["target"] = m_channelCurrents[1];
    registerIndiPropertyNew(m_indiP_outlet2curr, INDI_NEWCALLBACK(m_indiP_outlet2curr));
    }

    // Channel 3 (only created if numChannels >= 3)
    if (m_numChannels >= 3) {
        createStandardIndiNumber<float>(m_indiP_outlet3volt, "ch_3_volt", -240.0, 240.0, 0.001, "%.3f");
    m_indiP_outlet3volt["current"] = m_channelVoltages[2];
    m_indiP_outlet3volt["target"] = m_channelVoltages[2];
    registerIndiPropertyNew(m_indiP_outlet3volt, INDI_NEWCALLBACK(m_indiP_outlet3volt));

        createStandardIndiNumber<float>(m_indiP_outlet3curr, "ch_3_curr", 0, 1000, 0.001, "%.3f");
    m_indiP_outlet3curr["current"] = m_channelCurrents[2];
    m_indiP_outlet3curr["target"] = m_channelCurrents[2];
    registerIndiPropertyNew(m_indiP_outlet3curr, INDI_NEWCALLBACK(m_indiP_outlet3curr));
    }

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
    
    // Reset toggle
    m_indiP_resetToggle = pcf::IndiProperty(pcf::IndiProperty::Switch);
    m_indiP_resetToggle.setDevice(configName());
    m_indiP_resetToggle.setName("reset");
    m_indiP_resetToggle.setPerm(pcf::IndiProperty::WriteOnly);
    m_indiP_resetToggle.setState(pcf::IndiProperty::Idle);
    m_indiP_resetToggle.setRule(pcf::IndiProperty::AtMostOne);
    m_indiP_resetToggle.add(pcf::IndiElement("reset"));
    m_indiP_resetToggle["reset"].setSwitchState(pcf::IndiElement::Off);
    registerIndiPropertyNew(m_indiP_resetToggle, INDI_NEWCALLBACK(m_indiP_resetToggle));

    // Measurement configuration via INDI
    m_indiP_samplingRateHz = pcf::IndiProperty(pcf::IndiProperty::Number);
    m_indiP_samplingRateHz.setDevice(configName());
    m_indiP_samplingRateHz.setName("samplingRateHz");
    m_indiP_samplingRateHz.setPerm(pcf::IndiProperty::ReadWrite);
    m_indiP_samplingRateHz.setState(pcf::IndiProperty::Idle);
    m_indiP_samplingRateHz.add(pcf::IndiElement("value"));
    m_indiP_samplingRateHz["value"].set<double>(m_sampleRateHz);
    registerIndiPropertyNew(m_indiP_samplingRateHz, INDI_NEWCALLBACK(m_indiP_samplingRateHz));

    m_indiP_bufferSize = pcf::IndiProperty(pcf::IndiProperty::Number);
    m_indiP_bufferSize.setDevice(configName());
    m_indiP_bufferSize.setName("bufferSize");
    m_indiP_bufferSize.setPerm(pcf::IndiProperty::ReadWrite);
    m_indiP_bufferSize.setState(pcf::IndiProperty::Idle);
    m_indiP_bufferSize.add(pcf::IndiElement("target"));
    m_indiP_bufferSize["target"].set<int>(m_bufferSize);
    registerIndiPropertyNew(m_indiP_bufferSize, INDI_NEWCALLBACK(m_indiP_bufferSize));
    
    // Current buffer size (read-only)
    m_indiP_bufferSizeCurrent = pcf::IndiProperty(pcf::IndiProperty::Number);
    m_indiP_bufferSizeCurrent.setDevice(configName());
    m_indiP_bufferSizeCurrent.setName("bufferSizeCurrent");
    m_indiP_bufferSizeCurrent.setPerm(pcf::IndiProperty::ReadOnly);
    m_indiP_bufferSizeCurrent.setState(pcf::IndiProperty::Idle);
    m_indiP_bufferSizeCurrent.add(pcf::IndiElement("current"));
    m_indiP_bufferSizeCurrent["current"].set<int>(m_bufferSize);
    registerIndiPropertyNew(m_indiP_bufferSizeCurrent, nullptr);

    m_indiP_measurementMode = pcf::IndiProperty(pcf::IndiProperty::Text);
    m_indiP_measurementMode.setDevice(configName());
    m_indiP_measurementMode.setName("measurementMode");
    m_indiP_measurementMode.setPerm(pcf::IndiProperty::ReadWrite);
    m_indiP_measurementMode.setState(pcf::IndiProperty::Idle);
    m_indiP_measurementMode.add(pcf::IndiElement("value"));
    std::string modeStrInit = (m_measurementMode == MeasurementMode::BUFFERED ? "buffered" : (m_measurementMode == MeasurementMode::DIGITIZED ? "digitized" : "polling"));
    m_indiP_measurementMode["value"].set<std::string>(modeStrInit);
    registerIndiPropertyNew(m_indiP_measurementMode, INDI_NEWCALLBACK(m_indiP_measurementMode));

    m_indiP_measurementFunction = pcf::IndiProperty(pcf::IndiProperty::Text);
    m_indiP_measurementFunction.setDevice(configName());
    m_indiP_measurementFunction.setName("measurementFunction");
    m_indiP_measurementFunction.setPerm(pcf::IndiProperty::ReadWrite);
    m_indiP_measurementFunction.setState(pcf::IndiProperty::Idle);
    m_indiP_measurementFunction.add(pcf::IndiElement("target"));
    std::string functionStrInit = (m_measurementFunction == MeasurementFunction::CURRENT ? "current" : "voltage");
    m_indiP_measurementFunction["target"].set<std::string>(functionStrInit);
    registerIndiPropertyNew(m_indiP_measurementFunction, INDI_NEWCALLBACK(m_indiP_measurementFunction));
    
    // Current measurement function (read-only)
    m_indiP_measurementFunctionCurrent = pcf::IndiProperty(pcf::IndiProperty::Text);
    m_indiP_measurementFunctionCurrent.setDevice(configName());
    m_indiP_measurementFunctionCurrent.setName("measurementFunctionCurrent");
    m_indiP_measurementFunctionCurrent.setPerm(pcf::IndiProperty::ReadOnly);
    m_indiP_measurementFunctionCurrent.setState(pcf::IndiProperty::Idle);
    m_indiP_measurementFunctionCurrent.add(pcf::IndiElement("current"));
    m_indiP_measurementFunctionCurrent["current"].set<std::string>(functionStrInit);
    registerIndiPropertyNew(m_indiP_measurementFunctionCurrent, nullptr);

    m_indiP_currentConversionFactor = pcf::IndiProperty(pcf::IndiProperty::Number);
    m_indiP_currentConversionFactor.setDevice(configName());
    m_indiP_currentConversionFactor.setName("currentConversionFactor");
    m_indiP_currentConversionFactor.setPerm(pcf::IndiProperty::ReadWrite);
    m_indiP_currentConversionFactor.setState(pcf::IndiProperty::Idle);
    m_indiP_currentConversionFactor.add(pcf::IndiElement("value"));
    m_indiP_currentConversionFactor["value"].set<double>(m_currentConversionFactor);
    registerIndiPropertyNew(m_indiP_currentConversionFactor, INDI_NEWCALLBACK(m_indiP_currentConversionFactor));

    // Telemetry directory
    REG_INDI_NEWPROP(m_indiP_telemDir, "telemDir", pcf::IndiProperty::Text);
    m_indiP_telemDir.add(pcf::IndiElement("value"));
    m_indiP_telemDir["value"].set<std::string>(m_telemDir);

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
    
    if(dev::outletController<scpiCtrl>::setupINDI() < 0)
    {
        return log<text_log,-1>("Error setting up INDI for outlet control.", logPrio::LOG_CRITICAL);
    }

    state(stateCodes::NOTCONNECTED);

    return 0;
}

int scpiCtrl::appLogic()
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
                std::string logs = "Connected to " + m_deviceAddr;;
                log<text_log>(logs);
            }
            lastrv = rv;
            lasterrno = errno;
        }
        else
        {
            if(!stateLogged())
            {
               log<text_log>({"Failed to connect to " + m_deviceAddr}, logPrio::LOG_ERROR);
            }
            if( rv != lastrv )
            {
               log<software_error>( {__FILE__,__LINE__, 0, rv,  tty::ttyErrorString(rv)} );
               lastrv = rv;
            }
            if( errno != lasterrno )
            {
               log<software_error>( {__FILE__,__LINE__, errno});
               lasterrno = errno;
            }
            return 0;
        }
    }
 
    if(state() == stateCodes::CONNECTED)
    {
        // Test device communication with a simple SCPI command
        std::string idn_response;
        if (!send_scpi("*IDN?\n", idn_response)) {
            log<software_error>({__FILE__, __LINE__, "Device not responding to SCPI commands - disconnecting"});
            devDisconnect();
            state(stateCodes::NOTCONNECTED);
            return 0;
        }
        
        log<text_log>("Device handshake successful: " + idn_response);
        
        // Configure measurement mode for measurement devices
        if (!m_isPowerSupply) {
            if (m_measurementMode == MeasurementMode::BUFFERED) {
                if (setupBufferedAcquisition() < 0) {
                    log<software_error>({__FILE__, __LINE__, "Failed to setup buffered acquisition"});
                    state(stateCodes::FAILURE);
                    return -1;
                }
            } else if (m_measurementMode == MeasurementMode::DIGITIZED) {
                // Default digitized parameters - could be made configurable
                if (setupDigitizedAcquisition(1000000, 1e-6, 1000) < 0) {
                    log<software_error>({__FILE__, __LINE__, "Failed to setup digitized acquisition"});
                    state(stateCodes::FAILURE);
                    return -1;
                }
            }
        }

        state(stateCodes::READY);
        startPollThread();
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
          
          // Periodic connection validation and parameter checking (every 10 polls)
          static int poll_count = 0;
          poll_count++;
          if (poll_count >= 10) {
              poll_count = 0;
              std::string test_response;
              if (!send_scpi("*IDN?\n", test_response)) {
                  log<software_error>({__FILE__, __LINE__, "Device no longer responding - disconnecting"});
                  devDisconnect();
                  state(stateCodes::NOTCONNECTED);
                  return 0;
              }
              
              // Check for external changes to device parameters
              if (!m_isPowerSupply) {
                  readDeviceBufferSize();
                  readDeviceSamplingRate();
                  readDeviceMeasurementMode();
                  readDeviceMeasurementFunction();
                  
                  // Check buffer status for buffered/digitized modes
                  if (m_measurementMode == MeasurementMode::BUFFERED || m_measurementMode == MeasurementMode::DIGITIZED) {
                      checkBufferStatus();
                  }
              }
          }
          
          int rv = updateOutletStates();
 
          if(rv < 0) return log<software_error,-1>({__FILE__, __LINE__});
       }
 
       updateAlarmsAndWarnings();

       // Telemetry write at configured interval (only for polling mode)
        if(m_telemetryEnabled && m_measurementMode == MeasurementMode::POLLING)
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

int scpiCtrl::appShutdown()
{
    stopPollThread();
    devDisconnect();
    return 0;
}

int scpiCtrl::updateOutletState( int outletNum )
{
    int rv = devStatus();
    if(rv < 0)
    {
        log<software_error>({__FILE__, __LINE__, "device status error"});
        state(stateCodes::NOTCONNECTED);
        return 0;
    }

    // For measurement devices, handle differently than power supplies
    if (!m_isPowerSupply) {
        // For measurement devices, we don't have "outlet states" in the traditional sense
        // Just mark as "on" since they're always measuring
        m_outletStates[outletNum] = OUTLET_STATE_ON;
    } else {
        // Select channel n for power supplies
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
    }

    updateChannel(outletNum);

    updateIfChanged(m_indiP_status, "value", m_status);

    if( outletNum == 0 && m_numChannels >= 1 ) {
        updateIfChanged(m_indiP_outlet1volt, "current", m_channelVoltages[0]);
        updateIfChanged(m_indiP_outlet1curr, "current", m_channelCurrents[0]);
    } else if( outletNum == 1 && m_numChannels >= 2 ) {
        updateIfChanged(m_indiP_outlet2volt, "current", m_channelVoltages[1]);
        updateIfChanged(m_indiP_outlet2curr, "current", m_channelCurrents[1]);
    } else if( outletNum == 2 && m_numChannels >= 3 ) {
        updateIfChanged(m_indiP_outlet3volt, "current", m_channelVoltages[2]);
        updateIfChanged(m_indiP_outlet3curr, "current", m_channelCurrents[2]);
    }

    dev::outletController<scpiCtrl>::updateINDI();

    return 0;
}


int scpiCtrl::updateOutletStates()
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

    // Update INDI properties only for configured channels
    if (m_numChannels >= 1) {
    updateIfChanged(m_indiP_outlet1volt, "current", m_channelVoltages[0]);
    updateIfChanged(m_indiP_outlet1curr, "current", m_channelCurrents[0]);
    }
    if (m_numChannels >= 2) {
    updateIfChanged(m_indiP_outlet2volt, "current", m_channelVoltages[1]);
    updateIfChanged(m_indiP_outlet2curr, "current", m_channelCurrents[1]);
    }
    if (m_numChannels >= 3) {
    updateIfChanged(m_indiP_outlet3volt, "current", m_channelVoltages[2]);
    updateIfChanged(m_indiP_outlet3curr, "current", m_channelCurrents[2]);
    }

    dev::outletController<scpiCtrl>::updateINDI();

    return 0;
}

int scpiCtrl::turnOutletOn( int outletNum )
{
    std::lock_guard<std::mutex> guard(m_indiMutex);  //Lock the mutex before doing anything

    // For measurement devices, "turning on" means starting measurements
    if (!m_isPowerSupply) {
        log<text_log>("Measurement device - outlet control not applicable for channel " + std::to_string(outletNum));
        // For measurement devices, we just report them as "on" since they're always measuring
        m_outletStates[outletNum] = OUTLET_STATE_ON;
        return 0;
    }

    // Select channel, then turn output ON (power supply SCPI pattern)
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

int scpiCtrl::turnOutletOff( int outletNum )
{
    std::lock_guard<std::mutex> guard(m_indiMutex);  //Lock the mutex before doing anything

    // For measurement devices, "turning off" doesn't make sense
    if (!m_isPowerSupply) {
        log<text_log>("Measurement device - outlet control not applicable for channel " + std::to_string(outletNum));
        // For measurement devices, we just report them as "off" but continue measuring
        m_outletStates[outletNum] = OUTLET_STATE_OFF;
        return 0;
    }

    // Select channel, then turn output OFF (power supply SCPI pattern)
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

int scpiCtrl::devConnect()
{
    // Disconnect any existing connection
    devDisconnect();
    
    switch(m_protocol) {
        case SCPIProtocol::USB_TMC:
            return connectUSB();
        case SCPIProtocol::TCP_IP:
            return connectTCP();
        default:
            return log<text_log,-1>("Unknown communication protocol", logPrio::LOG_CRITICAL);
    }
}

int scpiCtrl::devStatus()
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

int scpiCtrl::updateChannels()
{
    for(int i=0; i<m_numChannels; i++)
    {
        updateChannel(i);
    }

    return 0; 
}

int scpiCtrl::updateChannel(int channel)
{
    // Check if device is properly connected before attempting to read
    if (state() != stateCodes::READY && state() != stateCodes::CONNECTED) {
        log<text_log>("updateChannel(" + std::to_string(channel) + ") called but device not ready (state: " + std::to_string(state()) + ")");
        return -1;
    }
    
    // Skip updateChannel during buffer acquisition to avoid interference
    if (m_bufferAcquisitionActive) {
        return 0;
    }
    
    // Debug logging removed - only log errors
    
    std::string volt, curr;
    bool ok_v = false, ok_c = false;

    if (m_isPowerSupply && m_numChannels > 1) {
        // Multi-channel power supply - select channel first
    std::string res;
    std::string cmd_sel = "INST:NSEL " + std::to_string(channel + 1) + "\n";
    if (!send_scpi(cmd_sel, res)) {
        return log<text_log,-1>("Could not select outlet channel " + std::to_string(channel), logPrio::LOG_WARNING);
    }

        ok_v = send_scpi("MEAS:VOLT?\n", volt);
        ok_c = send_scpi("MEAS:CURR?\n", curr);
    } else if (m_isPowerSupply) {
        // Single-channel power supply - no channel selection needed
        ok_v = send_scpi("MEAS:VOLT?\n", volt);
        ok_c = send_scpi("MEAS:CURR?\n", curr);
    } else {
        // Measurement device (DMM, etc.) - always use simple READ? for INDI display
        // Measurement modes (polling/buffered/digitized) only apply to telemetry recording, not real-time display
        ok_v = send_scpi("READ?\n", volt);  // Keithley DMMs typically use READ? for measurements
        // For measurement devices, current will be calculated from voltage using conversion factor
        // The actual current calculation happens in the voltage processing section below
        curr = "0.0";
        ok_c = true;
    }

    if (ok_v) {
        volt.erase(volt.find_last_not_of(" \n\r\t") + 1);
        try {
            float voltageValue = std::stof(volt);
            
            // Always store the raw voltage measurement
            m_channelVoltages[channel] = voltageValue;
            
            // Always calculate current using conversion factor (for both voltage and current modes)
            if (!m_isPowerSupply && m_currentConversionFactor > 0) {
                float currentValue = voltageValue / m_currentConversionFactor;
                m_channelCurrents[channel] = currentValue;
                
                // Channel values updated successfully (logging removed for performance)
            } else {
                // No conversion factor or power supply - set current to 0
                m_channelCurrents[channel] = 0.0f;
            }
        } catch (...) {
            log<software_error>({__FILE__, __LINE__, "Failed to parse voltage reading: " + volt});
            m_channelVoltages[channel] = 0.0f;
            m_channelCurrents[channel] = 0.0f;
        }
    } else {
        log<software_error>({__FILE__, __LINE__, "Failed to read voltage from channel " + std::to_string(channel + 1)});
        return -1;
    }

    if (ok_c) {
        curr.erase(curr.find_last_not_of(" \n\r\t") + 1);
        try {
        m_channelCurrents[channel] = std::stof(curr);
        } catch (...) {
            log<software_error>({__FILE__, __LINE__, "Failed to parse current reading: " + curr});
            m_channelCurrents[channel] = 0.0f;
        }
    } else if (m_isPowerSupply) {
        // For power supplies, current might not be available
        m_channelCurrents[channel] = 0.0f;
    }
    // For measurement devices, current is already calculated from voltage above, so no action needed

    return 0;
}

int scpiCtrl::collectTelemetryData(std::vector<float>& voltages, std::vector<double>& timestamps)
{
    // This function collects data for telemetry logging using the selected measurement mode
    // It's separate from updateChannel() which is used for real-time INDI display
    
    if (m_isPowerSupply) {
        // For power supplies, use simple polling for telemetry too
        std::string volt_str;
        if (send_scpi("MEAS:VOLT?\n", volt_str)) {
            try {
                float voltage = std::stof(volt_str);
                voltages.push_back(voltage);
                timestamps.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::high_resolution_clock::now().time_since_epoch()).count());
            } catch (...) {
                log<software_error>({__FILE__, __LINE__, "Failed to parse voltage for telemetry: " + volt_str});
                return -1;
            }
    } else {
            log<software_error>({__FILE__, __LINE__, "Failed to read voltage for telemetry"});
        return -1;
        }
    } else {
        // For measurement devices, use the selected measurement mode
        if (m_measurementMode == MeasurementMode::BUFFERED) {
            return getBufferedData(voltages, timestamps);
        } else if (m_measurementMode == MeasurementMode::DIGITIZED) {
            return getDigitizedData(voltages, timestamps);
        } else {
            // Polling mode - single measurement
            std::string volt_str;
            if (send_scpi("READ?\n", volt_str)) {
                try {
                    float voltage = std::stof(volt_str);
                    voltages.push_back(voltage);
                    timestamps.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::high_resolution_clock::now().time_since_epoch()).count());
                } catch (...) {
                    log<software_error>({__FILE__, __LINE__, "Failed to parse voltage for telemetry: " + volt_str});
                    return -1;
                }
            } else {
                log<software_error>({__FILE__, __LINE__, "Failed to read voltage for telemetry"});
                return -1;
            }
        }
    }
    
    return 0;
}

int scpiCtrl::readDeviceBufferSize()
{
    if (m_isPowerSupply) {
        // Power supplies don't have buffer size
        return 0;
    }
    
    // Skip during buffer acquisition to avoid interference
    if (m_bufferAcquisitionActive) {
        return 0;
    }
    
    std::string res;
    if (!send_scpi("TRAC:POIN?\n", res)) {
        log<software_error>({__FILE__, __LINE__, "Failed to read buffer size from device"});
        return -1;
    }
    
    // Check if we got a valid response (should be a number)
    if (res.find("KEITHLEY") != std::string::npos || res.find("MODEL") != std::string::npos) {
        log<software_error>({__FILE__, __LINE__, "Got device ID instead of buffer size - device may be confused"});
        return -1;
    }
    
    try {
        int deviceBufferSize = std::stoi(res);
        if (deviceBufferSize != m_bufferSize) {
            log<text_log>("Device buffer size changed from " + std::to_string(m_bufferSize) + 
                         " to " + std::to_string(deviceBufferSize) + " (external change detected)");
            m_bufferSize = deviceBufferSize;
            
            // Update INDI properties
            updateIfChanged(m_indiP_bufferSize, "target", m_bufferSize);
            updateIfChanged(m_indiP_bufferSizeCurrent, "current", m_bufferSize);
        }
    } catch (...) {
        log<software_error>({__FILE__, __LINE__, "Failed to parse buffer size from device: " + res});
        return -1;
    }
    
    return 0;
}

int scpiCtrl::readDeviceSamplingRate()
{
    // For now, we can't directly read sampling rate from device
    // This would require reading NPLC and calculating the rate
    // For now, just return 0 (no change detected)
    return 0;
}

int scpiCtrl::readDeviceMeasurementMode()
{
    // For now, we can't directly read measurement mode from device
    // This would require checking various device settings
    // For now, just return 0 (no change detected)
    return 0;
}

int scpiCtrl::readDeviceMeasurementFunction()
{
    if (m_isPowerSupply) {
        // Power supplies don't have measurement function
        return 0;
    }
    
    // Skip during buffer acquisition to avoid interference
    if (m_bufferAcquisitionActive) {
        return 0;
    }
    
    std::string res;
    if (!send_scpi("SENS:FUNC?\n", res)) {
        log<software_error>({__FILE__, __LINE__, "Failed to read measurement function from device"});
        return -1;
    }
    
    // Check if we got a valid response
    if (res.find("KEITHLEY") != std::string::npos || res.find("MODEL") != std::string::npos) {
        log<software_error>({__FILE__, __LINE__, "Got device ID instead of measurement function - device may be confused"});
        return -1;
    }
    
    // Parse the response - typically returns something like "VOLT" or "CURR"
    std::string deviceFunction = res;
    // Remove quotes and whitespace
    deviceFunction.erase(std::remove(deviceFunction.begin(), deviceFunction.end(), '"'), deviceFunction.end());
    deviceFunction.erase(0, deviceFunction.find_first_not_of(" \t\r\n"));
    deviceFunction.erase(deviceFunction.find_last_not_of(" \t\r\n") + 1);
    
    // Convert to our enum
    MeasurementFunction deviceMeasurementFunction;
    if (deviceFunction == "VOLT" || deviceFunction == "VOLTAGE") {
        deviceMeasurementFunction = MeasurementFunction::VOLTAGE;
    } else if (deviceFunction == "CURR" || deviceFunction == "CURRENT") {
        deviceMeasurementFunction = MeasurementFunction::CURRENT;
    } else {
        log<software_error>({__FILE__, __LINE__, "Unknown measurement function from device: " + deviceFunction});
        return -1;
    }
    
    if (deviceMeasurementFunction != m_measurementFunction) {
        std::string oldStr = (m_measurementFunction == MeasurementFunction::CURRENT ? "current" : "voltage");
        std::string newStr = (deviceMeasurementFunction == MeasurementFunction::CURRENT ? "current" : "voltage");
        log<text_log>("Device measurement function changed from " + oldStr + 
                     " to " + newStr + " (external change detected)");
        m_measurementFunction = deviceMeasurementFunction;
        
        // Update INDI properties
        updateIfChanged(m_indiP_measurementFunction, "target", newStr);
        updateIfChanged(m_indiP_measurementFunctionCurrent, "current", newStr);
    }

    return 0;
}

int scpiCtrl::setPollRate()
{
    return 0;
}

int scpiCtrl::setChannelVolts(int channel, double volts)
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

int scpiCtrl::setChannelAmps(int channel, double amps)
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

int scpiCtrl::setChannelHighVolt(int channel, double highVolt)
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

int scpiCtrl::setChannelLowVolt(int channel, double lowVolt)
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

int scpiCtrl::setChannelHighCurr(int channel, double highCurr)
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

int scpiCtrl::setChannelLowCurr(int channel, double lowCurr)
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

void scpiCtrl::updateAlarmsAndWarnings()
{
    // TODO poll the various alarm statuses from the PDU
}

bool scpiCtrl::send_scpi(const std::string& cmd, std::string& response) {
    int active_fd = (m_protocol == SCPIProtocol::TCP_IP) ? m_socketFd : fd;
    
    if (active_fd < 0) {
        log<software_error>({__FILE__, __LINE__, "Invalid file descriptor for SCPI communication"});
        return false;
    }
    
    // Write command
    ssize_t bytes_written = write(active_fd, cmd.c_str(), cmd.size());
    if (bytes_written < 0) {
        log<software_error>({__FILE__, __LINE__, "SCPI write failed: " + std::string(strerror(errno))});
        return false;
    }

    // Only read if this is a query (contains '?')
    if (cmd.find('?') == std::string::npos) {
        response.clear();
        return true;
    }

    // For large responses (like buffer data), use a larger buffer and read in chunks
    std::vector<char> buffer(8192); // 8KB buffer for large responses
    response.clear();
    
    while (true) {
        ssize_t n = read(active_fd, buffer.data(), buffer.size() - 1);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available yet, try again after a short delay
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                n = read(active_fd, buffer.data(), buffer.size() - 1);
                if (n < 0) {
                    log<software_error>({__FILE__, __LINE__, "SCPI read failed after retry: " + std::string(strerror(errno))});
                    return false;
                }
            } else {
                log<software_error>({__FILE__, __LINE__, "SCPI read failed: " + std::string(strerror(errno))});
                return false;
            }
        }
        
        if (n == 0) {
            // End of data
            break;
        }
        
        // Append to response
        response.append(buffer.data(), n);
        
        // Check if we have a complete response (ends with newline)
        if (response.back() == '\n') {
            break;
        }
        
        // If response is getting too large, break to avoid infinite loop
        if (response.size() > 100000) { // 100KB limit
            log<software_error>({__FILE__, __LINE__, "Response too large, truncating at 100KB"});
            break;
        }
    }
    
    return true;
}

INDI_NEWCALLBACK_DEFN(scpiCtrl, m_indiP_outlet1volt)(const pcf::IndiProperty &ipRecv)
{
   if (ipRecv.getName() != m_indiP_outlet1volt.getName())
   {
      log<software_error>({__FILE__, __LINE__, "wrong INDI property received."});
      return -1;
   }

   int vc = 0;

   if (ipRecv.find("current"))
      vc = ipRecv["current"].get<int>();

   if (ipRecv.find("target"))
      vc = ipRecv["target"].get<int>();

   std::unique_lock<std::mutex> lock(m_indiMutex);
   m_channelVoltages[0] = vc;
   int rv = setChannelVolts(0, vc);
   
   if(rv < 0)
   {
      log<software_error>({__FILE__, __LINE__, "Error setting channel 1 volts!"});
      return -1;
   }

   if (m_numChannels >= 1) {
   updateIfChanged(m_indiP_outlet1volt, "target", vc);
   updateIfChanged(m_indiP_outlet1volt, "current", m_channelVoltages[0]);
   }

   return 0;
}

INDI_NEWCALLBACK_DEFN(scpiCtrl, m_indiP_outlet2volt)(const pcf::IndiProperty &ipRecv)
{
   if (ipRecv.getName() != m_indiP_outlet2volt.getName())
   {
      log<software_error>({__FILE__, __LINE__, "wrong INDI property received."});
      return -1;
   }

   int vc = 0;

   if (ipRecv.find("current"))
      vc = ipRecv["current"].get<int>();

   if (ipRecv.find("target"))
      vc = ipRecv["target"].get<int>();

   std::unique_lock<std::mutex> lock(m_indiMutex);
   m_channelVoltages[1] = vc;
   int rv = setChannelVolts(1, vc);
   
   if(rv < 0)
   {
      log<software_error>({__FILE__, __LINE__, "Error setting channel 2 volts!"});
      return -1;
   }

   if (m_numChannels >= 2) {
   updateIfChanged(m_indiP_outlet2volt, "target", vc);
   updateIfChanged(m_indiP_outlet2volt, "current", m_channelVoltages[1]);
   }

   return 0;
}

INDI_NEWCALLBACK_DEFN(scpiCtrl, m_indiP_outlet3volt)(const pcf::IndiProperty &ipRecv)
{
   if (ipRecv.getName() != m_indiP_outlet3volt.getName())
   {
      log<software_error>({__FILE__, __LINE__, "wrong INDI property received."});
      return -1;
   }

   int vc = 0;

   if (ipRecv.find("current"))
      vc = ipRecv["current"].get<int>();

   if (ipRecv.find("target"))
      vc = ipRecv["target"].get<int>();

   std::unique_lock<std::mutex> lock(m_indiMutex);
   m_channelVoltages[2] = vc;
   int rv = setChannelVolts(2, vc);
   
   if(rv < 0)
   {
      log<software_error>({__FILE__, __LINE__, "Error setting channel 1 volts!"});
      return -1;
   }

   if (m_numChannels >= 3) {
   updateIfChanged(m_indiP_outlet3volt, "target", vc);
   updateIfChanged(m_indiP_outlet3volt, "current", m_channelVoltages[2]);
   }

   return 0;
}

INDI_NEWCALLBACK_DEFN(scpiCtrl, m_indiP_outlet1curr)(const pcf::IndiProperty &ipRecv)
{
   if (ipRecv.getName() != m_indiP_outlet1curr.getName())
   {
      log<software_error>({__FILE__, __LINE__, "wrong INDI property received."});
      return -1;
   }

   int vc = 0;

   if (ipRecv.find("current"))
      vc = ipRecv["current"].get<int>();

   if (ipRecv.find("target"))
      vc = ipRecv["target"].get<int>();

   std::unique_lock<std::mutex> lock(m_indiMutex);
   m_channelCurrents[0] = vc;
   int rv = setChannelAmps(0, vc);
   
   if(rv < 0)
   {
      log<software_error>({__FILE__, __LINE__, "Error setting channel 1 current!"});
      return -1;
   }

   if (m_numChannels >= 1) {
   updateIfChanged(m_indiP_outlet1curr, "target", vc);
   updateIfChanged(m_indiP_outlet1curr, "current", m_channelCurrents[0]);
   }

   return 0;
}

INDI_NEWCALLBACK_DEFN(scpiCtrl, m_indiP_outlet2curr)(const pcf::IndiProperty &ipRecv)
{
   if (ipRecv.getName() != m_indiP_outlet2curr.getName())
   {
      log<software_error>({__FILE__, __LINE__, "wrong INDI property received."});
      return -1;
   }

   int vc = 0;

   if (ipRecv.find("current"))
      vc = ipRecv["current"].get<int>();

   if (ipRecv.find("target"))
      vc = ipRecv["target"].get<int>();

   std::unique_lock<std::mutex> lock(m_indiMutex);
   m_channelCurrents[1] = vc;
   int rv = setChannelAmps(1, vc);
   
   if(rv < 0)
   {
      log<software_error>({__FILE__, __LINE__, "Error setting channel 2 current!"});
      return -1;
   }

   if (m_numChannels >= 2) {
   updateIfChanged(m_indiP_outlet2curr, "target", vc);
   updateIfChanged(m_indiP_outlet2curr, "current", m_channelCurrents[1]);
   }

   return 0;
}

INDI_NEWCALLBACK_DEFN(scpiCtrl, m_indiP_outlet3curr)(const pcf::IndiProperty &ipRecv)
{
   if (ipRecv.getName() != m_indiP_outlet3curr.getName())
   {
      log<software_error>({__FILE__, __LINE__, "wrong INDI property received."});
      return -1;
   }

   int vc = 0;

   if (ipRecv.find("current"))
      vc = ipRecv["current"].get<int>();

   if (ipRecv.find("target"))
      vc = ipRecv["target"].get<int>();

   std::unique_lock<std::mutex> lock(m_indiMutex);
   m_channelCurrents[2] = vc;
   int rv = setChannelAmps(2, vc);
   
   if(rv < 0)
   {
      log<software_error>({__FILE__, __LINE__, "Error setting channel 3 current!"});
      return -1;
   }

   if (m_numChannels >= 3) {
   updateIfChanged(m_indiP_outlet3curr, "target", vc);
   updateIfChanged(m_indiP_outlet3curr, "current", m_channelCurrents[2]);
   }

   return 0;
}

INDI_NEWCALLBACK_DEFN(scpiCtrl, m_indiP_resetToggle)(const pcf::IndiProperty &ipRecv)
{
    if (ipRecv.getName() != m_indiP_resetToggle.getName())
    {
        return log<software_error>({__FILE__, __LINE__, "Unexpected INDI property name: " + ipRecv.getName()});
    }
    
    // Check if reset was toggled on
    if (ipRecv["reset"].getSwitchState() == pcf::IndiElement::On)
    {
        log<text_log>("Reset toggle activated - sending *RST command");
        
        // Send reset command
        std::string res;
        if (send_scpi("*RST\n", res)) {
            log<text_log>("Device reset successful");
            
            // Reconfigure device based on current settings
            if (m_measurementMode == MeasurementMode::BUFFERED) {
                setupBufferedAcquisition();
            }
        } else {
            log<software_error>({__FILE__, __LINE__, "Failed to reset device"});
        }
        
        // Reset the toggle back to off
        updateSwitchIfChanged(m_indiP_resetToggle, "reset", pcf::IndiElement::Off);
    }

   return 0;
}

INDI_NEWCALLBACK_DEFN(scpiCtrl, m_indiP_telemetryToggle)(const pcf::IndiProperty &ipRecv)
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
   
   if(m_telemetryEnabled) {
       std::string modeStr = (m_measurementMode == MeasurementMode::BUFFERED ? "buffered" : 
                             m_measurementMode == MeasurementMode::DIGITIZED ? "digitized" : "polling");
       log<text_log>("Telemetry toggle ON - starting acquisition for mode: " + modeStr);
       
       // Start acquisition depending on mode
       if (!m_isPowerSupply) {
           if (m_measurementMode == MeasurementMode::BUFFERED || m_measurementMode == MeasurementMode::DIGITIZED) {
               // For buffered/digitized modes, only use buffer data logging (not regular telemetry)
               log<text_log>("Attempting to start buffer acquisition...");
               if (startBufferAcquisition() < 0) {
                   log<software_error>({__FILE__, __LINE__, "Failed to start buffer acquisition"});
                   // Reset telemetry toggle to OFF if buffer acquisition fails
                   m_telemetryEnabled = false;
                   updateSwitchIfChanged(m_indiP_telemetryToggle, "toggle", pcf::IndiElement::Off);
                   return -1;
               } else {
                   log<text_log>("Started buffer acquisition for " + std::string(m_measurementMode == MeasurementMode::BUFFERED ? "buffered" : "digitized") + " mode");
               }
           } else {
               // POLLING: use regular telemetry logging
               startTelemetryLogging();
               if (!m_polling.load()) startPollThread();
           }
       } else {
           // Power supply: use regular telemetry logging
           startTelemetryLogging();
       }
       
       // Always start polling thread to keep INDI properties updated
       if (!m_polling.load()) startPollThread();
   } else {
       // Stop acquisition and save data
        if (!m_isPowerSupply) {
            if (m_bufferAcquisitionActive) {
                if (stopBufferAcquisition() < 0) {
                    log<software_error>({__FILE__, __LINE__, "Failed to stop buffer acquisition"});
                } else {
                    log<text_log>("Stopped buffer acquisition and saved data");
                }
            } else {
                log<text_log>("Buffer acquisition already stopped");
            }
            // Don't stop polling thread - we need it to keep INDI properties updated
        }
       
       // Only stop telemetry logging if it was started (for polling mode or power supplies)
       if (m_measurementMode == MeasurementMode::POLLING || m_isPowerSupply) {
           stopTelemetryLogging();
       }
   }

   // Reflect new state back to INDI so GUI shows it
   updateSwitchIfChanged(m_indiP_telemetryToggle, "toggle", m_telemetryEnabled ? pcf::IndiElement::On : pcf::IndiElement::Off);

   return 0;
}

INDI_NEWCALLBACK_DEFN(scpiCtrl, m_indiP_samplingRateHz)(const pcf::IndiProperty &ipRecv)
{
    if (ipRecv.getName() != m_indiP_samplingRateHz.getName()) return -1;
    if (!ipRecv.find("value")) return -1;
    double newRate = ipRecv["value"].get<double>();
    if (newRate <= 0) {
        log<software_error>({__FILE__, __LINE__, "Invalid sampling rate: " + std::to_string(newRate) + " Hz (must be > 0)"});
        return -1;
    }
    std::unique_lock<std::mutex> lock(m_indiMutex);
    m_sampleRateHz = newRate;
    m_indiP_samplingRateHz["value"].set<double>(m_sampleRateHz);
    m_indiP_samplingRateHz.setState(pcf::IndiProperty::Busy);
    sendNewProperty(m_indiP_samplingRateHz);
    m_indiP_samplingRateHz.setState(pcf::IndiProperty::Idle);
    sendNewProperty(m_indiP_samplingRateHz);
    log<text_log>("Sampling rate changed to " + std::to_string(m_sampleRateHz) + " Hz");
    // If in buffered mode and ready, reconfigure and auto-tune
    if (state() == stateCodes::READY && !m_isPowerSupply && m_measurementMode == MeasurementMode::BUFFERED) {
        setupBufferedAcquisition();
    }
    return 0;
}

INDI_NEWCALLBACK_DEFN(scpiCtrl, m_indiP_bufferSize)(const pcf::IndiProperty &ipRecv)
{
    if (ipRecv.getName() != m_indiP_bufferSize.getName()) return -1;
    if (!ipRecv.find("target")) return -1;
    int targetSize = ipRecv["target"].get<int>();
    if (targetSize <= 0) return -1;
    
    std::unique_lock<std::mutex> lock(m_indiMutex);
    
    // Apply the change to the device
    if (!m_isPowerSupply && state() == stateCodes::READY) {
        std::string res;
        std::string cmd = "TRAC:POIN " + std::to_string(targetSize) + ",'defbuffer1'\n";
        if (send_scpi(cmd, res)) {
            m_bufferSize = targetSize;
            log<text_log>("Buffer size set to " + std::to_string(targetSize) + " samples");
            
            // Update both target and current values
            m_indiP_bufferSize["target"].set<int>(m_bufferSize);
            m_indiP_bufferSizeCurrent["current"].set<int>(m_bufferSize);
            m_indiP_bufferSize.setState(pcf::IndiProperty::Busy);
            m_indiP_bufferSizeCurrent.setState(pcf::IndiProperty::Busy);
            sendNewProperty(m_indiP_bufferSize);
            sendNewProperty(m_indiP_bufferSizeCurrent);
            m_indiP_bufferSize.setState(pcf::IndiProperty::Idle);
            m_indiP_bufferSizeCurrent.setState(pcf::IndiProperty::Idle);
            sendNewProperty(m_indiP_bufferSize);
            sendNewProperty(m_indiP_bufferSizeCurrent);
            
            // Reconfigure buffered acquisition if active
            if (m_measurementMode == MeasurementMode::BUFFERED) {
                setupBufferedAcquisition();
            }
        } else {
            log<software_error>({__FILE__, __LINE__, "Failed to set buffer size on device"});
            // Don't call readDeviceBufferSize() here as it can cause recursive callbacks
            // The device will be checked on the next regular update cycle
        }
    } else {
        // Just update the target value (will be applied when device is ready)
        m_bufferSize = targetSize;
        m_indiP_bufferSize["target"].set<int>(m_bufferSize);
        m_indiP_bufferSize.setState(pcf::IndiProperty::Busy);
        sendNewProperty(m_indiP_bufferSize);
        m_indiP_bufferSize.setState(pcf::IndiProperty::Idle);
        sendNewProperty(m_indiP_bufferSize);
    }
    
    return 0;
}

INDI_NEWCALLBACK_DEFN(scpiCtrl, m_indiP_measurementMode)(const pcf::IndiProperty &ipRecv)
{
    if (ipRecv.getName() != m_indiP_measurementMode.getName()) return -1;
    if (!ipRecv.find("value")) return -1;
    std::string mode = ipRecv["value"].get<std::string>();
    std::unique_lock<std::mutex> lock(m_indiMutex);
    if (mode == "buffered") m_measurementMode = MeasurementMode::BUFFERED;
    else if (mode == "digitized") m_measurementMode = MeasurementMode::DIGITIZED;
    else m_measurementMode = MeasurementMode::POLLING;
    updateIfChanged(m_indiP_measurementMode, "value", mode);
    // Reconfigure behavior
    if (!m_isPowerSupply && state() == stateCodes::READY) {
        if (m_measurementMode == MeasurementMode::BUFFERED) {
            setupBufferedAcquisition();
        } else if (m_measurementMode == MeasurementMode::POLLING) {
            // Ensure polling thread respects samplingRateHz for cadence
            m_pollRateHz = static_cast<int>(std::max(1.0, m_sampleRateHz));
            if (!m_polling.load()) startPollThread();
        }
    }
    return 0;
}

INDI_NEWCALLBACK_DEFN(scpiCtrl, m_indiP_measurementFunction)(const pcf::IndiProperty &ipRecv)
{
    if (ipRecv.getName() != m_indiP_measurementFunction.getName()) return -1;
    if (!ipRecv.find("target")) return -1;
    std::string targetFunction = ipRecv["target"].get<std::string>();
    
    std::unique_lock<std::mutex> lock(m_indiMutex);
    
    // Apply the change to the device
    if (!m_isPowerSupply && state() == stateCodes::READY) {
        std::string res;
        std::string cmd;
        if (targetFunction == "current") {
            cmd = "SENS:FUNC 'CURR'\n";
        } else {
            cmd = "SENS:FUNC 'VOLT'\n";
        }
        
        if (send_scpi(cmd, res)) {
            if (targetFunction == "current") {
                m_measurementFunction = MeasurementFunction::CURRENT;
            } else {
                m_measurementFunction = MeasurementFunction::VOLTAGE;
            }
            log<text_log>("Measurement function set to: " + targetFunction);
            
            // Update both target and current values
            updateIfChanged(m_indiP_measurementFunction, "target", targetFunction);
            updateIfChanged(m_indiP_measurementFunctionCurrent, "current", targetFunction);
        } else {
            log<software_error>({__FILE__, __LINE__, "Failed to set measurement function on device"});
            // Read back current value from device
            readDeviceMeasurementFunction();
        }
    } else {
        // Just update the target value (will be applied when device is ready)
        if (targetFunction == "current") {
            m_measurementFunction = MeasurementFunction::CURRENT;
        } else {
            m_measurementFunction = MeasurementFunction::VOLTAGE;
        }
        updateIfChanged(m_indiP_measurementFunction, "target", targetFunction);
    }
    
    return 0;
}

INDI_NEWCALLBACK_DEFN(scpiCtrl, m_indiP_currentConversionFactor)(const pcf::IndiProperty &ipRecv)
{
    if (ipRecv.getName() != m_indiP_currentConversionFactor.getName()) return -1;
    if (!ipRecv.find("value")) return -1;
    double factor = ipRecv["value"].get<double>();
    if (factor <= 0) {
        log<software_error>({__FILE__, __LINE__, "Invalid conversion factor: " + std::to_string(factor)});
        return -1;
    }
    std::unique_lock<std::mutex> lock(m_indiMutex);
    m_currentConversionFactor = factor;
    updateIfChanged(m_indiP_currentConversionFactor, "value", m_currentConversionFactor);
    log<text_log>("Current conversion factor changed to: " + std::to_string(factor) + " V/A");
   return 0;
}

INDI_NEWCALLBACK_DEFN(scpiCtrl, m_indiP_telemDir)(const pcf::IndiProperty &ipRecv)
{
    if (ipRecv.getName() != m_indiP_telemDir.getName()) return -1;
    if (!ipRecv.find("value")) return -1;
    std::string newDir = ipRecv["value"].get<std::string>();
    if (newDir.empty()) {
        log<software_error>({__FILE__, __LINE__, "Telemetry directory cannot be empty"});
        return -1;
    }
    std::unique_lock<std::mutex> lock(m_indiMutex);
    m_telemDir = newDir;
    m_indiP_telemDir["value"].set<std::string>(m_telemDir);
    m_indiP_telemDir.setState(pcf::IndiProperty::Busy);
    sendNewProperty(m_indiP_telemDir);
    m_indiP_telemDir.setState(pcf::IndiProperty::Idle);
    sendNewProperty(m_indiP_telemDir);
    log<text_log>("Telemetry directory changed to: " + m_telemDir);
    return 0;
}

int scpiCtrl::st_newCallBack_m_indiP_telemDir(void * p, const pcf::IndiProperty &ipRecv)
{
    return static_cast<scpiCtrl *>(p)->newCallBack_m_indiP_telemDir(ipRecv);
}


}//namespace app
} //namespace MagAOX

// Header-only consumption: include implementation unit so a single header provides all logic.
// Ensure your build does not also compile scpiCtrl.cpp separately.
// For full header-only, we inline key helpers here in small chunks.
namespace MagAOX { namespace app {

inline SCPIProtocol scpiCtrl::detectProtocol(const std::string& address)
{
    if (address.find("TCPIP") != std::string::npos) return SCPIProtocol::TCP_IP;
    if (address.find("/dev/usbtmc") != std::string::npos) return SCPIProtocol::USB_TMC;
    return SCPIProtocol::USB_TMC;
}

inline int scpiCtrl::parseAddress(const std::string& address)
{
    if (m_protocol == SCPIProtocol::TCP_IP) {
        std::regex tcpip_regex(R"(TCPIP\d*::([^:]+)::.*::INSTR)");
        std::smatch match;
        if (std::regex_match(address, match, tcpip_regex)) {
            m_ipAddress = match[1].str();
            log<text_log>("Parsed TCP/IP address: " + m_ipAddress + ":" + std::to_string(m_port));
            return 0;
        } else {
            log<software_error>({__FILE__, __LINE__, "Invalid TCPIP address format: " + address});
            return -1;
        }
    }
    return 0;
}

inline int scpiCtrl::connectUSB()
{
    fd = open(m_deviceAddr.c_str(), O_RDWR);
    if (fd < 0) {
        return log<text_log,-1>("Error connecting to USB TMC device: " + m_deviceAddr + " (" + std::string(strerror(errno)) + ")", logPrio::LOG_CRITICAL);
    }
    log<text_log>("Connected to USB TMC device: " + m_deviceAddr);
    return 0;
}

inline int scpiCtrl::connectTCP()
{
    m_socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socketFd < 0) {
        return log<text_log,-1>("Failed to create TCP socket: " + std::string(strerror(errno)), logPrio::LOG_CRITICAL);
    }
    int opt = 1; setsockopt(m_socketFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); setsockopt(m_socketFd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
    int flags = fcntl(m_socketFd, F_GETFL, 0);
    if (flags < 0 || fcntl(m_socketFd, F_SETFL, flags | O_NONBLOCK) < 0) { close(m_socketFd); m_socketFd = -1; return log<text_log,-1>("Failed to set socket non-blocking: " + std::string(strerror(errno)), logPrio::LOG_CRITICAL); }
    struct sockaddr_in server_addr; memset(&server_addr, 0, sizeof(server_addr)); server_addr.sin_family = AF_INET; server_addr.sin_port = htons(m_port);
    if (inet_pton(AF_INET, m_ipAddress.c_str(), &server_addr.sin_addr) <= 0) { close(m_socketFd); m_socketFd = -1; return log<text_log,-1>("Invalid IP address: " + m_ipAddress, logPrio::LOG_CRITICAL); }
    int connect_result = connect(m_socketFd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (connect_result < 0 && errno != EINPROGRESS) { close(m_socketFd); m_socketFd = -1; return log<text_log,-1>("Failed to initiate connection to " + m_ipAddress + ":" + std::to_string(m_port) + " (" + std::string(strerror(errno)) + ")", logPrio::LOG_CRITICAL); }
    if (connect_result < 0 && errno == EINPROGRESS) {
        fd_set write_fds, except_fds; struct timeval timeout; FD_ZERO(&write_fds); FD_ZERO(&except_fds);
        FD_SET(m_socketFd, &write_fds); FD_SET(m_socketFd, &except_fds);
        timeout.tv_sec = m_connectTimeout / 1000; timeout.tv_usec = (m_connectTimeout % 1000) * 1000;
        int select_result = select(m_socketFd + 1, NULL, &write_fds, &except_fds, &timeout);
        if (select_result <= 0) { close(m_socketFd); m_socketFd = -1; if (select_result == 0) return log<text_log,-1>("Connection timeout to " + m_ipAddress + ":" + std::to_string(m_port), logPrio::LOG_CRITICAL); else return log<text_log,-1>("Connection select error: " + std::string(strerror(errno)), logPrio::LOG_CRITICAL); }
        if (FD_ISSET(m_socketFd, &except_fds)) { close(m_socketFd); m_socketFd = -1; return log<text_log,-1>("Connection failed to " + m_ipAddress + ":" + std::to_string(m_port), logPrio::LOG_CRITICAL); }
        int error = 0; socklen_t len = sizeof(error);
        if (getsockopt(m_socketFd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) { close(m_socketFd); m_socketFd = -1; return log<text_log,-1>("Failed to get socket error: " + std::string(strerror(errno)), logPrio::LOG_CRITICAL); }
        if (error != 0) { close(m_socketFd); m_socketFd = -1; return log<text_log,-1>("Connection failed to " + m_ipAddress + ":" + std::to_string(m_port) + " (" + std::string(strerror(error)) + ")", logPrio::LOG_CRITICAL); }
    }
    if (fcntl(m_socketFd, F_SETFL, flags) < 0) { close(m_socketFd); m_socketFd = -1; return log<text_log,-1>("Failed to restore socket blocking mode: " + std::string(strerror(errno)), logPrio::LOG_CRITICAL); }
    struct timeval recv_timeout; recv_timeout.tv_sec = 2; recv_timeout.tv_usec = 0; setsockopt(m_socketFd, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));
    log<text_log>("Connected to TCP/IP device: " + m_ipAddress + ":" + std::to_string(m_port));
    return 0;
}

inline int scpiCtrl::devDisconnect()
{
    int result = 0; if (fd >= 0) { if (close(fd) < 0) { log<software_error>({__FILE__, __LINE__, "Error closing USB TMC device: " + std::string(strerror(errno))}); result = -1; } fd = -1; } if (m_socketFd >= 0) { if (close(m_socketFd) < 0) { log<software_error>({__FILE__, __LINE__, "Error closing TCP socket: " + std::string(strerror(errno))}); result = -1; } m_socketFd = -1; } return result;
}

// ---- Buffered auto-tune helpers ----
inline int scpiCtrl::applyBaseSpeedSettings()
{
    std::string res;
    if (!send_scpi("SENS:FUNC 'VOLT'\n", res)) return -1;
    if (!send_scpi("SENS:VOLT:RANG 20\n", res)) return -1;
    if (!send_scpi("SYST:AZER:STAT OFF\n", res)) return -1;
    // If not supported, ignore
    send_scpi("SENS:VOLT:DFIL:STAT OFF\n", res);
    return 0;
}

inline int scpiCtrl::applyNPLC(double nplc)
{
    std::string res; std::ostringstream oss; oss << std::fixed << std::setprecision(3) << nplc;
    std::string cmd = std::string("SENS:VOLT:NPLC ") + oss.str() + "\n";
    return send_scpi(cmd, res) ? 0 : -1;
}

inline int scpiCtrl::autoTuneBuffered(double targetRateHz, int maxIterations, double toleranceSeconds)
{
    if (targetRateHz <= 0) return -1;
    double period = 1.0 / targetRateHz;
    double nplc = std::max(0.02, period * 60.0); // initial guess
    for (int iter = 0; iter < maxIterations; ++iter) {
        if (applyNPLC(nplc) < 0) break;
        std::string res;
        // Short test acquisition (up to 200 samples)
        int testN = std::min(m_bufferSize, 200);
        send_scpi("TRAC:CLE 'defbuffer1'\n", res);
        {
            std::ostringstream oss; oss << "TRAC:POIN " << testN << ", 'defbuffer1'\n"; send_scpi(oss.str(), res);
        }
        {
            std::ostringstream oss; oss << "TRIG:LOAD 'SimpleLoop'," << testN << ",0\n"; send_scpi(oss.str(), res);
        }
        send_scpi("INIT\n", res);
        send_scpi("*WAI\n", res);
        std::string data_str;
        {
            std::ostringstream oss; oss << "TRAC:DATA? 1," << testN << ", 'defbuffer1',READ,REL\n";
            if (!send_scpi(oss.str(), data_str)) break;
        }
        // Parse alternating value,time
        std::vector<double> times; times.reserve(testN);
        data_str.erase(std::remove_if(data_str.begin(), data_str.end(), ::isspace), data_str.end());
        std::stringstream ss(data_str); std::string tok; bool isTime=false;
        while (std::getline(ss, tok, ',')) {
            try { double v = std::stod(tok); if (isTime) times.push_back(v); isTime = !isTime; } catch(...) {}
        }
        if (times.size() < 3) break;
        double sum=0.0; int cnt=0;
        for (size_t i=2; i<times.size(); ++i) { double dt = times[i]-times[i-1]; if (dt > 0) { sum += dt; cnt++; } }
        if (cnt == 0) break;
        double avg = sum / cnt; double err = std::abs(avg - period);
        log<text_log>("AutoTune iter " + std::to_string(iter+1) + ": avg dt " + std::to_string(avg) + " s with NPLC=" + std::to_string(nplc));
        if (err < toleranceSeconds) return 0;
        double scale = period / avg;
        nplc = std::max(0.02, std::min(10.0, nplc * scale));
    }
    return 0;
}

// ---- Buffered acquisition methods ----
inline int scpiCtrl::setupBufferedAcquisition()
{
    // Prevent recursive calls during setup
    static bool setupInProgress = false;
    if (setupInProgress) {
        log<text_log>("Buffered acquisition setup already in progress, skipping");
        return 0;
    }
    setupInProgress = true;
    
    // RAII helper to ensure flag is reset on any exit
    struct SetupGuard {
        bool& flag;
        SetupGuard(bool& f) : flag(f) {}
        ~SetupGuard() { flag = false; }
    } guard(setupInProgress);
    
    log<text_log>("Setting up buffered acquisition: " + std::to_string(m_bufferSize) + " samples, target rate " + 
                  std::to_string(m_sampleRateHz) + " Hz");
    
    std::string res;
    
    // Configure for voltage measurement (skip reset to avoid connection issues)
    if (!send_scpi("SENS:FUNC 'VOLT'\n", res)) {
        log<text_log>("Warning: Failed to set voltage function, trying reset", logPrio::LOG_WARNING);
        // Only reset if the function setting fails
        if (!send_scpi("*RST\n", res)) {
            return log<text_log,-1>("Failed to reset device", logPrio::LOG_ERROR);
        }
        if (!send_scpi("SENS:FUNC 'VOLT'\n", res)) {
            return log<text_log,-1>("Failed to set voltage function after reset", logPrio::LOG_ERROR);
        }
    }
    
    // Clear and resize buffer to exactly what we need
    if (!send_scpi("TRAC:CLE 'defbuffer1'\n", res)) {
        return log<text_log,-1>("Failed to clear buffer", logPrio::LOG_ERROR);
    }
    
    // Configure buffer to store timestamps
    if (!send_scpi("TRAC:FORMAT TST\n", res)) {
        log<text_log>("Warning: Failed to set buffer format to include timestamps", logPrio::LOG_WARNING);
    }
    
    std::string buffer_cmd = "TRAC:POIN " + std::to_string(m_bufferSize) + ",'defbuffer1'\n";
    if (!send_scpi(buffer_cmd, res)) {
        return log<text_log,-1>("Failed to configure buffer size", logPrio::LOG_ERROR);
    }
    // Give the device time to process the buffer configuration
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Apply base speed settings and auto-tune NPLC for target rate with zero delay
    if (applyBaseSpeedSettings() < 0) {
        return log<text_log,-1>("Failed to apply base speed settings", logPrio::LOG_ERROR);
    }
    if (autoTuneBuffered(m_sampleRateHz, 6, 0.0005) < 0) {
        log<text_log>("Auto-tune did not converge; proceeding with best effort", logPrio::LOG_WARNING);
    }
    // Configure zero-delay SimpleLoop
    std::string trigger_cmd = "TRIG:LOAD 'SimpleLoop'," + std::to_string(m_bufferSize) + ",0\n";
    if (!send_scpi(trigger_cmd, res)) {
        return log<text_log,-1>("Failed to configure trigger system (SimpleLoop,0)", logPrio::LOG_ERROR);
    }
    // Give the device time to process the trigger configuration
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    log<text_log>("Buffered acquisition configured successfully");
    return 0;
}

inline int scpiCtrl::startBufferAcquisition()
{
    if (m_bufferAcquisitionActive) {
        log<text_log>("Buffer acquisition already active", logPrio::LOG_WARNING);
        return 0;
    }
    
    log<text_log>("Starting buffer acquisition...");
    std::string res;
    
    // Clear the buffer before starting acquisition
    log<text_log>("Clearing buffer before acquisition...");
    if (!send_scpi("TRAC:CLE 'defbuffer1'\n", res)) {
        return log<text_log,-1>("Failed to clear buffer", logPrio::LOG_ERROR);
    }
    
    // Start acquisition
    log<text_log>("Sending INIT command...");
    if (!send_scpi("INIT\n", res)) {
        return log<text_log,-1>("Failed to initiate buffer acquisition", logPrio::LOG_ERROR);
    }
    
    // Don't wait for completion with *WAI - let the device run and check status periodically
    m_bufferAcquisitionActive = true;
    m_acquisitionStartTime = std::chrono::steady_clock::now();
    
    log<text_log>("Buffer acquisition started - will collect " + std::to_string(m_bufferSize) + " samples");
    
    return 0;
}

inline int scpiCtrl::stopBufferAcquisition()
{
    if (!m_bufferAcquisitionActive) {
        log<text_log>("Buffer acquisition not active", logPrio::LOG_WARNING);
        return 0;
    }
    
    std::string res;
    std::vector<float> voltages;
    std::vector<double> timestamps;
    
    // Stop any ongoing acquisition
    if (!send_scpi("ABOR\n", res)) {
        log<software_error>({__FILE__, __LINE__, "Failed to abort acquisition"});
    }
    
    // Wait for completion if still running
    if (!send_scpi("*WAI\n", res)) {
        log<software_error>({__FILE__, __LINE__, "Failed to wait for completion"});
    }
    
    // Restart continuous trigger (like Python getDataSet)
    if (!send_scpi("TRIG:CONT RESTART\n", res)) {
        log<software_error>({__FILE__, __LINE__, "Failed to restart continuous trigger"});
    }
    
    // Read out any data that was collected
    if (getBufferedData(voltages, timestamps) == 0 && !voltages.empty()) {
        // Save the data to file
        std::string filename = generateBufferFilename();
        if (saveBufferData(voltages, timestamps, filename) == 0) {
            log<text_log>("Saved " + std::to_string(voltages.size()) + " samples to " + filename);
        } else {
            log<software_error>({__FILE__, __LINE__, "Failed to save buffer data"});
        }
    } else {
        log<text_log>("No buffer data to save");
    }
    
    m_bufferAcquisitionActive = false;
    
    return 0;
}

inline int scpiCtrl::checkBufferStatus()
{
    if (!m_bufferAcquisitionActive) {
        return 0;
    }
    
    // Check for timeout - if acquisition has been running too long, stop it
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_acquisitionStartTime);
    if (elapsed.count() > 30) { // 30 second timeout
        log<text_log>("Buffer acquisition timeout - stopping after " + std::to_string(elapsed.count()) + " seconds", logPrio::LOG_WARNING);
        return stopBufferAcquisition();
    }
    
    // Check for overrun risk
    if (isBufferOverrunRisk()) {
        log<text_log>("Buffer overrun risk detected - stopping acquisition", logPrio::LOG_WARNING);
        return stopBufferAcquisition();
    }
    
    // Check if acquisition completed or buffer full; if full, auto-stop telemetry
    std::string res;
    if (send_scpi("TRIG:STAT?\n", res)) {
        res.erase(res.find_last_not_of(" \n\r\t") + 1);
        if (res == "IDLE") {
            log<text_log>("Buffer acquisition completed naturally");
            return stopBufferAcquisition();
        }
    }
    // If buffer reached size, stop and save
    std::string act;
    if (send_scpi("TRAC:ACT? 'defbuffer1'\n", act)) {
        try {
            int count = std::stoi(act);
            log<text_log>("Buffer status check: " + std::to_string(count) + "/" + std::to_string(m_bufferSize) + " samples");
            if (count >= m_bufferSize) {
                // Wait a bit more to ensure buffer is completely full
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
                // Check again to make sure we have the full buffer
                std::string act2;
                if (send_scpi("TRAC:ACT? 'defbuffer1'\n", act2)) {
                    try {
                        int count2 = std::stoi(act2);
                        log<text_log>("Buffer full (" + std::to_string(count2) + "/" + std::to_string(m_bufferSize) + ") - stopping acquisition");
                        int rv = stopBufferAcquisition();
                        // Auto-toggle telemetry OFF (but don't call callback to avoid double-stop)
                        m_telemetryEnabled = false;
                        m_indiP_telemetryToggle["toggle"].setSwitchState(pcf::IndiElement::Off);
                        m_indiP_telemetryToggle.setState(pcf::IndiProperty::Idle);
                        sendNewProperty(m_indiP_telemetryToggle);
                        // Don't stop telemetry logging here - it's not started for buffered mode
                        return rv;
                    } catch(...) {
                        // If second check fails, proceed with first count
                        log<text_log>("Buffer full (" + std::to_string(count) + "/" + std::to_string(m_bufferSize) + ") - stopping acquisition");
                        int rv = stopBufferAcquisition();
                        // Auto-toggle telemetry OFF (but don't call callback to avoid double-stop)
                        m_telemetryEnabled = false;
                        m_indiP_telemetryToggle["toggle"].setSwitchState(pcf::IndiElement::Off);
                        m_indiP_telemetryToggle.setState(pcf::IndiProperty::Idle);
                        sendNewProperty(m_indiP_telemetryToggle);
                        return rv;
                    }
                }
            }
        } catch(...) {}
    }
    
    return 0;
}

inline bool scpiCtrl::isBufferOverrunRisk()
{
    if (!m_bufferAcquisitionActive) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_acquisitionStartTime);
    
    return elapsed.count() >= m_maxAcquisitionTime;
}

inline std::string scpiCtrl::generateBufferFilename()
{
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now;
    gmtime_r(&now_time_t, &tm_now);
    
    // Create full path with telemetry directory
    std::string fullPath = m_bufferDataPath + "/" + m_telemDir;
    
    // Determine mode-specific filename prefix
    std::string modePrefix;
    if (m_measurementMode == MeasurementMode::BUFFERED) {
        modePrefix = "scpiCtrl_buffered";
    } else if (m_measurementMode == MeasurementMode::DIGITIZED) {
        modePrefix = "scpiCtrl_digitized";
    } else {
        modePrefix = "scpiCtrl_polling";
    }
    
    char fname[512];
    std::snprintf(fname, sizeof(fname), "%s/%s_%04d%02d%02d_%02d%02d%02d.csv", 
                  fullPath.c_str(), modePrefix.c_str(),
                  tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
                  tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
    
    return std::string(fname);
}

inline int scpiCtrl::saveBufferData(const std::vector<float>& voltages, const std::vector<double>& timestamps, const std::string& filename)
{
    if (voltages.empty()) {
        return log<text_log,-1>("No data to save", logPrio::LOG_WARNING);
    }
    
    std::string fname = filename.empty() ? generateBufferFilename() : filename;
    
    // Create directory if it doesn't exist (including telemetry subdirectory)
    std::string dir = m_bufferDataPath + "/" + m_telemDir;
    if (mkdir(dir.c_str(), 0755) < 0 && errno != EEXIST) {
        return log<text_log,-1>("Failed to create data directory: " + dir, logPrio::LOG_ERROR);
    }
    
    std::ofstream file(fname);
    if (!file.is_open()) {
        return log<text_log,-1>("Failed to open file for writing: " + fname, logPrio::LOG_ERROR);
    }
    
    // Write header
    file << "timestamp_s,voltage_v,sample_index" << std::endl;
    
    // Write data
    for (size_t i = 0; i < voltages.size(); ++i) {
        double timestamp = (i < timestamps.size()) ? timestamps[i] : (i * m_sampleInterval);
        file << std::fixed << std::setprecision(9) << timestamp << "," 
             << std::setprecision(6) << voltages[i] << "," 
             << i << std::endl;
    }
    
    file.close();
    
    log<text_log>("Saved " + std::to_string(voltages.size()) + " samples to " + fname);
    return 0;
}

inline int scpiCtrl::getBufferedData(std::vector<float>& voltages, std::vector<double>& timestamps)
{
    std::string res;
    
    // First check how many samples are actually available
    std::string count_cmd = "TRAC:ACT?\n";
    std::string count_res;
    
    // Add a small delay to ensure device is ready
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Retry logic for buffer status check
    int retry_count = 0;
    while (retry_count < 3) {
        if (send_scpi(count_cmd, count_res)) {
            break;
        }
        retry_count++;
        if (retry_count < 3) {
            log<text_log>("Buffer status check failed, retrying... (" + std::to_string(retry_count) + "/3)");
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }
    
    if (retry_count >= 3) {
        log<software_error>({__FILE__, __LINE__, "Failed to check buffer status after retries"});
        return -1;
    }
    
    int available_samples = 0;
    try {
        available_samples = std::stoi(count_res);
    } catch (...) {
        log<software_error>({__FILE__, __LINE__, "Failed to parse buffer count: " + count_res});
        return -1;
    }
    
    log<text_log>("Buffer has " + std::to_string(available_samples) + " samples available");
    
    if (available_samples == 0) {
        log<text_log>("No samples available in buffer");
        return 0;
    }
    
    // Read data from buffer - only read what's available
    int samples_to_read = std::min(available_samples, m_bufferSize);
    std::string data_cmd = "TRAC:DATA? 1," + std::to_string(samples_to_read) + ",'defbuffer1',READ,REL\n";
    std::string data_str;
    log<text_log>("Sending buffer data command: " + data_cmd);
    if (!send_scpi(data_cmd, data_str)) {
        log<software_error>({__FILE__, __LINE__, "Failed to read buffered data - device may be unresponsive"});
        return -1;
    }
    log<text_log>("Received buffer data length: " + std::to_string(data_str.length()));
    log<text_log>("Buffer data sample: " + data_str.substr(0, std::min(100, (int)data_str.length())));
    
    // Debug: Check if we have timestamps by looking for the pattern
    if (data_str.find(',') != std::string::npos) {
        log<text_log>("Buffer data contains commas - likely has timestamps");
    } else {
        log<text_log>("Buffer data has no commas - likely values only");
    }
    
    // Parse the data - format should be [timestamp1, voltage1, timestamp2, voltage2, ...]
    voltages.clear();
    timestamps.clear();
    
    // Debug: log first 200 characters of data to understand format
    std::string debug_data = data_str.substr(0, std::min(200, (int)data_str.length()));
    log<text_log>("Buffer data sample: " + debug_data);
    
    // Use a simpler approach: split by commas and handle each token
    std::vector<std::string> tokens;
    std::stringstream ss(data_str);
    std::string token;
    
    while (std::getline(ss, token, ',')) {
        // Trim whitespace from token
        token.erase(0, token.find_first_not_of(" \t\r\n"));
        token.erase(token.find_last_not_of(" \t\r\n") + 1);
        
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    
    // Parse tokens to floats
    log<text_log>("Parsed " + std::to_string(tokens.size()) + " tokens from buffer data");
    std::vector<float> all_data;
    for (const auto& token : tokens) {
        if (!token.empty()) {
            try {
                all_data.push_back(std::stof(token));
            } catch (...) {
                // Try to handle common parsing issues
                std::string clean_token = token;
                
                // Remove any remaining whitespace
                clean_token.erase(std::remove_if(clean_token.begin(), clean_token.end(), ::isspace), clean_token.end());
                
                // Try parsing again
                try {
                    all_data.push_back(std::stof(clean_token));
                } catch (...) {
                    // If still failing, try to handle scientific notation manually
                    if (clean_token.find('E') != std::string::npos || clean_token.find('e') != std::string::npos) {
                        try {
                            // Convert to lowercase for consistent parsing
                            std::string lower_token = clean_token;
                            std::transform(lower_token.begin(), lower_token.end(), lower_token.begin(), ::tolower);
                            all_data.push_back(std::stof(lower_token));
                        } catch (...) {
                            log<software_error>({__FILE__, __LINE__, "Failed to parse scientific notation token: '" + token + "' (cleaned: '" + clean_token + "')"});
                        }
                    } else {
                        log<software_error>({__FILE__, __LINE__, "Failed to parse data token: '" + token + "' (cleaned: '" + clean_token + "')"});
                    }
                }
            }
        }
    }
    
     // The data format from TRAC:DATA? with READ,REL and TST format should be [voltage1, timestamp1, voltage2, timestamp2, ...]
     // Timestamps are in seconds from the start of acquisition
     
     if (all_data.size() % 2 == 0) {
         // We have pairs: [voltage1, timestamp1, voltage2, timestamp2, ...]
         for (size_t i = 0; i < all_data.size(); i += 2) {
             if (i + 1 < all_data.size()) {
                 // First value is voltage, second is timestamp
                 voltages.push_back(all_data[i]);
                 // Timestamps are already in seconds, convert to nanoseconds
                 timestamps.push_back(static_cast<double>(all_data[i + 1]) * 1e9);
             }
         }
         log<text_log>("Parsed " + std::to_string(timestamps.size()) + " samples with device timestamps (voltage, timestamp pairs)");
     } else {
         // We have just values: [value1, value2, value3, ...]
         // This means timestamps weren't stored in buffer, generate them
         for (size_t i = 0; i < all_data.size(); i++) {
             voltages.push_back(all_data[i]);
             // Generate timestamps based on sampling rate
             double timestamp = static_cast<double>(i) / m_sampleRateHz;
             timestamps.push_back(timestamp * 1e9); // Convert to nanoseconds
         }
         log<text_log>("Parsed " + std::to_string(timestamps.size()) + " samples with generated timestamps (buffer not configured for timestamps)");
     }
    
    log<text_log>("Retrieved " + std::to_string(voltages.size()) + " buffered samples");
    return 0;
}

inline int scpiCtrl::setupDigitizedAcquisition(double sampleRate, double aperture, double voltageRange)
{
    log<text_log>("Setting up digitized acquisition: " + std::to_string(sampleRate) + " S/s, " + 
                  std::to_string(voltageRange) + "V range");
    
    std::string res;
    
    // Reset and configure for digitized voltage measurement
    if (!send_scpi("*RST\n", res)) {
        return log<text_log,-1>("Failed to reset device", logPrio::LOG_ERROR);
    }
    
    if (!send_scpi("SENS:DIG:FUNC 'VOLT'\n", res)) {
        return log<text_log,-1>("Failed to set digitize function", logPrio::LOG_ERROR);
    }
    
    // Configure digitization parameters
    std::string range_cmd = "SENS:DIG:VOLT:RANG " + std::to_string(voltageRange) + "\n";
    if (!send_scpi(range_cmd, res)) {
        return log<text_log,-1>("Failed to set voltage range", logPrio::LOG_ERROR);
    }
    
    std::string rate_cmd = "SENS:DIG:VOLT:SRAT " + std::to_string(sampleRate) + "\n";
    if (!send_scpi(rate_cmd, res)) {
        return log<text_log,-1>("Failed to set sample rate", logPrio::LOG_ERROR);
    }
    
    std::string aperture_cmd = "SENS:DIG:VOLT:APER " + std::to_string(aperture) + "\n";
    if (!send_scpi(aperture_cmd, res)) {
        return log<text_log,-1>("Failed to set aperture", logPrio::LOG_ERROR);
    }
    
    if (!send_scpi("SENS:DIG:VOLT:COUP DC\n", res)) {
        return log<text_log,-1>("Failed to set coupling", logPrio::LOG_ERROR);
    }
    
    if (!send_scpi("SENS:DIG:VOLT:INP AUTO\n", res)) {
        return log<text_log,-1>("Failed to set input impedance", logPrio::LOG_ERROR);
    }
    
    std::string count_cmd = "SENS:DIG:COUN " + std::to_string(m_bufferSize) + "\n";
    if (!send_scpi(count_cmd, res)) {
        return log<text_log,-1>("Failed to set sample count", logPrio::LOG_ERROR);
    }
    
    // Configure buffer to store timestamps
    if (!send_scpi("TRAC:FORMAT TST\n", res)) {
        log<text_log>("Warning: Failed to set buffer format to include timestamps", logPrio::LOG_WARNING);
    }
    
    // Configure buffer
    std::string buffer_cmd = "TRAC:POIN " + std::to_string(m_bufferSize) + ",'defbuffer1'\n";
    if (!send_scpi(buffer_cmd, res)) {
        return log<text_log,-1>("Failed to configure buffer", logPrio::LOG_ERROR);
    }
    
    log<text_log>("Digitized acquisition configured successfully");
    return 0;
}

inline int scpiCtrl::getDigitizedData(std::vector<float>& voltages, std::vector<double>& timestamps)
{
    std::string res;
    
    // Trigger digitized acquisition
    if (!send_scpi("TRAC:TRIG:DIG 'defbuffer1'\n", res)) {
        return log<text_log,-1>("Failed to trigger digitized acquisition", logPrio::LOG_ERROR);
    }
    
    // Read data with timestamps
    std::string data_cmd = "TRAC:DATA? 1," + std::to_string(m_bufferSize) + ",'defbuffer1',READ,REL\n";
    std::string data_str;
    if (!send_scpi(data_cmd, data_str)) {
        return log<text_log,-1>("Failed to read digitized data", logPrio::LOG_ERROR);
    }
    
    // Parse the data (same format as buffered)
    voltages.clear();
    timestamps.clear();
    
    data_str.erase(std::remove_if(data_str.begin(), data_str.end(), ::isspace), data_str.end());
    
    std::stringstream ss(data_str);
    std::string token;
    std::vector<float> all_data;
    
    while (std::getline(ss, token, ',')) {
        try {
            all_data.push_back(std::stof(token));
        } catch (...) {
            log<software_error>({__FILE__, __LINE__, "Failed to parse digitized data token: " + token});
        }
    }
    
    // Split into voltages and timestamps - format is [voltage1, timestamp1, voltage2, timestamp2, ...]
    for (size_t i = 0; i < all_data.size(); i += 2) {
        if (i + 1 < all_data.size()) {
            voltages.push_back(all_data[i]);
            timestamps.push_back(static_cast<double>(all_data[i + 1]) * 1e9); // Convert to nanoseconds
        }
    }
    
    // Restart continuous triggering
    send_scpi("TRIG:CONT RESTART\n", res);
    
    log<text_log>("Retrieved " + std::to_string(voltages.size()) + " digitized samples");
    return 0;
}

// ---- Threading and polling methods ----
inline void scpiCtrl::startPollThread()
{
    if(m_pollThreadStarted) return;
    m_polling = true;
    m_pollThread = std::thread(&scpiCtrl::pollLoop, this);
    m_pollThreadStarted = true;
}

inline void scpiCtrl::stopPollThread()
{
    m_polling = false;
    if(m_pollThreadStarted && m_pollThread.joinable()) m_pollThread.join();
    m_pollThreadStarted = false;
}

inline void scpiCtrl::pollLoop()
{
    const int sleep_us = std::max(1, 1000000 / std::max(1, m_pollRateHz));
    log<text_log>("Poll thread started with rate " + std::to_string(m_pollRateHz) + " Hz");
    
    while(m_polling)
    {
        auto loop_start = std::chrono::steady_clock::now();
        
        // Debug log every 60 seconds (only when mode changes or periodically)
        static auto last_debug = std::chrono::steady_clock::now();
        static int last_mode = -1;
        int current_mode = static_cast<int>(m_measurementMode);
        if (std::chrono::duration_cast<std::chrono::seconds>(loop_start - last_debug).count() >= 60 || 
            current_mode != last_mode) {
            log<text_log>("Poll loop active - device type: " + std::string(m_isPowerSupply ? "PowerSupply" : "Measurement") + 
                         ", channels: " + std::to_string(m_numChannels) + 
                         ", mode: " + std::string(m_measurementMode == MeasurementMode::POLLING ? "POLLING" : 
                                                 m_measurementMode == MeasurementMode::BUFFERED ? "BUFFERED" : "DIGITIZED"));
            last_debug = loop_start;
            last_mode = current_mode;
        }

        {
            std::unique_lock<std::mutex> lock(m_indiMutex, std::try_to_lock);
            if(lock.owns_lock())
            {
                for(int ch = 0; ch < m_numChannels; ++ch)
                {
                    // Use the device-aware update method instead of hardcoded SCPI commands
                    if (updateChannel(ch) == 0) {
                        // Update outlet state based on device type
                        if (!m_isPowerSupply) {
                            // For measurement devices, always mark as "on"
                            m_outletStates[ch] = OUTLET_STATE_ON;
                        } else if (m_numChannels > 1) {
                            // Multi-channel power supply - query output state
                            std::string res;
                            std::string cmd_sel = "INST:NSEL " + std::to_string(ch + 1) + "\n";
                            if (send_scpi(cmd_sel, res)) {
                                std::string outp;
                                if (send_scpi("OUTP?\n", outp)) {
                                    int st = 0; try { st = std::stoi(outp); } catch(...) { st = 0; }
                                    m_outletStates[ch] = (st == 1 ? OUTLET_STATE_ON : OUTLET_STATE_OFF);
                                }
                            }
                        } else {
                            // Single-channel power supply - query output state directly
                            std::string outp;
                            if (send_scpi("OUTP?\n", outp)) {
                                int st = 0; try { st = std::stoi(outp); } catch(...) { st = 0; }
                                m_outletStates[ch] = (st == 1 ? OUTLET_STATE_ON : OUTLET_STATE_OFF);
                            }
                        }
                    }
                }

                // Push INDI updates quickly
                updateIfChanged(m_indiP_outlet1volt, "current", m_channelVoltages[0]);
                updateIfChanged(m_indiP_outlet1curr, "current", m_channelCurrents[0]);
                if(m_numChannels > 1)
                {
                    updateIfChanged(m_indiP_outlet2volt, "current", m_channelVoltages[1]);
                    updateIfChanged(m_indiP_outlet2curr, "current", m_channelCurrents[1]);
                }
                if(m_numChannels > 2)
                {
                    updateIfChanged(m_indiP_outlet3volt, "current", m_channelVoltages[2]);
                    updateIfChanged(m_indiP_outlet3curr, "current", m_channelCurrents[2]);
                }
                dev::outletController<scpiCtrl>::updateINDI();
                
                // Telemetry write at high rate if enabled (only for polling mode)
                if(m_telemetryEnabled && m_measurementMode == MeasurementMode::POLLING)
                {
                    writeTelemetryData();
                }
            }
        }

        // Sleep to maintain rate
        auto elapsed = std::chrono::steady_clock::now() - loop_start;
        auto sleep_dur = std::chrono::microseconds(sleep_us) - std::chrono::duration_cast<std::chrono::microseconds>(elapsed);
        if(sleep_dur.count() > 0) std::this_thread::sleep_for(sleep_dur);
    }
}

// ---- Telemetry methods ----
inline int scpiCtrl::startTelemetryLogging()
{
    if(!m_telemetryFile.is_open())
    {
        // Create telemetry directory if it doesn't exist
        std::string dir = m_telemetryPath + "/" + m_telemDir;
        if (mkdir(dir.c_str(), 0755) < 0 && errno != EEXIST) {
            return log<text_log,-1>("Failed to create telemetry directory: " + dir, logPrio::LOG_ERROR);
        }
        
        m_telemetryFilename = generateTelemetryFilename();
        m_telemetryFile.open(m_telemetryFilename, std::ios::out | std::ios::app);
        if(!m_telemetryFile.is_open())
        {
            return log<text_log,-1>("Failed to open telemetry file: " + m_telemetryFilename, logPrio::LOG_ERROR);
        }
        m_telemetryFile << "epoch_ns,volts_1,amps_1,volts_2,amps_2" << std::endl;
    }
    return 0;
}

inline int scpiCtrl::stopTelemetryLogging()
{
    if(m_telemetryFile.is_open())
    {
        m_telemetryFile.flush();
        m_telemetryFile.close();
    }
    return 0;
}

inline int scpiCtrl::writeTelemetryData()
{
    if(!m_telemetryEnabled) {
        static bool logged_disabled = false;
        if (!logged_disabled) {
            log<text_log>("Telemetry disabled - not writing data");
            logged_disabled = true;
        }
        return 0;
    }
    
    if(!m_telemetryFile.is_open())
    {
        log<text_log>("Opening telemetry file...");
        if(startTelemetryLogging() < 0) return -1;
    }

    auto now = std::chrono::system_clock::now();
    long long epoch_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

    float v1 = (m_channelVoltages.size() > 0 ? m_channelVoltages[0] : 0.0f);
    float a1 = (m_channelCurrents.size() > 0 ? m_channelCurrents[0] : 0.0f);
    float v2 = (m_channelVoltages.size() > 1 ? m_channelVoltages[1] : 0.0f);
    float a2 = (m_channelCurrents.size() > 1 ? m_channelCurrents[1] : 0.0f);

    // Debug log every 500 writes to avoid spam
    static int write_count = 0;
    write_count++;
    if (write_count % 500 == 0) {
        log<text_log>("Writing telemetry #" + std::to_string(write_count) + ": V1=" + std::to_string(v1) + 
                     ", A1=" + std::to_string(a1) + ", V2=" + std::to_string(v2) + ", A2=" + std::to_string(a2));
    }

    m_telemetryFile << epoch_ns << "," << v1 << "," << a1 << "," << v2 << "," << a2 << std::endl;
    m_telemetryFile.flush(); // Ensure data is written immediately
    return 0;
}

inline std::string scpiCtrl::generateTelemetryFilename()
{
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now;
    gmtime_r(&now_time_t, &tm_now);
    
    // Create full path with telemetry directory
    std::string fullPath = m_telemetryPath + "/" + m_telemDir;
    
    char fname[512];
    std::snprintf(fname, sizeof(fname), "%s/scpiCtrl_polling_%04d%02d%02d_%02d%02d%02d.csv", fullPath.c_str(),
                  tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
                  tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
    return std::string(fname);
}

}} // ns MagAOX::app

#ifndef MAGAOX_SCPICTRL_HEADER_ONLY_IMPL
#include "scpiCtrl.cpp"
#endif

#endif //scpiCtrl_hpp
