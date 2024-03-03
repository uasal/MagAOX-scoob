/** \file fsmCtrl.hpp
 * \brief The MagAO-X XXXXXX header file
 *
 * \ingroup fsmCtrl_files
 */

#pragma once

#include "../../libMagAOX/libMagAOX.hpp" //Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"

#include <string.h>
#include <stdio.h>

#include <iostream>
using namespace std;

#include <pthread.h>

typedef MagAOX::app::MagAOXApp<true> MagAOXAppT; // This needs to be before the other header files for logging to work in other headers

#include "conversion.hpp"
#include "fsmCommands.hpp"
#include "binaryUart.hpp"
#include "cGraphPacket.hpp"
#include "linux_pinout_client_socket.hpp"
#include "socket.hpp"

/** \defgroup fsmCtrl
 * \brief Application to interface with ESC FSM
 *
 * <a href="../handbook/operating/software/apps/XXXXXX.html">Application Documentation</a>
 *
 * \ingroup apps
 *
 */

/** \defgroup fsmCtrl_files
 * \ingroup fsmCtrl
 */

namespace MagAOX
{
  namespace app
  {

    /// The MagAO-X ESC FSM interface
    /**
     * \ingroup fsmCtrl
     */
    class fsmCtrl : public MagAOXApp<true>, public dev::telemeter<fsmCtrl>, public dev::shmimMonitor<fsmCtrl>
    {

      // Give the test harness access.
      friend class fsmCtrl_test;

      friend class dev::telemeter<fsmCtrl>;
      typedef dev::telemeter<fsmCtrl> telemeterT;

      friend class dev::shmimMonitor<fsmCtrl>;

    protected:
      /** \name Configurable Parameters
       *@{
       */
      std::string type;
      std::string PortName;
      int nHostPort;
      int period_s;
      double m_a;
      double m_b;
      double m_v;
      double m_dac1_min;
      double m_dac1_max;
      double m_dac2_min;
      double m_dac2_max;
      double m_dac3_min;
      double m_dac3_max;

      // input parameters
      std::string m_inputType;
      std::string m_inputToggle;

      // here add parameters which will be config-able at runtime
      ///@}

      char Buffer[4096];
      CGraphPacket SocketProtocol;
      linux_pinout_client_socket LocalPortPinout;
      BinaryUart UartParser;
      PZTQuery *statusQuery = new StatusQuery();
      PZTQuery *adcsQuery = new AdcsQuery();
      PZTQuery *dacsQuery = new DacsQuery();
      uint32_t targetSetpoints[3];

      double m_dac1{0};
      double m_dac2{0};
      double m_dac3{0};

      const std::string DACS = "dacs";
      const std::string VOLTAGES = "voltages";
      const std::string ANGLES = "angles";
      const std::string SHMIM = "shmim";
      const std::string INDI = "indi";

    protected:
      // INDI properties
      pcf::IndiProperty m_indiP_val1;
      pcf::IndiProperty m_indiP_val2;
      pcf::IndiProperty m_indiP_val3;
      pcf::IndiProperty m_indiP_dac1;
      pcf::IndiProperty m_indiP_dac2;
      pcf::IndiProperty m_indiP_dac3;
      pcf::IndiProperty m_conversion_factors;
      pcf::IndiProperty m_input;

    public:
      INDI_NEWCALLBACK_DECL(fsmCtrl, m_indiP_val1);
      INDI_NEWCALLBACK_DECL(fsmCtrl, m_indiP_val2);
      INDI_NEWCALLBACK_DECL(fsmCtrl, m_indiP_val3);
      INDI_NEWCALLBACK_DECL(fsmCtrl, m_indiP_dac1);
      INDI_NEWCALLBACK_DECL(fsmCtrl, m_indiP_dac2);
      INDI_NEWCALLBACK_DECL(fsmCtrl, m_indiP_dac3);
      INDI_NEWCALLBACK_DECL(fsmCtrl, m_conversion_factors);
      INDI_NEWCALLBACK_DECL(fsmCtrl, m_input);

    public:
      /// Default c'tor.
      fsmCtrl();

      /// D'tor, declared and defined for noexcept.
      ~fsmCtrl() noexcept
      {
      }

      virtual void setupConfig();

      /// Implementation of loadConfig logic, separated for testing.
      /** This is called by loadConfig().
       */
      int loadConfigImpl(mx::app::appConfigurator &_config /**< [in] an application configuration from which to load values*/);

      virtual void loadConfig();

      /// Startup function
      /** Set up INDI props & other startup prep
       *
       */
      virtual int appStartup();

      /// Implementation of the logic for fsmCtrl.
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

      /// TODO: Test the connection to the fsm
      int testConnection();

      /// Connect to fsm via Socket
      /**
       *
       * \returns 0 if connection successful
       * \returns -1 on an error
       */
      int socketConnect();

      /**
       * @brief Request fsm status
       *
       * Wrapper that calls query() with instance of StatusQuery.
       * Response is stored in instance's Status member.
       * It returns fsm telemetry that is logged every 10s by the telemeter.
       * Output in /opt/telem/fsmCtrl_xxxxx.bintel
       */
      void queryStatus();

