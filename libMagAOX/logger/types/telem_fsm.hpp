/** \file telem_fsm.hpp
  * \brief The MagAO-X logger telem_fsm log type.
  * \author Irina Stefan (istefan@arizona.edu)
  *
  * \ingroup logger_types_files
  *
  * History:
  * - 2023-11-22 - Created by IS
  */
#ifndef logger_types_telem_fsm_hpp
#define logger_types_telem_fsm_hpp

#include "generated/telem_fsm_generated.h"
#include "flatbuffer_log.hpp"

namespace MagAOX
{
namespace logger
{


/// Log entry recording FSM telemetry
/** \ingroup logger_types
  */
struct telem_fsm : public flatbuffer_log
{

  static const flatlogs::eventCodeT eventCode = eventCodes::TELEM_FSM;
  static const flatlogs::logPrioT defaultLevel = flatlogs::logPrio::LOG_TELEM;

  static timespec lastRecord; ///< The time of the last time this log was recorded.  Used by the telemetry system.

   ///The type of the input message
   struct messageT : public fbMessage
   {
        ///Construct from components
        messageT(const double & P1V2,
                const double & P2V2,
                const double & P28V,
                const double & P2V5,
                const double & P3V3A,
                const double & P6V,
                const double & P5V,
                const double & P3V3D,
                const double & P4V3,
                const double & N5V,
                const double & N6V,
                const double & P150V
                )
        {
        auto fp = CreateTelem_fsm_fb(builder, P1V2, P2V2, P28V, P2V5, P3V3A, P6V, P5V, P3V3D, P4V3, N5V, N6V, P150V);
        builder.Finish(fp);
        }
   };

   static bool verify( flatlogs::bufferPtrT & logBuff,  ///< [in] Buffer containing the flatbuffer serialized message.
                       flatlogs::msgLenT len            ///< [in] length of msgBuffer.
                     )
   {
      auto verifier = flatbuffers::Verifier( (uint8_t*) flatlogs::logHeader::messageBuffer(logBuff), static_cast<size_t>(len));
      return VerifyTelem_fsm_fbBuffer(verifier);
   }

   ///Get the message format for human consumption.
   static std::string msgString( void * msgBuffer,  /**< [in] Buffer containing the flatbuffer serialized message.*/
                                 flatlogs::msgLenT len  /**< [in] [unused] length of msgBuffer.*/
                               )
   {
    static_cast<void>(len); // unused by most log types

    auto fbs = GetTelem_fsm_fb(msgBuffer);

    std::string msg = "P1V2: ";
    msg += std::to_string(fbs->P1V2()) + " V ";

    msg += "P2V2: ";
    msg += std::to_string(fbs->P2V2()) + " V ";

    msg += "P28V: ";
    msg += std::to_string(fbs->P28V()) + " V ";

    msg += "P2V5: ";
    msg += std::to_string(fbs->P2V5()) + " V ";

    msg += "P3V3A: ";
    msg += std::to_string(fbs->P3V3A()) + " V ";

    msg += "P6V: ";
    msg += std::to_string(fbs->P6V()) + " V ";

    msg += "P5V: ";
    msg += std::to_string(fbs->P5V()) + " V ";

    msg += "P3V3D: ";
    msg += std::to_string(fbs->P3V3D()) + " V ";

    msg += "P4V3: ";
    msg += std::to_string(fbs->P4V3()) + " V ";

    msg += "N5V: ";
    msg += std::to_string(fbs->N5V()) + " V ";

    msg += "N6V: ";
    msg += std::to_string(fbs->N6V()) + " V ";

    msg += "P150V: ";
    msg += std::to_string(fbs->P150V()) + " V ";

     return msg;

    }

   static double P1V2( void * msgBuffer )
   {
      auto fbs = GetTelem_fsm_fb(msgBuffer);
      return fbs->P1V2();
   }

   static double P2V2( void * msgBuffer )
   {
      auto fbs = GetTelem_fsm_fb(msgBuffer);
      return fbs->P2V2();
   }

   static double P28V( void * msgBuffer )
   {
      auto fbs = GetTelem_fsm_fb(msgBuffer);
      return fbs->P28V();
   }

