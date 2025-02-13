/** \file dmCtrl.hpp
  * \brief The MagAO-X XXXXXX header file
  *
  * \ingroup dmCtrl_files
  */

#ifndef dmCtrl_hpp
#define dmCtrl_hpp


#include "../../libMagAOX/libMagAOX.hpp" //Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"

typedef MagAOX::app::MagAOXApp<true> MagAOXAppT; // This needs to be before the other header files for logging to work in other headers

#include "dmCommands.hpp"
// #include "binaryUart.hpp"
// #include "cGraphPacket.hpp"
// #include "linux_pinout_uart.hpp"
// #include "socket.hpp"
// #include "IUart.h"


/** \defgroup dmCtrl
  * \brief The XXXXXX application to do YYYYYYY
  *
  * <a href="../handbook/operating/software/apps/XXXXXX.html">Application Documentation</a>
  *
  * \ingroup apps
  *
  */

/** \defgroup dmCtrl_files
  * \ingroup dmCtrl
  */

namespace MagAOX
{
  namespace app
  {

    /// The MagAO-X xxxxxxxx
    /** 
      * \ingroup dmCtrl
      */
    class dmCtrl : public MagAOXApp<true>, public dev::telemeter<dmCtrl>, public dev::shmimMonitor<dmCtrl>
    {

      //Give the test harness access.
      friend class dmCtrl_test;

      friend class dev::telemeter<dmCtrl>;
      typedef dev::telemeter<dmCtrl> telemeterT;

      friend class dev::shmimMonitor<dmCtrl>;

    protected:
      /** \name Constants
       *@{
       */
      const std::string USB0 = "/dev/ttyUSB0";
      ///@}

      /** \name Configurable Parameters
         *@{
         */
      
      //here add parameters which will be config-able at runtime
      
        // Connection parameters
        std::string PortName;
        uint32_t BaudRate = 115200; // serial-port-specific

        // Telemeter callback parameters
        int period_s;

        // Shmim size
        double width = 3; // shm size
        double height = 1; // shm size
      ///@}

        char Buffer[4096];
        CGraphPacket PacketProtocol;
        std::unique_ptr<IUart> LocalPortPinout;
        std::unique_ptr<BinaryUart> UartParser;
        PZTQuery *telemetryQuery = new TelemetryQuery();
        std::vector<PZTQuery*> queries = { telemetryQuery };


    public:
      /// Default c'tor.
      dmCtrl();

      /// D'tor, declared and defined for noexcept.
      ~dmCtrl() noexcept
      {}

      virtual void setupConfig();

      /// Implementation of loadConfig logic, separated for testing.
      /** This is called by loadConfig().
         */
      int loadConfigImpl( mx::app::appConfigurator & _config /**< [in] an application configuration from which to load values*/);

      virtual void loadConfig();

      /// Startup function
      /**
         *
         */
      virtual int appStartup();

      /// Implementation of the DM for dmCtrl.
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

      /// Initialize UartParser
      /**
       *
       */
      void initUartParser();

      /// TODO: Test the connection to the dm
      int testConnection();


      /// Connect to dm via Serial Port
      /**
       *
       * \returns 0 if connection successful
       * \returns -1 on an error
       */
      int serialPortConnect();

      /**
       * @brief Query interface for the dm
       *
       * Function that sends a command packet to the dm.
       *
       * @param pztQuery pointer to a class inheriting from PZTQuery (see dmCommands.hpp)
       */
      void query(PZTQuery *);

      /**
       * @brief Function that listens for responses from the dm
       *
       * Function that checks for a response from the dm and processes it.
       * If a response is received it processes the response as appropriate for the
       * command sent.
       *
       * @param pztQuery pointer to a class inheriting from PZTQuery (see dmCommands.hpp)
       */
      void receive();

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
       * @param telem_dm_ptr pointer to telem_dm flatbuffer_log structure describing telem inputs & outputs
       * \returns 0 on succcess
       * \returns -1 on error
       */
      int recordTelem(const telem_dm *);