      /**
       * @brief Request fsm's ADC values
       *
       * Wrapper that calls query() with instance of AdcsQuery.
       * Response is stored in instance's AdcVals member.
       * Response is also logged in /opt/tele/fsmCtrl_xxxxxx.binlog
       */
      void queryAdcs();

      /**
       * @brief Request fsm's DAC values
       *
       * Wrapper that calls query() with instance of DacsQuery.
       * Response is stored in instance's DacSetpoints member.
       * Response is also logged in /opt/tele/fsmCtrl_xxxxxx.binlog
       */
      void queryDacs();

      /**
       * @brief Set fsm's DACs values to those in the argument.
       *
       * Wrapper that calls query() with instance of DacsQuery and
       * three new values for the DACs.
       * Response is stored in instance's DacSetpoints member.
       * Response is also logged in /opt/tele/fsmCtrl_xxxxxx.binlog
       *
       * @param Setpoints pointer to an array of three uint32_t values
       */
      int setDacs(uint32_t *);

      /**
       * @brief Query interface for the fsm
       *
       * Function that sends a command packet to the fsm and waits for a response.
       * If a response it received it processes the response as appropriate for the
       * command sent.
       *
       * @param pztQuery pointer to a class inheriting from PZTQuery (see fsmCommands.hpp)
       */
      void query(PZTQuery *);

      /**
       * @brief Utility function that sets 'current' INDI values, if updated
       *
       * Function that takes the values in m_dac1, m_dac2 and m_dac3, transforms them
       * (if necessary) to the type specified by m_inputType and updates the corresponding
       * INDI parameter's 'current' value.
       */
      void updateINDICurrentParams();

      /** \name Telemeter Interface
       *
       * @{
       */
      /**
       * @brief Required by Telemeter Interface
       *
       * \returns 0 on succcess
       * \returns -1 on error
       */
      int checkRecordTimes();

      /**
       * @brief Required by Telemeter Interface
       *
       * @param telem_fsm_ptr pointer to telem_fsm flatbuffer_log structure describing telem inputs & outputs
       * \returns 0 on succcess
       * \returns -1 on error
       */
      int recordTelem(const telem_fsm *);

      /**
       * @brief Required by Telemeter Interface
       *
       * @param force boolean; Telemetry is recorded every m_maxInterval (default value of 10) seconds.
       *  If 'true', force telemetry record outside of interval.
       * \returns 0 on succcess
       */
      int recordFsm(bool force = false);
      ///@}

      /** \name shmim Monitor Interface
       *
       * @{
       */

      /**
       * Called after shmimMonitor connects to the fsm stream.
       *
       * \returns 0 on success
       * \returns -1 if incorrect size or data type in stream.
       */
      int allocate(const dev::shmimT &sp);

      /**
       * Called by shmimMonitor when a new fsm command is available.
       *
       * \returns 0 on success
       * \returns -1 if incorrect size or data type in stream.
       */
      int processImage(void *curr_src,
                       const dev::shmimT &sp);

      /**
       * @brief Send to fsm new DAC values from shmim
       *
       * Called as part of processImage.
       * Checks shmim has an inputType keyword and that its value is 'dacs', 'voltages' or 'angles'.
       * Updates INDI input.type property, if different.
       * Updates corresponding INDI 'target' values with shmim values.
       * Converts shmim values from specified inputType to DACs.
       * Calls setDacs function with new DAC values.
       *
       * \returns 0 on success
       * \returns -1 if incorrect size or data type in stream.
       */
      int commandFSM(void *curr_src);
      ///@}
    };

    fsmCtrl::fsmCtrl() : MagAOXApp(MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED), UartParser(LocalPortPinout, SocketProtocol, PacketCallbacks, false)
    {
      m_powerMgtEnabled = true;
      m_getExistingFirst = true; // get existing shmim (??? should or shouldn't)
      return;
    }

