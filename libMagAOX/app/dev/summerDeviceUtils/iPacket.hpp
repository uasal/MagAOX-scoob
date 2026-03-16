#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

namespace MagAOX
{
namespace app
{
namespace dev
{

class IPacket
{
public:
	IPacket() { }
	virtual ~IPacket() { }
	
	virtual void Debug(bool dbg) = 0;
	virtual bool Debug() const = 0;
	
	virtual bool FindPacketStartPos(const uint8_t* Buffer, const size_t BufferLen, size_t& Offset) const = 0;
	virtual bool FindPacketEndPos(const uint8_t* Buffer, const size_t BufferLen, size_t& Offset) const = 0;
	virtual bool FindPacketStartPos(const uint8_t* Buffer, const size_t BufferLen, const size_t SearchStartPos, size_t& Offset) const = 0;
	virtual bool FindPacketEndPos(const uint8_t* Buffer, const size_t BufferLen, const size_t SearchStartPos, size_t& Offset) const = 0;
	
	virtual const size_t HeaderLen() const = 0;
	virtual const size_t FooterLen() const = 0;
	virtual const size_t EndTokenLen() const = 0;
	virtual const size_t PayloadOffset() const = 0;
	virtual const size_t MaxPayloadLength() const = 0;	
	virtual size_t PayloadLen(const uint8_t* Buffer, const size_t BufferCount, const size_t PacketStartPos) const = 0;
	virtual bool IsValid(const uint8_t* Buffer, const size_t BufferCount, const size_t PacketStartPos, const size_t PacketEndPos) const = 0;
	virtual bool IsBroadcastSerialNum(const uint8_t* Buffer, const size_t PacketStartPos, const size_t PacketEndPos) const = 0;
	virtual uint64_t SerialNum(const uint8_t* Buffer, const size_t PacketStartPos, const size_t PacketEndPos) const = 0;
	virtual uint64_t PayloadType(const uint8_t* Buffer, const size_t PacketStartPos, const size_t PacketEndPos) const = 0;
	virtual bool DoesPayloadTypeMatch(const uint8_t* Buffer, const size_t BufferCount, const size_t PacketStartPos, const size_t PacketEndPos, const uint32_t CmdType) const = 0;
	virtual size_t MakePacket(uint8_t* Buffer, const size_t BufferCount, const void* Payload, const uint16_t PayloadType, const size_t PayloadLen) const = 0;
	
	virtual void formatf() const = 0;
};

} // namespace dev
} // namespace app
} // namespace MagAOX