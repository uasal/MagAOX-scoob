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
#include <vector> // For std::vector
#include <cstring> // For memset
#include <iomanip> // For std::setw and std::fill

#include "IUart.h"

#include "iPacket.hpp"

#include "cGraphPacket.hpp"
#include "commands.hpp"

namespace MagAOX
{
namespace app
{
namespace dev
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
	virtual void BufferOverflow(const uint8_t* Buffer, const size_t& BufferLen) { }
};


struct BinaryUart
{
	//Default values
	static const uint16_t RxCountInit = 0;
	static const size_t PacketStartInit = 0;
	static const size_t PacketLenInit = 0;
	static const size_t PayloadLenInit = 0;
	static const size_t HeaderLenInit = 0;
	static const size_t FooterLenInit = 0;
	static const bool InPacketInit = false;
	static const bool debugDefault = false;
	static const char EmptyBufferChar = '\0';

	static const size_t RxBufferLenBytes = 4096;
	static const size_t TxBufferLenBytes = 4096;
    uint8_t RxBuffer[RxBufferLenBytes];     //This is where the received characters go while we are building a line up from the input
    uint16_t RxCount;
    IUart& Pinout;
	IPacket& Packet;
	BinaryUartCallbacks& Callbacks;
	const std::vector<sdevQuery*>& Queries;
    bool debug;
    bool InPacket;
	size_t PacketStart;
    size_t PacketLen;
	size_t PayloadLen;
	size_t HeaderLen;
	size_t FooterLen;
	size_t PacketEnd = 0;
	//~ const void* Argument;
	uint64_t SerialNum;
	static const uint64_t InvalidSerialNumber = 0xFFFFFFFFFFFFFFFFULL;

    BinaryUart(struct IUart& pinout, struct IPacket& packet, struct BinaryUartCallbacks& callbacks, const std::vector<sdevQuery*>& queries, const uint64_t serialnum = InvalidSerialNumber)
        :
		RxCount(RxCountInit),
        Pinout(pinout),
		Packet(packet),
		Callbacks(callbacks),
		Queries(queries),
		debug(debugDefault),
		//~ debug(true),
		InPacket(InPacketInit),
		PacketStart(PacketStartInit),
		PacketLen(PacketLenInit),
		PayloadLen(PayloadLenInit),
		HeaderLen(HeaderLenInit),
		FooterLen(FooterLenInit),
		SerialNum(serialnum)

    {
		Init(serialnum);
    }

    void Debug(bool dbg)
    {
        debug = dbg;
    }

    bool Debug() { return(debug); }

    const uint8_t* GetRxBuffer() const
    {
        return RxBuffer;
    }

    int Init(uint64_t serialnum)
    {
		SerialNum = serialnum;
        RxCount = RxCountInit;
		PacketStart = PacketStartInit;
		PacketLen = PacketLenInit;
		PayloadLen = PayloadLenInit;
		HeaderLen = HeaderLenInit;
		FooterLen = FooterLenInit;
		InPacket = InPacketInit;		
        memset(RxBuffer, EmptyBufferChar, RxBufferLenBytes);

        // std::ostringstream oss;
        // oss << "Binary Uart: Init(PktH " << Packet.HeaderLen() << ", PktF " << Packet.FooterLen() << ").";
        // MagAOXAppT::log<text_log>(oss.str());

        return(0);
    }

    int InitFast(uint64_t serialnum = 0)
    {
		SerialNum = serialnum;
        RxCount = RxCountInit;
		PacketStart = PacketStartInit;
		PacketLen = PacketLenInit;
		PayloadLen = PayloadLenInit;
		HeaderLen = HeaderLenInit;
		FooterLen = FooterLenInit;
		InPacket = InPacketInit;

		//if (debug) { ... }

        return(0);
    }

    bool Process()
    {
        bool gotStart = false;

	    //New char?
        if ( !(Pinout.dataready()) ) { return(false); }

		//pull it off the hardware
        uint8_t c = Pinout.getcqq();

		if (debug) {
			printf(".%.2x", c);
		}

		ProcessByte(c);
		
		if (!InPacket) {
			gotStart = CheckPacketStart();
			if (gotStart) {
				PayloadLen = Packet.PayloadLen(RxBuffer, RxCount, PacketStart);
				HeaderLen = Packet.HeaderLen();
				FooterLen = Packet.FooterLen();
			}
		}
		else {
			if (!(RxCount < HeaderLen + FooterLen + PayloadLen)) {
				CheckPacketEnd();
			}
		}

		return(true); //We just want to know if there's chars in the buffer to put threads to sleep or not...
	}

	void ProcessByte(const char c)
	{
		//Put the current character into the buffer
		if (RxCount < RxBufferLenBytes)
		{
			RxBuffer[RxCount] = c;
			RxCount++;
		}
		else
		{
			// if (debug) 
			// {
			// 	std::ostringstream oss;
			// 	oss << "BinaryUart: Buffer(" << RxBuffer <<") overflow; this packet will not fit (" << RxCount << "b), flushing buffer.";
			// 	MagAOXAppT::log<software_debug>({__FILE__, __LINE__, oss.str()});
			// }				

			Callbacks.BufferOverflow(RxBuffer, RxCount);

			Init(SerialNum);
		}
	}

