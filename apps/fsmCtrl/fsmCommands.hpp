/** \file fsmCommands.hpp
  * \brief Currently defined commands for the fsmCtrl app
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

static const uint16_t CGraphPayloadTypePZTDacs = 0x0002U; //Payload: 3 uint32's
static const uint16_t CGraphPayloadTypePZTDacsFloatingPoint = 0x0003U; //Payload: 3 double-precision floats
static const uint16_t CGraphPayloadTypePZTAdcs = 0x0004U; //Payload: 3 AdcAcumulators
static const uint16_t CGraphPayloadTypePZTAdcsFloatingPoint = 0x0005U; //Payload: 3 double-precision floats
static const uint16_t CGraphPayloadTypePZTStatus = 0x0006U;
struct CGraphPZTStatusPayload
{
	double P1V2;
	double P2V2;
	double P24V;
	double P2V5;
	double P3V3A;
	double P6V;
	double P5V;
	double P3V3D;
	double P4V3;
	double N5V;
	double N6V;
	double P150V;

   bool operator==(const CGraphPZTStatusPayload* p /**< [in] the pointer to the struct to compare to*/)
   {
		return (P1V2 == p->P1V2 && P2V2 == p->P2V2 && P24V == p->P24V && P2V5 == p->P2V5 && P3V3A == p->P3V3A && P6V == p->P6V && P5V == p->P5V &&
				P3V3D == p->P3V3D && P4V3 == p->P4V3 && N5V == p->N5V && N6V == p->N6V && P150V == p->P150V);
   }

   CGraphPZTStatusPayload& operator=(const CGraphPZTStatusPayload* p /**< [in] the pointer to the struct to be copied*/)
   {
		this->P1V2 = p->P1V2;
		this->P2V2 = p->P2V2;
		this->P24V = p->P24V;
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
};

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


// Base class
class PZTQuery {
public:
    const uint16_t CGraphPayloadType = -1;
    const std::string startLog = "";
    const std::string endLog = "";

    virtual ~PZTQuery() = default;
    virtual void errorLogString(const size_t ParamsLen) = 0;
    virtual void processReply(char const* Params, const size_t ParamsLen) = 0;
    virtual void logReply() = 0;

};

// Derived classes
class StatusQuery : public PZTQuery {
public:
    const uint16_t CGraphPayloadType = CGraphPayloadTypePZTStatus;
    const std::string startLog = "PZTStatus: Querying status.";
    const std::string endLog = "PZTStatus: Finished querying status.";

    const CGraphPZTStatusPayload* Status = nullptr;

    void processReply(char const* Params, const size_t ParamsLen) override {
        if ( (NULL != Params) && (ParamsLen >= (3 * sizeof(double))) )
        {
            Status = reinterpret_cast<const CGraphPZTStatusPayload*>(Params);
        }
        else
        {
            errorLogString(ParamsLen);
        }
    }

    void errorLogString(const size_t ParamsLen) override 
    {
        std::ostringstream oss;
        oss << "PZTStatus: Short packet: " << ParamsLen << " (expected " << (3 * sizeof(double)) << " bytes): ";
        MagAOXAppT::log<software_error>({__FILE__, __LINE__, oss.str()});
    }


    void logReply() override 
    {
        std::ostringstream oss;        
		oss << "BinaryPZTStatus Command: Values with corrected units follow:\n";
        oss << "P1V2: " << std::fixed << std::setprecision(6) << Status->P1V2 << " V\n";
        oss << "P2V2: " << std::fixed << std::setprecision(6) << Status->P2V2 << " V\n";
        oss << "P24V: " << std::fixed << std::setprecision(6) << Status->P24V << " V\n";
        oss << "P2V5: " << std::fixed << std::setprecision(6) << Status->P2V5 << " V\n";
        oss << "P3V3A: " << std::fixed << std::setprecision(6) << Status->P3V3A << " V\n";
        oss << "P6V: " << std::fixed << std::setprecision(6) << Status->P6V << " V\n";
        oss << "P5V: " << std::fixed << std::setprecision(6) << Status->P5V << " V\n";
        oss << "P3V3D: " << std::fixed << std::setprecision(6) << Status->P3V3D << " V\n";
        oss << "P4V3: " << std::fixed << std::setprecision(6) << Status->P4V3 << " V\n";
        oss << "N5V: " << std::fixed << std::setprecision(6) << Status->N5V << " V\n";
        oss << "N6V: " << std::fixed << std::setprecision(6) << Status->N6V << " V\n";
        oss << "P150V: " << std::fixed << std::setprecision(6) << Status->P150V << " V";
        MagAOXAppT::log<text_log>(oss.str());
    } 
};

class AdcsQuery : public PZTQuery {
public:
    const uint16_t CGraphPayloadType = CGraphPayloadTypePZTAdcs;
    const std::string startLog = "PZTAdcs: Querying ADCs.";
    const std::string endLog = "PZTStatus: Finished querying ADCs.";

    const AdcAccumulator* AdcVals = nullptr;

    void processReply(char const* Params, const size_t ParamsLen) override {
        if ( (NULL != Params) && (ParamsLen >= (3 * sizeof(AdcAccumulator))) )
        {
            AdcVals = reinterpret_cast<const AdcAccumulator*>(Params);
        }
        else
        {
            errorLogString(ParamsLen);
        }
    }

    void errorLogString(const size_t ParamsLen) override 
    {
        std::ostringstream oss;
        oss << "BinaryPZTAdcsCommand: Short packet: " << ParamsLen << " (expected " << (3 * sizeof(AdcAccumulator)) << " bytes): ";
        MagAOXAppT::log<software_error>({__FILE__, __LINE__, oss.str()});
    }

    void logReply() override 
    {
        MagAOXAppT::log<text_log>("BinaryPZTAdcsCommand: ");
        AdcVals[0].log();
        AdcVals[1].log();
        AdcVals[2].log();   
    } 
};

class DacsQuery : public PZTQuery {
public:
    const uint16_t CGraphPayloadType = CGraphPayloadTypePZTDacs;
    const std::string startLog = "PZTDacs: Querying DACs.";
    const std::string endLog = "PZTStatus: Finished querying DACs.";

    const uint32_t* DacSetpoints = nullptr;

    void processReply(char const* Params, const size_t ParamsLen) override {
        if ( (NULL != Params) && (ParamsLen >= (3 * sizeof(uint32_t))) )
        {
            const uint32_t* DacSetpoints = reinterpret_cast<const uint32_t*>(Params);
        }
        else
        {
            errorLogString(ParamsLen);
        }
    }

    void errorLogString(const size_t ParamsLen) override 
    {
        std::ostringstream oss;
        oss << "BinaryPZTDacsCommand: Short packet: " << ParamsLen << " (expected " << (3 * sizeof(uint32_t)) << " bytes): ";
        MagAOXAppT::log<software_error>({__FILE__, __LINE__, oss.str()});
    }

    void logReply() override 
    {
        std::ostringstream oss;
  
        oss << "BinaryPZTDacsCommand: 0x" << std::hex << DacSetpoints[0] << " | 0x" << std::hex << DacSetpoints[1] << " | 0x " << std::hex << DacSetpoints[2];
        MagAOXAppT::log<text_log>(oss.str());
    }
};

} //namespace app
} //namespace MagAOX