      /**
       * @brief Required by Telemeter Interface
       *
       * @param force boolean; Telemetry is recorded every m_maxInterval (default value of 10) seconds.
       *  If 'true', force telemetry record outside of interval.
       * \returns 0 on succcess
       */
      int recordDM(bool force = false);
      ///@}

      /** \name shmim Monitor Interface
       *
       * @{
       */

      /**
       * @brief Required by smimMonitor Interface
       * Called after shmimMonitor connects to the dm stream.
       *
       * \returns 0 on success
       * \returns -1 if incorrect size or data type in stream.
       */
      int allocate(const dev::shmimT &sp);

      /**
       * @brief Required by smimMonitor Interface
       * Called by shmimMonitor when a new dm command is available.
       *
       * \returns 0 on success
       * \returns -1 if incorrect size or data type in stream.
       */
      int processImage(void *curr_src,
                       const dev::shmimT &sp);

      /**
       * @brief Send to dm new values from shmim
       *
       * Called as part of processImage.
       *
       * \returns 0 on success
       * \returns -1 if incorrect size or data type in stream.
       */
      int commandDM(void *curr_src);

    };

    dmCtrl::dmCtrl() : MagAOXApp(MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED)
    {
      m_powerMgtEnabled = true;
      m_getExistingFirst = true; // get existing shmim (??? should or shouldn't)
      return;
    }

    void dmCtrl::setupConfig()
    {
      config.add("parameters.period_s", "", "parameters.period_s", argType::Optional, "parameters", "period_s", false, "int", "The period of telemetry queries to the dm.");

      config.add("serial_port.port_address", "", "serial_port.port_address", argType::Optional, "serial_port", "port_address", false, "string", "The address where the client machine is connected to.");
      config.add("serial_port.baud_rate", "", "serial_port.baud_rate", argType::Optional, "serial_port", "baud_rate", false, "int", "The baud rate for the serial port.");

      // shmim parameters
      config.add("shmimMonitor.shmimName", "", "shmimMonitor.shmimName", argType::Required, "shmimMonitor", "shmimName", false, "string", "The name of the ImageStreamIO shared memory image. Will be used as /tmp/<shmimName>.im.shm. Default is dm");

      config.add("shmimMonitor.width", "", "shmimMonitor.width", argType::Required, "shmimMonitor", "width", false, "string", "The width of the DM in actuators.");
      config.add("shmimMonitor.height", "", "shmimMonitor.height", argType::Required, "shmimMonitor", "height", false, "string", "The height of the DM in actuators.");

    }

    int dmCtrl::loadConfigImpl( mx::app::appConfigurator & _config )
    {
      /// CONNECTION PARAMETERS ///
      PortName = USB0;
      _config(period_s, "parameters.period_s");
      _config(PortName, "serial_port.port_address");
      _config(BaudRate, "serial_port.baud_rate");
    
      dmCtrl::LocalPortPinout = std::make_unique<linux_pinout_uart>();
      initUartParser();

      /// SHMIM PARAMETERS ///
      _config(width, "shmimMonitor.width");
      _config(height, "shmimMonitor.height");

      shmimMonitor::loadConfig(_config);
      return 0;
    }

    void dmCtrl::loadConfig()
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

    int dmCtrl::appStartup()
    {
      if (telemeterT::appStartup() < 0)
      {
        return log<software_error, -1>({__FILE__, __LINE__});
      }

      if (shmimMonitor::appStartup() < 0)
      {
        return log<software_error, -1>({__FILE__, __LINE__});
      }

      // if(!streamExists()) {
      //   if (createStream() < 0) {
      //     log<software_error>({__FILE__, __LINE__});
      //     return -1;
      //   }
      // }

      return 0;
    }

    int dmCtrl::appLogic()
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
        rv = serialPortConnect();

        if (rv == 0)
        {
          state(stateCodes::CONNECTED);
        }
      }

      if (state() == stateCodes::CONNECTED)
      {
      }

