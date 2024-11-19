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
        /**
         * @brief Payload types for the commands
         *
         * The payload type is sent with the command packet and
         * tells the fsm what command is being sent.
         */
        static const uint16_t CGraphPayloadTypeFSMDacs = 0x2002U;              // Payload: 3 uint32's
        static const uint16_t CGraphPayloadTypeFSMDacsFloatingPoint = 0x2003U; // Payload: 3 double-precision floats
        static const uint16_t CGraphPayloadTypeFSMAdcs = 0x2004U;              // Payload: 3 AdcAcumulators
        static const uint16_t CGraphPayloadTypeFSMAdcsFloatingPoint = 0x2005U; // Payload: 3 double-precision floats
        static const uint16_t CGraphPayloadTypeFSMTelemetry = 0x2006U;

        /**
         * @brief Structure for the response payload of the Telemetry command
         */
        struct CGraphFSMTelemetryPayload
        {
            double P1V2;
            double P2V2;
            double P28V;
            double P2V5;
            double P3V3A;
            double P6V;
            double P5V;
            double P3V3D;
            double P4V3;
            double N5V;
            double N6V;
            double P150V;

            bool operator==(const CGraphFSMTelemetryPayload *p /**< [in] the pointer to the struct to compare to*/)
            {
                return (P1V2 == p->P1V2 && P2V2 == p->P2V2 && P28V == p->P28V && P2V5 == p->P2V5 && P3V3A == p->P3V3A && P6V == p->P6V && P5V == p->P5V &&
                        P3V3D == p->P3V3D && P4V3 == p->P4V3 && N5V == p->N5V && N6V == p->N6V && P150V == p->P150V);
            }

            bool operator==(const CGraphFSMTelemetryPayload p /**< [in] the struct to compare to*/)
            {
                return (P1V2 == p.P1V2 && P2V2 == p.P2V2 && P28V == p.P28V && P2V5 == p.P2V5 && P3V3A == p.P3V3A && P6V == p.P6V && P5V == p.P5V &&
                        P3V3D == p.P3V3D && P4V3 == p.P4V3 && N5V == p.N5V && N6V == p.N6V && P150V == p.P150V);
            }

            CGraphFSMTelemetryPayload &operator=(const CGraphFSMTelemetryPayload *p /**< [in] the pointer to the struct to be copied*/)
            {
                this->P1V2 = p->P1V2;
                this->P2V2 = p->P2V2;
                this->P28V = p->P28V;
                this->P2V5 = p->P2V5;
                this->P3V3A = p->P3V3A;
                this->P6V = p->P6V;
                this->P5V = p->P5V;
                this->P3V3D = p->P3V3D;
                this->P4V3 = p->P4V3;
                this->N5V = p->N5V;
                this->N6V = p->N6V;
                this->P150V = p->P150V;
                return *this;
            }

            CGraphFSMTelemetryPayload &operator=(const CGraphFSMTelemetryPayload &p /**< [in] struct to be copied*/)
            {
                this->P1V2 = p.P1V2;
                this->P2V2 = p.P2V2;
                this->P28V = p.P28V;
                this->P2V5 = p.P2V5;
                this->P3V3A = p.P3V3A;
                this->P6V = p.P6V;
                this->P5V = p.P5V;
                this->P3V3D = p.P3V3D;
                this->P4V3 = p.P4V3;
                this->N5V = p.N5V;
                this->N6V = p.N6V;
                this->P150V = p.P150V;
                return *this;
            }
        };

        /**
         * @brief Structure for the response payload of the ADC query command
         */
        union AdcAccumulator
        {
            uint64_t all;
            struct
            {
                int64_t Samples : 24;
                int64_t reserved : 24;
                uint16_t NumAccums;

            } __attribute__((__packed__));

            AdcAccumulator() { all = 0; }

            void log() const
            {
                std::ostringstream oss;
                oss << "AdcAccumulator: Samples: " << (double)Samples << " (x" << (unsigned long)(all >> 32) << (unsigned long)(all) << "), NumAccums: " << (unsigned long)NumAccums << "(0x" << (unsigned long)NumAccums << ")";
                MagAOXAppT::log<text_log>(oss.str());
            }

        } __attribute__((__packed__));

        /**
         * @brief Base class for all the fsm queries
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
         * @brief Child query class that handles sending a telemetry query to the fsm
         */
        class TelemetryQuery : public PZTQuery
        {
        public:
            TelemetryQuery()
            {
                PayloadType = CGraphPayloadTypeFSMTelemetry;
                startLog = "PZTTelemetry: Querying telemetry.";
                endLog = "PZTTelemetry: Finished querying telemetry.";
            }

            const CGraphFSMTelemetryPayload *ParamsPtr = nullptr;
            CGraphFSMTelemetryPayload Telemetry;

            void processReply(char const *Params, const size_t ParamsLen) override
            {
                if ((NULL != Params) && (ParamsLen >= (sizeof(CGraphFSMTelemetryPayload))) )
                {
                    ParamsPtr = reinterpret_cast<const CGraphFSMTelemetryPayload *>(Params);
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
                oss << "PZTTelemetry: Short packet: " << ParamsLen << " (expected " << sizeof(CGraphFSMTelemetryPayload) << " bytes): ";
                MagAOXAppT::log<software_error>({__FILE__, __LINE__, oss.str()});
            }

            void logReply() override
            {
                std::ostringstream oss;
                oss << "BinaryFSMTelemetry Command: Telemetry received";
                // debug
                // oss << "BinaryFSMTelemetry Command: Values with corrected units follow:\n";
                // oss << "P1V2: " << std::fixed << std::setprecision(6) << Telemetry.P1V2 << " V\n";
                // oss << "P2V2: " << std::fixed << std::setprecision(6) << Telemetry.P2V2 << " V\n";
                // oss << "P28V: " << std::fixed << std::setprecision(6) << Telemetry.P28V << " V\n";
                // oss << "P2V5: " << std::fixed << std::setprecision(6) << Telemetry.P2V5 << " V\n";
                // oss << "P3V3A: " << std::fixed << std::setprecision(6) << Telemetry.P3V3A << " V\n";
                // oss << "P6V: " << std::fixed << std::setprecision(6) << Telemetry.P6V << " V\n";
                // oss << "P5V: " << std::fixed << std::setprecision(6) << Telemetry.P5V << " V\n";
                // oss << "P3V3D: " << std::fixed << std::setprecision(6) << Telemetry.P3V3D << " V\n";
                // oss << "P4V3: " << std::fixed << std::setprecision(6) << Telemetry.P4V3 << " V\n";
                // oss << "N5V: " << std::fixed << std::setprecision(6) << Telemetry.N5V << " V\n";
                // oss << "N6V: " << std::fixed << std::setprecision(6) << Telemetry.N6V << " V\n";
                // oss << "P150V: " << std::fixed << std::setprecision(6) << Telemetry.P150V << " V";
                MagAOXAppT::log<text_log>(oss.str());
            }
        };

        /**
         * @brief Child query class that handles querying the fsm for the ADC values
         */
        class AdcsQuery : public PZTQuery
        {
        public:
            AdcsQuery()
            {
                PayloadType = CGraphPayloadTypeFSMAdcs;
                startLog = "PZTAdcs: Querying ADCs.";
                endLog = "PZTAdcs: Finished querying ADCs.";
            }

            const AdcAccumulator *ParamsPtr = nullptr;
            AdcAccumulator AdcVals[3];

            void processReply(char const *Params, const size_t ParamsLen) override
            {
                if ((NULL != Params) && (ParamsLen >= (3 * sizeof(AdcAccumulator))))
                {
                    ParamsPtr = reinterpret_cast<const AdcAccumulator *>(Params);
                    std::copy(ParamsPtr, ParamsPtr + 3, AdcVals);
                }
                else
                {
                    errorLogString(ParamsLen);
                }
            }

            void errorLogString(const size_t ParamsLen) override
            {
                std::ostringstream oss;
                oss << "BinaryFSMAdcsCommand: Short packet: " << ParamsLen << " (expected " << (3 * sizeof(AdcAccumulator)) << " bytes): ";
                MagAOXAppT::log<software_error>({__FILE__, __LINE__, oss.str()});
            }

            void logReply() override
            {
                MagAOXAppT::log<text_log>("BinaryFSMAdcsCommand: ");
                AdcVals[0].log();
                AdcVals[1].log();
                AdcVals[2].log();
            }
        };

        /**
         * @brief Child query class that handles querying the fsm for the DAC values
         * and sending new DAC values to the fsm
         */
        class DacsQuery : public PZTQuery
        {
        public:
            DacsQuery()
            {
                PayloadType = CGraphPayloadTypeFSMDacs;
                startLog = "PZTDacs: Querying DACs.";
                endLog = "PZTDacs: Finished querying DACs.";
            }

            const uint32_t *ParamsPtr = nullptr;
            uint32_t DacSetpoints[3];

            virtual void setPayload(const void *Setpoints, uint16_t SetpointsLen)
            {
                PayloadData = const_cast<void *>(Setpoints);
                PayloadLen = SetpointsLen;
            }

            void processReply(char const *Params, const size_t ParamsLen) override
            {
                if ((NULL != Params) && (ParamsLen >= (3 * sizeof(uint32_t))))
                {
                    ParamsPtr = reinterpret_cast<const uint32_t *>(Params);
                    std::copy(ParamsPtr, ParamsPtr + 3, DacSetpoints);

                    // 1. Currently the fsm appends 58 to query (not set) dac response packages
                    // (24 vs 32 bytes -> there are 8 free bytes that currently are
                    // filled with 58; can replace with another meaningful value if helpful,
                    // details from Summer) - need to remove that.
                    // 2. Also, the returned value is half of the actual value (bit shift operation
                    // on the fsm side) - so will need to multiply by two for the forseeable
                    // future.
                    // NOTE: This is only the case for the queries that 'get' the dac values, not
                    // for the ones that 'set' them, so we need to first check which type of query
                    // we're dealing with. 'Set' queries have an empty payload

                    if (PayloadLen == 0)
                    {
                        // Drop the first two hex digits
                        DacSetpoints[0] = DacSetpoints[0] & 0x00FFFFFF; // Mask out the first two bytes
                        DacSetpoints[1] = DacSetpoints[1] & 0x00FFFFFF; // Mask out the first two bytes
                        DacSetpoints[2] = DacSetpoints[2] & 0x00FFFFFF; // Mask out the first two bytes

                        // // Double
                        // DacSetpoints[0] *= 2;
                        // DacSetpoints[1] *= 2;
                        // DacSetpoints[2] *= 2;
                    }
                }
                else
                {
                    errorLogString(ParamsLen);
                }
            }

            void errorLogString(const size_t ParamsLen) override
            {
                std::ostringstream oss;
                oss << "BinaryFSMDacsCommand: Short packet: " << ParamsLen << " (expected " << (3 * sizeof(uint32_t)) << " bytes): ";
                MagAOXAppT::log<software_error>({__FILE__, __LINE__, oss.str()});
            }

            void logReply() override
            {
                std::ostringstream oss;
                oss << "BinaryFSMDacsCommand: 0x" << std::hex << DacSetpoints[0] << " | 0x" << std::hex << DacSetpoints[1] << " | 0x" << std::hex << DacSetpoints[2];
                MagAOXAppT::log<text_log>(oss.str());
            }
        };

    } // namespace app
} // namespace MagAOX