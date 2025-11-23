/** \file hwpTracker.hpp
  * \brief The MagAO-X HWP rotation tracker header file
  *
  * \ingroup hwpTracker_files
  */

#ifndef hwpTracker_hpp
#define hwpTracker_hpp


#include "../../libMagAOX/libMagAOX.hpp" //Note this is included on command line to trigger pch
#include "../../magaox_git_version.h"

#include <mx/math/gslInterpolation.hpp>
#include <mx/ioutils/readColumns.hpp>

/** \defgroup hwpTracker
  * \brief The MagAO-X application to track pupil rotation with the HWP.
  *
  * <a href="../handbook/operating/software/apps/hwpTracker.html">Application Documentation</a>
  *
  * \ingroup apps
  *
  */

/** \defgroup hwpTracker_files
  * \ingroup hwpTracker
  */

namespace MagAOX
{
namespace app
{

/// The MagAO-X ADC Tracker
/** 
  * \ingroup hwpTracker
  */
class hwpTracker : public MagAOXApp<true>
{

   //Give the test harness access.
   friend class hwpTracker_test;

protected:

   /** \name Configurable Parameters
     *@{
     */
   
   
   float m_zero {0}; ///< The starting point for the HWP at zd=0.

   int m_sign {1}; ///< The sign to apply to the zd to rotate the HWP

   std::string m_devName {"stagehwprot"}; ///< The device name of the HWP stage.  Default is 'stagehwprot'
   std::string m_devName2 {"stagehwplin"}; ///< The device name of the HWP insertion stage.  Default is 'stagehwplin'
   std::string m_tcsDevName {"tcsi"}; ///< The device name of the TCS Interface providing 'teldata.zd'.  Default is 'tcsi'
   
   float m_updateInterval {1}; ///< The interval at which to update positions, in seconds.  Default is 10 secs.
   
   bool m_tracking {false}; ///< Is the HWP in ADI synchronization mode?
   
   float m_zd {0}; ///< Current zenith distance

   float m_angSeq[4] std::array{0.0, 45.0, 22.5, 67.5}; ///< The sequence of HWP angles for a single cycle. Default is 0,45,22.5,67.5
   int m_numCycles -1; ///< The number of HWP cycles to run (-1 is no limit). Default is -1

   
public:
   /// Default c'tor.
   hwpTracker();

   /// D'tor, declared and defined for noexcept.
   ~hwpTracker() noexcept
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

   /// Implementation of the FSM for hwpTracker.
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


   /** @name INDI
     *
     * @{
     */
protected:
   
   pcf::IndiProperty m_indiP_tracking;
   
   pcf::IndiProperty m_indiP_teldata;
   
   pcf::IndiProperty m_indiP_kpos;
   
public:
   INDI_NEWCALLBACK_DECL(hwpTracker, m_indiP_tracking);
   
   INDI_SETCALLBACK_DECL(hwpTracker, m_indiP_teldata);
   
   ///@}
};

hwpTracker::hwpTracker() : MagAOXApp(MAGAOX_CURRENT_SHA1, MAGAOX_REPO_MODIFIED)
{
   
   return;
}

void hwpTracker::setupConfig()
{
   config.add("k.zero", "", "k.zero", argType::Required, "k", "zero", false, "float", "The HWP zero position.  Default is ??.");
   
   config.add("k.sign", "", "k.sign", argType::Required, "k", "sign", false, "int", "The HWP rotation sign. Default is +1.");
   
   
   
   config.add("k.devName", "", "k.devName", argType::Required, "k", "devName", false, "string", "The device name of the HWPstage.  Default is 'stagek'");
   
   config.add("tcs.devName", "", "tcs.devName", argType::Required, "tcs", "devName", false, "string", "The device name of the TCS Interface providing 'teldata.zd'.  Default is 'tcsi'");
   
   config.add("tracking.updateInterval", "", "tracking.updateInterval", argType::Required, "tracking", "updateInterval", false, "float", "The interval at which to update positions, in seconds.  Default is 10 secs.");
}

int hwpTracker::loadConfigImpl( mx::app::appConfigurator & _config )
{
   _config(m_zero, "k.zero");
   _config(m_sign, "k.sign");
   _config(m_devName, "k.devName");
   _config(m_tcsDevName, "tcs.devName");
   _config(m_updateInterval, "tracking.updateInterval");

   _config()

   return 0;
}

void hwpTracker::loadConfig()
{
   loadConfigImpl(config);
}

int hwpTracker::appStartup()
{
   
   
   createStandardIndiToggleSw( m_indiP_tracking, "tracking");
   registerIndiPropertyNew( m_indiP_tracking, INDI_NEWCALLBACK(m_indiP_tracking));
   
   
   REG_INDI_SETPROP(m_indiP_teldata, m_tcsDevName, "teldata");
   
   m_indiP_kpos = pcf::IndiProperty(pcf::IndiProperty::Number);
   m_indiP_kpos.setDevice(m_devName);
   m_indiP_kpos.setName("position");
   m_indiP_kpos.add(pcf::IndiElement("target"));
      
   state(stateCodes::READY);
   
   return 0;
}

int hwpTracker::appLogic()
{
   
   static double lastupdate = 0;
   
   if(m_tracking && mx::sys::get_curr_time() - lastupdate > m_updateInterval)
   {
      float k = m_zero + m_sign*0.5*m_zd;
      
      std::cerr << "Sending HWP to: " << k << "\n";
      
      m_indiP_kpos["target"] = k;
      sendNewProperty (m_indiP_kpos); 
      
      lastupdate = mx::sys::get_curr_time();
      
      
   }
   else if(!m_tracking) lastupdate = 0;
      
   return 0;
}

int hwpTracker::appShutdown()
{
   return 0;
}

INDI_NEWCALLBACK_DEFN(hwpTracker, m_indiP_tracking)(const pcf::IndiProperty &ipRecv)
{
   if(ipRecv.getName() != m_indiP_tracking.getName())
   {
      log<software_error>({__FILE__,__LINE__, "wrong INDI property received."});
      return -1;
   }
   
   if(!ipRecv.find("toggle")) return 0;
   
   if( ipRecv["toggle"].getSwitchState() == pcf::IndiElement::On)
   {
      updateSwitchIfChanged(m_indiP_tracking, "toggle", pcf::IndiElement::On, INDI_IDLE);
      
      m_tracking = true;
      
      log<text_log>("started HWP rotation tracking");
   }
   else
   {
      updateSwitchIfChanged(m_indiP_tracking, "toggle", pcf::IndiElement::Off, INDI_IDLE);
      
      m_tracking = false;
      
      log<text_log>("stopped HWP rotation tracking");
   }
   
   return 0;
}


INDI_SETCALLBACK_DEFN(hwpTracker, m_indiP_teldata)(const pcf::IndiProperty &ipRecv)
{
   if(ipRecv.getName() != m_indiP_teldata.getName())
   {
      log<software_error>({__FILE__,__LINE__,"wrong INDI property received"});
      
      return -1;
   }
   
   if(!ipRecv.find("zd")) return 0;
   
   m_zd = ipRecv["zd"].get<float>();
   
   return 0;
}

} //namespace app
} //namespace MagAOX

#endif //hwpTracker_hpp

