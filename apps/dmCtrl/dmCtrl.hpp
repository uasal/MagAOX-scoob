/** \file dmCtrl.hpp
  * \brief The MagAO-X XXXXXX header file
  *
  * \ingroup dmCtrl_files
  */

#ifndef dmCtrl_hpp
#define dmCtrl_hpp

#include <filesystem>

#include <mx/ioutils/fits/fitsFile.hpp>
#include "../../libMagAOX/libMagAOX.hpp" //Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"

typedef MagAOX::app::MagAOXApp<true> MagAOXAppT; // This needs to be before the other header files for logging to work in other headers

#include "dmCommands.hpp"



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
    class dmCtrl : public MagAOXApp<true>, public dev::telemeter<dmCtrl>, public dev::dm<dmCtrl,float>, public dev::shmimMonitor<dmCtrl>, public dev::summerDevice<dmCtrl>
    {

      //Give the test harness access.
      friend class dmCtrl_test;

      friend class dev::dm<dmCtrl,float>;
      friend class dev::telemeter<dmCtrl>;
      friend class dev::shmimMonitor<dmCtrl>;
      friend class dev::summerDevice<dmCtrl>;

      typedef float realT;  ///< This defines the datatype used to signal the DM using the ImageStreamIO library.
      
      typedef dev::telemeter<dmCtrl> telemeterT;
      typedef dev::dm<dmCtrl,float> dmT;
      typedef dev::shmimMonitor<dmCtrl> shmimMonitorT;
      typedef dev::summerDevice<dmCtrl> summerDeviceT;

    protected:
      /** \name Constants
       *@{
       */
        const std::string USB0 = "/dev/ttyUSB0";
        const std::string FITS = ".fits";
        const std::string SHORT = "short";
        const std::string LONG = "long";
        const std::string DITHER = "dither";
      ///@}

      /** \name Configurable Parameters
         *@{
         */
        std::string m_shmim_map_filename = "actuator_mapping.fits"; ///< Filename of fits image containing the mapping from 2D grid position to linear index in the command vector; must exist in the calibPath (or calibRelDir if set)
        std::string m_dm_map_filename = ""; ///< Request the pixel array to actuator map from the DM. If false, a map will be sent to the DM.
        std::string m_mode = ""; ///< Operating mode of the DM. Takes one value from: "short", "long", "dither"

        // Telemeter callback parameters
        int period_s;

      ///@}

      std::unique_ptr<dev::sdevQuery> versionQuery = std::make_unique<dev::VersionQuery>();
      std::unique_ptr<dev::sdevQuery> telemetryQuery = std::make_unique<TelemetryQuery>();
      std::unique_ptr<dev::sdevQuery> shortPixelQuery = std::make_unique<ShortPixelsQuery>();
      std::unique_ptr<dev::sdevQuery> longPixelQuery = std::make_unique<LongPixelsQuery>();
      std::unique_ptr<dev::sdevQuery> ditherPixelQuery = std::make_unique<DitherQuery>();
      std::unique_ptr<dev::sdevQuery> mappingQuery = std::make_unique<MappingQuery>();
      std::vector<dev::sdevQuery*> customQueries = { telemetryQuery.get(), versionQuery.get(), shortPixelQuery.get(), longPixelQuery.get(), ditherPixelQuery.get(), mappingQuery.get() };

      // Cached typed pointers to avoid per-frame dynamic_cast in send_array;
      // lifetime is managed by the unique_ptrs above.
      ShortPixelsQuery *m_shortQ = static_cast<ShortPixelsQuery *>(shortPixelQuery.get());
      LongPixelsQuery *m_longQ = static_cast<LongPixelsQuery *>(longPixelQuery.get());
      DitherQuery *m_ditherQ = static_cast<DitherQuery *>(ditherPixelQuery.get());

      // INDI properties
      pcf::IndiProperty m_indiP_mode;

    public:
      INDI_NEWCALLBACK_DECL(dmCtrl, m_indiP_mode);

      /// Default c'tor.
      dmCtrl();

      /// D'tor
      ~dmCtrl() noexcept = default;

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

      const std::vector<dev::sdevQuery*>& getQueries() const override;

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

      /** \name DM Template Interface
         * (The DM template implements the shmim interface, so that is not needed here.)
         *
         *@{
        */

      protected:
        double m_act_gain {0}; ///< Actuator gain (microns/volt)
        double m_volume_factor {0}; ///< the volume factor to convert from displacement to commands
        double m_volumeOverGain {0}; ///< Pre-computed m_volume_factor / m_act_gain, updated when either changes
        uint32_t m_nbAct {DMMaxActuators}; ///< The number of actuators

        std::vector<int> m_actuator_mapping;///< Vector containing the mapping from 2D grid position to linear index in the command vector
        std::vector<double> m_dminputs; ///< Pre-allocated command vector, only used in commandDM

        long m_satThresh {100000} ;///< Threshold above which to log saturation.
        long m_nsat {0};

        // Not using this (yet?). The DM doesn't have a handle
        // bool m_dmopen {false}; ///< Track whether the DM connection has been opened
  
        public:
        
        /// Read the shmim to pixel mapping from a FITS file
        /**
           * \returns 0 on success
           * \returns -1 on error
           */
          int get_shmim_to_pixel_mapping();
        
        
        /// Read the pixel to DM actuators mapping from file
        /**
           * Sets values on map_lut
           * 
           * TODO: Do we ever want to send only a subset of the mapping?
           * If yes, we need to also read in startPixel (from the file or config) 
           * and request existing mapping from the DM first.
           * 
           * \returns 0 on success
           * \returns -1 on error
           */
          int get_array_to_actuator_mapping(CGraphDMMappings &map_lut);
        
        /// Send the array of values to the DM
        /**
           * \returns 0 on success
           * \returns -1 on error
           */
          int send_array(const std::vector<double> &inputs, uint16_t nbInputs, uint16_t startPixel);

        /// Initialize the DM and prepare for operation.
        /** Application is in state OPERATING upon successful conclusion.
         *
         * @brief Required by DM Interface
         * 
         * \returns 0 on success
         * \returns -1 on error
         */
        int initDM();

        /// Zero all commands on the DM
        /** This does not update the shared memory buffer.
          *
          * @brief Required by DM Interface
          * 
          * \returns 0 on success
          * \returns -1 on error
          */
        int zeroDM();
    
        /// Send a command to the DM
        /** This is called by the shmim monitoring thread in response to a semaphore trigger.
          *
          *  @brief Required by DM Interface
          * 
          * \returns 0 on success
          * \returns -1 on error
          */
        int commandDM(void * curr_src);
    
        /// Release the DM, making it safe to turn off power.
        /** The application will be state READY at the conclusion of this.
          *
          *  @brief Required by DM Interface
          * 
          * \returns 0 on success
          * \returns -1 on error
          */
        int releaseDM();

        /** \name Other methods
         *
         *@{
        */
    };

    dmCtrl::dmCtrl() : MagAOXApp(MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED)
    {
      m_powerMgtEnabled = true;
      m_getExistingFirst = true; // get existing shmim (??? should or shouldn't)
      return;
    }

    void dmCtrl::setupConfig()
    {
      summerDeviceT::setupConfig(config);
      shmimMonitorT::setupConfig(config);

      config.add("parameters.period_s", "", "parameters.period_s", argType::Optional, "parameters", "period_s", false, "int", "The period of telemetry queries to the dm.");
      config.add("dm.calibRelDir", "", "dm.calibRelDir", argType::Optional, "dm", "calibRelDir", false, "string", "Used to find the default config directory.");
      config.add("dm.satThresh", "", "dm.satThresh", argType::Required, "dm", "satThresh", false, "string", "Threshold above which to log saturation.");
      config.add("dm.mapFilename", "", "dm.mapFilename", argType::Required, "dm", "mapFilename", false, "string", "The filename of fits image containing the mapping from 2D grid position to linear index in the command vector. Must exist in the calibPath (or calibRelDir if set).");
      config.add("dm.dmMapFilename", "", "dm.dmMapFilename", argType::Optional, "dm", "dmMapFilename", false, "string", "The filename of file containing the mapping from the pixel array to actuator map from the DM. If empty, a map will be requested from the DM.");
      config.add("dm.mode", "", "dm.mode", argType::Required, "dm", "mode", false, "string", "Operating mode of the DM. Takes one value from: 'short', 'long', 'dither'.");
      config.add("dm.actuatorGain", "", "dm.actuatorGain", argType::Required, "dm", "actuatorGain", false, "float", "Actuator gain (microns/volt).");
      config.add("dm.volumeFactor", "", "dm.volumeFactor", argType::Required, "dm", "volumeFactor", false, "float", "The volume factor to convert from displacement to commands.");
      
      dev::dm<dmCtrl,float>::setupConfig(config);
      telemeterT::setupConfig(config);
    }

    int dmCtrl::loadConfigImpl( mx::app::appConfigurator & _config )
    {
      log<software_info>({__FILE__, __LINE__, "Loading config"});
      
      /// CONNECTION PARAMETERS ///
      config(period_s, "parameters.period_s");
      
      /// DM PARAMETERS ///
      config(m_calibRelDir, "dm.calibRelDir");
      config(m_shmim_map_filename, "dm.mapFilename");
      config(m_dm_map_filename, "dm.dmMapFilename");
      config(m_satThresh, "dm.satThresh");

      config(m_mode, "dm.mode");
      if (!(m_mode == SHORT || m_mode == LONG || m_mode == DITHER))
      {
        log<software_critical>({__FILE__, __LINE__, errno, "The provided mode, " + m_mode + ", is not a valid option. Valid options are: 'short', 'long', 'dither'."});
        return -1;
      }

      config(m_act_gain, "dm.actuatorGain");
      config(m_volume_factor, "dm.volumeFactor");
      if (m_act_gain != 0)
      {
        m_volumeOverGain = m_volume_factor / m_act_gain;
      }
      
      // If map_filename is shorter than FITS or doesn't end with FITS, it is not a valid fits file.
      if ((m_shmim_map_filename.length() < FITS.length()) || (0 != m_shmim_map_filename.compare (m_shmim_map_filename.length() - FITS.length(), FITS.length(), FITS)))
      {
        std::ostringstream oss;
        oss << "The provided map_filename, " << m_shmim_map_filename << ", is not a valid fits file.";
        log<software_critical>({__FILE__, __LINE__, errno, oss.str()});
        return -1;
      }
      
      dev::dm<dmCtrl,float>::loadConfig(_config);
      m_shmim_map_filename = m_calibPath + "/" + m_shmim_map_filename;

      if (!m_dm_map_filename.empty())
      {
        m_dm_map_filename = m_calibPath + "/" + m_dm_map_filename;
      }

      return 0;
    }

    void dmCtrl::loadConfig()
    {
      if (loadConfigImpl(config) < 0)
      {
        log<software_critical>({__FILE__, __LINE__, "Error during config"});
        m_shutdown = true;
      }

      if (summerDeviceT::loadConfig(config) < 0)
      {
        log<software_critical>({__FILE__, __LINE__, "Error during summerDevice config"});
        m_shutdown = true;
      }

      if (telemeterT::loadConfig(config) < 0)
      {
        log<software_critical>({__FILE__, __LINE__, "Error during telemeter config"});
        m_shutdown = true;
      }

      if (shmimMonitorT::loadConfig(config) < 0)
      {
        log<software_critical>({__FILE__, __LINE__, "Error during shmimMonitor config"});
        m_shutdown = true;
      }
    }

    int dmCtrl::appStartup()
    {
      if (telemeterT::appStartup() < 0)
      {
        return log<software_error, -1>({__FILE__, __LINE__});
      }

      if (shmimMonitorT::appStartup() < 0)
      {
        return log<software_error, -1>({__FILE__, __LINE__});
      }

      if (dev::dm<dmCtrl,float>::appStartup() < 0)
      {
        return log<software_error, -1>({__FILE__, __LINE__});
      }

      if (summerDeviceT::appStartup() < 0)
      {
        return log<software_error, -1>({__FILE__, __LINE__});
      }

      REG_INDI_NEWPROP(m_indiP_mode, "mode", pcf::IndiProperty::Text);
      m_indiP_mode.add(pcf::IndiElement("current"));
      m_indiP_mode.add(pcf::IndiElement("target"));
      m_indiP_mode["current"] = m_mode;
      m_indiP_mode["target"] = m_mode;

      return 0;
    }

    int dmCtrl::appLogic()
    {
      if (dev::dm<dmCtrl,float>::appLogic() < 0)
      {
        return log<software_error, -1>({__FILE__, __LINE__});
      }

      if (shmimMonitorT::appLogic() < 0)
      {
        return log<software_error, -1>({__FILE__, __LINE__});
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
        rv = summerDeviceT::connect();

        if (rv == 0)
        {
          state(stateCodes::CONNECTED);
        }
      }

      if (state() == stateCodes::CONNECTED)
      {
        int rv = initDM();

        if (rv == 0)
        {
          // shmimMonitor executes processImage as long as the state is OPERATING.
          // The DM template implements shmimMonitor's processImage to execute commandDM.
          state(stateCodes::READY);
        }
      }

      if (state() == stateCodes::OPERATING)
      {
        // Test setpoints
        // uint16_t setpoints[10] = {100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};
        // uint16_t setpointsLen = sizeof(setpoints);
        // uint16_t startPixel = 50;

        // log<text_log>("Sending setpoints to dm");

        // ShortPixelsQuery *castShortPixelQuery = dynamic_cast<ShortPixelsQuery *>(shortPixelQuery);
        // castShortPixelQuery->setPayload(setpoints, setpointsLen, startPixel);

        // dev::summerDevice<dmCtrl>::query(castShortPixelQuery);
      
        // dev::summerDevice<dmCtrl>::receive();

        sleep(10);

        // LongPixelsQuery *castLongPixelQuery = dynamic_cast<LongPixelsQuery *>(longPixelQuery);
        // castLongPixelQuery->setPayload(setpoints, setpointsLen, startPixel);

        // dev::summerDevice<dmCtrl>::query(castLongPixelQuery);
      
        // dev::summerDevice<dmCtrl>::receive();

        if(m_nsat > m_satThresh)
        {
            log<software_warning>({__FILE__, __LINE__, "Saturated actuators in last second: " + std::to_string(m_nsat)});
        }

        m_nsat = 0;
      }

      if ((state() == stateCodes::CONNECTED) || (state() == stateCodes::OPERATING) || (state() == stateCodes::READY))
      {
        if (telemeterT::appLogic() < 0)
        {
          log<software_error>({__FILE__, __LINE__});
          return 0;
        }
      }

      return 0;
    }

    int dmCtrl::appShutdown()
    {
      telemeterT::appShutdown();
      shmimMonitorT::appShutdown();
      summerDeviceT::appShutdown();

      return 0;
    }

    //////////////
    // CONNECTION
    //////////////

    const std::vector<dev::sdevQuery*>& dmCtrl::getQueries() const {
      return customQueries;
    }

    void dmCtrl::receive() {
      summerDeviceT::receive();
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
      summerDeviceT::query(telemetryQuery.get());
      
      summerDeviceT::receive();
      telemetryQuery->logReply();

      return recordDM(true);
    }

    int dmCtrl::recordDM(bool force)
    {
      static CGraphDMTelemetryPayload LastTelemetry; ///< Structure holding the previous dm voltage measurement.

      if (auto telemetryQueryPtr = dynamic_cast<TelemetryQuery *>(telemetryQuery.get())) {
        if (!(LastTelemetry == telemetryQueryPtr->Telemetry) || force)
        {
          LastTelemetry = telemetryQueryPtr->Telemetry;
          telem<telem_dm>({LastTelemetry.P1V2, LastTelemetry.P2V2, LastTelemetry.P28V, LastTelemetry.P2V5, LastTelemetry.P6V, LastTelemetry.P5V, LastTelemetry.P3V3D, LastTelemetry.P4V3, LastTelemetry.P2I2, LastTelemetry.P4I3, LastTelemetry.P6I});
        }
      } else {
        log<software_error>({__FILE__, __LINE__, "Query casting failed."});
        return -1;
      }

      return 0;
    }


    /////////////////////////
    // DM INTERFACE
    /////////////////////////

    int dmCtrl::initDM()
    {
      //  // enable high resolution mode (dithering filter)
      //  ret = BMC_PCIeEnableHighRes(&m_dm, 1);
      //  if(ret != NO_ERR)
      //  {
      //     const char *err;
      //     err = BMCErrorString(ret);
      //     log<text_log>(std::string("Enabling high resolution (pseudo 16-bit) mode failed: ") + err, logPrio::LOG_ERROR);
      //  }
      //  log<text_log>("BMC high resolution mode enabled", logPrio::LOG_NOTICE);

      CGraphDMMappings map_lut;
      uint16_t startPixel = 0;
      uint16_t payloadLen = static_cast<uint16_t>(m_nbAct * sizeof(CGraphDMMappingPayload));

      // Prepare to request or send mapping by casting mappingQuery to the correct type
      if (auto castMappingQuery = dynamic_cast<MappingQuery *>(mappingQuery.get())) {
        // If no map file provided, query the DM for the mapping
        if (m_dm_map_filename.empty()) {
          log<software_info>({__FILE__, __LINE__, "Querying DM for mapping."});
          castMappingQuery->setPayload(map_lut.Mappings, 0, 0);
        // If map file provided, read it and send a new mapping to the DM instead
        } else {
          log<software_info>({__FILE__, __LINE__, "Sending mapping to DM."});

          int ret = get_array_to_actuator_mapping(map_lut);
          if (ret < 0)
          {
            log<software_critical>({__FILE__, __LINE__, errno, "Failed to get array to actuator mapping."});
            return -1;
          }
          
          castMappingQuery->setPayload(map_lut.Mappings, payloadLen, startPixel);
        }

        query(castMappingQuery);
        receive();
      } else {
        log<software_error>({__FILE__, __LINE__, "Query casting failed."});
        return -1;
      }

      /* initialize to 0 to allow for handling addressable but ignored actuators */
      m_dminputs.assign(m_nbAct, 0.0);
  
      if(zeroDM() < 0)
      {
        log<software_error>({__FILE__, __LINE__, errno, "DM initialization failed.  Error zeroing DM."});
        return -1;
      }
  
      /* initialize to -1 to allow for handling addressable but ignored actuators */
      m_actuator_mapping.assign(m_nbAct, -1);
  
      if(get_shmim_to_pixel_mapping() < 0)
      {
        log<software_error>({__FILE__, __LINE__, "DM initialization failed.  Failed to get actuator mapping."});
        return -1;
      }
  
      return 0;
    }
    
    int dmCtrl::zeroDM()
    {
      uint16_t startPixel = 0;

      std::vector<double> dminputs;
      dminputs.assign(m_nbAct, 0);

      /* Send the all 0 command to the DM */
      if (send_array(dminputs, m_nbAct, startPixel) < 0) {
        log<software_error>({__FILE__, __LINE__, errno, "Failed to send zero command to DM."});
        return -1;
      }
  
      log<software_info>({__FILE__, __LINE__, "DM zeroed"});
      return 0;
    }
    
    int dmCtrl::commandDM(void * curr_src)
    {
       //This is based on Kyle Van Gorkoms original sendCommand function.
    
       /*This loop performs the following steps:
         1) converts from float to double
         2) convert to volume-normalized displacement
         3) convert to squared fractional voltage clamped from 0 to 1.
       */
    
       #ifdef XWC_DMTIMINGS
       dmT::m_tact0 = mx::sys::get_curr_time();
       #endif
    
       for (uint32_t idx = 0; idx < m_nbAct; ++idx)
       {
          int address = m_actuator_mapping[idx];
          if(address == -1)
          {
             m_dminputs[idx] = 0.; // addressable but ignored actuators set to 0
          }
          else
          {
             m_dminputs[idx] = ((double)  (static_cast<realT *>(curr_src)[address])) * m_volumeOverGain;
    
             if (m_dminputs[idx] > 1)
             {
                m_dminputs[idx] = 1;
             }
             else if (m_dminputs[idx] < 0)
             {
                m_dminputs[idx] = 0;
             }
             else
             {
                m_dminputs[idx] = sqrt(m_dminputs[idx]);
             }
          }
       }
    
       #ifdef XWC_DMTIMINGS
       dmT::m_tact1 = mx::sys::get_curr_time();
       #endif
    
       /* Send the command to the DM */
       uint16_t startPixel = 0;
       int ret = send_array(m_dminputs, m_nbAct, startPixel);
    
       #ifdef XWC_DMTIMINGS
       dmT::m_tact2 = mx::sys::get_curr_time();
       #endif
    
       /* Return immediately upon error, logging the error
       message first and then return the failure code. */
       if(ret != 0)
       {
          log<software_error>({__FILE__, __LINE__, "DM command failed"});
          return -1;
       }
    
       #ifdef XWC_DMTIMINGS
       dmT::m_tact3 = mx::sys::get_curr_time();
       #endif
    
       /* Now update the instantaneous sat map */
       for (uint32_t idx = 0; idx < m_nbAct; ++idx)
       {
          int address = m_actuator_mapping[idx];
    
          if(address == -1)
          {
             continue;
          }
          else if(m_dminputs[idx] >= 1 || m_dminputs[idx] <= 0)
          {
             ++m_nsat;
             m_instSatMap.data()[address] = 1;
          }
          else
          {
             m_instSatMap.data()[address] = 0;
          }
       }
    
       #ifdef XWC_DMTIMINGS
       dmT::m_tact4 = mx::sys::get_curr_time();
       #endif
    
      return 0;
    }
    
    int dmCtrl::releaseDM()
    {
       // Safe DM shutdown on interrupt
    
       state(stateCodes::READY);
    
       if(!shutdown())
       {
          pthread_kill(m_smThread.native_handle(), SIGUSR1);
       }
    
       sleep(1);
    
       if(zeroDM() < 0)
       {
          log<software_error>({__FILE__, __LINE__, "DM release failed. Error zeroing DM."});
          return -1;
       }
    
      //   // disable high resolution mode (releasing segfaults unless you disable)
      //  ret = BMC_PCIeEnableHighRes(&m_dm, 0);
      //  if(ret != NO_ERR)
      //  {
      //     const char *err;
      //     err = BMCErrorString(ret);
      //     log<text_log>(std::string("Disabling high resolution (pseudo 16-bit) mode failed: ") + err, logPrio::LOG_ERROR);
      //  }
      //  log<text_log>("BMC high resolution mode disabled", logPrio::LOG_NOTICE);
    
       log<software_notice>({__FILE__, __LINE__, "DM reset and released"});
    
       return 0;
    }


    // Assuming the map is a 2D fits image where the value of each pixel is the index in the command vector for that position.
    // Read the map and populate m_actuator_mapping.
    int dmCtrl::get_shmim_to_pixel_mapping()
    {
      mx::fits::fitsFile<realT> ff;
      mx::fits::fitsHeader fh;
      mx::improc::eigenImage<realT> map_data;
      
      if(!std::filesystem::exists(m_shmim_map_filename)) {
        log<software_critical>({__FILE__, __LINE__, errno, "Mapping fits file " + m_shmim_map_filename + " does not exist."});
        return -1;
      }

      mx::error_t ret = ff.read(map_data, fh, m_shmim_map_filename);
      
      if (ret != mx::error_t::noerror)
      {
        log<software_critical>({__FILE__, __LINE__, errno, "Could not read mapping fits file " + m_shmim_map_filename});
        return -1;
      }

      // Assuming this is an image, not a table
      // TODO: How to check this with fitsFile?

      // Check image dimensions
      if (ff.naxis() != 2)
      {
        std::ostringstream oss;
        oss << "Error: NAXIS = " << ff.naxis() << " Only 2-D images are supported.";
        log<software_critical>({__FILE__, __LINE__, errno, oss.str()});
        return -1;
      }

      // // Something like this could be a good check that we're sending a sensible map, but max value might be the way to go
      // if (map_data.size() > m_nbAct) {
      //     log<software_critical>({__FILE__, __LINE__, "Mapping image is too large for actuator count"});
      //     return -1;
      // }

      int ij = 0; /* actuator mapping index */
      // Currently, array starts at index 1, as it did for the original CFITSIO get_actuator_mapping implementation.
      // Subtract 1 to convert to 0-based indexing.
      for (auto it = map_data.data(); it != map_data.data() + map_data.size(); it++) {
        int element = *it;
        if (element > 0) {
          m_actuator_mapping[element - 1] = ij;
        }
        ij++;
      }

      ff.close();

      log<software_info>({__FILE__, __LINE__, "DM: Using actuator mapping from " + m_shmim_map_filename});

      // std::ostringstream oss;
      // for (int i = 0; i < m_nbAct; i++)
      // {
      //   oss << "New Actuator mapping[" << i << "] = " << m_actuator_mapping[i] << "\n";
      // }  
      // log<text_log>(oss.str());
      return 0;
    }


    // TODO: Do we ever want to send only a subset of the mapping?
    // If yes, we need to also read in startPixel (from the file or config) 
    // and request existing mapping from the DM first.
    int dmCtrl::get_array_to_actuator_mapping(CGraphDMMappings &map_lut)
    {
      if(!std::filesystem::exists(m_dm_map_filename)) {
        log<software_critical>({__FILE__, __LINE__, errno, "Actuator mapping file " + m_dm_map_filename + " does not exist."});
        return -1;
      }

      // Read the data from the file
      std::ifstream file(m_dm_map_filename);
      std::string line;
      size_t i = 0;
      bool hasValidData = false;
      while (std::getline(file, line))
      {
        std::istringstream iss(line);
        char firstChar;
        char discard; 

        // Skip leading whitespace
        iss >> std::ws; 

        // Check if the first character is '['
        if (iss >> firstChar && firstChar == '[') {
          int controlerBoard, dacNumber, actuatorNumber;

          // Parse the line and create the CGraphDMMappingPayload
          if (iss >> controlerBoard >> discard >> dacNumber >> discard >> actuatorNumber >> discard)
          {
            if (controlerBoard >= 0 && controlerBoard < DMMaxControllerBoards &&
                dacNumber >= 0 && dacNumber < DMMDacsPerControllerBoard &&
                actuatorNumber >= 0 && actuatorNumber < DMActuatorsPerDac)
            {
              try  // Does the file provide more entries than the number of actuators?
              {
                map_lut[i] = CGraphDMMappingPayload(controlerBoard, dacNumber, actuatorNumber);
              } catch ( const std::out_of_range &e )
              {
                log<software_error>({__FILE__, __LINE__, "The number of entries in the file exceeds the number of actuators."});
                return -1;
              }
              i++;
              hasValidData = true;
            }
            else
            {
              std::ostringstream oss;
              oss << "Invalid data in line: " << line << " (controlerBoard: " << controlerBoard << ", dacNumber: " << dacNumber << ", actuatorNumber: " << actuatorNumber << ")";
              log<software_error>({__FILE__, __LINE__, errno, oss.str()});
              // Throwing an error here; assuming that if there is an invalid entry none can be trusted.
              // Not deleting entries already added to map_lut since this is a critical error, but might need to revisit this.
              return -1;
            }
          }
          else
          {
            log<software_error>({__FILE__, __LINE__, "Error parsing line: " + line});
            return -1;
          }
        }
      }

      if (!hasValidData)
      {
        log<software_error>({__FILE__, __LINE__, "No valid data found in the file."});
        return -1;
      }

      return 0; 
    }

    int dmCtrl::send_array(const std::vector<double> &inputs, uint16_t nbInputs, uint16_t startPixel)
    {
      if (m_mode == SHORT)
      {
        m_shortQ->setPayload(inputs.data(), nbInputs, startPixel);
        query(m_shortQ);
      }
      else if (m_mode == LONG)
      {
        m_longQ->setPayload(inputs.data(), nbInputs, startPixel);
        query(m_longQ);
      }
      else if (m_mode == DITHER)
      {
        m_ditherQ->setPayload(inputs.data(), nbInputs, startPixel);
        query(m_ditherQ);
      }
      else
      {
        log<software_error>({__FILE__, __LINE__, "Unkown dm mode: "+ m_mode + "."});
        return -1;
      }
      
      receive();

      return 0;
    }


    ////////////////////
    // INDI CALLBACKS
    ////////////////////

    // callback from setting m_indiP_val1
    // only 'target' is editable ('current' should be updated by code)
    INDI_NEWCALLBACK_DEFN(dmCtrl, m_indiP_mode)
    (const pcf::IndiProperty &ipRecv)
    {
      INDI_VALIDATE_CALLBACK_PROPS(m_indiP_mode, ipRecv);

      std::string current = "", target = "";

      if (ipRecv.find("current"))
      {
        current = ipRecv["current"].get<std::string>();
      }

      if (ipRecv.find("target"))
      {
        target = ipRecv["target"].get<std::string>();
      }

      if (!(target == SHORT || target == LONG || target == DITHER))
      {
        log<software_critical>({__FILE__, __LINE__, errno, "The provided mode, " + m_mode + ", is not a valid option. Valid options are: 'short', 'long', 'dither'."});
      }

      // Lock the mutex, waiting if necessary
      std::unique_lock<std::mutex> lock(m_indiMutex);

      updateIfChanged(m_indiP_mode, "target", target);
      m_mode = target;

      std::ostringstream oss;
      oss << "INDI mode callback: " << target;
      log<software_info>({__FILE__, __LINE__, oss.str()});

      return 0;
    }


  } //namespace app
} //namespace MagAOX

#endif //dmCtrl_hpp