    void fsmCtrl::setupConfig()
    {
      shmimMonitor::setupConfig(config);

      config.add("parameters.connection_type", "", "parameters.connection_type", argType::Required, "parameters", "connection_type", false, "string", "The type of connection: serial_port or socket.");
      config.add("parameters.period_s", "", "parameters.period_s", argType::Required, "parameters", "period_s", false, "int", "The period of status queries to the fsm.");

      config.add("socket.client_entrance_ip", "", "socket.client_entrance_ip", argType::Required, "socket", "client_entrance_ip", false, "string", "The IP address on the client machine that the tunnel is set up from.");
      config.add("socket.host_port", "", "socket.host_port", argType::Required, "socket", "host_port", false, "int", "The port at which the fsm driver is listening for connections.");

      config.add("fsm.a", "", "fsm.a", argType::Required, "fsm", "a", false, "double", "Conversion factor for converting from alpha/beta/z to actuator linear displacements.");
      config.add("fsm.b", "", "fsm.b", argType::Required, "fsm", "b", false, "double", "Conversion factor for converting from alpha/beta/z to actuator linear displacements.");
      config.add("fsm.v", "", "fsm.b", argType::Required, "fsm", "v", false, "double", "Conversion factor for converting from voltages to dacs.");
      config.add("fsm.dac1_min", "", "fsm.dac1_min", argType::Required, "fsm", "dac1_min", false, "double", "Min safe value for dac1.");
      config.add("fsm.dac1_max", "", "fsm.dac1_max", argType::Required, "fsm", "dac1_max", false, "double", "Max safe value for dac1.");
      config.add("fsm.dac2_min", "", "fsm.dac2_min", argType::Required, "fsm", "dac2_min", false, "double", "Min safe value for dac2.");
      config.add("fsm.dac2_max", "", "fsm.dac2_max", argType::Required, "fsm", "dac2_max", false, "double", "Max safe value for dac2.");
      config.add("fsm.dac3_min", "", "fsm.dac3_min", argType::Required, "fsm", "dac3_min", false, "double", "Min safe value for dac3.");
      config.add("fsm.dac3_max", "", "fsm.dac3_max", argType::Required, "fsm", "dac3_max", false, "double", "Max safe value for dac3.");

      // shmim parameters
      config.add("shmimMonitor.shmimName", "", "shmimMonitor.shmimName", argType::Required, "shmimMonitor", "shmimName", false, "string", "The name of the ImageStreamIO shared memory image. Will be used as /tmp/<shmimName>.im.shm. Default is fsm");

      config.add("shmimMonitor.width", "", "shmimMonitor.width", argType::Required, "shmimMonitor", "width", false, "string", "The width of the FSM in actuators.");
      config.add("shmimMonitor.height", "", "shmimMonitor.height", argType::Required, "shmimMonitor", "height", false, "string", "The height of the FSM in actuators.");

      config.add("input.type", "", "input.type", argType::Required, "input", "type", false, "string", "The type of values that the shmim contains. Can be 'dacs', 'voltages' or 'angles'.");
      config.add("input.toggle", "", "input.toggle", argType::Required, "input", "toggle", false, "string", "Where the input comes from. Can be 'shmim', 'indi'.");
      telemeterT::setupConfig(config);
    }

    int fsmCtrl::loadConfigImpl(mx::app::appConfigurator &_config)
    {
      _config(type, "parameters.connection_type");
      _config(period_s, "parameters.period_s");

      _config(PortName, "socket.client_entrance_ip");
      _config(nHostPort, "socket.host_port");

      _config(m_a, "fsm.a");
      _config(m_b, "fsm.b");
      _config(m_v, "fsm.v");
      _config(m_dac1_min, "fsm.dac1_min");
      _config(m_dac1_max, "fsm.dac1_max");
      _config(m_dac2_min, "fsm.dac2_min");
      _config(m_dac2_max, "fsm.dac2_max");
      _config(m_dac3_min, "fsm.dac3_min");
      _config(m_dac3_max, "fsm.dac3_max");

      _config(shmimMonitor::m_width, "shmimMonitor.width");
      _config(shmimMonitor::m_height, "shmimMonitor.height");

      m_inputType = DACS;
      _config(m_inputType, "input.type");
      m_inputToggle = INDI;
      _config(m_inputToggle, "input.toggle");

      shmimMonitor::loadConfig(_config);
      return 0;
    }

    void fsmCtrl::loadConfig()
    {
      if (loadConfigImpl(config) < 0)
      {
        log<text_log>("Error during config", logPrio::LOG_CRITICAL);
        m_shutdown = true;
      }

      if (telemeterT::loadConfig(config) < 0)
      {
        log<text_log>("Error during telemeter config", logPrio::LOG_CRITICAL);
        m_shutdown = true;
      }
    }

    int fsmCtrl::appStartup()
    {
      if (telemeterT::appStartup() < 0)
      {
        return log<software_error, -1>({__FILE__, __LINE__});
      }

      if (shmimMonitor::appStartup() < 0)
      {
        return log<software_error, -1>({__FILE__, __LINE__});
      }

      // set up the  INDI properties
      // dacs
      REG_INDI_NEWPROP(m_indiP_dac1, "dac_1", pcf::IndiProperty::Number);
      m_indiP_dac1.add(pcf::IndiElement("min"));
      m_indiP_dac1.add(pcf::IndiElement("max"));
      m_indiP_dac1["min"] = m_dac1_min;
      m_indiP_dac1["max"] = m_dac1_max;
      REG_INDI_NEWPROP(m_indiP_dac2, "dac_2", pcf::IndiProperty::Number);
      m_indiP_dac2.add(pcf::IndiElement("min"));
      m_indiP_dac2.add(pcf::IndiElement("max"));
      m_indiP_dac2["min"] = m_dac2_min;
      m_indiP_dac2["max"] = m_dac2_max;
      REG_INDI_NEWPROP(m_indiP_dac3, "dac_3", pcf::IndiProperty::Number);
      m_indiP_dac3.add(pcf::IndiElement("min"));
      m_indiP_dac3.add(pcf::IndiElement("max"));
      m_indiP_dac3["min"] = m_dac3_min;
      m_indiP_dac3["max"] = m_dac3_max;

      // dacs
      REG_INDI_NEWPROP(m_indiP_val1, "val_1", pcf::IndiProperty::Number);
      m_indiP_val1.add(pcf::IndiElement("current"));
      m_indiP_val1.add(pcf::IndiElement("target"));
      m_indiP_val1["current"] = -99999;
      m_indiP_val1["target"] = -99999;
      REG_INDI_NEWPROP(m_indiP_val2, "val_2", pcf::IndiProperty::Number);
      m_indiP_val2.add(pcf::IndiElement("current"));
      m_indiP_val2.add(pcf::IndiElement("target"));
      m_indiP_val2["current"] = -99999;
      m_indiP_val2["target"] = -99999;
      REG_INDI_NEWPROP(m_indiP_val3, "val_3", pcf::IndiProperty::Number);
      m_indiP_val3.add(pcf::IndiElement("current"));
      m_indiP_val3.add(pcf::IndiElement("target"));
      m_indiP_val3["current"] = -99999;
      m_indiP_val3["target"] = -99999;

      // conversion_factors
      REG_INDI_NEWPROP(m_conversion_factors, "conversion_factors", pcf::IndiProperty::Number);
      m_conversion_factors.add(pcf::IndiElement("a"));
      m_conversion_factors["a"] = m_a;
      m_conversion_factors.add(pcf::IndiElement("b"));
      m_conversion_factors["b"] = m_b;
      m_conversion_factors.add(pcf::IndiElement("v"));
      m_conversion_factors["v"] = m_v;

      // input
      REG_INDI_NEWPROP(m_input, "input", pcf::IndiProperty::Text);
      m_input.add(pcf::IndiElement("toggle"));
      m_input["toggle"] = m_inputType;
      m_input.add(pcf::IndiElement("type"));
      m_input["type"] = m_inputToggle;

      return 0;
    }

