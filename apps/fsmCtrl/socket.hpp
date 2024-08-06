#pragma once

#include <sstream> // for stringstreams

#include "binaryUart.hpp"

namespace MagAOX
{
namespace app
{

struct SocketBinaryUartCallbacks : public BinaryUartCallbacks
{
	SocketBinaryUartCallbacks() { }
	virtual ~SocketBinaryUartCallbacks() { }

	//Malformed/corrupted packet handler:
	virtual void InvalidPacket(const uint8_t* Buffer, const size_t& BufferLen)
	{
		if ( (NULL == Buffer) || (BufferLen < 1) )
		{ 
			std::ostringstream oss;
			oss << "SocketUartCallbacks: NULL(" << BufferLen << " ) InvalidPacket!";
			MagAOXAppT::log<software_error>({__FILE__, __LINE__, oss.str()});			
			return; 
		}

		size_t len = BufferLen;
		if (len > 32) { len = 32; }
		MagAOXAppT::log<software_error>({__FILE__, __LINE__, "SocketUartCallbacks: InvalidPacket! contents: :"});
		for(size_t i = 0; i < len; i++)
		{ 
			MagAOXAppT::log<software_error>({__FILE__, __LINE__, Buffer[i]}); 
		}
	}

	//Packet with no matching command handler:
	virtual void UnHandledPacket(const IPacket* Packet, const size_t& PacketLen)
	{
		if ( (NULL == Packet) || (PacketLen < sizeof(CGraphPacketHeader)) ) 
		{ 
			std::ostringstream oss;
			oss << "SocketUartCallbacks: NULL(" << PacketLen << " ) UnHandledPacket!";
			MagAOXAppT::log<software_error>({__FILE__, __LINE__, oss.str()});			
			return;
		}

		const CGraphPacketHeader* Header = reinterpret_cast<const CGraphPacketHeader*>(Packet);
		std::ostringstream oss;
		oss << "SocketUartCallbacks: Unhandled packet(" << PacketLen << "): ";
		MagAOXAppT::log<software_error>({__FILE__, __LINE__, oss.str()});			
		Header->formatf();
	}

	//In case we need to look at every packet that goes by...
	//~ virtual void EveryPacket(const IPacket& Packet, const size_t& PacketLen) { }

	//We just wanna see if this is happening, not much to do about it
	virtual void BufferOverflow(const size_t& BufferLen)
	{
		std::ostringstream oss;
		oss << "SocketBinaryUartCallbacks: BufferOverflow(" << BufferLen << ")!";
		MagAOXAppT::log<software_error>({__FILE__, __LINE__, oss.str()});			
	}

} PacketCallbacks;

} //namespace app
} //namespace MagAOX