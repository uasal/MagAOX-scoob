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
// #include "binaryUart.hpp"
// #include "cGraphPacket.hpp"
// #include "linux_pinout_client_socket.hpp"
// #include "linux_pinout_uart.hpp"
// #include "socket.hpp"
// #include "IUart.h"

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
    class fsmCtrl : public MagAOXApp<true>, public dev::telemeter<fsmCtrl>, public dev::shmimMonitor<fsmCtrl>, public dev::summerDevice<fsmCtrl>
    {

      // Give the test harness access.
      friend class fsmCtrl_test;

      friend class dev::telemeter<fsmCtrl>;
      typedef dev::telemeter<fsmCtrl> telemeterT;

      friend class dev::shmimMonitor<fsmCtrl>;

      friend class dev::summerDevice<fsmCtrl>;

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

      enum class InputType : uint8_t { DACS, VOLTAGES, TTP };
      ///@}


      /** \name Configurable Parameters
       *@{
       */
      // // Connection parameters
      // std::string type;
      // std::string PortName;
      // int nHostPort = 66873; // 65536 + 1337 ; socket-specific
      // uint32_t BaudRate = 115200; // serial-port-specific

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
      uint32_t width = 1; // shm size
      uint32_t height = 3; // shm size

      // input parameters
      std::string kw_name = "inputType";
      InputType m_inputType{InputType::VOLTAGES}; ///< The type of values in the shmim (dacs, voltages, or ttp)
      std::string m_inputToggle;

      // INDI update throttle
      uint32_t m_indiUpdateInterval{100}; ///< Update INDI target values every Nth shmim frame. Configurable via input.indi_update_interval.

      // here add parameters which will be config-able at runtime
      ///@}


      uint32_t m_indiSkipCounter{0};      ///< Frame counter for INDI update throttling

      uint32_t targetSetpoints[3];

      double m_dac1{0};
      double m_dac2{0};
      double m_dac3{0};

      double m_adc1{0};
      double m_adc2{0};
      double m_adc3{0};

    private:
      std::unique_ptr<dev::sdevQuery> telemetryQuery = std::make_unique<TelemetryQuery>();
      std::unique_ptr<dev::sdevQuery> adcsQuery = std::make_unique<AdcsQuery>();
      std::unique_ptr<dev::sdevQuery> dacsQuery = std::make_unique<DacsQuery>();
      std::vector<dev::sdevQuery*> customQueries = { telemetryQuery.get(), adcsQuery.get(), dacsQuery.get() };

      /// Cached typed pointers to avoid per-frame dynamic_cast
      DacsQuery *m_dacsQ = static_cast<DacsQuery *>(dacsQuery.get());
      AdcsQuery *m_adcsQ = static_cast<AdcsQuery *>(adcsQuery.get());
      TelemetryQuery *m_telemetryQ = static_cast<TelemetryQuery *>(telemetryQuery.get());

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

      const std::vector<dev::sdevQuery*>& getQueries() const override;

      /** \name FSM-specific functions
       *
       * @{
       */

      /**
       * @brief Request fsm's ADC values
       *
       * Wrapper that calls query() with instance of AdcsQuery.
       * Response is stored in instance's AdcVals member.
       */
      void receiveAdcs();

      /**
       * @brief Request fsm's DAC values
       *
       * Wrapper that calls query() with instance of DacsQuery.
       * Response is stored in instance's DacSetpoints member.
       */
      void receiveDacs();

      // /**
      //  * @brief Request fsm's telemetry
      //  *
      //  * Wrapper that calls query() with instance of TelemetryQuery.
      //  * Response is stored in instance's DacSetpoints member.
      //  * Response is also logged in /opt/tele/fsmCtrl_xxxxxx.binlog
      //  */
      // void receiveTelemetry();

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

      // /**
      //  * @brief Query interface for the fsm
      //  *
      //  * Function that sends a command packet to the fsm.
      //  *
      //  * @param pztQuery pointer to a class inheriting from PZTQuery (see fsmCommands.hpp)
      //  */
      // void query(PZTQuery *);

      /**
       * @brief Function that listens for responses from the fsm
       *
       * Function that checks for a response from the fsm and processes it.
       * If a response is received it processes the response as appropriate for the
       * command sent.
       *
       * @param pztQuery pointer to a class inheriting from PZTQuery (see fsmCommands.hpp)
       */
      void receive() override;

      ///@}


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
      int processImage(void *curr_src, const dev::shmimT &sp);

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

  
      /** \name Utility functions
        *
        * @{
        */

      /**
       * @brief Convert InputType enum value to a string
      */
      const std::string & inputTypeToString(InputType type) const
      {
        switch(type)
        {
          case InputType::DACS:     
            return DACS;
          case InputType::VOLTAGES:
            return VOLTAGES;
          case InputType::TTP:
            return TTP;
        }
        return VOLTAGES;
      }

      /**
       * @brief Convert string to InputType enum value
       * 
       * \returns 0 on success, with type set to the corresponding InputType value
       * \returns -1 if the string does not match any InputType value
       */
      int inputTypeFromString(const std::string &string, InputType &type) const
      {
        if(string == DACS) 
        {
          type = InputType::DACS;
          return 0; 
        }
        if(string == VOLTAGES)
        {
          type = InputType::VOLTAGES;
          return 0;
        }
        if(string == TTP)
        {
          type = InputType::TTP;
          return 0;
        }
        
        return -1;
      }

      /**
       * @brief Utility function that sets 'current' INDI values, if updated
       *
       * Function that takes the values in m_dac1, m_dac2 and m_dac3, transforms them
       * (if necessary) to the type specified by m_inputType and updates the corresponding
       * INDI parameter's 'current' value.
       */
      void updateINDICurrentParams();
        
      ///@}
    };

    fsmCtrl::fsmCtrl() : MagAOXApp(MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED)
    {
      m_powerMgtEnabled = true;
      m_getExistingFirst = true; // get existing shmim (??? should or shouldn't)
      return;
    }

    void fsmCtrl::setupConfig()
    {
      dev::summerDevice<fsmCtrl>::setupConfig(config);
      shmimMonitor::setupConfig(config);

      config.add("parameters.period_s", "", "parameters.period_s", argType::Optional, "parameters", "period_s", false, "int", "The period of telemetry queries to the fsm.");

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
      config.add("input.indi_update_interval", "", "input.indi_update_interval", argType::Optional, "input", "indi_update_interval", false, "int", "Update INDI target values every Nth shmim frame. Defaults to 100.");
      telemeterT::setupConfig(config);
    }

    int fsmCtrl::loadConfigImpl(mx::app::appConfigurator &_config)
    {
      /// CONNECTION PARAMETERS ///
      _config(period_s, "parameters.period_s");
      log<software_info>({__FILE__, __LINE__, "Loading config"});

      /// CONVERSION PARAMETERS ///
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
      std::string inputTypeStr = VOLTAGES;
      _config(inputTypeStr, "input.type");
      if(inputTypeFromString(inputTypeStr, m_inputType) < 0)
      {
        std::ostringstream oss;
        oss << "Config file sets inputType to a value other than 'dacs', 'voltages', or 'ttp': " << inputTypeStr;
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

      /// INDI UPDATE INTERVAL WHEN RUNNING FROM SHMIM ///
      _config(m_indiUpdateInterval, "input.indi_update_interval");

      return 0;
    }

    void fsmCtrl::loadConfig()
    {
      if (loadConfigImpl(config) < 0)
      {
        log<software_critical>({__FILE__, __LINE__, "Error during config"});
        m_shutdown = true;
      }

      if (dev::summerDevice<fsmCtrl>::loadConfig(config) < 0)
      {
        log<software_critical>({__FILE__, __LINE__, "Error during summerDevice config"});
        m_shutdown = true;
      }

      if (telemeterT::loadConfig(config) < 0)
      {
        log<software_critical>({__FILE__, __LINE__, "Error during telemeter config"});
        m_shutdown = true;
      }

      if (shmimMonitor::loadConfig(config) < 0)
      {
        log<software_critical>({__FILE__, __LINE__, "Error during shmimMonitor config"});
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

      if (dev::summerDevice<fsmCtrl>::appStartup() < 0)
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
      m_indiP_input["type"] = inputTypeToString(m_inputType);

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
        // if (type == "serial_port")
        // {
        //   rv = serialPortConnect();
        // }
        // else if (type == "socket")
        // {
        //   rv = socketConnect();
        // }
        rv = dev::summerDevice<fsmCtrl>::connect();

        if (rv == 0)
        {
          state(stateCodes::CONNECTED);
        }
      }

      if (state() == stateCodes::CONNECTED)
      {
        // // Get current adc values
        dev::summerDevice<fsmCtrl>::query(adcsQuery.get());

        // // Get current dac values
        dev::summerDevice<fsmCtrl>::query(dacsQuery.get());

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

      if ((state() == stateCodes::CONNECTED) || (state() == stateCodes::OPERATING) || (state() == stateCodes::READY))
      {
        receive();

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
      dev::summerDevice<fsmCtrl>::appShutdown();

      return 0;
    }

    // //////////////
    // // CONNECTION
    // //////////////

    const std::vector<dev::sdevQuery*>& fsmCtrl::getQueries() const {
      return customQueries;
    }

    void fsmCtrl::receive() {
      dev::summerDevice<fsmCtrl>::receive();

      // Once packet had been received, make sure updates are propagated.
      // Since we don't know the packet type, update all.
      receiveAdcs();
      receiveDacs();  
    }

    //////////////
    // FSM QUERIES
    //////////////

    // Function to request fsm ADCs
    void fsmCtrl::receiveAdcs()
    {
      double samples1 = static_cast<double>(m_adcsQ->AdcVals[0].Samples);
      double samples2 = static_cast<double>(m_adcsQ->AdcVals[1].Samples);
      double samples3 = static_cast<double>(m_adcsQ->AdcVals[2].Samples);

      double numAccums1 = static_cast<double>(m_adcsQ->AdcVals[0].NumAccums);
      double numAccums2 = static_cast<double>(m_adcsQ->AdcVals[1].NumAccums);
      double numAccums3 = static_cast<double>(m_adcsQ->AdcVals[2].NumAccums);

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
      m_dac1 = static_cast<float>(m_dacsQ->DacSetpoints[0]);
      m_dac2 = static_cast<float>(m_dacsQ->DacSetpoints[1]);
      m_dac3 = static_cast<float>(m_dacsQ->DacSetpoints[2]);

      updateINDICurrentParams();
    }

    // // Function to request fsm telemetry
    // void fsmCtrl::receiveTelemetry()
    // {
    //   TelemetryQuery *castTelemetryQuery = dynamic_cast<TelemetryQuery *>(telemetryQuery);
    // }

    // Function to set fsm DACs
    int fsmCtrl::setDacs(uint32_t *Setpoints)
    {
      if (Setpoints[0] < m_dac1_min || Setpoints[0] > m_dac1_max)
      {
        std::ostringstream oss;
        oss << "Requested dac1 out of range; (min|dac1|max) : (" << m_dac1_min << "|" << Setpoints[0] << "|" << m_dac1_max << ");";
        log<software_error>({__FILE__, __LINE__, oss.str()});
        return -1;
      }

      if (Setpoints[1] < m_dac2_min || Setpoints[1] > m_dac2_max)
      {
        std::ostringstream oss;
        oss << "Requested dac2 out of range; (min|dac2|max) : (" << m_dac2_min << "|" << Setpoints[1] << "|" << m_dac2_max << ");";
        log<software_error>({__FILE__, __LINE__, oss.str()});
        return -1;
      }

      if (Setpoints[2] < m_dac3_min || Setpoints[2] > m_dac3_max)
      {
        std::ostringstream oss;
        oss << "Requested dac3 out of range; (min|dac3|max) : (" << m_dac3_min << "|" << Setpoints[2] << "|" << m_dac3_max << ");";
        log<software_error>({__FILE__, __LINE__, oss.str()});
        return -1;
      }

      if(m_log.logLevel() >= flatlogs::logPrio::LOG_DEBUG)
      {
        std::ostringstream oss;
        oss << "SETDACS: " << Setpoints[0] << " | " << Setpoints[1] << " | " << Setpoints[2];
        log<software_debug>({__FILE__, __LINE__, oss.str()});
      }

      m_dacsQ->setPayload(Setpoints, 3 * sizeof(uint32_t));
      dev::summerDevice<fsmCtrl>::query(m_dacsQ);

      // castDacsQuery->logReply();
      m_dacsQ->resetPayload();

      // m_dac1 = castDacsQuery->DacSetpoints[0];
      // m_dac2 = castDacsQuery->DacSetpoints[1];
      // m_dac3 = castDacsQuery->DacSetpoints[2];
      // updateINDICurrentParams();

      // dev::summerDevice<fsmCtrl>::query(dacsQuery);
      // dev::summerDevice<fsmCtrl>::query(adcsQuery);
      return 0;
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
      dev::summerDevice<fsmCtrl>::query(telemetryQuery.get());
      
      dev::summerDevice<fsmCtrl>::receive();
      telemetryQuery->logReply();

      return recordFsm(true);
    }

    int fsmCtrl::recordFsm(bool force)
    {
      static CGraphFSMTelemetryPayload LastTelemetry; ///< Structure holding the previous fsm voltage measurement.

      if (!(LastTelemetry == m_telemetryQ->Telemetry) || force)
      {
        LastTelemetry = m_telemetryQ->Telemetry;
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
      
      switch(m_inputType)
      {
        case InputType::TTP:
          val3 = d_piston;
          break;
        default:
          val3 = ((float *)curr_src)[2];
          break;
      }

      // Throttle INDI updates to every m_indiUpdateInterval frames
      if(++m_indiSkipCounter >= m_indiUpdateInterval)
      {
        m_indiSkipCounter = 0;
        std::unique_lock<std::mutex> lock(m_indiMutex);
        updateIfChanged(m_indiP_val1, "target", val1);
        updateIfChanged(m_indiP_val2, "target", val2);
        updateIfChanged(m_indiP_val3, "target", val3);
      }

      switch(m_inputType)
      {
        case InputType::DACS:
          dacs[0] = val1;
          dacs[1] = val2;
          dacs[2] = val3;
          break;
        case InputType::VOLTAGES:
          dacs[0] = vi_to_daci(val1, m_v);
          dacs[1] = vi_to_daci(val2, m_v);
          dacs[2] = vi_to_daci(val3, m_v);
          break;
        case InputType::TTP:
          dacs[0] = ttp_to_dac1(val1, d_piston, m_B, D_per_V, m_v);
          dacs[1] = ttp_to_dac2(val1, val2, d_piston, m_B, m_L, D_per_V, m_v);
          dacs[2] = ttp_to_dac3(val1, val2, d_piston, m_B, m_L, D_per_V, m_v);
          break;
      }

      if(m_log.logLevel() >= flatlogs::logPrio::LOG_DEBUG)
      {
        std::ostringstream oss;
        oss << "SHMIM dacs callback: " << dacs[0] << " | " << dacs[1] << " | " << dacs[2];
        log<software_debug>({__FILE__, __LINE__, oss.str()});
      }

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
      if(m_imageStream.md->size[0] != width || m_imageStream.md->size[1] != height)
      {
        std::ostringstream oss;
        oss << "Shmim '" << shmimMonitor::m_shmimName << "' has the wrong shape: width = " << m_width << ", height = " << m_height << ". Destroying it." << std::endl;
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
          InputType parsedType;
          if (inputTypeFromString(inputType, parsedType) < 0)
          {
            std::ostringstream oss;
            oss << "Shmim '" << shmimMonitor::m_shmimName << "' has an inputType keyword with a value other than 'dacs', 'voltages', or 'ttp': " << inputType;
            log<software_critical>({__FILE__, __LINE__, errno, oss.str()});
            return -1;
          }

          // If keyword exists, it takes precedence
          m_inputType = parsedType;
          updateIfChanged(m_indiP_input, "type", inputTypeToString(m_inputType));
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
        oss << "No inputType keyword found for shmim '" << shmimMonitor::m_shmimName << ". Defaulting to pre-set input type: " << inputTypeToString(m_inputType) << std::endl;
        log<software_warning>({__FILE__, __LINE__, errno, oss.str()});           
      }

      return 0;
    }

    int fsmCtrl::createStream() { 
      uint32_t imsize[3] = {width, height, 0};

      // Not found, create it
      // _DATATYPE_FLOAT = 9
      // MATH_DATA = 2
      ImageStreamIO_createIm_gpu(&m_imageStream, m_shmimName.c_str(), 2, imsize, _DATATYPE_FLOAT, -1, 1, IMAGE_NB_SEMAPHORE, 1, MATH_DATA, 0);

      // Set name of first keyword is 'inputType'
      snprintf(m_imageStream.kw[0].name, sizeof(m_imageStream.kw[0].name), "%s", kw_name.c_str());
      // Set type
      m_imageStream.kw[0].type = 'S';
      // Set keyword value
      strncpy(m_imageStream.kw[0].value.valstr, inputTypeToString(m_inputType).c_str(), sizeof(m_imageStream.kw[0].value.valstr));
      // Ensure null termination
      m_imageStream.kw[0].value.valstr[sizeof(m_imageStream.kw[0].value.valstr) - 1] = '\0';

      std::ostringstream oss;
      oss << "Created: " << m_shmimName << std::endl;
      log<software_info>({__FILE__, __LINE__, oss.str()});

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

        switch(m_inputType)
        {
          case InputType::DACS:
            dacs[0] = target;
            break;
          case InputType::VOLTAGES:
            dacs[0] = vi_to_daci(target, m_v);
            break;
          case InputType::TTP:
            dacs[0] = ttp_to_dac1(target, d_piston, m_B, D_per_V, m_v);
            break;
        }

        dacs[1] = m_dac2;
        dacs[2] = m_dac3;

        std::ostringstream oss;
        oss << "INDI dacs callback: " << dacs[0] << " | " << dacs[1] << " | " << dacs[2];
        log<software_info>({__FILE__, __LINE__, oss.str()});

        return setDacs(dacs);
      }

      return 0;
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

        switch(m_inputType)
        {
          case InputType::DACS:
            dacs[1] = target;
            break;
          case InputType::VOLTAGES:
            dacs[1] = vi_to_daci(target, m_v);
            break;
          case InputType::TTP:
          {
            // Get current alpha and z to calculate dac2 from the target
            double tip = daci_to_tip(m_dac1, m_dac2, m_dac3, m_B, D_per_V, m_v);
            dacs[1] = ttp_to_dac2(tip, target, d_piston, m_B, m_L, D_per_V, m_v);
            break;
          }
        }

        dacs[2] = m_dac3;

        std::ostringstream oss;
        oss << "INDI dacs callback: " << dacs[0] << " | " << dacs[1] << " | " << dacs[2];
        log<software_info>({__FILE__, __LINE__, oss.str()});

        return setDacs(dacs);
      }

      return 0;
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

        switch(m_inputType)
        {
          case InputType::DACS:
            dacs[2] = target;
            break;
          case InputType::VOLTAGES:
            dacs[2] = vi_to_daci(target, m_v);
            break;
          case InputType::TTP:
          {
            // Get current tip and beta to calculate dac3 from the target
            double tip = daci_to_tip(m_dac1, m_dac2, m_dac3, m_B, D_per_V, m_v);
            double tilt = daci_to_tilt(m_dac2, m_dac3, m_L, D_per_V, m_v);
            dacs[2] = ttp_to_dac3(tip, tilt, target, m_B, m_L, D_per_V, m_v);
            break;
          }
        }

        std::ostringstream oss;
        oss << "INDI dacs callback: " << dacs[0] << " | " << dacs[1] << " | " << dacs[2];
        log<software_info>({__FILE__, __LINE__, oss.str()});

        return setDacs(dacs);
      }

      return 0;
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
      log<software_info>({__FILE__, __LINE__, oss.str()});

      return 0;
    }

    // callback from setting m_indiP_input (dacs, voltages, ttp)
    INDI_NEWCALLBACK_DEFN(fsmCtrl, m_indiP_input)
    (const pcf::IndiProperty &ipRecv)
    {
      INDI_VALIDATE_CALLBACK_PROPS(m_indiP_input, ipRecv);
      if (ipRecv.find("type"))
      {
        std::string type = ipRecv["type"].get<std::string>();
        InputType parsedType;
        if (inputTypeFromString(type, parsedType) < 0)
        {
          std::ostringstream oss;
          oss << "input.type '" << type << "' not dacs, voltages or ttp";
          log<software_critical>({__FILE__, __LINE__, errno, oss.str()});
          return -1;
        }

        m_inputType = parsedType;
        updateIfChanged(m_indiP_input, "type", inputTypeToString(m_inputType));

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
        oss << "INDI input type callback: " << inputTypeToString(m_inputType);
        log<software_info>({__FILE__, __LINE__, oss.str()});
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
        log<software_info>({__FILE__, __LINE__, oss.str()});
      }

      return 0;
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

      log<software_info>({__FILE__, __LINE__, oss.str()});

      return 0;
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

      log<software_info>({__FILE__, __LINE__, oss.str()});

      return 0;
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

      log<software_info>({__FILE__, __LINE__, oss.str()});

      return 0;
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
          log<software_info>({__FILE__, __LINE__, "INDI query ADCs."});
          dev::summerDevice<fsmCtrl>::query(adcsQuery.get());
          updateIfChanged(m_indiP_query, "query", "adc");
        }
        else if (query_obj == "dac")
        {
          log<software_info>({__FILE__, __LINE__, "INDI query DACs."});
          dev::summerDevice<fsmCtrl>::query(dacsQuery.get());
          updateIfChanged(m_indiP_query, "query", "dac");
        }
        else
        {
          log<software_warning>({__FILE__, __LINE__, "INDI query of unknown."});
          updateIfChanged(m_indiP_query, "query", "none");
        }
      }

      return 0;
    }

    /////////
    // UTILS
    /////////

    void fsmCtrl::updateINDICurrentParams()
    {
      float val1, val2, val3;

      switch(m_inputType)
      {
        case InputType::DACS:
          val1 = m_dac1;
          val2 = m_dac2;
          val3 = m_dac3;
          break;
        case InputType::VOLTAGES:
          val1 = daci_to_vi(m_dac1, m_v);
          val2 = daci_to_vi(m_dac2, m_v);
          val3 = daci_to_vi(m_dac3, m_v);
          break;
        case InputType::TTP:
          val1 = daci_to_tip(m_dac1, m_dac2, m_dac3, m_B, D_per_V, m_v);
          val2 = daci_to_tilt(m_dac2, m_dac3, m_L, D_per_V, m_v);
          val3 = d_piston;
          break;
      }

      updateIfChanged(m_indiP_val1, "current", val1);
      updateIfChanged(m_indiP_val2, "current", val2);
      updateIfChanged(m_indiP_val3, "current", val3);
    }

  } // namespace app
} // namespace MagAOX