    int fsmCtrl::appLogic()
    {
      if (shmimMonitor::appLogic() < 0)
      {
        return log<software_error, -1>({__FILE__, __LINE__});
      }

      // Set the INDI name, width & heigh properties to those of the shmim
      if (shmimMonitor::updateINDI() < 0)
      {
        log<software_error>({__FILE__, __LINE__});
      }

      if (state() == stateCodes::POWERON)
      {
        if (!powerOnWaitElapsed())
        {
          return 0;
        }
        state(stateCodes::NOTCONNECTED);
      }

      if (state() == stateCodes::NOTCONNECTED)
      {
        int rv;
        rv = socketConnect();

        if (rv == 0)
        {
          state(stateCodes::CONNECTED);
        }
      }

      if (state() == stateCodes::CONNECTED)
      {
        // queryAdcs();

        // Get current dac values
        queryDacs();

        // Get telemetry
        queryStatus();

        if (m_inputToggle == SHMIM)
        {
          state(stateCodes::OPERATING);
        }
        if (m_inputToggle == INDI)
        {
          state(stateCodes::READY);
        }
      }

      if ((state() == stateCodes::OPERATING) || (state() == stateCodes::READY))
      {
        if (telemeterT::appLogic() < 0)
        {
          log<software_error>({__FILE__, __LINE__});
          return 0;
        }
      }

      return 0;
    }

    int fsmCtrl::appShutdown()
    {
      telemeterT::appShutdown();
      shmimMonitor<fsmCtrl>::appShutdown();

      return 0;
    }

    //////////////
    // CONNECTION
    //////////////

    /// TODO: Test the connection to the device
    int fsmCtrl::testConnection()
    {
      return 0;
    }

    int fsmCtrl::socketConnect()
    {
      // Tell C lib (stdio.h) not to buffer output, so we can ditch all the fflush(stdout) calls...
      setvbuf(stdout, NULL, _IONBF, 0);

      log<text_log>("Welcome to SerialPortBinaryCmdr!");
      log<text_log>("In order to tunnel to the lab use the following command before running this program:\n ssh -L 1337:localhost:1337 -N -f fsm \n(where fsm is the ssh alias of the remote server)!");

      int err = fsmCtrl::LocalPortPinout.init(nHostPort, PortName.c_str());
      if (IUart::IUartOK != err)
      {
        log<software_error, -1>({__FILE__, __LINE__, errno, "SerialPortBinaryCmdr: can't open socket (" + PortName + ":" + std::to_string(nHostPort) + "), exiting.\n"});
        return -1;
      }

      log<text_log>("Starting to do something");

      UartParser.Debug(false);
      return 0;
    }

    //////////////
    // FSM QUERIES
    //////////////

    // Function to request fsm Status
    void fsmCtrl::queryStatus()
    {
      log<text_log>(statusQuery->startLog);
      query(statusQuery);
      log<text_log>(statusQuery->endLog);
      recordFsm(false);
    }

    // Function to request fsm ADCs
    void fsmCtrl::queryAdcs()
    {
      log<text_log>(adcsQuery->startLog);
      query(adcsQuery);
      log<text_log>(adcsQuery->endLog);

      adcsQuery->logReply();
    }

