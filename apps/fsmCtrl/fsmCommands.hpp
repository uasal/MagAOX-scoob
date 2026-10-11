/** \file fsmCommands.hpp
 * \brief Utility file for the fsmCtrl app with structre and class definitions for commands to be sent to the fsm
 *
 * \ingroup fsmCtrl_files
 */

#pragma once

#include <iostream>
#include <sstream> // for stringstreams
#include <cstddef> // for nullptr
#include <atomic>  // for std::atomic
#include <iomanip> // for std::setw, std::setfill
#include <limits>  // for std::numeric_limits
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
        static const uint16_t CGraphPayloadTypeFSMAdcs = 0x2003U;              // Payload: 3 AdcAcumulators
        static const uint16_t CGraphPayloadTypeFSMAdcsFloatingPoint = 0x2004U; // Payload: 3 double-precision floats
        static const uint16_t CGraphPayloadTypeFSMDacsFloatingPoint = 0x2005U; // Payload: 3 double-precision floats
        static const uint16_t CGraphPayloadTypeFSMTelemetry = 0x2006U;
        static const uint16_t CGraphPayloadTypeFSMHardwareConfigRegister = 0x2007U;
        static const uint16_t CGraphPayloadTypeFSMAdcConfigRegister = 0x2008U;
        static const uint16_t CGraphPayloadTypeFSMAccumulatorConfigRegister = 0x2009U;
        static const uint16_t CGraphPayloadTypeFSMDacConfigRegister = 0x200AU;

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
         *
         * Process CGraphPayloadTypeFSMAdcs request (as defined in firmware include/cgraph/CGraphCommon.hpp and
         * FineSteeringMirrorController/processor/CmdHandlersBinary.cpp):
         */
        union AdcAccumulator
        {
            uint64_t all;
            struct
            {
                int64_t Samples : 32;    ///< Bits 0-31 of the sum of NumAccums ADC readings.
                int64_t reserved : 16;   ///< Bits 32-47 of the sum (named "reserved" in the firmware).
                uint64_t NumAccums : 16; ///< Number of ADC readings in the sum. 0 means no new readings since the last read.

            } __attribute__((__packed__));

            AdcAccumulator() { all = 0; }

            /// The full signed 48-bit sum of ADC readings (Samples plus reserved as the high bits).
            int64_t Sum() const
            {
                // Shift bit 47 up to the sign bit to drop NumAccums, then shift back to extend the sign.
                return static_cast<int64_t>(all << 16) >> 16;
            }

            /// The average ADC reading in volts: Sum / NumAccums, scaled by 4.096 / 2^23 (from processor/CmdHandlersBinary.cpp).
            /**
             * \returns NaN if NumAccums is 0
             */
            double Volts() const
            {
                if (NumAccums == 0)
                {
                    return std::numeric_limits<double>::quiet_NaN();
                }
                return (4.096 * (static_cast<double>(Sum()) / static_cast<double>(NumAccums))) / 8388608.0;
            }

            /// Log the decoded value, then the raw 8 bytes as NumAccums | sum bits 47-32 | sum bits 31-0.
            void log() const
            {
                std::ostringstream oss;
                oss << "AdcAccumulator: " << std::scientific << std::setprecision(3) << Volts() << " V (avg " << std::fixed << std::setprecision(2)
                    << (NumAccums == 0 ? 0.0 : static_cast<double>(Sum()) / static_cast<double>(NumAccums)) << " ADC units = " << Sum() << " / "
                    << static_cast<unsigned>(NumAccums) << " readings); raw " << std::hex << std::setfill('0') << std::setw(4) << ((all >> 48) & 0xFFFF)
                    << "|" << std::setw(4) << ((all >> 32) & 0xFFFF) << "|" << std::setw(8) << (all & 0xFFFFFFFF);
                MagAOXAppT::log<text_log>(oss.str());
            }

        } __attribute__((__packed__));

        static_assert(sizeof(AdcAccumulator) == 8, "AdcAccumulator must match the 8-byte firmware layout");

        // /**
        //  * @brief Base class for all the fsm queries
        //  *
        //  * PZTQuery is the class from which all the query classes inherit.
        //  * It ensures that the all implement a minimal interfaces that includes
        //  * processReply, logReply and errorLogString.
        //  */
        // class PZTQuery
        // {
        // public:
        //     std::string startLog = "";
        //     std::string endLog = "";

        //     virtual ~PZTQuery() = default;
        //     virtual void errorLogString(const size_t ParamsLen) = 0;
        //     virtual void processReply(char const *Params, const size_t ParamsLen) = 0;
        //     virtual void logReply() = 0;
        //     virtual uint16_t getPayloadType() const
        //     {
        //         return PayloadType;
        //     }
        //     virtual void *getPayloadData() const
        //     {
        //         return PayloadData;
        //     }
        //     virtual uint16_t getPayloadLen() const
        //     {
        //         return PayloadLen;
        //     }
        //     virtual void setPayload(void *newPayloadData, uint16_t newPayloadLen)
        //     {
        //         PayloadData = newPayloadData;
        //         PayloadLen = newPayloadLen;
        //     }
        //     virtual void resetPayload()
        //     {
        //         PayloadData = DefaultPayloadData;
        //         PayloadLen = DefaultPayloadLen;
        //     }

        // protected:
        //     uint16_t PayloadType = -1;
        //     void *DefaultPayloadData = NULL;
        //     size_t DefaultPayloadLen = 0;
        //     void *PayloadData = DefaultPayloadData;
        //     size_t PayloadLen = DefaultPayloadLen;
        // };

        // Derived classes
        /**
         * @brief Child query class that handles sending a telemetry query to the fsm
         */
        class TelemetryQuery : public dev::sdevQuery
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
        class AdcsQuery : public dev::sdevQuery
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

            /// Number of valid ADC replies processed so far, so pollers can detect a new reply.
            std::atomic<uint64_t> m_replyCount{0};

            /// If false, logReply does nothing. Cleared by fsmCtrl while the ADC stream is polling, so replies aren't logged at the poll rate.
            std::atomic<bool> m_logReplies{true};

            void processReply(char const *Params, const size_t ParamsLen) override
            {
                if ((NULL != Params) && (ParamsLen >= (3 * sizeof(AdcAccumulator))))
                {
                    ParamsPtr = reinterpret_cast<const AdcAccumulator *>(Params);
                    std::copy(ParamsPtr, ParamsPtr + 3, AdcVals);
                    ++m_replyCount;
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
                if (!m_logReplies)
                {
                    return;
                }

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
        class DacsQuery : public dev::sdevQuery
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