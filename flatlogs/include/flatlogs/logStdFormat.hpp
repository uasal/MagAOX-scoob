/** \file logStdFormat.hpp
  * \brief Standard formating of log entries for readable output.
  * \author Jared R. Males (jaredmales@gmail.com)
  *
  * \ingroup flatlogs_files
  * 
  * History:
  * - 2017-12-24 created by JRM
  * - 2018-08-18 moved to flatlogs
  */

#ifndef flatlogs_logStdFormat_hpp
#define flatlogs_logStdFormat_hpp

#include "logHeader.hpp"
#include "logPriority.hpp"

namespace flatlogs
{

/// Worker function that formats a log into the standard text representation.
/** 
  *
  * \ingroup logformat
  */
template<typename logT, typename iosT>
iosT & stdFormat( iosT & ios, ///< [out] the iostream to output the log too
                  bufferPtrT & logBuffer ///< [in] the binary log buffer to output
                )
{
   logPrioT prio;
   eventCodeT ec;
   timespecX ts;
   msgLenT len;

   logHeader::extractBasicLog( prio, ec, ts, len, logBuffer);

   ios << ts.ISO8601DateTimeStrX() << " " << priorityString(prio) << " " << logT::msgString(logHeader::messageBuffer(logBuffer) , len);
   
   return ios;
}

/// Worker function that formats a log into the standard text representation with short timespec.
/** 
  *
  * \ingroup logformat
  */
template<typename logT, typename iosT>
iosT & stdShortFormat( iosT & ios, ///< [out] the iostream to output the log to
                       const std::string & appName,
                       bufferPtrT & logBuffer ///< [in] the binary log buffer to output
                     )
{
   logPrioT prio;
   eventCodeT ec;
   timespecX ts;
   msgLenT len;

   logHeader::extractBasicLog( prio, ec, ts, len, logBuffer);

   std::string outApp;
   
   if(appName.size() > 14)
   {
      outApp.resize(16, ' ');
      outApp[15] = ':';
      for(size_t n=14; n >= 0; --n)
      {
         if( 14-n + 1 > appName.size()) break;
         outApp[n] = appName[appName.size()-1 - (14-n)];
      }
   }
   else
   {
      outApp = appName;
      outApp += ":";
      outApp += std::string( 14-appName.size(), ' ');
   }
      
   
   ios << outApp << " " << ts.secondStrX() << " " << priorityString(prio) << " " << logT::msgString(logHeader::messageBuffer(logBuffer) , len);
   
   return ios;
}

} //namespace flatlogs

#endif //flatlogs_logStdFormat_hpp
