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

            // Overload operator<< for logging
            friend std::ostream& operator<<(std::ostream& os, const CGraphDMMappingPayload& payload) {
                os << "{ControllerBoardIndex: " << static_cast<int>(payload.ControllerBoardIndex)
                << ", DacIndex: " << static_cast<int>(payload.DacIndex)
                << ", DacChannel: " << static_cast<int>(payload.DacChannel) << "}";
                return os;
            }
        };

        struct CGraphDMMappings
        {
            CGraphDMMappingPayload* Mappings;
            size_t size;

            // Constructor to initialize the array with an optional size parameter, defaulting to DMMaxActuators
            CGraphDMMappings(size_t actuatorCount = DMMaxActuators) : size(actuatorCount) {
                // Allocate memory
                Mappings = new CGraphDMMappingPayload[size];

                // Should we initialize the mappings to an 'unset' value (-1)?
            }

            // Destructor to free the allocated memory
            ~CGraphDMMappings() {
                delete[] Mappings; // Free memory
            }

            size_t length() const {
                return size;
            }

            // Overload operator<< for logging
            friend std::ostream& operator<<(std::ostream& os, const CGraphDMMappings& mappings) {
                for (size_t i = 0; i < DMMaxActuators; i++)
                {
                    os << "DMMapping: Pixel " << i << ": " << mappings.Mappings[i] << "\n";
                }
                return os;
            }

            // Method to reject out of bounds access
            CGraphDMMappingPayload& operator[](size_t index) {
                if (index >= size) {
                    throw std::out_of_range("Index out of bounds");
                }
                return Mappings[index];
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

            // Assumes that SetPoints is an array of uint16_t values, each corresponding to a pixel.
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
                    const CGraphDMPixelPayloadHeader PixelHeader = *reinterpret_cast<const CGraphDMPixelPayloadHeader*>(Params);
                    const uint16_t StartPixel = PixelHeader.StartPixel;
                    std::ostringstream oss;
                    oss << "DMShortPixels: Returned StartPixel: " << StartPixel;
                    MagAOXAppT::log<text_log>( oss.str());
                    
                    uint16_t NumPixels = (ParamsLen - sizeof(CGraphDMPixelPayloadHeader)) / sizeof(uint16_t);
                    if ((NumPixels + StartPixel) > DMMaxActuators) 
                    {
                        oss.str("");
                        oss << "DMShortPixels: Invalid NumPixels (truncating):  " << NumPixels;
                        MagAOXAppT::log<text_log>(oss.str());
                        NumPixels = DMMaxActuators - StartPixel; 
                    }
                                    
                    for (size_t i = 0; i < NumPixels; i++)
                    {
                        const uint16_t Pixel = *reinterpret_cast<const uint16_t*>(Params+sizeof(CGraphDMPixelPayloadHeader)+(i*sizeof(uint16_t)));
                        oss.str("");
                        oss << "DMShortPixels: Pixel " << i << ": " << Pixel;
                        MagAOXAppT::log<text_log>(oss.str());
                    }
                }
                else
                {
                    MagAOXAppT::log<software_error>({__FILE__, __LINE__, "DMShortPixels: Empty packet returned!"});
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
                oss << "DMShortPixels: Short pixels received";
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
                PayloadType = CGraphPayloadTypeDMLongPixels;
                startLog = "DMLongPixels: Querying long pixels.";
                endLog = "DMLongPixels: Finished querying long pixels.";
            }

            // const CGraphDMTelemetryPayload *ParamsPtr = nullptr;
            // CGraphDMTelemetryPayload Telemetry;

            // Assumes that SetPoints is an array of uint8_t values, where each group of 3 successive values makes a pixel.
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
                    const CGraphDMPixelPayloadHeader PixelHeader = *reinterpret_cast<const CGraphDMPixelPayloadHeader*>(Params);
                    const unsigned long StartPixel = PixelHeader.StartPixel;
                    std::ostringstream oss;
                    oss << "DMLongPixels: Returned StartPixel: " << StartPixel;
                    MagAOXAppT::log<text_log>( oss.str());

                    // Each long pixel is 3 bytes (24-bit) packed in the payload
                    unsigned long NumPixels = (ParamsLen - sizeof(CGraphDMPixelPayloadHeader)) / (3 * sizeof(uint8_t));
                    
                    if ((NumPixels + StartPixel) > DMMaxActuators) 
                    {
                        oss.str("");
                        oss << "DMLongPixels: Invalid NumPixels (truncating):  " << NumPixels;
                        MagAOXAppT::log<text_log>(oss.str());
                        NumPixels = DMMaxActuators - StartPixel; 
                    }
                                    
                    for (size_t i = 0; i < NumPixels; i++)
                    {
                        // Read 4 bytes at the 3-byte-aligned offset and mask to 24 bits
                        const uint32_t Pixel = *reinterpret_cast<const uint32_t*>(Params+sizeof(CGraphDMPixelPayloadHeader)+(i*3*sizeof(uint8_t))) & 0x00FFFFFFUL;
                        oss.str("");
                        oss << "DMLongPixels: Pixel " << i << ": " << Pixel;
                        MagAOXAppT::log<text_log>(oss.str());
                    }
                }
                else
                {
                    MagAOXAppT::log<software_error>({__FILE__, __LINE__, "DMLongPixels: Empty packet returned!"});
                }
            }

            void errorLogString(const size_t ParamsLen) override
            {
                std::ostringstream oss;
                oss << "DMLongPixels: Short packet: " << ParamsLen << " (expected at least " << sizeof(CGraphDMPixelPayloadHeader) << " bytes): ";
                MagAOXAppT::log<software_error>({__FILE__, __LINE__, oss.str()});
            }

            void logReply() override
            {
                std::ostringstream oss;
                oss << "DMLongPixels: Long pixels received";
                // debug
                MagAOXAppT::log<text_log>(oss.str());
            }
        };

        // Derived class
        /**
         * @brief Child query class that handles sending a dither (8-bit) pixels query to the dm
         */
        class DitherQuery : public dev::sdevQuery
        {
        public:
            DitherQuery()
            {
                PayloadType = CGraphPayloadTypeDMDither;
                startLog = "DMDither: Querying dither pixels.";
                endLog = "DMDither: Finished querying dither pixels.";
            }

            // Assumes that SetPoints is an array of uint8_t values, each corresponding to a pixel.
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
                    const CGraphDMPixelPayloadHeader PixelHeader = *reinterpret_cast<const CGraphDMPixelPayloadHeader*>(Params);
                    const unsigned long StartPixel = PixelHeader.StartPixel;
                    std::ostringstream oss;
                    oss << "DMDither: Returned StartPixel: " << StartPixel;
                    MagAOXAppT::log<text_log>( oss.str());
                    unsigned long NumPixels = (ParamsLen - sizeof(CGraphDMPixelPayloadHeader)) / sizeof(uint8_t);

                    if ((NumPixels + StartPixel) > DMMaxActuators)
                    {
                        oss.str("");
                        oss << "DMDither: Invalid NumPixels (truncating):  " << NumPixels;
                        MagAOXAppT::log<text_log>(oss.str());
                        NumPixels = DMMaxActuators - StartPixel;
                    }

                    for (size_t i = 0; i < NumPixels; i++)
                    {
                        const uint8_t Pixel = *reinterpret_cast<const uint8_t*>(Params+sizeof(CGraphDMPixelPayloadHeader)+(i*sizeof(uint8_t)));
                        oss.str("");
                        oss << "DMDither: Pixel " << i << ": " << static_cast<int>(Pixel);
                        MagAOXAppT::log<text_log>(oss.str());
                    }
                }
                else
                {
                    MagAOXAppT::log<software_error>({__FILE__, __LINE__, "DMDither: Empty packet returned!"});
                }
            }

            void errorLogString(const size_t ParamsLen) override
            {
                std::ostringstream oss;
                oss << "DMDither: Short packet: " << ParamsLen << " (expected at least " << sizeof(CGraphDMPixelPayloadHeader) << " bytes): ";
                MagAOXAppT::log<software_error>({__FILE__, __LINE__, oss.str()});
            }

            void logReply() override
            {
                std::ostringstream oss;
                oss << "DMDither: Dither pixels received";
                MagAOXAppT::log<text_log>(oss.str());
            }
        };

        // Derived class
        /**
         * @brief Child query class that handles sending a mapping query to the dm
         */
        class MappingQuery : public dev::sdevQuery
        {
        public:
            MappingQuery()
            {
                PayloadType = CGraphPayloadTypeDMMappings;
                startLog = "DMMapping: Querying mapping.";
                endLog = "DMMapping: Finished querying mapping.";
            }

            // const CGraphDMTelemetryPayload *ParamsPtr = nullptr;
            // CGraphDMTelemetryPayload Telemetry;

            // Assumes that MappingPayloads is an array of CGraphDMMappingPayload values.
            virtual void setPayload(const void *MappingPayloads, uint16_t MappingPayloadsLen, uint16_t StartPixel=0)
            {
                // Make payload header
                CGraphDMPixelPayloadHeader payloadHeader(StartPixel);

                // Calculate sizes
                const size_t headerSize = sizeof(payloadHeader);
                const size_t totalSize = headerSize + static_cast<size_t>(MappingPayloadsLen);

                // Allocate new buffer for the combined payload.
                uint8_t *buffer = new uint8_t[totalSize];

                // Copy header first.
                std::memcpy(buffer, &payloadHeader, headerSize);
                // Then copy the setpoints after the header.
                std::memcpy(buffer + headerSize, MappingPayloads, MappingPayloadsLen);

                // Save new payload.
                PayloadData = buffer;
                PayloadLen = totalSize;
            }

            void processReply(char const *Params, const size_t ParamsLen) override
            {
                if ( (nullptr != Params) && (ParamsLen >= sizeof(CGraphDMPixelPayloadHeader)) )
                {
                    const CGraphDMPixelPayloadHeader PixelHeader = *reinterpret_cast<const CGraphDMPixelPayloadHeader*>(Params);
                    const unsigned long StartPixel = PixelHeader.StartPixel;
                    std::ostringstream oss;
                    oss << "DMMapping: Returned StartPixel: " << StartPixel;
                    MagAOXAppT::log<text_log>( oss.str());                    
                    unsigned long NumPixels = (ParamsLen - sizeof(CGraphDMPixelPayloadHeader)) / sizeof(CGraphDMMappingPayload);
                    
                    if ((NumPixels + StartPixel) > DMMaxActuators) 
                    {
                        oss.str("");
                        oss << "DMMapping: Invalid NumPixels (truncating):  " << NumPixels;
                        MagAOXAppT::log<text_log>(oss.str());
                        NumPixels = DMMaxActuators - StartPixel; 
                    }
                                    
                    for (size_t i = 0; i < NumPixels; i++)
                    {
                        oss.str("");
                        const CGraphDMMappingPayload Mapping = *reinterpret_cast<const CGraphDMMappingPayload*>(Params+sizeof(CGraphDMPixelPayloadHeader)+(i*sizeof(CGraphDMMappingPayload)));
                        oss << "DMMapping: Pixel " << i << ": " << Mapping;
                        MagAOXAppT::log<text_log>(oss.str());
                    }
                }
                else
                {
                    MagAOXAppT::log<software_error>({__FILE__, __LINE__, "DMMapping: Empty packet returned!"});
                }
            }

            void errorLogString(const size_t ParamsLen) override
            {
                std::ostringstream oss;
                oss << "DMMapping: Short packet: " << ParamsLen << " (expected " << sizeof(CGraphDMPixelPayloadHeader) << " bytes): ";
                MagAOXAppT::log<software_error>({__FILE__, __LINE__, oss.str()});
            }

            void logReply() override
            {
                std::ostringstream oss;
                oss << "DMMapping: Mapping received";
                // debug
                MagAOXAppT::log<text_log>(oss.str());
            }
        };

    } // namespace app
} // namespace MagAOX