    // Function to request fsm DACs
    void fsmCtrl::queryDacs()
    {
      log<text_log>(dacsQuery->startLog);
      query(dacsQuery);
      log<text_log>(dacsQuery->endLog);

      DacsQuery *castDacsQuery = dynamic_cast<DacsQuery *>(dacsQuery);

      m_dac1 = static_cast<float>(castDacsQuery->DacSetpoints[0]);
      m_dac2 = static_cast<float>(castDacsQuery->DacSetpoints[1]);
      m_dac3 = static_cast<float>(castDacsQuery->DacSetpoints[2]);

      updateINDICurrentParams();

      dacsQuery->logReply();
    }

    // Function to set fsm DACs
    int fsmCtrl::setDacs(uint32_t *Setpoints)
    {
      if (Setpoints[0] < m_dac1_min || Setpoints[0] > m_dac1_max)
      {
        std::ostringstream oss;
        oss << "Requested dac1 out of range; (min|dac1|max) : (" << m_dac1_min << "|" << Setpoints[0] << "|" << m_dac1_max << ");";
        log<text_log>(oss.str(), logPrio::LOG_ERROR);
        return -1;
      }

      if (Setpoints[1] < m_dac2_min || Setpoints[1] > m_dac2_max)
      {
        std::ostringstream oss;
        oss << "Requested dac2 out of range; (min|dac2|max) : (" << m_dac2_min << "|" << Setpoints[1] << "|" << m_dac2_max << ");";
        log<text_log>(oss.str(), logPrio::LOG_ERROR);
        return -1;
      }

      if (Setpoints[2] < m_dac3_min || Setpoints[2] > m_dac3_max)
      {
        std::ostringstream oss;
        oss << "Requested dac3 out of range; (min|dac3|max) : (" << m_dac3_min << "|" << Setpoints[2] << "|" << m_dac3_max << ");";
        log<text_log>(oss.str(), logPrio::LOG_ERROR);
        return -1;
      }

      std::ostringstream oss;
      oss << "SETDACS: " << Setpoints[0] << " | " << Setpoints[1] << " | " << Setpoints[2];
      log<text_log>(oss.str());

      DacsQuery *castDacsQuery = dynamic_cast<DacsQuery *>(dacsQuery);

      log<text_log>(dacsQuery->startLog);
      castDacsQuery->setPayload(Setpoints, 3 * sizeof(uint32_t));
      query(castDacsQuery);
      log<text_log>(castDacsQuery->endLog);

      castDacsQuery->logReply();
      castDacsQuery->resetPayload();

      m_dac1 = castDacsQuery->DacSetpoints[0];
      m_dac2 = castDacsQuery->DacSetpoints[1];
      m_dac3 = castDacsQuery->DacSetpoints[2];
      updateINDICurrentParams();

      queryDacs();
      queryAdcs();
      return 0;
    }

    void fsmCtrl::query(PZTQuery *pztQuery)
    {
      // Send command packet
      (&UartParser)->TxBinaryPacket(pztQuery->getPayloadType(), pztQuery->getPayloadData(), pztQuery->getPayloadLen());

      // Allow time for fsm to respond, it's not instantaneous
      sleep(3);

      // The packet is read byte by byte, so keep going while there are bytes left
      bool Bored = false;
      while (!Bored)
      {
        Bored = true;
        if (UartParser.Process(pztQuery))
        {
          Bored = false;
        }

        if (false == LocalPortPinout.connected())
        {
          int err = LocalPortPinout.init(nHostPort, PortName.c_str());
          if (IUart::IUartOK != err)
          {
            log<software_error>({__FILE__, __LINE__, errno, "SerialPortBinaryCmdr: can't open socket (" + PortName + ":" + std::to_string(nHostPort)});
          }
        }
      }
    }

    /////////////////////////
    // TELEMETER INTERFACE
    /////////////////////////

    int fsmCtrl::checkRecordTimes()
    {
      return telemeterT::checkRecordTimes(telem_fsm());
    }

    int fsmCtrl::recordTelem(const telem_fsm *)
    {
      return recordFsm(true);
    }

    int fsmCtrl::recordFsm(bool force)
    {
      static CGraphPZTStatusPayload LastStatus; ///< Structure holding the previous fsm voltage measurement.
      StatusQuery *statusQueryPtr = dynamic_cast<StatusQuery *>(statusQuery);

      if (!(LastStatus == statusQueryPtr->Status) || force)
      {
        LastStatus = statusQueryPtr->Status;
        telem<telem_fsm>({LastStatus.P1V2, LastStatus.P2V2, LastStatus.P24V, LastStatus.P2V5, LastStatus.P3V3A, LastStatus.P6V, LastStatus.P5V, LastStatus.P3V3D, LastStatus.P4V3, LastStatus.N5V, LastStatus.N6V, LastStatus.P150V});
      }

      return 0;
    }

    /////////////////////////
    // SHMIMMONITOR INTERFACE
    /////////////////////////

    int fsmCtrl::allocate(const dev::shmimT &sp)
    {
      static_cast<void>(sp); // be unused

      int err = 0;

      //  if(m_width != m_fsmWidth)
      //  {
      //     log<software_critical>({__FILE__,__LINE__, "shmim width does not match configured FSM width"});
      //     ++err;
      //  }

      //  if(m_height != m_fsmHeight)
      //  {
      //     log<software_critical>({__FILE__,__LINE__, "shmim height does not match configured FSM height"});
      //     ++err;
      //  }

      if (err)
        return -1;

      return 0;
    }

