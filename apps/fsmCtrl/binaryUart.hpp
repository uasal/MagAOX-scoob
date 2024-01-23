//
///           Copyright (c)2007 by Franks Development, LLC
//
// This software is copyrighted by and is the sole property of Franks
// Development, LLC. All rights, title, ownership, or other interests
// in the software remain the property of Franks Development, LLC. This
// software may only be used in accordance with the corresponding
// license agreement.  Any unauthorized use, duplication, transmission,
// distribution, or disclosure of this software is expressly forbidden.
//
// This Copyright notice may not be removed or modified without prior
// written consent of Franks Development, LLC.
//
// Franks Development, LLC. reserves the right to modify this software
// without notice.
//
// Franks Development, LLC            support@franks-development.com
// 500 N. Bahamas Dr. #101           http://www.franks-development.com
// Tucson, AZ 85710
// USA
//

#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

#include <sstream> // for stringstreams

#include "IUart.h"

#include "iPacket.hpp"

#include "cGraphPacket.hpp"
#include "fsmCommands.hpp"

namespace MagAOX
{
namespace app
{

struct BinaryUartCallbacks
{
	BinaryUartCallbacks() { }
	virtual ~BinaryUartCallbacks() { }

	//Malformed/corrupted packet handler:
	virtual void InvalidPacket(const uint8_t* Buffer, const size_t& BufferLen) { }

	//Packet with no matching command handler:
	virtual void UnHandledPacket(const IPacket* Packet, const size_t& PacketLen) { }

	//In case we need to look at every packet that goes by...
	virtual void EveryPacket(const IPacket* Packet, const size_t& PacketLen) { }

	//Seems like someone, sometime might wanna handle this...
	virtual void BufferOverflow(const size_t& BufferLen) { }
};


struct BinaryUart
{
    //This is where the received characters go while we are building a line up from the input
	static const size_t RxBufferLenBytes = 4096;
	static const size_t TxBufferLenBytes = 4096;
    uint8_t RxBuffer[RxBufferLenBytes];
    uint16_t RxCount;
    IUart& Pinout;
	IPacket& Packet;
	BinaryUartCallbacks& Callbacks;
    bool debug;
    bool InPacket;
	size_t PacketStart;
    size_t PacketLen;
	//~ const void* Argument;
	uint64_t SerialNum;
	static const uint64_t InvalidSerialNumber = 0xFFFFFFFFFFFFFFFFULL;

    BinaryUart(struct IUart& pinout, struct IPacket& packet, struct BinaryUartCallbacks& callbacks, const uint64_t serialnum = InvalidSerialNumber)
        :
		RxCount(0),
        Pinout(pinout),
		Packet(packet),
		Callbacks(callbacks),
		debug(false),
		//~ debug(true),
		InPacket(false),
		PacketStart(0),
		PacketLen(0),
		SerialNum(serialnum)

    {

    }

    void Debug(bool dbg)
    {
        debug = dbg;
    }

    int Init(uint32_t serialnum)
    {
		SerialNum = serialnum;
        RxCount = 0;
        memset(RxBuffer, '\0', RxBufferLenBytes);

        std::ostringstream oss;
        oss << "Binary Uart: Init(PktH " << Packet.HeaderLen() << ", PktF " << Packet.FooterLen() << ").";
        MagAOXAppT::log<text_log>(oss.str());

        return(0);
    }

    bool Process(MagAOX::app::PZTQuery& pztQuery)
    {
	    //New char?
        if ( !(Pinout.dataready()) ) { return(false); }

		//pull it off the hardware
        uint8_t c = Pinout.getcqq();

		//~ if (debug) {
			//~ printf(".%.2x", c);
		//~ }

		ProcessByte(c);
		CheckPacketStart();
		CheckPacketEnd(pztQuery);

		return(true); //We just want to know if there's chars in the buffer to put threads to sleep or not...
	}

	void ProcessByte(const char c)
	{
		//Put the current character into the buffer
		if (RxCount < RxBufferLenBytes)
		{
			RxCount++;
			RxBuffer[RxCount - 1] = c;
		}
		else
		{
			if (debug) 
			{
				std::ostringstream oss;
				oss << "BinaryUart: Buffer(" << RxBuffer <<") overflow; this packet will not fit (" << RxCount << "b), flushing buffer.";
				MagAOXAppT::log<software_debug>({__FILE__, __LINE__, oss.str()});
			}				

			Callbacks.BufferOverflow(RxCount);

			InPacket = false;
			RxCount = 0;
			RxBuffer[0] = 0;
		}
	}

	void CheckPacketStart()
	{
		//Packet Start?
		if ( (!InPacket) && (RxCount >= Packet.HeaderLen()) )
		{
			if (Packet.FindPacketStart(RxBuffer, RxCount, PacketStart)) //This is wasteful, we really only need to look at the 4 newest bytes every time...
			{
				if (debug) { MagAOXAppT::log<software_debug>({__FILE__, __LINE__, "BinaryUart: Packet start detected! Buffering."}); }

				InPacket = true;
			}
		}
	}

