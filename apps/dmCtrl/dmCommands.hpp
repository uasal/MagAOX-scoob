/** \file dmCommands.hpp
 * \brief Utility file for the dmCtrl app with structre and class definitions for commands to be sent to the dm
 *
 * \ingroup dmCtrl_files
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
        /**
         * @brief Payload types for the commands
         *
         * The payload type is sent with the command packet and
         * tells the dm what command is being sent.
         */
        static const uint16_t CGraphPayloadTypeDMDac = 0x3002U;
        static const uint16_t CGraphPayloadTypeDMTelemetry = 0x3004U;
        static const uint16_t CGraphPayloadTypeDMHVSwitch = 0x3007U;
        static const uint16_t CGraphPayloadTypeDMDacConfig = 0x3009U;

        struct CGraphDMTelemetryPayload
        {
            double P1V2;
            double P2V2;
            double P28V;
            double P2V5;
            double P6V;
            double P5V;
            double P3V3D;
            double P4V3;
            double P2I2;
            double P4I3;
            double P6I;	


            bool operator==(const CGraphDMTelemetryPayload *p /**< [in] the pointer to the struct to compare to*/)
            {
                return (P1V2 == p->P1V2 && P2V2 == p->P2V2 && P28V == p->P28V && P2V5 == p->P2V5 && P6V == p->P6V && P5V == p->P5V &&
                        P3V3D == p->P3V3D && P4V3 == p->P4V3 && P2I2 == p->P2I2 && P4I3 == p->P4I3 && P6I == p->P6I);
            }

            bool operator==(const CGraphDMTelemetryPayload p /**< [in] the struct to compare to*/)
            {
                return (P1V2 == p.P1V2 && P2V2 == p.P2V2 && P28V == p.P28V && P2V5 == p.P2V5 && P6V == p.P6V && P5V == p.P5V &&
                        P3V3D == p.P3V3D && P4V3 == p.P4V3 && P2I2 == p.P2I2 && P4I3 == p.P4I3 && P6I == p.P6I);
            }

            CGraphDMTelemetryPayload &operator=(const CGraphDMTelemetryPayload *p /**< [in] the pointer to the struct to be copied*/)
            {
                this->P1V2 = p->P1V2;
                this->P2V2 = p->P2V2;
                this->P28V = p->P28V;
                this->P2V5 = p->P2V5;
                this->P6V = p->P6V;
                this->P5V = p->P5V;
                this->P3V3D = p->P3V3D;
                this->P4V3 = p->P4V3;
                this->P2I2 = p->P2I2;
                this->P4I3 = p->P4I3;
                this->P6I = p->P6I;
                return *this;
            }

            CGraphDMTelemetryPayload &operator=(const CGraphDMTelemetryPayload &p /**< [in] struct to be copied*/)
            {
                this->P1V2 = p.P1V2;
                this->P2V2 = p.P2V2;
                this->P28V = p.P28V;
                this->P2V5 = p.P2V5;
                this->P6V = p.P6V;
                this->P5V = p.P5V;
                this->P3V3D = p.P3V3D;
                this->P4V3 = p.P4V3;
                this->P2I2 = p.P2I2;
                this->P4I3 = p.P4I3;
                this->P6I = p.P6I;
                return *this;
            }
        };

        /**
         * @brief Base class for all the dm queries
         *
         * PZTQuery is the class from which all the query classes inherit.
         * It ensures that the all implement a minimal interfaces that includes
         * processReply, logReply and errorLogString.
         */
        class PZTQuery
        {
        public:
            std::string startLog = "";
            std::string endLog = "";

            virtual ~PZTQuery() = default;
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
         * @brief Child query class that handles sending a telemetry query to the dm
         */
        class TelemetryQuery : public PZTQuery
        {
        public:
            TelemetryQuery()
            {
                PayloadType = CGraphPayloadTypeDMTelemetry;
                startLog = "PZTTelemetry: Querying telemetry.";
                endLog = "PZTTelemetry: Finished querying telemetry.";
            }

            const CGraphDMTelemetryPayload *ParamsPtr = nullptr;
            CGraphDMTelemetryPayload Telemetry;

            void processReply(char const *Params, const size_t ParamsLen) override
            {
                if ((NULL != Params) && (ParamsLen >= (sizeof(CGraphDMTelemetryPayload))) )
                {
                    ParamsPtr = reinterpret_cast<const CGraphDMTelemetryPayload *>(Params);
                    Telemetry = *ParamsPtr;
                }
                else
                {
                    errorLogString(ParamsLen);
                }
            }

            void errorLogString(const size_t ParamsLen) override
            {
                std::ostringstream oss;
                oss << "PZTTelemetry: Short packet: " << ParamsLen << " (expected " << sizeof(CGraphDMTelemetryPayload) << " bytes): ";
                MagAOXAppT::log<software_error>({__FILE__, __LINE__, oss.str()});
            }

            void logReply() override
            {
                std::ostringstream oss;
                oss << "BinaryDMTelemetry Command: Telemetry received";
                // debug
                oss << "BinaryDMTelemetry Command: Values with corrected units follow:\n";
                oss << "P1V2: " << std::fixed << std::setprecision(6) << Telemetry.P1V2 << " V\n";
                oss << "P2V2: " << std::fixed << std::setprecision(6) << Telemetry.P2V2 << " V\n";
                oss << "P28V: " << std::fixed << std::setprecision(6) << Telemetry.P28V << " V\n";
                oss << "P2V5: " << std::fixed << std::setprecision(6) << Telemetry.P2V5 << " V\n";
                oss << "P6V: " << std::fixed << std::setprecision(6) << Telemetry.P6V << " V\n";
                oss << "P5V: " << std::fixed << std::setprecision(6) << Telemetry.P5V << " V\n";
                oss << "P3V3D: " << std::fixed << std::setprecision(6) << Telemetry.P3V3D << " V\n";
                oss << "P4V3: " << std::fixed << std::setprecision(6) << Telemetry.P4V3 << " V\n";
                oss << "P2I2: " << std::fixed << std::setprecision(6) << Telemetry.P2I2 << " V\n";
                oss << "P4I3: " << std::fixed << std::setprecision(6) << Telemetry.P4I3 << " V\n";
                oss << "P6I: " << std::fixed << std::setprecision(6) << Telemetry.P6I << " V";
                MagAOXAppT::log<text_log>(oss.str());
            }
        };

    } // namespace app
} // namespace MagAOX