    int fsmCtrl::processImage(void *curr_src,
                              const dev::shmimT &sp)
    {
      static_cast<void>(sp); // be unused

      int rv = commandFSM(curr_src);

      if (rv < 0)
      {
        log<software_critical>({__FILE__, __LINE__, errno, rv, "Error from commandFSM"});
        return rv;
      }

      return rv;
    }

    int fsmCtrl::commandFSM(void *curr_src)
    {
      std::string inputType = "";

      // Check that shmim has inputType keyword
      int kwn = 0;
      while ((m_imageStream.kw[kwn].type != 'N') && (kwn < m_imageStream.md->NBkw))
      {
        std::string name(m_imageStream.kw[kwn].name);
        if (name == "inputType")
        {
          inputType = m_imageStream.kw[kwn].value.valstr;
          if (!(inputType == DACS || inputType == VOLTAGES || inputType == ANGLES))
          {
            std::ostringstream oss;
            oss << "Shmim '" << shmimMonitor::m_shmimName << "' has an inputType keyword with a value other than 'dacs', 'voltages', or 'angles': " << inputType;
            log<software_critical>({__FILE__, __LINE__, errno, oss.str()});
            return -1;
          }

          m_inputType = inputType;
          updateIfChanged(m_input, "type", m_inputType);
        }
        kwn++;
      }

      if (inputType == "")
      {
        std::ostringstream oss;
        oss << "Shmim '" << shmimMonitor::m_shmimName << "' does not have an inputType keyword with a value of 'dacs', 'voltages', or 'angles'.";
        log<software_critical>({__FILE__, __LINE__, errno, oss.str()});
        return -1;
      }

      uint32_t dacs[3] = {0, 0, 0};

      //  if(state() != stateCodes::OPERATING) return 0;
      float val1, val2, val3;
      val1 = ((float *)curr_src)[0];
      val2 = ((float *)curr_src)[1];
      val3 = ((float *)curr_src)[2];

      updateIfChanged(m_indiP_val1, "target", val1);
      updateIfChanged(m_indiP_val2, "target", val2);
      updateIfChanged(m_indiP_val3, "target", val3);

      if (m_inputType == DACS)
      {
        dacs[0] = val1;
        dacs[1] = val2;
        dacs[2] = val3;
      }
      else if (m_inputType == VOLTAGES)
      {
        dacs[0] = v1_to_dac1(val1, m_v);
        dacs[1] = v2_to_dac2(val2, m_v);
        dacs[2] = v3_to_dac3(val3, m_v);
      }
      else if (m_inputType == ANGLES)
      {

        dacs[0] = angles_to_dac1(val1, val3, m_a);
        dacs[1] = angles_to_dac2(val1, val2, val3, m_a, m_b);
        dacs[2] = angles_to_dac3(val1, val2, val3, m_a, m_b);
      }

      std::ostringstream oss;
      oss << "SHMIM dacs callback: " << dacs[0] << " | " << dacs[1] << " | " << dacs[2];
      log<text_log>(oss.str());

      std::unique_lock<std::mutex> lock(m_indiMutex);

      return setDacs(dacs);
    }

    ////////////////////
    // INDI CALLBACKS
    ////////////////////

    // callback from setting m_indiP_val1
    // only 'target' is editable ('current' should be updated by code)
    INDI_NEWCALLBACK_DEFN(fsmCtrl, m_indiP_val1)
    (const pcf::IndiProperty &ipRecv)
    {
      if (ipRecv.createUniqueKey() == m_indiP_val1.createUniqueKey())
      {
        float current = -999999, target = -999999;

        if (ipRecv.find("current"))
        {
          current = ipRecv["current"].get<float>();
        }

        if (ipRecv.find("target"))
        {
          target = ipRecv["target"].get<float>();
        }

        if (target == -999999)
          target = current;

        if (target == -999999)
          return 0;

        if (state() == stateCodes::READY)
        {
          // Lock the mutex, waiting if necessary
          std::unique_lock<std::mutex> lock(m_indiMutex);

          updateIfChanged(m_indiP_val1, "target", target);

          uint32_t dacs[3] = {0, 0, 0};

          if (m_inputType == DACS)
          {
            dacs[0] = target;
          }
          else if (m_inputType == VOLTAGES)
          {
            dacs[0] = v1_to_dac1(target, m_v);
          }
          else if (m_inputType == ANGLES)
          {
            // Get current z to calculate dac1 from the target
            double z = get_z(m_dac1, m_dac2, m_dac3);
            dacs[0] = angles_to_dac1(target, z, m_a);
          }

          dacs[1] = m_dac2;
          dacs[2] = m_dac3;

          std::ostringstream oss;
          oss << "INDI dacs callback: " << dacs[0] << " | " << dacs[1] << " | " << dacs[2];
          log<text_log>(oss.str());

          return setDacs(dacs);
        }
      }
      return -1;
    }

