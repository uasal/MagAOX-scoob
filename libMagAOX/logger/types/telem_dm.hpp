/** \file telem_dm.hpp
  * \brief The MagAO-X logger telem_dm log type.
  * \author Irina Stefan (istefan@arizona.edu)
  *
  * \ingroup logger_types_files
  *
  * History:
  * - 2023-11-22 - Created by IS
  */
#ifndef logger_types_telem_dm_hpp
#define logger_types_telem_dm_hpp

#include "generated/telem_dm_generated.h"
#include "flatbuffer_log.hpp"

namespace MagAOX
{
namespace logger
{


/// Log entry recording DM telemetry
/** \ingroup logger_types
  */
struct telem_dm : public flatbuffer_log
{

  static const flatlogs::eventCodeT eventCode = eventCodes::TELEM_DM;
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
                const double & P6V,
                const double & P5V,
                const double & P3V3D,
                const double & P4V3,
                const double & P2I2,
                const double & P4I3,
                const double & P6I
                )
        {
        auto fp = CreateTelem_dm_fb(builder, P1V2, P2V2, P28V, P2V5, P6V, P5V, P3V3D, P4V3, P2I2, P4I3, P6I);
        builder.Finish(fp);
        }
   };

   static bool verify( flatlogs::bufferPtrT & logBuff,  ///< [in] Buffer containing the flatbuffer serialized message.
                       flatlogs::msgLenT len            ///< [in] length of msgBuffer.
                     )
   {
      auto verifier = flatbuffers::Verifier( (uint8_t*) flatlogs::logHeader::messageBuffer(logBuff), static_cast<size_t>(len));
      return VerifyTelem_dm_fbBuffer(verifier);
   }

   ///Get the message format for human consumption.
   static std::string msgString( void * msgBuffer,  /**< [in] Buffer containing the flatbuffer serialized message.*/
                                 flatlogs::msgLenT len  /**< [in] [unused] length of msgBuffer.*/
                               )
   {
    static_cast<void>(len); // unused by most log types

    auto fbs = GetTelem_dm_fb(msgBuffer);

    std::string msg = "P1V2: ";
    msg += std::to_string(fbs->P1V2()) + " V ";

    msg += "P2V2: ";
    msg += std::to_string(fbs->P2V2()) + " V ";

    msg += "P28V: ";
    msg += std::to_string(fbs->P28V()) + " V ";

    msg += "P2V5: ";
    msg += std::to_string(fbs->P2V5()) + " V ";

    msg += "P6V: ";
    msg += std::to_string(fbs->P6V()) + " V ";

    msg += "P5V: ";
    msg += std::to_string(fbs->P5V()) + " V ";

    msg += "P3V3D: ";
    msg += std::to_string(fbs->P3V3D()) + " V ";

    msg += "P4V3: ";
    msg += std::to_string(fbs->P4V3()) + " V ";

    msg += "P2I2: ";
    msg += std::to_string(fbs->P2I2()) + " V ";

    msg += "P4I3: ";
    msg += std::to_string(fbs->P4I3()) + " V ";

    msg += "P6I: ";
    msg += std::to_string(fbs->P6I()) + " V ";

    return msg;

   }

}; //telem_dm



} //namespace logger
} //namespace MagAOX

#endif //logger_types_telem_dm_hpp