	bool CheckPacketEnd(MagAOX::app::PZTQuery& pztQuery)
	{
		bool Processed = false;
		size_t PacketEnd = 0;

		//Packet End?
		if ( (InPacket) && (RxCount >= (Packet.HeaderLen() + Packet.FooterLen())) )
		{
			if (Packet.FindPacketEnd(RxBuffer, RxCount, PacketEnd)) //This is wasteful, we really only need to look at the 4 newest bytes every time...
			{
				if (debug) { MagAOXAppT::log<software_debug>({__FILE__, __LINE__, "BinaryUart: Packet end detected; Looking for matching packet handlers."}); }

				if (RxCount >= Packet.PayloadLen(RxBuffer, RxCount, PacketStart) + Packet.HeaderLen() + Packet.FooterLen())
				{
					if (Packet.IsValid(RxBuffer, RxCount, PacketStart))
					{
						if ( (SerialNum == InvalidSerialNumber) || (SerialNum == Packet.SerialNum() ) )
						{

							//strip the part of the line with the arguments to this command (chars following command) for compatibility with the  parsing code, the "params" officially start with the s/n
							const char* Params = reinterpret_cast<char*>(&(RxBuffer[PacketStart + Packet.PayloadOffset()]));

							const size_t ParamsLen = Packet.PayloadLen(RxBuffer, RxCount, PacketStart);
							
							//call the actual command							
							pztQuery.processReply(Params, Packet.PayloadLen(RxBuffer, RxCount, PacketStart));

							Processed = true;
						}
						else
						{
							if (debug)
							{ 
								std::ostringstream oss;
								oss << "BinaryUart: Packet received, but SerialNumber comparison failed (expected: 0x" << SerialNum << "; got: 0x" << Packet.SerialNum() << ").";
								MagAOXAppT::log<software_debug>({__FILE__, __LINE__, oss.str()});								
							}

							Callbacks.UnHandledPacket(reinterpret_cast<IPacket*>(&RxBuffer[PacketStart]), PacketEnd - PacketStart);
						}

						//Now just let the user do whatever they want with it...
						Callbacks.EveryPacket(reinterpret_cast<IPacket*>(&RxBuffer[PacketStart]), PacketEnd - PacketStart);
					}
					else
					{
						if (debug) { MagAOXAppT::log<software_debug>({__FILE__, __LINE__, "BinaryUart: Packet received, but invalid."}); }

						Callbacks.InvalidPacket(reinterpret_cast<uint8_t*>(RxBuffer), RxCount);
					}

					InPacket = false;

					if (RxCount > (PacketEnd + 4) )
					{
						size_t pos = 0;
						size_t clr = 0;
						for (; pos < (RxCount - (PacketEnd + 4)); pos++)
						{
							RxBuffer[pos] = RxBuffer[(PacketEnd + 4) + pos];
						}
						for (clr = pos; clr < RxCount; clr++)
						{
							RxBuffer[clr] = 0;
						}
						RxCount = pos;
					}
					else
					{
						RxCount = 0;
						RxBuffer[0] = 0;
					}
				}
				else
				{
					if ( ((Packet.PayloadLen(RxBuffer, RxCount, PacketStart)) > RxBufferLenBytes) || ((Packet.PayloadLen(RxBuffer, RxCount, PacketStart)) > Packet.MaxPayloadLength())  )
					{
						if (debug)
						{ 
							std::ostringstream oss;
							oss << "BinaryUart: Short packet (" << RxCount << " bytes) with unrealistic payload len; ignoring corrupted packet (should have been header() + payload(" << Packet.PayloadLen(RxBuffer, RxCount, PacketStart) << ") + footer().";
							MagAOXAppT::log<software_debug>({__FILE__, __LINE__, oss.str()});				
						}

						Callbacks.InvalidPacket(reinterpret_cast<uint8_t*>(RxBuffer), RxCount);

						InPacket = false;
						RxCount = 0;
						RxBuffer[0] = 0;
					}
					else
					{
						if (debug)
						{
							std::ostringstream oss;
							oss << "BinaryUart: Short packet (" << RxCount << " bytes); we'll assume the packet footer was part of the payload data and keep searching for the packet end (should have been header() + payload(" << Packet.PayloadLen(RxBuffer, RxCount, PacketStart) << ") + footer().";
							MagAOXAppT::log<software_debug>({__FILE__, __LINE__, oss.str()});
						}
					}
				}
			}
			else { if (debug) { MagAOXAppT::log<software_debug>({__FILE__, __LINE__, "BinaryUart: Still waiting for packet end..."}); } }
		}

		return(Processed);
    }

	void TxBinaryPacket(const uint16_t PayloadType, const void* PayloadData, const size_t PayloadLen) const
	{
		uint8_t TxBuffer[TxBufferLenBytes];
		size_t PacketLen = Packet.MakePacket(TxBuffer, TxBufferLenBytes, PayloadData, PayloadType, PayloadLen);

        std::ostringstream oss;
        oss << "Packet length: " << PacketLen;
        MagAOXAppT::log<text_log>(oss.str());

		for (size_t i = 0; i < PacketLen; i++) { Pinout.putcqq(TxBuffer[i]); }

		oss.str("");
        oss << "Binary Uart: Sending packet(" << PayloadType << ", " << PayloadLen << "): ";
        MagAOXAppT::log<text_log>(oss.str());	

		oss.str("");
		for(size_t i = 0; i < PacketLen; i++) { oss << std::setw(2) << std::setfill('0') << std::hex << static_cast<unsigned>(TxBuffer[i]) << ":"; }
		MagAOXAppT::log<text_log>(oss.str());		

	}
};

} //namespace app
} //namespace MagAOX