    // callback from setting m_indiP_val2
    // only 'target' is editable ('current' should be updated by code)
    INDI_NEWCALLBACK_DEFN(fsmCtrl, m_indiP_val2)
    (const pcf::IndiProperty &ipRecv)
    {
      if (ipRecv.createUniqueKey() == m_indiP_val2.createUniqueKey())
      {
        float current = -999999, target = -999999;

        if (ipRecv.find("current"))
        {
          current = ipRecv["current"].get<float>();
        }

        if (ipRecv.find("target"))
        {
          target = ipRecv["target"].get<float>();
        }

        if (target == -999999)
          target = current;

        if (target == -999999)
          return 0;

        if (state() == stateCodes::READY)
        {
          // Lock the mutex, waiting if necessary
          std::unique_lock<std::mutex> lock(m_indiMutex);

          updateIfChanged(m_indiP_val2, "target", target);

          uint32_t dacs[3] = {0, 0, 0};
          dacs[0] = m_dac1;

          if (m_inputType == DACS)
          {
            dacs[1] = target;
          }
          else if (m_inputType == VOLTAGES)
          {
            dacs[1] = v2_to_dac2(target, m_v);
          }
          else if (m_inputType == ANGLES)
          {
            // Get current alpha and z to calculate dac2 from the target
            double alpha = get_alpha(m_dac1, m_dac2, m_dac3, m_a);
            double z = get_z(m_dac1, m_dac2, m_dac3);
            dacs[1] = angles_to_dac2(alpha, target, z, m_a, m_b);
          }

          dacs[2] = m_dac3;

          std::ostringstream oss;
          oss << "INDI dacs callback: " << dacs[0] << " | " << dacs[1] << " | " << dacs[2];
          log<text_log>(oss.str());

          return setDacs(dacs);
        }
      }
      return -1;
    }

    // callback from setting m_indiP_val3
    // only 'target' is editable ('current' should be updated by code)
    INDI_NEWCALLBACK_DEFN(fsmCtrl, m_indiP_val3)
    (const pcf::IndiProperty &ipRecv)
    {
      if (ipRecv.createUniqueKey() == m_indiP_val3.createUniqueKey())
      {
        float current = -999999, target = -999999;

        if (ipRecv.find("current"))
        {
          current = ipRecv["current"].get<float>();
        }

        if (ipRecv.find("target"))
        {
          target = ipRecv["target"].get<float>();
        }

        if (target == -999999)
          target = current;

        if (target == -999999)
          return 0;

        if (state() == stateCodes::READY)
        {
          // Lock the mutex, waiting if necessary
          std::unique_lock<std::mutex> lock(m_indiMutex);

          updateIfChanged(m_indiP_val3, "target", target);

          uint32_t dacs[3] = {0, 0, 0};
          dacs[0] = m_dac1;
          dacs[1] = m_dac2;

          if (m_inputType == DACS)
          {
            dacs[2] = target;
          }
          else if (m_inputType == VOLTAGES)
          {
            dacs[2] = v3_to_dac3(target, m_v);
          }
          else if (m_inputType == ANGLES)
          {
            // Get current alpha and beta to calculate dac3 from the target
            double alpha = get_alpha(m_dac1, m_dac2, m_dac3, m_a);
            double beta = get_beta(m_dac2, m_dac3, m_b);
            dacs[2] = angles_to_dac3(alpha, beta, target, m_a, m_b);
          }

          std::ostringstream oss;
          oss << "INDI dacs callback: " << dacs[0] << " | " << dacs[1] << " | " << dacs[2];
          log<text_log>(oss.str());

          return setDacs(dacs);
        }
      }
      return -1;
    }

    // callback from setting conversion_factors
    INDI_NEWCALLBACK_DEFN(fsmCtrl, m_conversion_factors)
    (const pcf::IndiProperty &ipRecv)
    {
      if (ipRecv.createUniqueKey() == m_conversion_factors.createUniqueKey())
      {
        if (ipRecv.find("a"))
        {
          m_a = ipRecv["a"].get<float>();
          updateIfChanged(m_conversion_factors, "a", m_a);
        }

        if (ipRecv.find("b"))
        {
          m_b = ipRecv["b"].get<float>();
          updateIfChanged(m_conversion_factors, "b", m_b);
        }

        if (ipRecv.find("v"))
        {
          m_v = ipRecv["v"].get<float>();
          updateIfChanged(m_conversion_factors, "v", m_v);
        }

        std::ostringstream oss;
        oss << "INDI conversion_factors callback: " << m_a << " | " << m_b << " | " << m_v;
        log<text_log>(oss.str());
      }

      log<text_log>("INDI callback.");
      return 0;
    }

