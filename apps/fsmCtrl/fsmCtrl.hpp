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

typedef MagAOX::app::MagAOXApp<true> MagAOXAppT; // This needs to be before the other header files for logging to work

#include "conversion.hpp"
#include "fsmCommands.hpp"
#include "binaryUart.hpp"
#include "cGraphPacket.hpp"
#include "linux_pinout_client_socket.hpp"
#include "socket.hpp"

/** \defgroup fsmCtrl
 * \brief The XXXXXX application to do YYYYYYY
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

    /// The MagAO-X xxxxxxxx
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
      double m_dac1_min;
      double m_dac1_max;
      double m_dac2_min;
      double m_dac2_max;
      double m_dac3_min;
      double m_dac3_max;

      // Shmim-related Parameters
      std::string m_shmimName; ///< The name of the shmim stream to write to.

      // here add parameters which will be config-able at runtime
      ///@}

      // uint16_t CGraphPayloadType;
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

      double m_v1{0};
      double m_v2{0};
      double m_v3{0};

      double m_alpha{0};
      double m_beta{0};
      double m_z{0};

    protected:
      // INDI properties
      pcf::IndiProperty m_indiP_dac1;
      pcf::IndiProperty m_indiP_dac2;
      pcf::IndiProperty m_indiP_dac3;
      pcf::IndiProperty m_indiP_v1;
      pcf::IndiProperty m_indiP_v2;
      pcf::IndiProperty m_indiP_v3;
      pcf::IndiProperty m_indiP_alpha;
      pcf::IndiProperty m_indiP_beta;
      pcf::IndiProperty m_indiP_z;
      pcf::IndiProperty m_conversion_factors;

    public:
      INDI_NEWCALLBACK_DECL(fsmCtrl, m_indiP_dac1);
      INDI_NEWCALLBACK_DECL(fsmCtrl, m_indiP_dac2);
      INDI_NEWCALLBACK_DECL(fsmCtrl, m_indiP_dac3);
      INDI_NEWCALLBACK_DECL(fsmCtrl, m_indiP_v1);
      INDI_NEWCALLBACK_DECL(fsmCtrl, m_indiP_v2);
      INDI_NEWCALLBACK_DECL(fsmCtrl, m_indiP_v3);
      INDI_NEWCALLBACK_DECL(fsmCtrl, m_indiP_alpha);
      INDI_NEWCALLBACK_DECL(fsmCtrl, m_indiP_beta);
      INDI_NEWCALLBACK_DECL(fsmCtrl, m_indiP_z);
      INDI_NEWCALLBACK_DECL(fsmCtrl, m_conversion_factors);

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
      /**
       *
       */
      virtual int appStartup();

      /// Implementation of the FSM for fsmCtrl.
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

      /// TODO: Test the connection to the device
      int testConnection();

      /// Connect to fsm via Socket
      /**
       *
       * \returns 0 if connection successful
       * \returns -1 on an error
       */
      int socketConnect();

      /// Query fsm status
      void queryStatus();

      /// Query fsm's ADCs
      void queryAdcs();

      /// Query fsm's DACs
      void queryDacs();

      /// Set fsm's DACs
      //  void setDacs();
      int setDacs(uint32_t *);

      void query(PZTQuery *);

      /** \name Telemeter Interface
       *
       * @{
       */
      int checkRecordTimes();

      int recordTelem(const telem_fsm *);

      int recordFsm(bool force = false);
      ///@}

      /** \name shmim Monitor Interface
       *
       * @{
       */

      /// Called after shmimMonitor connects to the dmXXdisp stream.  Checks for proper size.
      /**
       * \returns 0 on success
       * \returns -1 if incorrect size or data type in stream.
       */
      int allocate(const dev::shmimT &sp);

      /// Called by shmimMonitor when a new fsm command is available.
      int processImage(void *curr_src,
                       const dev::shmimT &sp);

      int commandFSM(void *curr_src);
      ///@}
    };

    fsmCtrl::fsmCtrl() : MagAOXApp(MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED), UartParser(LocalPortPinout, SocketProtocol, PacketCallbacks, false)
    {
      m_powerMgtEnabled = true;
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
      config.add("fsm.dac1_min", "", "fsm.dac1_min", argType::Required, "fsm", "dac1_min", false, "double", "Min safe value for dac1.");
      config.add("fsm.dac1_max", "", "fsm.dac1_max", argType::Required, "fsm", "dac1_max", false, "double", "Max safe value for dac1.");
      config.add("fsm.dac2_min", "", "fsm.dac2_min", argType::Required, "fsm", "dac2_min", false, "double", "Min safe value for dac2.");
      config.add("fsm.dac2_max", "", "fsm.dac2_max", argType::Required, "fsm", "dac2_max", false, "double", "Max safe value for dac2.");
      config.add("fsm.dac3_min", "", "fsm.dac3_min", argType::Required, "fsm", "dac3_min", false, "double", "Min safe value for dac3.");
      config.add("fsm.dac2_max", "", "fsm.dac2_max", argType::Required, "fsm", "dac2_max", false, "double", "Max safe value for dac3.");

      // shmim parameters
      config.add("shmimMonitor.shmimName", "", "shmimMonitor.shmimName", argType::Required, "shmimMonitor", "shmimName", false, "string", "The name of the ImageStreamIO shared memory image. Will be used as /tmp/<shmimName>.im.shm. Default is fsm");

      config.add("shmimMonitor.width", "", "shmimMonitor.width", argType::Required, "shmimMonitor", "width", false, "string", "The width of the FSM in actuators.");
      config.add("shmimMonitor.height", "", "shmimMonitor.height", argType::Required, "shmimMonitor", "height", false, "string", "The height of the FSM in actuators.");
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
      _config(m_dac1_min, "fsm.dac1_min");
      _config(m_dac1_max, "fsm.dac1_max");
      _config(m_dac2_min, "fsm.dac2_min");
      _config(m_dac2_max, "fsm.dac2_max");
      _config(m_dac3_min, "fsm.dac3_min");
      _config(m_dac3_max, "fsm.dac3_max");

      // _config(m_shmimName, "shmimMonitor.shmim_name");

      _config(shmimMonitor::m_width, "shmimMonitor.width");
      _config(shmimMonitor::m_height, "shmimMonitor.height");

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
      m_indiP_dac1.add(pcf::IndiElement("current"));
      m_indiP_dac1.add(pcf::IndiElement("target"));
      m_indiP_dac1["current"] = -99999;
      m_indiP_dac1["target"] = -99999;
      m_indiP_dac1["min"] = m_dac1_min;
      m_indiP_dac1["max"] = m_dac1_max;
      REG_INDI_NEWPROP(m_indiP_dac2, "dac_2", pcf::IndiProperty::Number);
      m_indiP_dac2.add(pcf::IndiElement("current"));
      m_indiP_dac2.add(pcf::IndiElement("target"));
      m_indiP_dac2["current"] = -99999;
      m_indiP_dac2["target"] = -99999;
      m_indiP_dac2["min"] = m_dac2_min;
      m_indiP_dac2["max"] = m_dac2_max;
      REG_INDI_NEWPROP(m_indiP_dac3, "dac_3", pcf::IndiProperty::Number);
      m_indiP_dac3.add(pcf::IndiElement("current"));
      m_indiP_dac3.add(pcf::IndiElement("target"));
      m_indiP_dac3["current"] = -99999;
      m_indiP_dac3["target"] = -99999;
      m_indiP_dac3["min"] = m_dac3_min;
      m_indiP_dac3["max"] = m_dac3_max;

      // voltages
      REG_INDI_NEWPROP(m_indiP_v1, "v_1", pcf::IndiProperty::Number);
      m_indiP_v1.add(pcf::IndiElement("current"));
      m_indiP_v1.add(pcf::IndiElement("target"));
      m_indiP_v1["current"] = -99999;
      m_indiP_v1["target"] = -99999;
      REG_INDI_NEWPROP(m_indiP_v2, "v_2", pcf::IndiProperty::Number);
      m_indiP_v2.add(pcf::IndiElement("current"));
      m_indiP_v2.add(pcf::IndiElement("target"));
      m_indiP_v2["current"] = -99999;
      m_indiP_v2["target"] = -99999;
      REG_INDI_NEWPROP(m_indiP_v3, "v_3", pcf::IndiProperty::Number);
      m_indiP_v3.add(pcf::IndiElement("current"));
      m_indiP_v3.add(pcf::IndiElement("target"));
      m_indiP_v3["current"] = -99999;
      m_indiP_v3["target"] = -99999;

      // angles
      REG_INDI_NEWPROP(m_indiP_alpha, "alpha", pcf::IndiProperty::Number);
      m_indiP_alpha.add(pcf::IndiElement("current"));
      m_indiP_alpha.add(pcf::IndiElement("target"));
      m_indiP_alpha["current"] = -99999;
      m_indiP_alpha["target"] = -99999;
      REG_INDI_NEWPROP(m_indiP_beta, "beta", pcf::IndiProperty::Number);
      m_indiP_beta.add(pcf::IndiElement("current"));
      m_indiP_beta.add(pcf::IndiElement("target"));
      m_indiP_beta["current"] = -99999;
      m_indiP_beta["target"] = -99999;
      REG_INDI_NEWPROP(m_indiP_z, "z", pcf::IndiProperty::Number);
      m_indiP_z.add(pcf::IndiElement("current"));
      m_indiP_z.add(pcf::IndiElement("target"));
      m_indiP_z["current"] = -99999;
      m_indiP_z["target"] = -99999;

      // conversion_factors
      REG_INDI_NEWPROP(m_conversion_factors, "conversion_factors", pcf::IndiProperty::Number);
      m_conversion_factors.add(pcf::IndiElement("a"));
      m_conversion_factors["a"] = m_a;
      m_conversion_factors.add(pcf::IndiElement("b"));
      m_conversion_factors["b"] = m_b;

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
        queryAdcs();

        // Get current values for dacs & alpha/beta/z
        queryDacs();

        // Get telemetry
        queryStatus();

        // Ready to receive alpha / beta / z
        state(stateCodes::READY);
      }

      // (For now?) Need to change state to READY in INDI
      if (state() == stateCodes::READY)
      {
        std::ostringstream oss;
        oss << "State is: " << state();
        log<text_log>(oss.str());

        // queryAdcs();

        log<text_log>("Doing something");

        sleep(10);

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

    // Function to request fsm Status
    void fsmCtrl::queryStatus()
    {
      log<text_log>(statusQuery->startLog);
      query(statusQuery);
      log<text_log>(statusQuery->endLog);
      recordFsm(false);

      // statusQuery->logReply();
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

      m_dac1 = castDacsQuery->DacSetpoints[0];
      m_dac2 = castDacsQuery->DacSetpoints[1];
      m_dac3 = castDacsQuery->DacSetpoints[2];
      updateINDICurrentParams();

      dacsQuery->logReply();
    }

    // Function to set fsm DACs
    int fsmCtrl::setDacs(uint32_t *Setpoints)
    {
      // void fsmCtrl::setDacs() {
      // uint32_t Setpoints[3];

      // Setpoints[0] = 60000;
      // Setpoints[1] = 60000;
      // Setpoints[2] = 60000;

      // if (Setpoints[0] < m_dac1_min || Setpoints[0] > m_dac1_max)
      // {
      //   log<text_log>("Requested dac1 out of range", logPrio::LOG_ERROR);
      //   return -1;
      // }

      // if (Setpoints[1] < m_dac2_min || Setpoints[1] > m_dac2_max)
      // {
      //   log<text_log>("Requested dac2 out of range", logPrio::LOG_ERROR);
      //   return -1;
      // }

      // if (Setpoints[2] < m_dac3_min || Setpoints[2] > m_dac3_max)
      // {
      //   log<text_log>("Requested dac3 out of range", logPrio::LOG_ERROR);
      //   return -1;
      // }

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

      //  m_instSatMap.resize(m_dmWidth,m_dmHeight);
      //  m_instSatMap.setZero();

      //  m_accumSatMap.resize(m_dmWidth,m_dmHeight);
      //  m_accumSatMap.setZero();

      //  m_satPercMap.resize(m_dmWidth,m_dmHeight);
      //  m_satPercMap.setZero();

      //  if(findDMChannels() < 0)
      //  {
      //     log<software_critical>({__FILE__,__LINE__, "error finding DM channels"});

      //     return -1;
      //  }

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
      //  //Tell the sat thread to get going
      //  if(sem_post(&m_satSemaphore) < 0)
      //  {
      //     log<software_critical>({__FILE__, __LINE__, errno, 0, "Error posting to semaphore"});
      //     return -1;
      //  }

      return rv;
    }

    int fsmCtrl::commandFSM(void *curr_src)
    {
      //  if(state() != stateCodes::OPERATING) return 0;
      //  float pos1 = ((float *) curr_src)[0];
      //  float pos2 = ((float *) curr_src)[1];

      //  float pos3 = 0;
      //  if(m_naxes == 3) pos3 = ((float *) curr_src)[2];

      //  std::unique_lock<std::mutex> lock(m_indiMutex);

      //  int rv;
      //  if( (rv = move_1(pos1)) < 0) return rv;

      //  if( (rv = move_2(pos2)) < 0) return rv;

      //  if(m_naxes == 3) if( (rv = move_3(pos3)) < 0) return rv;

      return 0;
    }

    void fsmCtrl::updateINDICurrentParams()
    {
      updateIfChanged(m_indiP_dac1, "current", m_dac1);
      updateIfChanged(m_indiP_dac2, "current", m_dac2);
      updateIfChanged(m_indiP_dac3, "current", m_dac3);

      m_v1 = get_v1(m_dac1);
      m_v2 = get_v2(m_dac2);
      m_v3 = get_v3(m_dac3);
      updateIfChanged(m_indiP_v1, "current", m_v1);
      updateIfChanged(m_indiP_v2, "current", m_v2);
      updateIfChanged(m_indiP_v3, "current", m_v3);

      m_alpha = get_alpha(m_dac1, m_dac2, m_dac3, m_a);
      m_beta = get_beta(m_dac2, m_dac3, m_b);
      m_z = get_z(m_dac1, m_dac2, m_dac3);
      updateIfChanged(m_indiP_alpha, "current", m_alpha);
      updateIfChanged(m_indiP_beta, "current", m_beta);
      updateIfChanged(m_indiP_z, "current", m_z);
    }

    // callback from setting m_indiP_dac1
    // only 'target' is editable ('current' should be updated by code)
    INDI_NEWCALLBACK_DEFN(fsmCtrl, m_indiP_dac1)
    (const pcf::IndiProperty &ipRecv)
    {
      if (ipRecv.createUniqueKey() == m_indiP_dac1.createUniqueKey())
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

          updateIfChanged(m_indiP_dac1, "target", target);

          uint32_t dacs[3] = {0, 0, 0};
          dacs[0] = target;
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

    // callback from setting m_indiP_dac2
    // only 'target' is editable ('current' should be updated by code)
    INDI_NEWCALLBACK_DEFN(fsmCtrl, m_indiP_dac2)
    (const pcf::IndiProperty &ipRecv)
    {
      if (ipRecv.createUniqueKey() == m_indiP_dac2.createUniqueKey())
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

          updateIfChanged(m_indiP_dac2, "target", target);

          uint32_t dacs[3] = {0, 0, 0};
          dacs[0] = m_dac1;
          dacs[1] = target;
          dacs[2] = m_dac3;

          std::ostringstream oss;
          oss << "INDI dacs callback: " << dacs[0] << " | " << dacs[1] << " | " << dacs[2];
          log<text_log>(oss.str());

          return setDacs(dacs);
        }
      }
      return -1;
    }

    // callback from setting m_indiP_dac3
    // only 'target' is editable ('current' should be updated by code)
    INDI_NEWCALLBACK_DEFN(fsmCtrl, m_indiP_dac3)
    (const pcf::IndiProperty &ipRecv)
    {
      if (ipRecv.createUniqueKey() == m_indiP_dac3.createUniqueKey())
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

          updateIfChanged(m_indiP_dac3, "target", target);

          uint32_t dacs[3] = {0, 0, 0};
          dacs[0] = m_dac1;
          dacs[1] = m_dac2;
          dacs[2] = target;

          std::ostringstream oss;
          oss << "INDI dacs callback: " << dacs[0] << " | " << dacs[1] << " | " << dacs[2];
          log<text_log>(oss.str());

          return setDacs(dacs);
        }
      }
      return -1;
    }

    // callback from setting m_indiP_alpha
    // only 'target' is editable ('current' should be updated by code)
    INDI_NEWCALLBACK_DEFN(fsmCtrl, m_indiP_alpha)
    (const pcf::IndiProperty &ipRecv)
    {
      if (ipRecv.createUniqueKey() == m_indiP_alpha.createUniqueKey())
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

          updateIfChanged(m_indiP_alpha, "target", target);

          uint32_t dacs[3] = {0, 0, 0};
          dacs[0] = get_dac1(target, m_z, m_a);
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

    // callback from setting m_indiP_beta
    // only 'target' is editable ('current' should be updated by code)
    INDI_NEWCALLBACK_DEFN(fsmCtrl, m_indiP_beta)
    (const pcf::IndiProperty &ipRecv)
    {
      if (ipRecv.createUniqueKey() == m_indiP_beta.createUniqueKey())
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

          updateIfChanged(m_indiP_beta, "target", target);

          uint32_t dacs[3] = {0, 0, 0};
          dacs[0] = m_dac1;
          dacs[1] = get_dac2(m_alpha, target, m_z, m_a, m_b);
          dacs[2] = m_dac3;

          std::ostringstream oss;
          oss << "INDI dacs callback: " << dacs[0] << " | " << dacs[1] << " | " << dacs[2];
          log<text_log>(oss.str());

          return setDacs(dacs);
        }
      }
      return -1;
    }

    // callback from setting m_indiP_z
    // only 'target' is editable ('current' should be updated by code)
    INDI_NEWCALLBACK_DEFN(fsmCtrl, m_indiP_z)
    (const pcf::IndiProperty &ipRecv)
    {
      if (ipRecv.createUniqueKey() == m_indiP_z.createUniqueKey())
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

          updateIfChanged(m_indiP_z, "target", target);

          uint32_t dacs[3] = {0, 0, 0};
          dacs[0] = m_dac1;
          dacs[1] = m_dac2;
          dacs[2] = get_dac3(m_alpha, m_beta, target, m_a, m_b);

          std::ostringstream oss;
          oss << "INDI dacs callback: " << dacs[0] << " | " << dacs[1] << " | " << dacs[2];
          log<text_log>(oss.str());

          return setDacs(dacs);
        }
      }
      return -1;
    }

    // callback from setting m_indiP_v1
    // only 'target' is editable ('current' should be updated by code)
    INDI_NEWCALLBACK_DEFN(fsmCtrl, m_indiP_v1)
    (const pcf::IndiProperty &ipRecv)
    {
      if (ipRecv.createUniqueKey() == m_indiP_v1.createUniqueKey())
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

          updateIfChanged(m_indiP_v1, "target", target);

          uint32_t dacs[3] = {0, 0, 0};
          dacs[0] = get_dac1(m_indiP_v1);
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

    // callback from setting m_indiP_v2
    // only 'target' is editable ('current' should be updated by code)
    INDI_NEWCALLBACK_DEFN(fsmCtrl, m_indiP_v2)
    (const pcf::IndiProperty &ipRecv)
    {
      if (ipRecv.createUniqueKey() == m_indiP_v2.createUniqueKey())
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

          updateIfChanged(m_indiP_v2, "target", target);

          uint32_t dacs[3] = {0, 0, 0};
          dacs[0] = m_dac1;
          dacs[1] = get_dac2(m_indiP_v2);
          dacs[2] = m_dac3;

          std::ostringstream oss;
          oss << "INDI dacs callback: " << dacs[0] << " | " << dacs[1] << " | " << dacs[2];
          log<text_log>(oss.str());

          return setDacs(dacs);
        }
      }
      return -1;
    }

    // callback from setting m_indiP_v3
    // only 'target' is editable ('current' should be updated by code)
    INDI_NEWCALLBACK_DEFN(fsmCtrl, m_indiP_v3)
    (const pcf::IndiProperty &ipRecv)
    {
      if (ipRecv.createUniqueKey() == m_indiP_v3.createUniqueKey())
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

          updateIfChanged(m_indiP_z, "target", target);

          uint32_t dacs[3] = {0, 0, 0};
          dacs[0] = m_dac1;
          dacs[1] = m_dac2;
          dacs[2] = get_dac3(m_indiP_v3);

          std::ostringstream oss;
          oss << "INDI dacs callback: " << dacs[0] << " | " << dacs[1] << " | " << dacs[2];
          log<text_log>(oss.str());

          return setDacs(dacs);
        }
      }
      return -1;
    }

    // // callback from setting dacs
    // INDI_NEWCALLBACK_DEFN(fsmCtrl, m_dacs)(const pcf::IndiProperty &ipRecv)
    // {
    //   if (ipRecv.createUniqueKey() == m_dacs.createUniqueKey())
    //   {
    //     uint32_t dacs[3] = {0, 0, 0};

    //     if(ipRecv.find("one"))
    //     {
    //       dacs[0] = ipRecv["one"].get<uint32_t>();
    //     }

    //     if(ipRecv.find("two"))
    //     {
    //       dacs[1] = ipRecv["two"].get<uint32_t>();
    //     }

    //     if(ipRecv.find("three"))
    //     {
    //       dacs[2] = ipRecv["three"].get<uint32_t>();
    //     }

    //     std::ostringstream oss;
    //     oss << "INDI dacs callback: " << dacs[0] << " | " << dacs[1] << " | " << dacs[2];
    //     log<text_log>(oss.str());

    //     setDacs(dacs);
    //   }

    //   log<text_log>("INDI callback.");
    //   return 0;
    // }

    // callback from setting conversion_factors
    INDI_NEWCALLBACK_DEFN(fsmCtrl, m_conversion_factors)
    (const pcf::IndiProperty &ipRecv)
    {
      if (ipRecv.createUniqueKey() == m_conversion_factors.createUniqueKey())
      {
        if (ipRecv.find("a"))
        {
          m_a = ipRecv["a"].get<uint32_t>();
        }

        if (ipRecv.find("b"))
        {
          m_b = ipRecv["b"].get<uint32_t>();
        }

        std::ostringstream oss;
        oss << "INDI conversion_factors callback: " << m_a << " | " << m_b;
        log<text_log>(oss.str());
      }

      log<text_log>("INDI callback.");
      return 0;
    }

  } // namespace app
} // namespace MagAOX