   static double P2V5( void * msgBuffer )
   {
      auto fbs = GetTelem_fsm_fb(msgBuffer);
      return fbs->P2V5();
   }

   static double P3V3A( void * msgBuffer )
   {
      auto fbs = GetTelem_fsm_fb(msgBuffer);
      return fbs->P3V3A();
   }

   static double P6V( void * msgBuffer )
   {
      auto fbs = GetTelem_fsm_fb(msgBuffer);
      return fbs->P6V();
   }

   static double P5V( void * msgBuffer )
   {
      auto fbs = GetTelem_fsm_fb(msgBuffer);
      return fbs->P5V();
   }

   static double P3V3D( void * msgBuffer )
   {
      auto fbs = GetTelem_fsm_fb(msgBuffer);
      return fbs->P3V3D();
   }

   static double P4V3( void * msgBuffer )
   {
      auto fbs = GetTelem_fsm_fb(msgBuffer);
      return fbs->P4V3();
   }

   static double N5V( void * msgBuffer )
   {
      auto fbs = GetTelem_fsm_fb(msgBuffer);
      return fbs->N5V();
   }

   static double N6V( void * msgBuffer )
   {
      auto fbs = GetTelem_fsm_fb(msgBuffer);
      return fbs->N6V();
   }

   static double P150V( void * msgBuffer )
   {
      auto fbs = GetTelem_fsm_fb(msgBuffer);
      return fbs->P150V();
   }

   /// Get the logMetaDetail for a member by name
   /**
     * \returns the a logMetaDetail filled in with the appropriate details
     * \returns an empty logMetaDetail if member not recognized
     */
   static logMetaDetail getAccessor( const std::string & member /**< [in] the name of the member */ )
   {
      if(     member == "P1V2") return logMetaDetail({"P1V2", logMeta::valTypes::Double, logMeta::metaTypes::Continuous, reinterpret_cast<void*>(&P1V2)});
      else if(member == "P2V2") return logMetaDetail({"P2V2", logMeta::valTypes::Double, logMeta::metaTypes::Continuous, reinterpret_cast<void*>(&P2V2)});
      else if(member == "P28V") return logMetaDetail({"P28V", logMeta::valTypes::Double, logMeta::metaTypes::Continuous, reinterpret_cast<void*>(&P28V)});
      else if(member == "P2V5") return logMetaDetail({"P2V5", logMeta::valTypes::Double, logMeta::metaTypes::Continuous, reinterpret_cast<void*>(&P2V5)});
      else if(member == "P3V3A") return logMetaDetail({"P3V3A", logMeta::valTypes::Double, logMeta::metaTypes::Continuous, reinterpret_cast<void*>(&P3V3A)});
      else if(member == "P6V") return logMetaDetail({"P6V", logMeta::valTypes::Double, logMeta::metaTypes::Continuous, reinterpret_cast<void*>(&P6V)});
      else if(member == "P5V") return logMetaDetail({"P5V", logMeta::valTypes::Double, logMeta::metaTypes::Continuous, reinterpret_cast<void*>(&P5V)});
      else if(member == "P3V3D") return logMetaDetail({"P3V3D", logMeta::valTypes::Double, logMeta::metaTypes::Continuous, reinterpret_cast<void*>(&P3V3D)});
      else if(member == "P4V3") return logMetaDetail({"P4V3", logMeta::valTypes::Double, logMeta::metaTypes::Continuous, reinterpret_cast<void*>(&P4V3)});
      else if(member == "N5V") return logMetaDetail({"N5V", logMeta::valTypes::Double, logMeta::metaTypes::Continuous, reinterpret_cast<void*>(&N5V)});
      else if(member == "N6V") return logMetaDetail({"N6V", logMeta::valTypes::Double, logMeta::metaTypes::Continuous, reinterpret_cast<void*>(&N6V)});
      else if(member == "P150V") return logMetaDetail({"P150V", logMeta::valTypes::Double, logMeta::metaTypes::Continuous, reinterpret_cast<void*>(&P150V)});
      else
      {
         std::cerr << "No member " << member << " in telem_fsm\n";
         return logMetaDetail();
      }
   }

}; //telem_fsm



} //namespace logger
} //namespace MagAOX

#endif //logger_types_telem_fsm_hpp