    // callback from setting m_input (dacs, voltages, angles)
    INDI_NEWCALLBACK_DEFN(fsmCtrl, m_input)
    (const pcf::IndiProperty &ipRecv)
    {
      if (ipRecv.createUniqueKey() == m_input.createUniqueKey())
      {
        if (ipRecv.find("type"))
        {
          std::string type = ipRecv["type"].get<std::string>();
          if (!(m_inputType == DACS || m_inputType == VOLTAGES || m_inputType == ANGLES))
          {
            std::ostringstream oss;
            oss << "input.type '" << m_inputType << "' not dacs, voltages or angles";
            log<software_critical>({__FILE__, __LINE__, errno, oss.str()});
            return -1;
          }

          if (state() == stateCodes::READY)
          {
            m_inputType = type;
            updateIfChanged(m_input, "type", m_inputType);

            // Reset target values
            updateIfChanged(m_indiP_val1, "target", -99999);
            updateIfChanged(m_indiP_val2, "target", -99999);
            updateIfChanged(m_indiP_val3, "target", -99999);
            // Update current values
            updateINDICurrentParams();
          }

          std::ostringstream oss;
          oss << "INDI input type callback: " << m_inputType;
          log<text_log>(oss.str());
        }

        if (ipRecv.find("toggle"))
        {
          std::string toggle = ipRecv["toggle"].get<std::string>();
          if (toggle == SHMIM)
          {
            state(stateCodes::OPERATING);
            updateIfChanged(m_input, "toggle", toggle);
          }
          if (toggle == INDI)
          {
            state(stateCodes::READY);
            updateIfChanged(m_input, "toggle", toggle);
          }

          std::ostringstream oss;
          oss << "INDI input toggle: " << m_inputToggle;
          log<text_log>(oss.str());
        }
      }

      log<text_log>("INDI callback.");
      return 0;
    }

    // callback from setting m_indiP_dac1 (min, max)
    INDI_NEWCALLBACK_DEFN(fsmCtrl, m_indiP_dac1)
    (const pcf::IndiProperty &ipRecv)
    {
      if (ipRecv.createUniqueKey() == m_indiP_dac1.createUniqueKey())
      {
        std::ostringstream oss;

        if (ipRecv.find("min"))
        {
          m_dac1_min = ipRecv["min"].get<uint32_t>();
          oss << "INDI dac1 min callback: " << m_dac1_min;
          updateIfChanged(m_indiP_dac1, "min", m_dac1_min);
        }

        if (ipRecv.find("max"))
        {
          m_dac1_max = ipRecv["max"].get<uint32_t>();
          oss << "INDI dac1 max callback: " << m_dac1_max;
          updateIfChanged(m_indiP_dac1, "max", m_dac1_max);
        }

        log<text_log>(oss.str());
      }

      log<text_log>("INDI callback.");
      return 0;
    }

    // callback from setting m_indiP_dac2 (min, max)
    INDI_NEWCALLBACK_DEFN(fsmCtrl, m_indiP_dac2)
    (const pcf::IndiProperty &ipRecv)
    {
      if (ipRecv.createUniqueKey() == m_indiP_dac2.createUniqueKey())
      {
        std::ostringstream oss;

        if (ipRecv.find("min"))
        {
          m_dac2_min = ipRecv["min"].get<uint32_t>();
          oss << "INDI dac2 min callback: " << m_dac2_min;
          updateIfChanged(m_indiP_dac2, "min", m_dac2_min);
        }

        if (ipRecv.find("max"))
        {
          m_dac2_max = ipRecv["max"].get<uint32_t>();
          oss << "INDI dac2 max callback: " << m_dac2_max;
          updateIfChanged(m_indiP_dac2, "max", m_dac2_max);
        }

        log<text_log>(oss.str());
      }

      log<text_log>("INDI callback.");
      return 0;
    }

    // callback from setting m_indiP_dac3 (min, max)
    INDI_NEWCALLBACK_DEFN(fsmCtrl, m_indiP_dac3)
    (const pcf::IndiProperty &ipRecv)
    {
      if (ipRecv.createUniqueKey() == m_indiP_dac3.createUniqueKey())
      {
        std::ostringstream oss;

        if (ipRecv.find("min"))
        {
          m_dac3_min = ipRecv["min"].get<uint32_t>();
          oss << "INDI dac3 min callback: " << m_dac3_min;
          updateIfChanged(m_indiP_dac3, "min", m_dac3_min);
        }

        if (ipRecv.find("max"))
        {
          m_dac3_max = ipRecv["max"].get<uint32_t>();
          oss << "INDI dac3 max callback: " << m_dac3_max;
          updateIfChanged(m_indiP_dac3, "max", m_dac3_max);
        }

        log<text_log>(oss.str());
      }

      log<text_log>("INDI callback.");
      return 0;
    }

    /////////
    // UTILS
    /////////

    void fsmCtrl::updateINDICurrentParams()
    {
      float val1, val2, val3;

      if (m_inputType == DACS)
      {
        val1 = m_dac1;
        val2 = m_dac2;
        val3 = m_dac3;
      }
      else if (m_inputType == VOLTAGES)
      {
        val1 = get_v1(m_dac1, m_v);
        val2 = get_v2(m_dac2, m_v);
        val3 = get_v3(m_dac3, m_v);
      }
      else if (m_inputType == ANGLES)
      {
        val1 = get_alpha(m_dac1, m_dac2, m_dac3, m_a);
        val2 = get_beta(m_dac2, m_dac3, m_b);
        val3 = get_z(m_dac1, m_dac2, m_dac3);
      }

      updateIfChanged(m_indiP_val1, "current", val1);
      updateIfChanged(m_indiP_val2, "current", val2);
      updateIfChanged(m_indiP_val3, "current", val3);
    }

  } // namespace app
} // namespace MagAOX