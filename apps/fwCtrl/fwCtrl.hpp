/** \file fwCtrl.hpp
  * \brief The MagAO-X XXXXXX header file
  *
  * \ingroup fwCtrl_files
  */

#pragma once

#include "../../libMagAOX/libMagAOX.hpp" //Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"

#include <string.h>
#include <stdio.h>
#include <cmath>  // For pow(), cos() and M_PI

#include <iostream>

typedef MagAOX::app::MagAOXApp<true> MagAOXAppT; // This needs to be before the other header files for logging to work in other headers

#include "fwCommands.hpp"

/** \defgroup fwCtrl
  * \brief Application to interface with ESC filterwheel
  *
  * <a href="../handbook/operating/software/apps/XXXXXX.html">Application Documentation</a>
  *
  * \ingroup apps
  *
  */

/** \defgroup fwCtrl_files
  * \ingroup fwCtrl
  */


namespace MagAOX
{
  namespace app
  {

    /// The MagAO-X ESC FW interface
    /**
     * \ingroup fwCtrl
     */

    class fwCtrl : public MagAOXApp<true>, public dev::telemeter<fwCtrl>, public dev::summerDevice<fwCtrl>
    {

      // Give the test harness access.
      friend class fwCtrl_test;

      friend class dev::telemeter<fwCtrl>;
      typedef dev::telemeter<fwCtrl> telemeterT;

      friend class dev::summerDevice<fwCtrl>;

    protected:
      /** \name Constants
       *@{
       */
      ///@}


      /** \name Configurable Parameters
       *@{
       */
      // here add parameters which will be config-able at runtime
      
      // Telemeter callback parameters
      int period_s;
      ///@}

      std::optional<FWFilterSelectPositions> m_currentPos; ///< The current position of the filter wheel (empty if unknown)
      std::optional<FWFilterSelectPositions> m_targetPos; ///< The target position of the filter wheel (empty if not set)

    private:
      std::unique_ptr<dev::sdevQuery> telemetryQuery = std::make_unique<TelemetryQuery>();
      std::unique_ptr<dev::sdevQuery> filterSelectQuery = std::make_unique<FilterSelectQuery>();
      std::vector<dev::sdevQuery*> customQueries = { telemetryQuery.get(), filterSelectQuery.get() };

      /// Cached typed pointers to avoid per-frame dynamic_cast
      TelemetryQuery *m_telemetryQ = static_cast<TelemetryQuery *>(telemetryQuery.get());
      FilterSelectQuery *m_filterSelectQ = static_cast<FilterSelectQuery *>(filterSelectQuery.get());

    protected:
      // INDI properties
      pcf::IndiProperty m_indiP_position;

    public:
      INDI_NEWCALLBACK_DECL(fwCtrl, m_indiP_position);

    public:
      /// Default c'tor.
      fwCtrl();

      /// D'tor, declared and defined for noexcept.
      ~fwCtrl() noexcept
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

      /// Implementation of the logic for fwCtrl.
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


      /** \name FW-specific functions
       *
       * @{
       */

      const std::vector<dev::sdevQuery*>& getQueries() const override;


      /**
       * @brief Request the firmware to move to filter position
       *
       * \returns 0 on success
       * \returns -1 if pos is out of range.
       */
      int selectFilter(int32_t pos);

      /**
       * @brief Request fw's position
       *
       */
      void receivePosition();

      /**
       * @brief Function that listens for responses from the fw
       *
       * Function that checks for a response from the fw and processes it.
       * If a response is received it processes the response as appropriate for the
       * command sent.
       *
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
       * @param telem_fw_ptr pointer to telem_fw flatbuffer_log structure describing telem inputs & outputs
       * \returns 0 on succcess
       * \returns -1 on error
       */
      int recordTelem(const telem_fw *);

      /**
       * @brief Required by Telemeter Interface
       *
       * @param force boolean; Telemetry is recorded every m_maxInterval (default value of 10) seconds.
       *  If 'true', force telemetry record outside of interval.
       * \returns 0 on succcess
       */
      int recordFw(bool force = false);
      ///@}


