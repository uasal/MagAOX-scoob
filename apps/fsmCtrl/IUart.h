#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

struct PinoutConfig;

class IUart
{
public:
	IUart() { }
	virtual ~IUart() { }

	static const bool OddParity = true;
	static const bool NoParity = false;
	static const bool IUartOK = 0x00;
	static const bool UseRTSCTS = true;
	static const bool NoRTSCTS = false;

	virtual int init(const PinoutConfig& config) = 0;
	virtual bool dataready() const = 0;
	virtual char getcqq() = 0;
	virtual char putcqq(char c) = 0;
	virtual void flushoutput() = 0;
	virtual void purgeinput() = 0;
	virtual bool isopen() const = 0;
	
	void puts(const char* s, const size_t len)
	{
		for (size_t i = 0; i < len; i++)
		{
			if ('\0' == s[i]) { break; }
			
			putcqq(s[i]);
		}
	}
};


struct PinoutConfig {
	// linux_pinout_client_socket parameters
	int HostPort;
	const char *HostName;
	// linux_pinout_uart parameters
	uint32_t Baudrate;
	const char *device;
	bool UseRtsCts;
	bool UseOddParity;
	int OpenFlags;

private:
    // Private constructor for socket configuration
    PinoutConfig(int HostPort, const char *HostName)
        : HostPort(HostPort), HostName(HostName) {}

    // Private constructor for serial configuration
    PinoutConfig(uint32_t Baudrate, const char *device, bool UseRtsCts, bool UseOddParity, int OpenFlags)
        : Baudrate(Baudrate), device(device), UseRtsCts(UseRtsCts), UseOddParity(UseOddParity), OpenFlags(OpenFlags) {}


public:
    // Factory method for socket connection
    static PinoutConfig CreateSocketConfig(int HostPort, const char *HostName) {
        return PinoutConfig(HostPort, HostName);
    }

    // Factory method for serial connection
    static PinoutConfig CreateSerialConfig(uint32_t Baudrate, const char *device,
                                           bool UseRtsCts = IUart::NoRTSCTS, bool UseOddParity = IUart::NoParity, 
                                           int OpenFlags = O_RDWR | O_NOCTTY | O_NONBLOCK | O_SYNC) {
        return PinoutConfig(Baudrate, device, UseRtsCts, UseOddParity, OpenFlags);
    }

};