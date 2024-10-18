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
#include <cmath>  // For pow(), cos() and M_PI

#include <iostream>
using namespace std;

#include <pthread.h>

typedef MagAOX::app::MagAOXApp<true> MagAOXAppT; // This needs to be before the other header files for logging to work in other headers

#include "conversion.hpp"
#include "fsmCommands.hpp"
#include "binaryUart.hpp"
#include "cGraphPacket.hpp"
#include "linux_pinout_client_socket.hpp"
#include "linux_pinout_uart.hpp"
#include "socket.hpp"
#include "IUart.h"

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
      /** \name Constants
       *@{
       */
      const std::string DACS = "dacs";
      const std::string VOLTAGES = "voltages";
      const std::string TTP = "ttp";
      const std::string SHMIM = "shmim";
      const std::string INDI = "indi";
      const std::string LOCALHOST = "127.0.0.1";
      const std::string USB0 = "/dev/ttyUSB0";
      ///@}

      /** \name Configurable Parameters
       *@{
       */
      // Connection parameters
      std::string type;
      std::string PortName;
      int nHostPort = 66873; // 65536 + 1337 ; socket-specific
      uint32_t BaudRate = 115200; // serial-port-specific

      // Telemeter callback parameters
      int period_s;

      // Safe operating range parameters
      double m_dac1_min;
      double m_dac1_max;
      double m_dac2_min;
      double m_dac2_max;
      double m_dac3_min;
      double m_dac3_max;

      // Conversion parameters
      double D_per_V;
      double m_B;
      double m_L;      
      double m_voltage_max = 100.0; // in volts
      double m_stroke_max = 10.0; // in micrometers
      double m_v = (4.096 / (std::pow(2.0, 24))) * 60;
      double d_piston = 5.0; // in micrometers 

      // Shmim size
      double width = 3; // shm size
      double height = 1; // shm size

      // input parameters
      std::string kw_name = "inputType";
      std::string m_inputType;
      std::string m_inputToggle;

      // here add parameters which will be config-able at runtime
      ///@}

      char Buffer[4096];
      CGraphPacket SocketProtocol;
      std::unique_ptr<IUart> LocalPortPinout;
      std::unique_ptr<BinaryUart> UartParser;
      PZTQuery *telemetryQuery = new TelemetryQuery();
      PZTQuery *adcsQuery = new AdcsQuery();
      PZTQuery *dacsQuery = new DacsQuery();
      std::vector<PZTQuery*> queries = { telemetryQuery, adcsQuery, dacsQuery };
      uint32_t targetSetpoints[3];

      double m_dac1{0};
      double m_dac2{0};
      double m_dac3{0};

      double m_adc1{0};
      double m_adc2{0};
      double m_adc3{0};

    protected:
      // INDI properties
      pcf::IndiProperty m_indiP_val1;
      pcf::IndiProperty m_indiP_val2;
      pcf::IndiProperty m_indiP_val3;
      pcf::IndiProperty m_indiP_dac1;
      pcf::IndiProperty m_indiP_dac2;
      pcf::IndiProperty m_indiP_dac3;
      pcf::IndiProperty m_indiP_adc1;
      pcf::IndiProperty m_indiP_adc2;
      pcf::IndiProperty m_indiP_adc3;
      pcf::IndiProperty m_indiP_conversion_factors;
      pcf::IndiProperty m_indiP_input;
      pcf::IndiProperty m_indiP_query;

    public:
      INDI_NEWCALLBACK_DECL(fsmCtrl, m_indiP_val1);
      INDI_NEWCALLBACK_DECL(fsmCtrl, m_indiP_val2);
      INDI_NEWCALLBACK_DECL(fsmCtrl, m_indiP_val3);
      INDI_NEWCALLBACK_DECL(fsmCtrl, m_indiP_dac1);
      INDI_NEWCALLBACK_DECL(fsmCtrl, m_indiP_dac2);
      INDI_NEWCALLBACK_DECL(fsmCtrl, m_indiP_dac3);
      // INDI_NEWCALLBACK_DECL(fsmCtrl, m_indiP_adc1);
      // INDI_NEWCALLBACK_DECL(fsmCtrl, m_indiP_adc2);
      // INDI_NEWCALLBACK_DECL(fsmCtrl, m_indiP_adc3);
      INDI_NEWCALLBACK_DECL(fsmCtrl, m_indiP_conversion_factors);
      INDI_NEWCALLBACK_DECL(fsmCtrl, m_indiP_input);
      INDI_NEWCALLBACK_DECL(fsmCtrl, m_indiP_query);

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

      /// Initialize UartParser
      /**
       *
       */
      void initUartParser();

      /// TODO: Test the connection to the fsm
      int testConnection();

      /// Connect to fsm via Socket
      /**
       *
       * \returns 0 if connection successful
       * \returns -1 on an error
       */
      int socketConnect();

      /// Connect to fsm via Serial Port
      /**
       *
       * \returns 0 if connection successful
       * \returns -1 on an error
       */
      int serialPortConnect();

      // /**
      //  * @brief Request fsm telemetry
      //  *
      //  * Wrapper that calls query() with instance of TelemetryQuery.
      //  * Response is stored in instance's Telemetry member.
      //  * It returns fsm telemetry that is logged every 10s by the telemeter.
      //  * Output in /opt/telem/fsmCtrl_xxxxx.bintel
      //  */
      // void queryTelemetry();

      /**
       * @brief Request fsm's ADC values
       *
       * Wrapper that calls query() with instance of AdcsQuery.
       * Response is stored in instance's AdcVals member.
       * Response is also logged in /opt/tele/fsmCtrl_xxxxxx.binlog
       */
      void receiveAdcs();

      /**
       * @brief Request fsm's DAC values
       *
       * Wrapper that calls query() with instance of DacsQuery.
       * Response is stored in instance's DacSetpoints member.
       * Response is also logged in /opt/tele/fsmCtrl_xxxxxx.binlog
       */
      void receiveDacs();

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
       * Function that sends a command packet to the fsm.
       *
       * @param pztQuery pointer to a class inheriting from PZTQuery (see fsmCommands.hpp)
       */
      void query(PZTQuery *);

      /**
       * @brief Function that listens for responses from the fsm
       *
       * Function that checks for a response from the fsm and processes it.
       * If a response is received it processes the response as appropriate for the
       * command sent.
       *
       * @param pztQuery pointer to a class inheriting from PZTQuery (see fsmCommands.hpp)
       */
      void receive();

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
       * @brief Required by smimMonitor Interface
       * Called after shmimMonitor connects to the fsm stream.
       *
       * \returns 0 on success
       * \returns -1 if incorrect size or data type in stream.
       */
      int allocate(const dev::shmimT &sp);

      /**
       * @brief Required by smimMonitor Interface
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
       * Checks shmim has an inputType keyword and that its value is 'dacs', 'voltages' or 'ttp'.
       * Updates INDI input.type property, if different.
       * Updates corresponding INDI 'target' values with shmim values.
       * Converts shmim values from specified inputType to DACs.
       * Calls setDacs function with new DAC values.
       *
       * \returns 0 on success
       * \returns -1 if incorrect size or data type in stream.
       */
      int commandFSM(void *curr_src);

      /**
       * @brief Checks if shmim exists
       *
       * \returns true if shmim exists
       * \returns false otherwise
       */
      bool streamExists();

      /**
       * @brief Checks if shmim has expected size & keyword. If it doesn't deletes it.
       *
       * \returns 0 on success
       * \returns -1 on failure
       */
      int validateStream();

      /**
       * @brief Create shmim if it doesn't exist
       *
       * \returns 0 on success
       * \returns -1 on failure
       */
      int createStream();
      ///@}
    };

    fsmCtrl::fsmCtrl() : MagAOXApp(MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED), LocalPortPinout(nullptr), UartParser(nullptr)
    {
      m_powerMgtEnabled = true;
      m_getExistingFirst = true; // get existing shmim (??? should or shouldn't)
      return;
    }

    void fsmCtrl::setupConfig()
    {
      shmimMonitor::setupConfig(config);

      config.add("parameters.connection_type", "", "parameters.connection_type", argType::Required, "parameters", "connection_type", false, "string", "The type of connection: serial_port or socket.");
      config.add("parameters.period_s", "", "parameters.period_s", argType::Optional, "parameters", "period_s", false, "int", "The period of telemetry queries to the fsm.");

      config.add("socket.client_entrance_ip", "", "socket.client_entrance_ip", argType::Optional, "socket", "client_entrance_ip", false, "string", "The IP address on the client machine that the tunnel is set up from.");
      config.add("socket.host_port", "", "socket.host_port", argType::Optional, "socket", "host_port", false, "int", "The port at which the fsm driver is listening for connections.");
      
      config.add("serial_port.port_address", "", "serial_port.port_address", argType::Optional, "serial_port", "port_address", false, "string", "The address where the client machine is connected to.");
      config.add("serial_port.baud_rate", "", "serial_port.baud_rate", argType::Optional, "serial_port", "baud_rate", false, "int", "The baud rate for the serial port.");

      config.add("fsm.B", "", "fsm.B", argType::Optional, "fsm", "B", false, "double", "Baseline distance of the three piezos. Defaults to (L * cos(30deg)).");
      config.add("fsm.L", "", "fsm.L", argType::Optional, "fsm", "L", false, "double", "Distance between FSM piezo actuators. In units of micrometers. Defaults to 12000 micrometers.");
      config.add("fsm.v", "", "fsm.v", argType::Required, "fsm", "v", false, "double", "Conversion factor for converting from voltages to dacs.");
      config.add("fsm.dac1_min", "", "fsm.dac1_min", argType::Optional, "fsm", "dac1_min", false, "double", "Min safe value for dac1. Defaults to 0.");
      config.add("fsm.dac1_max", "", "fsm.dac1_max", argType::Optional, "fsm", "dac1_max", false, "double", "Max safe value for dac1. Defaults to voltage max conversion.");
      config.add("fsm.dac2_min", "", "fsm.dac2_min", argType::Optional, "fsm", "dac2_min", false, "double", "Min safe value for dac2. Defaults to 0.");
      config.add("fsm.dac2_max", "", "fsm.dac2_max", argType::Optional, "fsm", "dac2_max", false, "double", "Max safe value for dac2. Defaults to voltage max conversion.");
      config.add("fsm.dac3_min", "", "fsm.dac3_min", argType::Optional, "fsm", "dac3_min", false, "double", "Min safe value for dac3. Defaults to 0.");
      config.add("fsm.dac3_max", "", "fsm.dac3_max", argType::Optional, "fsm", "dac3_max", false, "double", "Max safe value for dac3. Defaults to voltage max conversion.");
      config.add("fsm.voltage_max", "", "fsm.voltage_max", argType::Optional, "fsm", "voltage_max", false, "double", "Max voltage safe value in volts. Defaults to 100.");
      config.add("fsm.stroke_max", "", "fsm.stroke_max", argType::Optional, "fsm", "stroke_max", false, "double", "Max stroke value in micrometers. Defaults to 10.");

      // shmim parameters
      config.add("shmimMonitor.shmimName", "", "shmimMonitor.shmimName", argType::Required, "shmimMonitor", "shmimName", false, "string", "The name of the ImageStreamIO shared memory image. Will be used as /tmp/<shmimName>.im.shm. Default is fsm");

      config.add("shmimMonitor.width", "", "shmimMonitor.width", argType::Required, "shmimMonitor", "width", false, "string", "The width of the FSM in actuators.");
      config.add("shmimMonitor.height", "", "shmimMonitor.height", argType::Required, "shmimMonitor", "height", false, "string", "The height of the FSM in actuators.");

      config.add("input.type", "", "input.type", argType::Optional, "input", "type", false, "string", "The type of values that the shmim contains. Can be 'dacs', 'voltages' or 'ttp'. Defaults to voltages.");
      config.add("input.toggle", "", "input.toggle", argType::Optional, "input", "toggle", false, "string", "Where the input comes from. Can be 'shmim', 'indi'. Defaults to shmim.");
      telemeterT::setupConfig(config);
    }

    int fsmCtrl::loadConfigImpl(mx::app::appConfigurator &_config)
    {
      /// CONNECTION PARAMETERS ///
      _config(type, "parameters.connection_type");
      _config(period_s, "parameters.period_s");

      if (type == "socket")
      {
        PortName = LOCALHOST;
        _config(PortName, "socket.client_entrance_ip");
        _config(nHostPort, "socket.host_port");

        fsmCtrl::LocalPortPinout = std::make_unique<linux_pinout_client_socket>();
      }
      else // defaulting to serial_port
      {
        PortName = USB0;
        _config(PortName, "serial_port.port_address");
        _config(BaudRate, "serial_port.baud_rate");

        fsmCtrl::LocalPortPinout = std::make_unique<linux_pinout_uart>();
      }
      // Since LocalPortPinout is now initialized, can also initialize UartParser
      initUartParser();

      /// CONVERSTION PARAMETERS ///
      _config(m_L, "fsm.L");
      m_B = m_L * cos(30 * (M_PI / 180.0));
      _config(m_B, "fsm.B");

      _config(m_v, "fsm.v");
      _config(m_voltage_max, "fsm.voltage_max");
      _config(m_stroke_max, "fsm.stroke_max");

      D_per_V = m_stroke_max / m_voltage_max;
      m_dac1_min = m_dac2_min = m_dac3_min = 0;
      m_dac1_max = m_dac2_max = m_dac3_max = vi_to_daci(m_voltage_max, m_v);

      /// DAC RANGE PARAMETERS ///
      _config(m_dac1_min, "fsm.dac1_min");
      _config(m_dac1_max, "fsm.dac1_max");
      _config(m_dac2_min, "fsm.dac2_min");
      _config(m_dac2_max, "fsm.dac2_max");
      _config(m_dac3_min, "fsm.dac3_min");
      _config(m_dac3_max, "fsm.dac3_max");

      /// SHMIM PARAMETERS ///
      _config(width, "shmimMonitor.width");
      _config(height, "shmimMonitor.height");

      /// COMMAND INPUT PARAMETERS ///
      m_inputType = VOLTAGES;
      _config(m_inputType, "input.type");
      if (!(m_inputType == DACS || m_inputType == VOLTAGES || m_inputType == TTP))
      {
        std::ostringstream oss;
        oss << "Config file sets inputType to a value other than 'dacs', 'voltages', or 'ttp': " << m_inputType;
        log<software_critical>({__FILE__, __LINE__, errno, oss.str()});
        return -1;
      }

      m_inputToggle = SHMIM;
      _config(m_inputToggle, "input.toggle");
      if (!(m_inputToggle == SHMIM || m_inputToggle == INDI))
      {
        std::ostringstream oss;
        oss << "Config file sets m_inputToggle to a value other than 'shmim', or 'indi': " << m_inputToggle;
        log<software_critical>({__FILE__, __LINE__, errno, oss.str()});
        return -1;
      }      

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
      // dac boundaries
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

      // vals
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

      // adcs
      REG_INDI_NEWPROP_NOCB(m_indiP_adc1, "adc_1", pcf::IndiProperty::Number);
      m_indiP_adc1.add(pcf::IndiElement("current"));
      m_indiP_adc1["current"] = -99999;
      REG_INDI_NEWPROP_NOCB(m_indiP_adc2, "adc_2", pcf::IndiProperty::Number);
      m_indiP_adc2.add(pcf::IndiElement("current"));
      m_indiP_adc2["current"] = -99999;
      REG_INDI_NEWPROP_NOCB(m_indiP_adc3, "adc_3", pcf::IndiProperty::Number);
      m_indiP_adc3.add(pcf::IndiElement("current"));
      m_indiP_adc3["current"] = -99999;

      // conversion_factors
      REG_INDI_NEWPROP(m_indiP_conversion_factors, "conversion_factors", pcf::IndiProperty::Number);
      m_indiP_conversion_factors.add(pcf::IndiElement("B"));
      m_indiP_conversion_factors["B"] = m_B;
      m_indiP_conversion_factors.add(pcf::IndiElement("L"));
      m_indiP_conversion_factors["L"] = m_L;
      m_indiP_conversion_factors.add(pcf::IndiElement("v"));
      m_indiP_conversion_factors["v"] = m_v;
      m_indiP_conversion_factors.add(pcf::IndiElement("voltage_max"));
      m_indiP_conversion_factors["voltage_max"] = m_voltage_max;
      m_indiP_conversion_factors.add(pcf::IndiElement("stroke_max"));
      m_indiP_conversion_factors["stroke_max"] = m_stroke_max;

      // input
      REG_INDI_NEWPROP(m_indiP_input, "input", pcf::IndiProperty::Text);
      m_indiP_input.add(pcf::IndiElement("toggle"));
      m_indiP_input["toggle"] = m_inputToggle;
      m_indiP_input.add(pcf::IndiElement("type"));
      m_indiP_input["type"] = m_inputType;

      // type of query
      REG_INDI_NEWPROP(m_indiP_query, "telemetry", pcf::IndiProperty::Text);
      m_indiP_query.add(pcf::IndiElement("query"));
      m_indiP_query["query"] = "none";

      if(!streamExists()) {
        if (createStream() < 0) {
          log<software_error>({__FILE__, __LINE__});
          return -1;
        }
      }

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
        if (type == "serial_port")
        {
          rv = serialPortConnect();
        }
        else if (type == "socket")
        {
          rv = socketConnect();
        }

        if (rv == 0)
        {
          state(stateCodes::CONNECTED);
        }
      }

      if (state() == stateCodes::CONNECTED)
      {
        // // Get current adc values
        query(adcsQuery);

        // // Get current dac values
        query(dacsQuery);

        // Get telemetry
        // queryTelemetry();

        if (m_inputToggle == SHMIM)
        {
          state(stateCodes::OPERATING);
        }
        if (m_inputToggle == INDI)
        {
          state(stateCodes::READY);
        }

        receive();
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

    void fsmCtrl::initUartParser()
    {
      UartParser = std::make_unique<BinaryUart>(*LocalPortPinout, SocketProtocol, PacketCallbacks, queries, false);
    }

    /// TODO: Test the connection to the device
    int fsmCtrl::testConnection()
    {
      return 0;
    }

    int fsmCtrl::socketConnect()
    {
      PinoutConfig pinoutConfig = PinoutConfig::CreateSocketConfig(nHostPort, PortName.c_str());
      int err = fsmCtrl::LocalPortPinout->init(pinoutConfig);
      if (IUart::IUartOK != err)
      {
        log<software_error, -1>({__FILE__, __LINE__, errno, "SerialPortBinaryCmdr: can't open socket (" + PortName + ":" + std::to_string(nHostPort) + "), exiting.\n"});
        return -1;
      }

      log<text_log>("Connected to socket (" + PortName + ":" + std::to_string(nHostPort) + ")");
      return 0;
    }

    int fsmCtrl::serialPortConnect()
    {
      PinoutConfig pinoutConfig = PinoutConfig::CreateSerialConfig(BaudRate, PortName.c_str());
      int err = fsmCtrl::LocalPortPinout->init(pinoutConfig);
      if (IUart::IUartOK != err)
      {
        log<software_error, -1>({__FILE__, __LINE__, errno, "SerialPortBinaryCmdr: can't open port (" + PortName + ":" + std::to_string(BaudRate) + "), exiting.\n"});
        return -1;
      }

      log<text_log>("Connected to port (" + PortName + ":" + std::to_string(BaudRate) + ")");
      return 0;
    }

    //////////////
    // FSM QUERIES
    //////////////

    // // Function to request fsm Telemetry
    // void fsmCtrl::queryTelemetry()
    // {
    //   query(telemetryQuery);
    //   // recordFsm(false);
    // }

    // Function to request fsm ADCs
    void fsmCtrl::receiveAdcs()
    {
      AdcsQuery *castAdcsQuery = dynamic_cast<AdcsQuery *>(adcsQuery);

      double samples1 = static_cast<double>(castAdcsQuery->AdcVals[0].Samples);
      double samples2 = static_cast<double>(castAdcsQuery->AdcVals[1].Samples);
      double samples3 = static_cast<double>(castAdcsQuery->AdcVals[2].Samples);

      double numAccums1 = static_cast<double>(castAdcsQuery->AdcVals[0].NumAccums);
      double numAccums2 = static_cast<double>(castAdcsQuery->AdcVals[1].NumAccums);
      double numAccums3 = static_cast<double>(castAdcsQuery->AdcVals[2].NumAccums);

      m_adc1 = (8.192 * ((samples1 - 0) / numAccums1)) / 16777216.0;
      m_adc2 = (8.192 * ((samples2 - 0) / numAccums2)) / 16777216.0;
      m_adc3 = (8.192 * ((samples3 - 0) / numAccums3)) / 16777216.0;

      updateIfChanged(m_indiP_adc1, "current", m_adc1);
      updateIfChanged(m_indiP_adc2, "current", m_adc2);
      updateIfChanged(m_indiP_adc3, "current", m_adc3);
    }

    // Function to request fsm DACs
    void fsmCtrl::receiveDacs()
    {
      DacsQuery *castDacsQuery = dynamic_cast<DacsQuery *>(dacsQuery);

      m_dac1 = static_cast<float>(castDacsQuery->DacSetpoints[0]);
      m_dac2 = static_cast<float>(castDacsQuery->DacSetpoints[1]);
      m_dac3 = static_cast<float>(castDacsQuery->DacSetpoints[2]);

      updateINDICurrentParams();
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

      castDacsQuery->setPayload(Setpoints, 3 * sizeof(uint32_t));
      query(castDacsQuery);

      castDacsQuery->logReply();
      castDacsQuery->resetPayload();

      m_dac1 = castDacsQuery->DacSetpoints[0];
      m_dac2 = castDacsQuery->DacSetpoints[1];
      m_dac3 = castDacsQuery->DacSetpoints[2];
      updateINDICurrentParams();

      query(dacsQuery);
      query(adcsQuery);
      return 0;
    }

    void fsmCtrl::query(PZTQuery *pztQuery)
    {
      log<text_log>(pztQuery->startLog);
      // Send command packet
      UartParser->TxBinaryPacket(pztQuery->getPayloadType(), pztQuery->getPayloadData(), pztQuery->getPayloadLen());
      // debug
      // log<text_log>(pztQuery->endLog);

      receive();
    }

    void fsmCtrl::receive() {
      // The packet is read byte by byte, so keep going while there are bytes left
      bool Bored = false;
      while (!Bored)
      {
        Bored = true;
        if (UartParser->Process())
        {
          Bored = false;
        }

        if (false == fsmCtrl::LocalPortPinout->isopen())
        {
          if (type == "serial_port")
          {
            serialPortConnect();
          }
          else if (type == "socket")
          {
            socketConnect();
          }
        }
      }

      // Once packet had been received, make sure updates are propagated.
      // Since we don't know the packet type, update all.
      receiveAdcs();
      receiveDacs();
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
      query(telemetryQuery);
      return recordFsm(true);
    }

    int fsmCtrl::recordFsm(bool force)
    {
      static CGraphFSMTelemetryPayload LastTelemetry; ///< Structure holding the previous fsm voltage measurement.
      TelemetryQuery *telemetryQueryPtr = dynamic_cast<TelemetryQuery *>(telemetryQuery);

      if (!(LastTelemetry == telemetryQueryPtr->Telemetry) || force)
      {
        LastTelemetry = telemetryQueryPtr->Telemetry;
        telem<telem_fsm>({LastTelemetry.P1V2, LastTelemetry.P2V2, LastTelemetry.P28V, LastTelemetry.P2V5, LastTelemetry.P3V3A, LastTelemetry.P6V, LastTelemetry.P5V, LastTelemetry.P3V3D, LastTelemetry.P4V3, LastTelemetry.N5V, LastTelemetry.N6V, LastTelemetry.P150V});
      }

      return 0;
    }

    /////////////////////////
    // SHMIMMONITOR INTERFACE
    /////////////////////////

    int fsmCtrl::allocate(const dev::shmimT &sp)
    {
      static_cast<void>(sp); // be unused

      // validateStream will delete & recreate stream if it doesn't match size & kw requirements
      if (streamExists()) {
        if (validateStream() < 0) {
          log<software_error>({__FILE__, __LINE__});
          return -1;
        }
      }   

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
      uint32_t dacs[3] = {0, 0, 0};

      //  if(state() != stateCodes::OPERATING) return 0;
      float val1, val2, val3;
      val1 = ((float *)curr_src)[0];
      val2 = ((float *)curr_src)[1];
      
      if (m_inputType == TTP) {
        val3 = d_piston;
      } else {
        val3 = ((float *)curr_src)[2];
      }

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
        dacs[0] = vi_to_daci(val1, m_v);
        dacs[1] = vi_to_daci(val2, m_v);
        dacs[2] = vi_to_daci(val3, m_v);
      }
      else if (m_inputType == TTP)
      {
        dacs[0] = ttp_to_dac1(val1, d_piston, m_B, D_per_V, m_v);
        dacs[1] = ttp_to_dac2(val1, val2, d_piston, m_B, m_L, D_per_V, m_v);
        dacs[2] = ttp_to_dac3(val1, val2, d_piston, m_B, m_L, D_per_V, m_v);
      }

      std::ostringstream oss;
      oss << "SHMIM dacs callback: " << dacs[0] << " | " << dacs[1] << " | " << dacs[2];
      log<text_log>(oss.str());

      std::unique_lock<std::mutex> lock(m_indiMutex);

      return setDacs(dacs);
    }

    bool fsmCtrl::streamExists() { 
      // Check if ImageStream exists.
      // From shmimMonitor::smThreadExec.
      int SM_fd;
      char SM_fname[200];
      ImageStreamIO_filename(SM_fname, sizeof(SM_fname), m_shmimName.c_str());
      SM_fd = open(SM_fname, O_RDWR);
      if (SM_fd == -1)
      {
          close(SM_fd);
          return false;
      } else {
        return true;
      }
    }

    int fsmCtrl::validateStream() { 
      std::string inputType = "";

      // If it has the wrong shape, destroy it
      if(m_imageStream.md->size[0] != height || m_imageStream.md->size[1] != width)
      {
        std::ostringstream oss;
        oss << "Shmim '" << shmimMonitor::m_shmimName << "' has the wrong shape: height = " << m_height << ", width = " << m_width << ". Destroying it." << std::endl;
        log<software_warning>({__FILE__, __LINE__, errno, oss.str()});

        ImageStreamIO_destroyIm(&m_imageStream);
        if (createStream() < 0) {
          log<software_error>({__FILE__, __LINE__});
          return -1;
        }
        return 0;        
      }

      // Check if shmim has inputType keyword
      int kwn = 0;
      bool kw_found = false;
      while ((m_imageStream.kw[kwn].type != 'N') && (kwn < m_imageStream.md->NBkw))
      {
        // std::string name(m_imageStream.kw[kwn].name);
        // if (name == kw_name)
        if (std::string(m_imageStream.kw[kwn].name) == kw_name)
        {
          kw_found = true;
          inputType = std::string(m_imageStream.kw[kwn].value.valstr);
          if (!(inputType == DACS || inputType == VOLTAGES || inputType == TTP))
          {
            std::ostringstream oss;
            oss << "Shmim '" << shmimMonitor::m_shmimName << "' has an inputType keyword with a value other than 'dacs', 'voltages', or 'ttp': " << inputType;
            log<software_critical>({__FILE__, __LINE__, errno, oss.str()});
            return -1;
          }

          // If keyword exists, it takes precedence
          m_inputType = inputType;
          updateIfChanged(m_indiP_input, "type", m_inputType);
        }
        kwn++;
      }

      if(!kw_found) 
      {
        // // If imageStream doesn't have an inputType keyword, destroy it        
        // std::ostringstream oss;
        // oss << "Shmim '" << shmimMonitor::m_shmimName << "' doesn't have an inputType keyword specifying its data type. Destroying it." << std::endl;
        // log<software_warning>({__FILE__, __LINE__, errno, oss.str()});      

        // ImageStreamIO_destroyIm(&m_imageStream);
        
        // if (createStream() < 0) {
        //   log<software_error>({__FILE__, __LINE__});
        //   return -1;
        // }

        std::ostringstream oss;
        oss << "No inputType keyword found for shmim '" << shmimMonitor::m_shmimName << ". Defaulting to pre-set input type: " << m_inputType << std::endl;
        log<software_warning>({__FILE__, __LINE__, errno, oss.str()});           
      }

      return 0;
    }

    int fsmCtrl::createStream() { 
      m_dataType = 9; // _DATATYPE_FLOAT
      uint32_t imsize[3] = {height, width, 0};

      // Not found, create it
      ImageStreamIO_createIm_gpu(&m_imageStream, m_shmimName.c_str(), 2, imsize, m_dataType, -1, 1, IMAGE_NB_SEMAPHORE, 1, CIRCULAR_BUFFER | ZAXIS_TEMPORAL, 0);

      // Set name of first keyword is 'inputType'
      strncpy(m_imageStream.kw[0].name, kw_name.c_str(), sizeof(m_imageStream.kw[0].name));
      // Ensure null termination
      m_imageStream.kw[0].name[sizeof(m_imageStream.kw[0].name) - 1] = '\0';
      // Set type
      m_imageStream.kw[0].type = 'S';
      // Set keyword value
      strncpy(m_imageStream.kw[0].value.valstr, m_inputType.c_str(), sizeof(m_imageStream.kw[0].value.valstr));
      // Ensure null termination
      m_imageStream.kw[0].value.valstr[sizeof(m_imageStream.kw[0].value.valstr) - 1] = '\0';

      std::ostringstream oss;
      oss << "Created: " << m_shmimName << std::endl;
      log<text_log>(oss.str());

      return 0;
    }

    ////////////////////
    // INDI CALLBACKS
    ////////////////////

    // callback from setting m_indiP_val1
    // only 'target' is editable ('current' should be updated by code)
    INDI_NEWCALLBACK_DEFN(fsmCtrl, m_indiP_val1)
    (const pcf::IndiProperty &ipRecv)
    {
      INDI_VALIDATE_CALLBACK_PROPS(m_indiP_val1, ipRecv);

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

      // Value only settable via INDI if FSM in READY state
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
          dacs[0] = vi_to_daci(target, m_v);
        }
        else if (m_inputType == TTP)
        {
          dacs[0] = ttp_to_dac1(target, d_piston, m_B, D_per_V, m_v);
        }

        dacs[1] = m_dac2;
        dacs[2] = m_dac3;

        std::ostringstream oss;
        oss << "INDI dacs callback: " << dacs[0] << " | " << dacs[1] << " | " << dacs[2];
        log<text_log>(oss.str());

        return setDacs(dacs);
      }
    }

    // callback from setting m_indiP_val2
    // only 'target' is editable ('current' should be updated by code)
    INDI_NEWCALLBACK_DEFN(fsmCtrl, m_indiP_val2)
    (const pcf::IndiProperty &ipRecv)
    {
      INDI_VALIDATE_CALLBACK_PROPS(m_indiP_val2, ipRecv);
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

      // Value only settable via INDI if FSM in READY state
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
          dacs[1] = vi_to_daci(target, m_v);
        }
        else if (m_inputType == TTP)
        {
          // Get current alpha and z to calculate dac2 from the target
          double tip = daci_to_tip(m_dac1, m_dac2, m_dac3, m_B, D_per_V, m_v);
          dacs[1] = ttp_to_dac2(tip, target, d_piston, m_B, m_L, D_per_V, m_v);
        }

        dacs[2] = m_dac3;

        std::ostringstream oss;
        oss << "INDI dacs callback: " << dacs[0] << " | " << dacs[1] << " | " << dacs[2];
        log<text_log>(oss.str());

        return setDacs(dacs);
      }
    }

    // callback from setting m_indiP_val3
    // only 'target' is editable ('current' should be updated by code)
    INDI_NEWCALLBACK_DEFN(fsmCtrl, m_indiP_val3)
    (const pcf::IndiProperty &ipRecv)
    {
      INDI_VALIDATE_CALLBACK_PROPS(m_indiP_val3, ipRecv);
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

      // Value only settable via INDI if FSM in READY state
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
          dacs[2] = vi_to_daci(target, m_v);
        }
        else if (m_inputType == TTP)
        {
          // Get current tip and beta to calculate dac3 from the target
          double tip = daci_to_tip(m_dac1, m_dac2, m_dac3, m_B, D_per_V, m_v);
          double tilt = daci_to_tilt(m_dac2, m_dac3, m_L, D_per_V, m_v);
          dacs[2] = ttp_to_dac3(tip, tilt, target, m_B, m_L, D_per_V, m_v);
        }

        std::ostringstream oss;
        oss << "INDI dacs callback: " << dacs[0] << " | " << dacs[1] << " | " << dacs[2];
        log<text_log>(oss.str());

        return setDacs(dacs);
      }
    }

    // callback from setting conversion_factors
    INDI_NEWCALLBACK_DEFN(fsmCtrl, m_indiP_conversion_factors)
    (const pcf::IndiProperty &ipRecv)
    {
      INDI_VALIDATE_CALLBACK_PROPS(m_indiP_conversion_factors, ipRecv);
      if (ipRecv.find("B"))
      {
        m_B = ipRecv["B"].get<float>();
        updateIfChanged(m_indiP_conversion_factors, "B", m_B);
      }

      if (ipRecv.find("L"))
      {
        m_L = ipRecv["L"].get<float>();
        updateIfChanged(m_indiP_conversion_factors, "L", m_L);
      }

      if (ipRecv.find("v"))
      {
        m_v = ipRecv["v"].get<float>();
        updateIfChanged(m_indiP_conversion_factors, "v", m_v);
      }

      if (ipRecv.find("voltage_max"))
      {
        m_v = ipRecv["voltage_max"].get<float>();
        updateIfChanged(m_indiP_conversion_factors, "voltage_max", m_voltage_max);
      }

      if (ipRecv.find("stroke_max"))
      {
        m_v = ipRecv["stroke_max"].get<float>();
        updateIfChanged(m_indiP_conversion_factors, "stroke_max", m_stroke_max);
      }

      std::ostringstream oss;
      oss << "INDI conversion_factors callback: " << m_B << " | " << m_L << " | " << m_v << " | " << m_voltage_max << " | " << m_stroke_max;
      log<text_log>(oss.str());
    }

    // callback from setting m_indiP_input (dacs, voltages, ttp)
    INDI_NEWCALLBACK_DEFN(fsmCtrl, m_indiP_input)
    (const pcf::IndiProperty &ipRecv)
    {
      INDI_VALIDATE_CALLBACK_PROPS(m_indiP_input, ipRecv);
      if (ipRecv.find("type"))
      {
        std::string type = ipRecv["type"].get<std::string>();
        if (!(type == DACS || type == VOLTAGES || type == TTP))
        {
          std::ostringstream oss;
          oss << "input.type '" << type << "' not dacs, voltages or ttp";
          log<software_critical>({__FILE__, __LINE__, errno, oss.str()});
          return -1;
        }

        m_inputType = type;
        updateIfChanged(m_indiP_input, "type", m_inputType);

        if (state() == stateCodes::READY)
        {
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
          updateIfChanged(m_indiP_input, "toggle", toggle);
        }
        if (toggle == INDI)
        {
          state(stateCodes::READY);
          updateIfChanged(m_indiP_input, "toggle", toggle);
        }

        std::ostringstream oss;
        oss << "INDI input toggle: " << m_inputToggle;
        log<text_log>(oss.str());
      }
    }

    // callback from setting m_indiP_dac1 (min, max)
    INDI_NEWCALLBACK_DEFN(fsmCtrl, m_indiP_dac1)
    (const pcf::IndiProperty &ipRecv)
    {
      INDI_VALIDATE_CALLBACK_PROPS(m_indiP_dac1, ipRecv);

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

    // callback from setting m_indiP_dac2 (min, max)
    INDI_NEWCALLBACK_DEFN(fsmCtrl, m_indiP_dac2)
    (const pcf::IndiProperty &ipRecv)
    {
      INDI_VALIDATE_CALLBACK_PROPS(m_indiP_dac2, ipRecv);

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

    // callback from setting m_indiP_dac3 (min, max)
    INDI_NEWCALLBACK_DEFN(fsmCtrl, m_indiP_dac3)
    (const pcf::IndiProperty &ipRecv)
    {
      INDI_VALIDATE_CALLBACK_PROPS(m_indiP_dac3, ipRecv);

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

    // // callback from setting m_indiP_adc1 - not a settable param
    // INDI_NEWCALLBACK_DEFN(fsmCtrl, m_indiP_adc1)
    // (const pcf::IndiProperty &ipRecv)
    // {
    //   log<text_log>("INDI callback.");
    //   return 0;
    // }

    // // callback from setting m_indiP_adc2 - not a settable param
    // INDI_NEWCALLBACK_DEFN(fsmCtrl, m_indiP_adc2)
    // (const pcf::IndiProperty &ipRecv)
    // {
    //   log<text_log>("INDI callback.");
    //   return 0;
    // }

    // // callback from setting m_indiP_adc3 - not a settable param
    // INDI_NEWCALLBACK_DEFN(fsmCtrl, m_indiP_adc3)
    // (const pcf::IndiProperty &ipRecv)
    // {
    //   log<text_log>("INDI callback.");
    //   return 0;
    // }

    // callback from setting m_indiP_query - trigger adc or dac query
    INDI_NEWCALLBACK_DEFN(fsmCtrl, m_indiP_query)
    (const pcf::IndiProperty &ipRecv)
    {
      INDI_VALIDATE_CALLBACK_PROPS(m_indiP_query, ipRecv);

      if (ipRecv.find("query"))
      {
        std::string query_obj = ipRecv["query"].get<std::string>();
        if (query_obj == "adc")
        {
          log<text_log>("INDI query ADCs.");
          query(adcsQuery);
          updateIfChanged(m_indiP_query, "query", "adc");
        }
        else if (query_obj == "dac")
        {
          log<text_log>("INDI query ADCs.");
          query(dacsQuery);
          updateIfChanged(m_indiP_query, "query", "dac");
        }
        else
        {
          log<text_log>("INDI query of unknown.");
          updateIfChanged(m_indiP_query, "query", "none");
        }
      }
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
        val1 = daci_to_vi(m_dac1, m_v);
        val2 = daci_to_vi(m_dac2, m_v);
        val3 = daci_to_vi(m_dac3, m_v);
      }
      else if (m_inputType == TTP)
      {
        val1 = daci_to_tip(m_dac1, m_dac2, m_dac3, m_B, D_per_V, m_v);
        val2 = daci_to_tilt(m_dac2, m_dac3, m_L, D_per_V, m_v);
        val3 = d_piston;
      }

      updateIfChanged(m_indiP_val1, "current", val1);
      updateIfChanged(m_indiP_val2, "current", val2);
      updateIfChanged(m_indiP_val3, "current", val3);
    }

  } // namespace app
} // namespace MagAOX