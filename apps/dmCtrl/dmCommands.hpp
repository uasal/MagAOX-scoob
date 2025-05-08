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
        static const uint16_t DMMaxControllerBoards = 6;
        static const uint16_t DMMDacsPerControllerBoard = 4;
        static const uint16_t DMActuatorsPerDac = 40;
        static const uint16_t DMMaxActuators = DMActuatorsPerDac * DMMDacsPerControllerBoard * DMMaxControllerBoards;        

        /**
         * @brief Payload types for the commands
         *
         * The payload type is sent with the command packet and
         * tells the dm what command is being sent.
         */
        static const uint16_t CGraphPayloadTypeDMTelemetry = 0x3004U;
        static const uint16_t CGraphPayloadTypeDMMappings = 0x300BU; //Payload: CGraphDMPixelPayloadHeader followed by one or more CGraphDMMappingPayload structs (num defined by packet payload length filed)
        static const uint16_t CGraphPayloadTypeDMShortPixels = 0x300CU; //Payload: CGraphDMPixelPayloadHeader followed by one or more 16b pixel values (num defined by packet payload length filed)
        static const uint16_t CGraphPayloadTypeDMDither = 0x300DU; //Payload: CGraphDMPixelPayloadHeader followed by one or more 8b dither values (num defined by packet payload length filed)[we reserve the right to be really tricky and bitpack multiple pixels per byte since dither will always be <8b / pix]
        static const uint16_t CGraphPayloadTypeDMLongPixels = 0x300EU; //Payload: CGraphDMPixelPayloadHeader followed by one or more 24b pixel values (num defined by packet payload length filed)- this is gonna cause some funky math & casts when parsing packet to ram...

        /**
         * @brief Structure for the response payload of the Telemetry command
         */
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

        struct CGraphDMPixelPayloadHeader
        {
            uint16_t StartPixel;
            
            CGraphDMPixelPayloadHeader() : StartPixel(0) { }
            CGraphDMPixelPayloadHeader(uint16_t sp) : StartPixel(sp) { }
        };
        
        struct CGraphDMMappingPayload
        {
            uint8_t ControllerBoardIndex; // 0 ... DMMaxControllerBoards - 1
            uint8_t DacIndex; // 0 ... DMMDacsPerControllerBoard - 1
            uint8_t DacChannel; // 0 ... DMActuatorsPerDac - 1
            
            CGraphDMMappingPayload() : ControllerBoardIndex(0), DacIndex(0), DacChannel(0) { }
            CGraphDMMappingPayload(unsigned long bi, unsigned long di, unsigned long dc) : ControllerBoardIndex(bi), DacIndex(di), DacChannel(dc) { }
        };
        
        //May send multiple copies per packet; array of 1...N of the following:
        struct CGraphDMMappings
        {
            CGraphDMMappingPayload Mappings[DMMaxActuators];
            
            //Let's just make a default initialization so it's not totally null until uploaded, cause that causes all actuators to write ram0:0:0 over & over
            CGraphDMMappings()
            {
                uint8_t ControllerBoardIndex = 0;
                uint8_t DacIndex = 0;
                uint8_t DacChannel = 0;
        
                for (size_t i = 0; i < DMMaxActuators; i++)
                {
                    Mappings[i].ControllerBoardIndex = ControllerBoardIndex;
                    Mappings[i].DacIndex = DacIndex;
                    Mappings[i].DacChannel = DacChannel;
                    
                    DacChannel++;
                    if (DacChannel >= DMActuatorsPerDac)
                    {
                        DacChannel = 0;
                        DacIndex++;
                        if (DacIndex >= DMMDacsPerControllerBoard)
                        {
                            DacIndex = 0;
                            ControllerBoardIndex++;
                            if (ControllerBoardIndex >= DMMaxControllerBoards)
                            {
                                //We really really really shouldn't get here, but just in case we do it's better than crashing...
                                ControllerBoardIndex = 0;
                            }
                        }
                    }
                }
            }
        };


        // Derived class
        /**
         * @brief Child query class that handles sending a telemetry query to the dm
         */
        class TelemetryQuery : public dev::sdevQuery
        {
        public:
            TelemetryQuery()
            {
                PayloadType = CGraphPayloadTypeDMTelemetry;
                startLog = "DMTelemetry: Querying telemetry.";
                endLog = "DMTelemetry: Finished querying telemetry.";
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
                oss << "DMTelemetry Command: Telemetry received\n";
                // debug
                oss << "DMTelemetry Command: Values with corrected units follow:\n";
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

        // Derived class
        /**
         * @brief Child query class that handles sending a short pixels query to the dm
         */
        class ShortPixelsQuery : public dev::sdevQuery
        {
        public:
            ShortPixelsQuery()
            {
                PayloadType = CGraphPayloadTypeDMShortPixels;
                startLog = "DMShortPixels: Querying short pixels.";
                endLog = "DMShortPixels: Finished querying short pixels.";
            }

            // const uint32_t *ParamsPtr = nullptr;
            // uint32_t DacSetpoints[3];

            virtual void setPayload(const void *Setpoints, uint16_t SetpointsLen, uint16_t StartPixel=0)
            {
                // Make payload header
                CGraphDMPixelPayloadHeader payloadHeader(StartPixel);

                // Calculate sizes
                const size_t headerSize = sizeof(payloadHeader);
                const size_t totalSize = headerSize + static_cast<size_t>(SetpointsLen);

                // Allocate new buffer for the combined payload.
                uint8_t *buffer = new uint8_t[totalSize];

                // Copy header first.
                std::memcpy(buffer, &payloadHeader, headerSize);
                // Then copy the setpoints after the header.
                std::memcpy(buffer + headerSize, Setpoints, SetpointsLen);

                // Save new payload.
                PayloadData = buffer;
                PayloadLen = totalSize;
            }

            void processReply(char const *Params, const size_t ParamsLen) override
            {
                if ( (nullptr != Params) && (ParamsLen >= sizeof(CGraphDMPixelPayloadHeader)) )
                {
                    // !!! At the moment, different exit points in the BinaryDMShortPixelsCommand function build the package differently.
                    // If the setpoints are set successfully, the return payload only contains the nb of setpoints set.
                    // However, other return payloads contains the full header + setpoints or variations thereof.
                    // !!! Leaving it like this for now, but we need a starndard payload structure here.
                    const CGraphDMPixelPayloadHeader PixelHeader = *reinterpret_cast<const CGraphDMPixelPayloadHeader*>(Params);
                    const uint16_t StartPixel = PixelHeader.StartPixel;
                    std::ostringstream oss;
                    oss << "BinaryDMShortPixelsCommand: Returned StartPixel: " << StartPixel;
                    MagAOXAppT::log<text_log>( oss.str());
                    
                    uint16_t NumPixels = (ParamsLen - sizeof(CGraphDMPixelPayloadHeader)) / sizeof(uint16_t);
                    if ((NumPixels + StartPixel) > DMMaxActuators) 
                    {
                        oss << "BinaryDMShortPixelsCommand: Invalid NumPixels (truncating):  " << NumPixels;
                        MagAOXAppT::log<text_log>(oss.str());
                        NumPixels = DMMaxActuators - StartPixel; 
                    }
                                    
                    for (size_t i = 0; i < NumPixels; i++)
                    {
                        const uint16_t Pixel = *reinterpret_cast<const uint16_t*>(Params+sizeof(CGraphDMPixelPayloadHeader)+(i*sizeof(uint16_t)));
                        oss << "BinaryDMShortPixelsCommand: Pixel " << i << ": " << Pixel;
                        MagAOXAppT::log<text_log>(oss.str());
                    }
                }
                else
                {
                    MagAOXAppT::log<software_error>({__FILE__, __LINE__, "BinaryDMShortPixelsCommand: Empty packet returned!"});
                }
            }

            void errorLogString(const size_t ParamsLen) override
            {
                std::ostringstream oss;
                oss << "DMShortPixels: Short packet: " << ParamsLen << " (expected " << sizeof(CGraphDMTelemetryPayload) << " bytes): ";
                MagAOXAppT::log<software_error>({__FILE__, __LINE__, oss.str()});
            }

            void logReply() override
            {
                std::ostringstream oss;
                oss << "DMShortPixels Command: Short pixels received";
                // debug
                MagAOXAppT::log<text_log>(oss.str());
            }
        };

        // Derived class
        /**
         * @brief Child query class that handles sending a long pixels query to the dm
         */
        class LongPixelsQuery : public dev::sdevQuery
        {
        public:
            LongPixelsQuery()
            {
                PayloadType = CGraphPayloadTypeDMShortPixels;
                startLog = "DMTelemetry: Querying short pixels.";
                endLog = "DMTelemetry: Finished querying short pixels.";
            }

            // const CGraphDMTelemetryPayload *ParamsPtr = nullptr;
            // CGraphDMTelemetryPayload Telemetry;

            virtual void setPayload(const void *Setpoints, uint16_t SetpointsLen, uint16_t StartPixel=0)
            {
                // Make payload header
                CGraphDMPixelPayloadHeader payloadHeader(StartPixel);

                // Calculate sizes
                const size_t headerSize = sizeof(payloadHeader);
                const size_t totalSize = headerSize + static_cast<size_t>(SetpointsLen);

                // Allocate new buffer for the combined payload.
                uint8_t *buffer = new uint8_t[totalSize];

                // Copy header first.
                std::memcpy(buffer, &payloadHeader, headerSize);
                // Then copy the setpoints after the header.
                std::memcpy(buffer + headerSize, Setpoints, SetpointsLen);

                // Save new payload.
                PayloadData = buffer;
                PayloadLen = totalSize;
            }

            void processReply(char const *Params, const size_t ParamsLen) override
            {
                if ( (nullptr != Params) && (ParamsLen >= sizeof(CGraphDMPixelPayloadHeader)) )
                {
                    // !!! At the moment, different exit points in the BinaryDMShortPixelsCommand function build the package differently.
                    // If the setpoints are set successfully, the return payload only contains the nb of setpoints set.
                    // However, other return payloads contains the full header + setpoints or variations thereof.
                    // !!! Leaving it like this for now, but we need a starndard payload structure here.
                    const CGraphDMPixelPayloadHeader PixelHeader = *reinterpret_cast<const CGraphDMPixelPayloadHeader*>(Params);
                    const uint16_t StartPixel = PixelHeader.StartPixel;
                    std::ostringstream oss;
                    oss << "BinaryDMShortPixelsCommand: Returned StartPixel: " << StartPixel;
                    MagAOXAppT::log<text_log>( oss.str());
                    
                    uint16_t NumPixels = (ParamsLen - sizeof(CGraphDMPixelPayloadHeader)) / sizeof(uint16_t);
                    if ((NumPixels + StartPixel) > DMMaxActuators) 
                    {
                        oss << "BinaryDMShortPixelsCommand: Invalid NumPixels (truncating):  " << NumPixels;
                        MagAOXAppT::log<text_log>(oss.str());
                        NumPixels = DMMaxActuators - StartPixel; 
                    }
                                    
                    for (size_t i = 0; i < NumPixels; i++)
                    {
                        const uint16_t Pixel = *reinterpret_cast<const uint16_t*>(Params+sizeof(CGraphDMPixelPayloadHeader)+(i*sizeof(uint16_t)));
                        oss << "BinaryDMShortPixelsCommand: Pixel " << i << ": " << Pixel;
                        MagAOXAppT::log<text_log>(oss.str());
                    }
                }
                else
                {
                    MagAOXAppT::log<software_error>({__FILE__, __LINE__, "BinaryDMShortPixelsCommand: Empty packet returned!"});
                }
            }

            void errorLogString(const size_t ParamsLen) override
            {
                std::ostringstream oss;
                oss << "DMTelemetry: Short packet: " << ParamsLen << " (expected " << sizeof(CGraphDMTelemetryPayload) << " bytes): ";
                MagAOXAppT::log<software_error>({__FILE__, __LINE__, oss.str()});
            }

            void logReply() override
            {
                std::ostringstream oss;
                oss << "DMShortPixels Command: Short pixels received";
                // debug
                MagAOXAppT::log<text_log>(oss.str());
            }
        };

    } // namespace app
} // namespace MagAOX