      return 0;
    }

    int dmCtrl::appShutdown()
    {
      telemeterT::appShutdown();
      shmimMonitor<dmCtrl>::appShutdown();

      return 0;
    }

    void dmCtrl::query(PZTQuery *pztQuery)
    {
      log<text_log>(pztQuery->startLog);
      // Send command packet
      UartParser->TxBinaryPacket(pztQuery->getPayloadType(), pztQuery->getPayloadData(), pztQuery->getPayloadLen());
      // debug
      // log<text_log>(pztQuery->endLog);

      receive();
    }

    void dmCtrl::receive() {
      // The packet is read byte by byte, so keep going while there are bytes left
      bool Bored = false;
      while (!Bored)
      {
        Bored = true;
        if (UartParser->Process())
        {
          Bored = false;
        }

        if (false == dmCtrl::LocalPortPinout->isopen())
        {
          serialPortConnect();
        }
      }

      // // Once packet had been received, make sure updates are propagated.
      // // Since we don't know the packet type, update all.
      // receiveAdcs();
      // receiveDacs();
    }

    //////////////
    // CONNECTION
    //////////////

    void dmCtrl::initUartParser()
    {
      UartParser = std::make_unique<BinaryUart>(*LocalPortPinout, PacketProtocol, PacketCallbacks, queries, false);
    }

    /// TODO: Test the connection to the device
    int dmCtrl::testConnection()
    {
      return 0;
    }

    int dmCtrl::serialPortConnect()
    {
      PinoutConfig pinoutConfig = PinoutConfig::CreateSerialConfig(BaudRate, PortName.c_str());
      int err = dmCtrl::LocalPortPinout->init(pinoutConfig);
      if (IUart::IUartOK != err)
      {
        log<software_error, -1>({__FILE__, __LINE__, errno, "SerialPortBinaryCmdr: can't open port (" + PortName + ":" + std::to_string(BaudRate) + "), exiting.\n"});
        return -1;
      }

      log<text_log>("Connected to port (" + PortName + ":" + std::to_string(BaudRate) + ")");
      return 0;
    }


    /////////////////////////
    // TELEMETER INTERFACE
    /////////////////////////

    int dmCtrl::checkRecordTimes()
    {
      return telemeterT::checkRecordTimes(telem_dm());
    }

    int dmCtrl::recordTelem(const telem_dm *)
    {
      query(telemetryQuery);
      return recordDM(true);
    }

    int dmCtrl::recordDM(bool force)
    {
      static CGraphDMTelemetryPayload LastTelemetry; ///< Structure holding the previous dm voltage measurement.
      TelemetryQuery *telemetryQueryPtr = dynamic_cast<TelemetryQuery *>(telemetryQuery);

      if (!(LastTelemetry == telemetryQueryPtr->Telemetry) || force)
      {
        LastTelemetry = telemetryQueryPtr->Telemetry;
        telem<telem_dm>({LastTelemetry.P1V2, LastTelemetry.P2V2, LastTelemetry.P28V, LastTelemetry.P2V5, LastTelemetry.P6V, LastTelemetry.P5V, LastTelemetry.P3V3D, LastTelemetry.P4V3, LastTelemetry.P2I2, LastTelemetry.P4I3, LastTelemetry.P6I});
      }

      return 0;
    }


    /////////////////////////
    // SHMIMMONITOR INTERFACE
    /////////////////////////

    int dmCtrl::allocate(const dev::shmimT &sp)
    {
      static_cast<void>(sp); // be unused

      // // validateStream will delete & recreate stream if it doesn't match size & kw requirements
      // if (streamExists()) {
      //   if (validateStream() < 0) {
      //     log<software_error>({__FILE__, __LINE__});
      //     return -1;
      //   }
      // }   

      return 0;
    }

    int dmCtrl::processImage(void *curr_src,
                              const dev::shmimT &sp)
    {
      static_cast<void>(sp); // be unused

      int rv = commandDM(curr_src);

      if (rv < 0)
      {
        log<software_critical>({__FILE__, __LINE__, errno, rv, "Error from commandDM"});
        return rv;
      }

      return rv;
    }

    int dmCtrl::commandDM(void *curr_src)
    {
    }

  } //namespace app
} //namespace MagAOX

#endif //dmCtrl_hpp
