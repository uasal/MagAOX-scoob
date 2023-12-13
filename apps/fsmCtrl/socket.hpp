#pragma once

#include "binaryUart.hpp"


struct SocketBinaryUartCallbacks : public BinaryUartCallbacks
{
	SocketBinaryUartCallbacks() { }
	virtual ~SocketBinaryUartCallbacks() { }

	//Malformed/corrupted packet handler:
	virtual void InvalidPacket(const uint8_t* Buffer, const size_t& BufferLen)
	{
		if ( (NULL == Buffer) || (BufferLen < 1) ) { printf("\nSocketUartCallbacks: NULL(%lu) InvalidPacket!\n\n", BufferLen); return; }

		size_t len = BufferLen;
		if (len > 32) { len = 32; }
		printf("\nSocketUartCallbacks: InvalidPacket! contents: :");
		for(size_t i = 0; i < len; i++) { printf("%.2X:", Buffer[i]); }
		printf("\n\n");
	}

	//Packet with no matching command handler:
	virtual void UnHandledPacket(const IPacket* Packet, const size_t& PacketLen)
	{
		if ( (NULL == Packet) || (PacketLen < sizeof(CGraphPacketHeader)) ) { printf("\nSocketUartCallbacks: NULL(%lu) UnHandledPacket!\n\n", PacketLen); return; }

		const CGraphPacketHeader* Header = reinterpret_cast<const CGraphPacketHeader*>(Packet);
		printf("\nSocketUartCallbacks: Unhandled packet(%lu): ", PacketLen);
		Header->formatf();
		printf("\n\n");
	}

	//In case we need to look at every packet that goes by...
	//~ virtual void EveryPacket(const IPacket& Packet, const size_t& PacketLen) { }

	//We just wanna see if this is happening, not much to do about it
	virtual void BufferOverflow(const size_t& BufferLen)
	{
		printf("\nSocketBinaryUartCallbacks: BufferOverflow(%zu)!\n", BufferLen);
	}

} PacketCallbacks;