      /** \name Utility functions
        *
        * @{
        */

    //   /**
    //    * @brief Utility function that sets 'current' INDI values, if updated
    //    *
    //    * Function that takes the values in m_dac1, m_dac2 and m_dac3, transforms them
    //    * (if necessary) to the type specified by m_inputType and updates the corresponding
    //    * INDI parameter's 'current' value.
    //    */
    //   void updateINDICurrentParams();
        
      ///@}
    };

    fwCtrl::fwCtrl() : MagAOXApp(MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED)
    {
      m_powerMgtEnabled = true;
      return;
    }


    void fwCtrl::setupConfig()
    {
        dev::summerDevice<fwCtrl>::setupConfig(config);
        telemeterT::setupConfig(config);
    }

    int fwCtrl::loadConfigImpl( mx::app::appConfigurator & _config )
    {
        return 0;
    }

    void fwCtrl::loadConfig()
    {
      if (loadConfigImpl(config) < 0)
      {
        log<software_critical>({__FILE__, __LINE__, "Error during config"});
        m_shutdown = true;
      }

      if (dev::summerDevice<fwCtrl>::loadConfig(config) < 0)
      {
        log<software_critical>({__FILE__, __LINE__, "Error during summerDevice config"});
        m_shutdown = true;
      }

      if (telemeterT::loadConfig(config) < 0)
      {
        log<software_critical>({__FILE__, __LINE__, "Error during telemeter config"});
        m_shutdown = true;
      }
    }

    int fwCtrl::appStartup()
    {
        if (telemeterT::appStartup() < 0)
        {
            return log<software_error, -1>({__FILE__, __LINE__});
        }

        if (dev::summerDevice<fwCtrl>::appStartup() < 0)
        {
            return log<software_error, -1>({__FILE__, __LINE__});
        }

        // set up the  INDI properties
        
        // filter position
        REG_INDI_NEWPROP(m_indiP_position, "position", pcf::IndiProperty::Number);
        m_indiP_position.add(pcf::IndiElement("current"));
        m_indiP_position.add(pcf::IndiElement("target"));

        return 0;
    }

    int fwCtrl::appLogic()
    {
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
            int rv = dev::summerDevice<fwCtrl>::connect();
            if (rv == 0)
            {
                state(stateCodes::CONNECTED);
            }
        }

        if (state() == stateCodes::CONNECTED)
        {
            // Query current filter position
            dev::summerDevice<fwCtrl>::query(m_filterSelectQ);
            receive();

            state(stateCodes::READY);
        }

        if (state() == stateCodes::READY || state() == stateCodes::OPERATING)
        {
            // Query current filter position
            dev::summerDevice<fwCtrl>::query(m_filterSelectQ);
            receive();


            // Detect move completion
            // Since we're in OPERATING state, we know a move command has been sent
            if (state() == stateCodes::OPERATING)
            {
              // Only check move completion if we have a known current position and it's not in the MOVING state
              if (m_currentPos && *m_currentPos != FWFilterSelectPositions::FW_MOVING)
              {
                // Move completed
                if (m_targetPos && *m_currentPos == *m_targetPos)
                {
                  log<software_info>({__FILE__, __LINE__, 
                    "Filter move complete: position " + std::to_string(static_cast<int32_t>(*m_currentPos))});
                }
                else
                {
                  log<software_warning>({__FILE__, __LINE__,
                    "Filter move ended at position " + std::to_string(static_cast<int32_t>(*m_currentPos))
                    + " but target was " + std::to_string(static_cast<int32_t>(*m_targetPos))});
                }
                state(stateCodes::READY);
              }
            }

            // Telemeter
            if (telemeterT::appLogic() < 0)
            {
                log<software_error>({__FILE__, __LINE__});
                return 0;
            }
        }

        return 0;
    }

    int fwCtrl::appShutdown()
    {
        telemeterT::appShutdown();
        dev::summerDevice<fwCtrl>::appShutdown();
        return 0;
    }