	bool CheckPacketStart()
	{
		//Packet Start?
		if ( (!InPacket) && (RxCount >= Packet.HeaderLen()) )
		{
			if (Packet.FindPacketStartPos(RxBuffer, RxCount, PacketStart)) //This is wasteful, we really only need to look at the 4 newest bytes every time...
			{
				// if (debug) { MagAOXAppT::log<software_debug>({__FILE__, __LINE__, "BinaryUart: Packet start detected! Buffering."}); }

				InPacket = true;
				return(true);
			}
			return(false);
		}
		return(false);
	}

	bool CheckPacketEnd()
	{
		PacketEnd = 0;
		bool Processed = false;

		// Look for the packet footer within the buffer, exit if no valid footer found yet
		if (!Packet.FindPacketEndPos(RxBuffer, RxCount, PacketEnd)) {
			// if (debug) { ... }
			return false;
		}

		const size_t payloadLen = Packet.PayloadLen(RxBuffer, RxCount, PacketStart);

		if (Packet.IsValid(RxBuffer, RxCount, PacketStart, PacketEnd))
		{
			if ( (SerialNum == InvalidSerialNumber) || (Packet.IsBroadcastSerialNum(RxBuffer, PacketStart, PacketEnd)) || (SerialNum == Packet.SerialNum(RxBuffer, PacketStart, PacketEnd) ) )
			{
				//strip the part of the line with the arguments to this command (chars following command) for compatibility with the  parsing code, the "params" officially start with the s/n
				const char* Params = reinterpret_cast<char*>(&(RxBuffer[PacketStart + Packet.PayloadOffset()]));

				//Check which query the packet corresponds to
			    for (sdevQuery* query : Queries) {
					if (Packet.DoesPayloadTypeMatch(RxBuffer, RxCount, PacketStart, PacketEnd, static_cast<uint32_t>(query->getPayloadType()))) {
						// Process the packet
						query->processReply(Params, payloadLen);
						query->logReply();
					}
				}

				Processed = true;
			}
			else
			{
				// if (debug)
				// { 
				// 	std::ostringstream oss;
				// 	oss << "BinaryUart: Packet received, but SerialNumber comparison failed (expected: 0x" << SerialNum << "; got: 0x" << Packet.SerialNum(RxBuffer, PacketStart, PacketEnd) << ").";
				// 	MagAOXAppT::log<software_debug>({__FILE__, __LINE__, oss.str()});								
				// }

				Callbacks.UnHandledPacket(reinterpret_cast<IPacket*>(&RxBuffer[PacketStart]), PacketEnd - PacketStart);
			}

			//Now just let the user do whatever they want with it...
			Callbacks.EveryPacket(reinterpret_cast<IPacket*>(&RxBuffer[PacketStart]), PacketEnd - PacketStart);
		}
		else
		{
			// if (debug) { MagAOXAppT::log<software_debug>({__FILE__, __LINE__, "BinaryUart: Packet received, but invalid."}); }

			Callbacks.InvalidPacket(reinterpret_cast<uint8_t*>(RxBuffer), RxCount);
		}

		InPacket = false;

		InitFast(SerialNum);

	return Processed;
    }

	virtual void TxBinaryPacket(const uint16_t PayloadType, const void* PayloadData, const size_t PayloadLen) const
	{
		TxBinaryPacket(PayloadType, SerialNum, PayloadData, PayloadLen);
	}

	virtual void TxBinaryPacket(const uint16_t PayloadType, const uint32_t SerialNumber, const void* PayloadData, const size_t PayloadLen) const
	{
		uint8_t TxBuffer[TxBufferLenBytes];
		size_t PktLen = Packet.MakePacket(TxBuffer, TxBufferLenBytes, PayloadData, PayloadType, PayloadLen);

        // std::ostringstream oss;
        // oss << "Packet length: " << PktLen;
        // MagAOXAppT::log<text_log>(oss.str());

		for (size_t i = 0; i < PktLen; i++) { Pinout.putcqq(TxBuffer[i]); }

		// Debug output: log the packet type, length, and contents in hex format
		if (debug)
		{
			printf("\n\nBinary Uart: Sending packet(%u, %lu): ", PayloadType, (unsigned long)PayloadLen);
			for(size_t i = 0; i < PktLen; i++) { printf("%.2X:", TxBuffer[i]); }
			printf("\n\n");
		}

	}

	void formatf() const
	{
		printf("\n\nBinaryUart(%u, %c, %lu): :", RxCount, InPacket?'Y':'N', (unsigned long)PacketStart);
		for(size_t i = 0; i < RxCount; i++) { printf("%2X:", RxBuffer[i]); }
		printf("\n\n");
	}
};

} //namespace dev
} //namespace app
} //namespace MagAOX