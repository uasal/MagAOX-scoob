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

   std::string m_deviceAddr; ///< The device address  -> /dev/usbtmc0 where 0 gets assigned on USB connect

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
    int m_numChannels = 0; ///< The number of channels on the device
    int m_currentChannel = 0; ///< The current channel being monitored

    int maxChannels = 4; // define maximum number of power channels

    int fd; ///< The file descriptor for the device
    int m_pollRateHz {1000};  ///< The timeout for writing to the device [msec].

    // array for voltages with length numChannels when it gets set
    // array for amps with length numChannels when it gets set

   std::string m_status; ///< The device status 

   // array for statuses on each channel (On Off) ?

public:

    std::vector<pcf::IndiProperty> m_indiP_load_channels;

    /// Default c'tor.
    scpiPowerCtrl();
 
    /// D'tor, declared and defined for noexcept.
    ~scpiPowerCtrl() noexcept
    {}
 
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
    
    void updateAlarmsAndWarnings();

    bool send_scpi(const std::string& cmd, std::string& response);

    ///@}

protected:

   //declare our properties
   pcf::IndiProperty m_indiP_status; ///< The device's status string

   // need to dynamically add indiProperties for each channel once connected and know the number of channels

};

scpiPowerCtrl::scpiPowerCtrl() : MagAOXApp(MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED)
{
   m_firstOne = true;
   //setNumberOfOutlets(m_numChannels); // should be m_numChannels after connecting to the device
   return;
}

void scpiPowerCtrl::setupConfig()
{
    config.add("device.address", "a", "device.address", argType::Required, "device", "address", false, "string", "The device address.");
    
    // force user to define the number of channels so that limits can be pre-defined
    config.add("device.numChannels", "", "device.numChannels", argType::Required, "device", "numChannels", false, "int", "The number of channels on the device.");

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

   dev::outletController<scpiPowerCtrl>::setupConfig(config);
   
}


void scpiPowerCtrl::loadConfig()
{
    config(m_deviceAddr, "device.address");

    dev::ioDevice::loadConfig(config);
    config(m_numChannels, "device.numChannels");

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

   dev::outletController<scpiPowerCtrl>::loadConfig(config);
   
}

int scpiPowerCtrl::appStartup()
{
    // set up the  INDI properties
    REG_INDI_NEWPROP_NOCB(m_indiP_status, "status", pcf::IndiProperty::Text);
    m_indiP_status.add (pcf::IndiElement("value"));

    m_indiP_load_channels.resize(m_numChannels);

    if(m_numChannels == 0)
    {
        log<text_log>("0 power channels defined", logPrio::LOG_WARNING);
    }

    for (int i = 0; i < m_numChannels; i++) {
        std::string name = "load_ch" + std::to_string(i + 1);
    
        REG_INDI_NEWPROP_NOCB(m_indiP_load_channels[i], name, pcf::IndiProperty::Number);
        m_indiP_load_channels[i].add(pcf::IndiElement("voltage"));
        m_indiP_load_channels[i].add(pcf::IndiElement("current"));
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
        // after fd open set for remote control
    
        // CONF:SETPT 3

        state(stateCodes::READY);
    }
 
    if(state() == stateCodes::READY)
    {
       std::unique_lock<std::mutex> lock(m_indiMutex, std::try_to_lock);
 
       if( !lock.owns_lock())
       {
          return 0;
       }
 
       int rv = updateOutletStates();
 
       if(rv < 0) return log<software_error,-1>({__FILE__, __LINE__});
 
       updateAlarmsAndWarnings();
 
       return 0;
    }
 
    state(stateCodes::FAILURE);
    log<text_log>("appLogic fell through", logPrio::LOG_CRITICAL);
    return -1;

}

int scpiPowerCtrl::appShutdown()
{

    // release power supply to user control 
    // CONF:SETPT 0 (ROTARY) | 1 (KEYPAD) | 2 (EXT PGM) | 3 (REMOTE)

    // CONF:SETPT 1

   return 0;
}