    //////////////
    // CONNECTION
    //////////////

    const std::vector<dev::sdevQuery*>& fwCtrl::getQueries() const {
        return customQueries;
    }

    int fwCtrl::selectFilter(int32_t pos)
    {
        auto targetPos = toFWFilterPositions(pos);
        if (!targetPos) {
            return log<software_error, -1>({__FILE__, __LINE__,
                "Requested filter position " + std::to_string(pos) + " out of range"});
        }

        m_targetPos = targetPos;

        // Set the payload to the target filter and send
        m_filterSelectQ->setPayload(const_cast<int32_t *>(&pos), sizeof(int32_t));
        dev::summerDevice<fwCtrl>::query(m_filterSelectQ);

        log<software_info>({__FILE__, __LINE__, "Commanding filter move to position " + std::to_string(pos)});

        state(stateCodes::OPERATING);

        return 0;
    }

    // Function to parse fw positin
    void fwCtrl::receivePosition()
    {
      if (!m_filterSelectQ->FilterSelect) {
        return;
      }

      m_currentPos = m_filterSelectQ->FilterSelect;
      updateIfChanged(m_indiP_position, "current", static_cast<int32_t>(*m_currentPos));
    }

    void fwCtrl::receive()
    {
        dev::summerDevice<fwCtrl>::receive();

        // Once packet had been received, make sure updates are propagated.
        // Since we don't know the packet type, update all.
        // If no other receive types are added, should consider refactor to move receivePosition() logic here.
        receivePosition();
    }



    /////////////////////////
    // TELEMETER INTERFACE
    /////////////////////////

    int fwCtrl::checkRecordTimes()
    {
        return telemeterT::checkRecordTimes(telem_fw());
    }

    int fwCtrl::recordTelem(const telem_fw *)
    {
        dev::summerDevice<fwCtrl>::query(m_telemetryQ);
        
        dev::summerDevice<fwCtrl>::receive();
        m_telemetryQ->logReply();

        return recordFw(true);
    }

    int fwCtrl::recordFw(bool force)
    {
        static CGraphFWTelemetryPayload LastTelemetry; ///< Structure holding the previous fw voltage measurement.

        if (!(LastTelemetry == m_telemetryQ->Telemetry) || force)
        {
        LastTelemetry = m_telemetryQ->Telemetry;
        telem<telem_fw>({LastTelemetry.P1V2, LastTelemetry.P2V2, LastTelemetry.P28V, LastTelemetry.P2V5, LastTelemetry.P6V, LastTelemetry.P5V, LastTelemetry.P3V3D, LastTelemetry.P4V3, LastTelemetry.P2I2, LastTelemetry.P4I3, LastTelemetry.P6I});
        }

        return 0;
    }

    ////////////////////
    // INDI CALLBACKS
    ////////////////////

    // callback from setting m_indiP_position
    // only 'target' is editable ('current' should be updated by code)
    INDI_NEWCALLBACK_DEFN(fwCtrl, m_indiP_position)
    (const pcf::IndiProperty &ipRecv)
    {
      INDI_VALIDATE_CALLBACK_PROPS(m_indiP_position, ipRecv);

      int32_t target;

      if (ipRecv.find("target"))
      {
        target = ipRecv["target"].get<int32_t>();

        auto pos = toFWFilterPositions(target);
        if (!pos)
        {
            log<software_error>({__FILE__, __LINE__, "Invalid filter target: " + std::to_string(target)});
            return -1;
        }

        if (m_targetPos && *m_targetPos == *pos)
        {
            log<software_info>({__FILE__, __LINE__, "Already at target position " + std::to_string(target)});
            return 0;
        }

        if (state() != stateCodes::READY)
        {
            log<software_warning>({__FILE__, __LINE__, "Cannot move filter: not in READY state, currently in state " + std::to_string(state())});
            return -1;
        }

        std::lock_guard<std::mutex> guard(m_indiMutex);
        return selectFilter(target);
      }

      return 0;
    }

} //namespace app
} //namespace MagAOX