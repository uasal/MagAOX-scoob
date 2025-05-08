/** \file fsmCommands.hpp
 * \brief Utility file for the fsmCtrl app with structre and class definitions for commands to be sent to the fsm
 *
 * \ingroup fsmCtrl_files
 */

#pragma once

#include <iostream>
#include <sstream> // for stringstreams
#include <cstddef> // for nullptr
using namespace std;

namespace MagAOX
{
    namespace app
    {
        namespace dev
        {
            /**
             * @brief Payload types common to all devices
             *
             * The payload type is sent with the command packet and
             * tells the device what command is being sent.
             */
            static const uint16_t CGraphPayloadTypeVersion = 0x1001U;

            struct CGraphVersionPayload
            {
                uint32_t SerialNum;
                uint32_t ProcessorFirmwareBuildNum;
                uint32_t FPGAFirmwareBuildNum;
            };


            /**
             * @brief Base class for all the summerDevice queries
             *
             * sdevQuery is the class from which all the query classes inherit.
             * It ensures that the all implement a minimal interfaces that includes
             * processReply, logReply and errorLogString.
             */
            class sdevQuery
            {
            public:
                std::string startLog = "";
                std::string endLog = "";

                virtual ~sdevQuery() = default;
                virtual void errorLogString(const size_t ParamsLen) = 0;
                virtual void processReply(char const *Params, const size_t ParamsLen) = 0;
                virtual void logReply() = 0;
                virtual uint16_t getPayloadType() const
                {
                    return PayloadType;
                }
                virtual void *getPayloadData() const
                {
                    return PayloadData;
                }
                virtual uint16_t getPayloadLen() const
                {
                    return PayloadLen;
                }
                virtual void setPayload(void *newPayloadData, uint16_t newPayloadLen)
                {
                    PayloadData = newPayloadData;
                    PayloadLen = newPayloadLen;
                }
                virtual void resetPayload()
                {
                    PayloadData = DefaultPayloadData;
                    PayloadLen = DefaultPayloadLen;
                }

            protected:
                uint16_t PayloadType = -1;
                void *DefaultPayloadData = NULL;
                size_t DefaultPayloadLen = 0;
                void *PayloadData = DefaultPayloadData;
                size_t PayloadLen = DefaultPayloadLen;
            };


            // Derived classes
            /**
             * @brief Child query class that handles sending a version query to the device
             */
            class VersionQuery : public dev::sdevQuery
            {
            public:
                VersionQuery()
                {
                    PayloadType = CGraphPayloadTypeVersion;
                    startLog = "PZTVersion: Querying version.";
                    endLog = "PZTVersion: Finished querying version.";
                }

                const CGraphVersionPayload *ParamsPtr = nullptr;
                CGraphVersionPayload Version;

                void processReply(char const *Params, const size_t ParamsLen) override
                {
                    if ((NULL != Params) && (ParamsLen >= (sizeof(CGraphVersionPayload))) )
                    {
                        ParamsPtr = reinterpret_cast<const CGraphVersionPayload *>(Params);
                        Version = *ParamsPtr;
                    }
                    else
                    {
                        errorLogString(ParamsLen);
                    }
                }

                void errorLogString(const size_t ParamsLen) override
                {
                    std::ostringstream oss;
                    oss << "PZTVersion: Short packet: " << ParamsLen << " (expected " << sizeof(CGraphVersionPayload) << " bytes): ";
                    MagAOX::app::MagAOXApp<true>::log<software_error>({__FILE__, __LINE__, oss.str()});
                }

                void logReply() override
                {
                    std::ostringstream oss;
                    oss << "PZTVersion Command: Version received\n";
                    // debug
                    oss << "PZTVersion Command: \n";
                    oss << "CGraphVersionPayload: SerialNum: 0x " << Version.SerialNum << ", ProcessorFirmwareBuildNum: " << Version.ProcessorFirmwareBuildNum << ", FPGAFirmwareBuildNum: " << Version.FPGAFirmwareBuildNum  << "\n";
                    MagAOX::app::MagAOXApp<true>::log<text_log>(oss.str());
                }
            };
        } // namespace dev
    } // namespace app
} // namespace MagAOX