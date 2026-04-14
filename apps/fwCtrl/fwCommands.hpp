/** \file fwCommands.hpp
 * \brief Utility file for the fwCtrl app with structre and class definitions for commands to be sent to the fw
 *
 * \ingroup fwCtrl_files
 */

#pragma once

#include <iostream>
#include <sstream> // for stringstreams
#include <cstddef> // for nullptr
#include <cstdint> // for std::uint16_t
#include <iomanip> // for std::setprecision
#include <optional>
using std::uint16_t;

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
        // static const uint16_t CGraphPayloadTypeFWHardwareControlStatus = 0x4001U; //Payload: CGraphFWHardwareInterface::CGraphFWHardwareControlRegister
        // static const uint16_t CGraphPayloadTypeFWMotorControlStatus = 0x4002U; //Payload: CGraphFWHardwareInterface::CGraphFWMotorControlStatusRegister
        // static const uint16_t CGraphPayloadTypeFWPositionSenseControlStatus = 0x4003U; //Payload: CGraphFWHardwareInterface::CGraphFWPositionSenseRegister
        // static const uint16_t CGraphPayloadTypeFWPositionSteps = 0x4004U; //Payload: 48 16-bit uint's: PosDetHomeA - PosDet7B of CGraphFWHardwareInterface
        static const uint16_t CGraphPayloadTypeFWTelemetry = 0x4005U;
        static const uint16_t CGraphPayloadTypeFWFilterSelect = 0x4006U; //Payload: uint32 (room to grow?) Read: Which filter is currently in position (1-8; 0 means the filterwheel is in transit to a new position); Write: move to the given filter (1-8)FILTERWHEEL_ONE = 1,

        /**
         * @brief Structure for the response payload of the Telemetry command
         */
        struct CGraphFWTelemetryPayload
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

            bool operator==(const CGraphFWTelemetryPayload *p /**< [in] the pointer to the struct to compare to*/)
            {
                return (P1V2 == p->P1V2 && P2V2 == p->P2V2 && P28V == p->P28V && P2V5 == p->P2V5 && P6V == p->P6V && P5V == p->P5V &&
                        P3V3D == p->P3V3D && P4V3 == p->P4V3 && P2I2 == p->P2I2 && P4I3 == p->P4I3 && P6I == p->P6I);
            }

            bool operator==(const CGraphFWTelemetryPayload &p /**< [in] the struct to compare to*/) const
            {
                return (P1V2 == p.P1V2 && P2V2 == p.P2V2 && P28V == p.P28V && P2V5 == p.P2V5 && P6V == p.P6V && P5V == p.P5V &&
                        P3V3D == p.P3V3D && P4V3 == p.P4V3 && P2I2 == p.P2I2 && P4I3 == p.P4I3 && P6I == p.P6I);
            }

            CGraphFWTelemetryPayload &operator=(const CGraphFWTelemetryPayload *p /**< [in] the pointer to the struct to be copied*/)
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

            CGraphFWTelemetryPayload &operator=(const CGraphFWTelemetryPayload &p /**< [in] struct to be copied*/)
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

        enum class FWFilterSelectPositions : int32_t {
            FW_MOVING = -1,
            FW_SUNSAFE = 0,
            FILTERWHEEL_ONE = 1,
            FILTERWHEEL_TWO = 2,
            FILTERWHEEL_THREE = 3,
            FILTERWHEEL_FOUR = 4,
            FILTERWHEEL_FIVE = 5,
            FILTERWHEEL_SIX = 6,
            FILTERWHEEL_SEVEN = 7,
            FILTERWHEEL_EIGHT = 8
        };

        /**
         * @brief Parse a int32 value into a FWFilterSelectPositions
         * @return std::optional with a valid enum value, or std::nullopt if invalid
         */
        std::optional<FWFilterSelectPositions> toFWFilterPositions(int32_t val) noexcept
        {
            switch (val)
            {
                case -1: return FWFilterSelectPositions::FW_MOVING;
                case 0: return FWFilterSelectPositions::FW_SUNSAFE;
                case 1: return FWFilterSelectPositions::FILTERWHEEL_ONE;
                case 2: return FWFilterSelectPositions::FILTERWHEEL_TWO;
                case 3: return FWFilterSelectPositions::FILTERWHEEL_THREE;
                case 4: return FWFilterSelectPositions::FILTERWHEEL_FOUR;
                case 5: return FWFilterSelectPositions::FILTERWHEEL_FIVE;
                case 6: return FWFilterSelectPositions::FILTERWHEEL_SIX;
                case 7: return FWFilterSelectPositions::FILTERWHEEL_SEVEN;
                case 8: return FWFilterSelectPositions::FILTERWHEEL_EIGHT;
                default: return std::nullopt;
            }
        }


        // Derived classes
        /**
         * @brief Child query class that handles sending a telemetry query to the fw
         */
        class TelemetryQuery : public dev::sdevQuery
        {
        public:
            TelemetryQuery()
            {
                PayloadType = CGraphPayloadTypeFWTelemetry;
                startLog = "PZTTelemetry: Querying telemetry.";
                endLog = "PZTTelemetry: Finished querying telemetry.";
            }

            const CGraphFWTelemetryPayload *ParamsPtr = nullptr;
            CGraphFWTelemetryPayload Telemetry;

            void processReply(char const *Params, const size_t ParamsLen) override
            {
                if ((NULL != Params) && (ParamsLen >= (sizeof(CGraphFWTelemetryPayload))) )
                {
                    ParamsPtr = reinterpret_cast<const CGraphFWTelemetryPayload *>(Params);
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
                oss << "PZTTelemetry: Short packet: " << ParamsLen << " (expected " << sizeof(CGraphFWTelemetryPayload) << " bytes): ";
                MagAOXAppT::log<software_error>({__FILE__, __LINE__, oss.str()});
            }

            void logReply() override
            {
                if(MagAOXAppT::m_log.logLevel() >= flatlogs::logPrio::LOG_DEBUG)
                {
                    std::ostringstream oss;
                    oss << "BinaryFWTelemetry Command: Telemetry received";
                    oss << "BinaryFWTelemetry Command: Values with corrected units follow:\n";
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
                    oss << "P6I: " << std::fixed << std::setprecision(6) << Telemetry.P6I << " V\n";
                    MagAOXAppT::log<software_debug>({__FILE__, __LINE__, oss.str()});
                }
             
                MagAOXAppT::log<software_info>({__FILE__, __LINE__, "BinaryFWTelemetry Command: Telemetry received"});
            }
        };


        // Derived classes
        /**
         * @brief Child query class that handles sending a position query to the fw
         */
        class FilterSelectQuery : public dev::sdevQuery
        {
        public:
            FilterSelectQuery()
            {
                PayloadType = CGraphPayloadTypeFWFilterSelect;
                startLog = "PZTFilterSelect: Querying filter select.";
                endLog = "PZTFilterSelect: Finished querying filter select.";
            }

            const int32_t *ParamsPtr = nullptr; ///< Pointer to the raw position data last received
            std::optional<FWFilterSelectPositions> FilterSelect; ///< The filter select position last received;


            virtual void setPayload(const void *Position, uint16_t PositionLen)
            {
                PayloadData = const_cast<void *>(Position);
                PayloadLen = PositionLen;
            }

            void processReply(char const *Params, const size_t ParamsLen) override
            {
                if ((NULL != Params) && (ParamsLen >= (sizeof(int32_t))) )
                {
                    ParamsPtr = reinterpret_cast<const int32_t *>(Params);
                    int32_t raw = *ParamsPtr;
                    auto pos = toFWFilterPositions(raw);
                    if (pos)
                    {
                        FilterSelect = pos;
                    }
                    else
                    {
                        // Clear stored value and log the unexpected payload
                        // !! Do we want to do this, or just ignore invalid values and keep the last known good value?
                        FilterSelect.reset();
                        std::ostringstream oss;
                        oss << "PZTFilterSelect: unexpected position value: " << raw;
                        MagAOXAppT::log<software_error>({__FILE__, __LINE__, oss.str()});
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
                oss << "PZTFilterSelect: Short packet: " << ParamsLen << " (expected " << sizeof(int32_t) << " bytes): ";
                MagAOXAppT::log<software_error>({__FILE__, __LINE__, oss.str()});
            }

            void logReply() override
            {
                if(MagAOXAppT::m_log.logLevel() >= flatlogs::logPrio::LOG_DEBUG)
                {
                    std::ostringstream oss;
                    oss << "PZTFilterSelect: Position received: ";
                    if (FilterSelect)
                        oss << static_cast<int32_t>(*FilterSelect);
                    else
                        oss << "(invalid)";
                    MagAOXAppT::log<software_debug>({__FILE__, __LINE__, oss.str()});
                }
             
                MagAOXAppT::log<software_info>({__FILE__, __LINE__, "PZTFilterSelect: Position received"});
            }
        };

    } // namespace app
} // namespace MagAOX