int scpiPowerCtrl::updateOutletState( int outletNum )
{
    int rv;

    rv = devStatus();

    if(rv < 0)
    {
        log<software_error>({__FILE__, __LINE__, "device status error"});
        state(stateCodes::NOTCONNECTED);
        return 0;
    }

    updateChannel(outletNum); 

    updateIfChanged(m_indiP_status, "value", m_status);

    std::string propName = "load_ch" + std::to_string(outletNum + 1);

    updateIfChanged(m_indiP_load_channels[outletNum], "voltage", m_channelVoltages[outletNum]);
    updateIfChanged(m_indiP_load_channels[outletNum], "current", m_channelCurrents[outletNum]);

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

    updateChannels(); 

    updateIfChanged(m_indiP_status, "value", m_status);

    for (int i = 0; i < m_numChannels; i++) {
        std::string propName = "load_ch" + std::to_string(i + 1);
    
        updateIfChanged(m_indiP_load_channels[i], "voltage", m_channelVoltages[i]);
        updateIfChanged(m_indiP_load_channels[i], "current", m_channelCurrents[i]);
    }

    dev::outletController<scpiPowerCtrl>::updateINDI();

    return 0;
}

int scpiPowerCtrl::turnOutletOn( int outletNum )
{
    std::lock_guard<std::mutex> guard(m_indiMutex);  //Lock the mutex before doing anything

    std::string cmd_sel = "INST:NSEL " + std::to_string(outletNum + 1) + "\n";
    std::string res;

    if (!send_scpi(cmd_sel, res)) {
        return log<text_log,-1>("Could not select outlet channel " + std::to_string(outletNum), logPrio::LOG_WARNING);
    }

    if (!send_scpi("OUTP ON\n", res)) {
        return log<text_log,-1>("Failed to turn output channel " + std::to_string(outletNum) + " on.", logPrio::LOG_WARNING);
    }

    return 0;
}

int scpiPowerCtrl::turnOutletOff( int outletNum )
{
 
    std::lock_guard<std::mutex> guard(m_indiMutex);  //Lock the mutex before doing anything

    std::string cmd_sel = "INST:NSEL " + std::to_string(outletNum + 1) + "\n";
    std::string res;

    if (!send_scpi(cmd_sel, res)) {
        return log<text_log,-1>("Could not select outlet channel " + std::to_string(outletNum), logPrio::LOG_WARNING);
    }

    if (!send_scpi("OUTP OFF\n", res)) {
        return log<text_log,-1>("Failed to turn output channel " + std::to_string(outletNum) + " off.", logPrio::LOG_WARNING);
    }

    return 0;
}

int scpiPowerCtrl::devConnect()
{
    fd = open(m_deviceAddr.c_str(), O_RDWR);
    if (fd < 0) {
        return log<text_log,-1>("Error connecting to power supply.", logPrio::LOG_CRITICAL);
    }

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
    std::string cmd_sel = "INST:NSEL " + std::to_string(channel + 1) + "\n";
    if(write(fd, cmd_sel.c_str(), cmd_sel.size()) < 0){
        return log<text_log,-1>("Unable to select channel.", logPrio::LOG_CRITICAL);
    } 

    std::string volt, curr;
    bool ok_v = send_scpi("MEAS:VOLT?\n", volt);
    bool ok_c = send_scpi("MEAS:CURR?\n", curr);

    if (ok_v && ok_c) {
        volt.erase(volt.find_last_not_of(" \n\r\t") + 1);
        curr.erase(curr.find_last_not_of(" \n\r\t") + 1);

    }

    m_channelVoltages[channel] = std::stof(volt);
    m_channelCurrents[channel] = std::stof(curr);

    return 0;
}

int scpiPowerCtrl::setPollRate()
{
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
    if (write(fd, cmd.c_str(), cmd.size()) < 0) {
        perror("Write failed");
        return false;
    }

    char buffer[1024] = {0};
    int n = read(fd, buffer, sizeof(buffer) - 1);
    if (n < 0) {
        perror("Read failed");
        return false;
    }

    response.assign(buffer, n);
    return true;
}


}//namespace app
} //namespace MagAOX

#endif //scpiPowerCtrl_hpp
