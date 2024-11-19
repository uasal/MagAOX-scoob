/// \file
/// $Source: /raincloud/src/projects/include/uart/linux_pinout_uart.hpp,v $
/// $Revision: 1.9 $
/// $Date: 2009/11/14 00:20:04 $
/// $Author: steve $

#pragma once

#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
//~ #include <termbits.h>
#include <sys/ioctl.h> /* ioctl() */
#include <sys/socket.h> /* FIONREAD on cygwin */
#include <linux/serial.h>

#include <spawn.h>


#include "IUart.h"

namespace MagAOX
{
	namespace app
	{

        class linux_pinout_uart : public IUart
        {
        public:

            linux_pinout_uart() : IUart(), ComFileDescriptor(-1), echo(false), autoreopen(false), Baud(0), RtsCts(false), OddParity(false) { }
                
            virtual ~linux_pinout_uart() { if (-1 != ComFileDescriptor) { close(ComFileDescriptor); } }

                //~ Table 3 - Termios Structure Members Member	Description
                    //~ c_cflag	Control options
                    //~ c_lflag	Line options
                    //~ c_iflag	Input options
                    //~ c_oflag	Output options
                    //~ c_cc	Control characters
                    //~ c_ispeed	Input baud (new interface)
                    //~ c_ospeed	Output baud (new interface)
                //~ The c_cflag member controls the baud rate, number of data bits, parity, stop bits, and hardware flow control. There are constants for all of the supported configurations.
                    //~ CS8	8 data bits
                //~ CREAD	Enable receiver
                //~ CLOCAL	Local line - do not change "owner" of port
                //~ The c_cflag member contains two options that should always be enabled, CLOCAL and CREAD. These will ensure that your program does not become the 'owner' of the port subject to sporatic job control and hangup signals, and also that the serial interface driver will read incoming data bytes.
                //~ The baud rate constants (CBAUD, B9600, etc.) are used for older interfaces that lack the c_ispeed and c_ospeed members. See the next section for information on the POSIX functions used to set the baud rate.
                //~ Never initialize the c_cflag (or any other flag) member directly; you should always use the bitwise AND, OR, and NOT operators to set or clear bits in the members. Different operating system versions (and even patches) can and do use the bits differently, so using the bitwise operators will prevent you from clobbering a bit flag that is needed in a newer serial driver.
                //~ The baud rate is stored in different places depending on the operating system. Older interfaces store the baud rate in the c_cflag member using one of the baud rate constants in table 4, while newer implementations provide the c_ispeed and c_ospeed members that contain the actual baud rate value.
                //~ The cfsetospeed(3) and cfsetispeed(3) functions are provided to set the baud rate in the termios structure regardless of the underlying operating system interface. Typically you'd use the code in Listing 2 to set the baud rate.
                //~ struct termios options;
                    //~ Get the current options for the port...
                    //~ tcgetattr(ComFileDescriptor, &options);
                    //~ Set the baud rates to 19200...
                    //~ cfsetispeed(&options, B19200);
                    //~ cfsetospeed(&options, B19200);
                    //~ * Enable the receiver and set local mode...
                    //~ com_settings.c_cflag |= (CLOCAL | CREAD);
                    //~ com_settings.c_cflag &= ~CSIZE; /* Mask the character size bits */
                    //~ com_settings.c_cflag |= CS8;    /* Select 8 data bits */
                    //~ com_settings.c_cflag &= ~PARENB
                    //~ com_settings.c_cflag &= ~CSTOPB
                    //~ com_settings.c_cflag &= ~CSIZE;
                    //~ com_settings.c_cflag |= CS8;
                    //~ com_settings.c_cflag &= ~CNEW_RTSCTS; //to disable hardware flow control
                    //~ Set the new options for the port...
                    //~ tcsetattr(ComFileDescriptor, TCSANOW, &options);
                //~ The tcgetattr(3) function fills the termios structure you provide with the current serial port configuration. After we set the baud rates and enable local mode and serial data receipt, we select the new configuration using tcsetattr(3). The TCSANOW constant specifies that all changes should occur immediately without waiting for output data to finish sending or input data to finish receiving. There are other constants to wait for input and output to finish or to flush the input and output buffers.
                //~ Most systems do not support different input and output speeds, so be sure to set both to the same value for maximum portability.
                //~ Table 5 - Constants for tcsetattr Constant	Description
                //~ TCSANOW	Make changes now without waiting for data to complete
                //~ TCSADRAIN	Wait until everything has been transmitted
                //~ TCSAFLUSH	Flush input and output buffers and make the change
                //~ The O_NOCTTY flag tells UNIX that this program doesn't want to be the "controlling terminal" for that port. If you don't specify this then any input (such as keyboard abort signals and so forth) will affect your process. Programs like getty(1M/8) use this feature when starting the login process, but normally a user program does not want this behavior.
                //~ The O_NDELAY flag tells UNIX that this program doesn't care what state the DCD signal line is in - whether the other end of the port is up and running. If you do not specify this flag, your process will be put to sleep until the DCD signal line is the space voltage.
                //~ The local modes member c_lflag controls how input characters are managed by the serial driver. In general you will configure the c_lflag member for canonical or raw input.
                //~ Table 6 - Constants for the c_lflag Member Constant	Description
                //~ ISIG	Enable SIGINTR, SIGSUSP, SIGDSUSP, and SIGQUIT signals
                //~ ICANON	Enable canonical input (else raw)
                //~ XCASE	Map uppercase \lowercase (obsolete)
                //~ ECHO	Enable echoing of input characters
                //~ ECHOE	Echo erase character as BS-SP-BS
                //~ ECHOK	Echo NL after kill character
                //~ ECHONL	Echo NL
                //~ NOFLSH	Disable flushing of input buffers after interrupt or quit characters
                //~ IEXTEN	Enable extended functions
                //~ ECHOCTL	Echo control characters as ^char and delete as ~?
                //~ ECHOPRT	Echo erased character as character erased
                //~ ECHOKE	BS-SP-BS entire line on line kill
                //~ FLUSHO	Output being flushed
                //~ PENDIN	Retype pending input at next read or input char
                //~ TOSTOP	Send SIGTTOU for background output
                    //~ Canonical input is line-oriented. Input characters are put into a buffer which can be edited interactively by the user until a CR (carriage return) or LF (line feed) character is received.
                //~ When selecting this mode you normally select the ICANON, ECHO, and ECHOE options:
                    //~ com_settings.c_lflag |= (ICANON | ECHO | ECHOE);
                //~ Raw input is unprocessed. Input characters are passed through exactly as they are received, when they are received. Generally you'll deselect the ICANON, ECHO, ECHOE, and ISIG options when using raw input:
                //~ com_settings.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
                //~ Never enable input echo (ECHO, ECHOE) when sending commands to a MODEM or other computer that is echoing characters, as you will generate a feedback loop between the two serial interfaces!
                //~ Constant	Description
                //~ INPCK	Enable parity check
                //~ IGNPAR	Ignore parity errors
                //~ PARMRK	Mark parity errors
                //~ ISTRIP	Strip parity bits
                //~ IXON	Enable software flow control (outgoing)
                //~ IXOFF	Enable software flow control (incoming)
                //~ IXANY	Allow any character to start flow again
                //~ IGNBRK	Ignore break condition
                //~ BRKINT	Send a SIGINT when a break condition is detected
                //~ INLCR	Map NL to CR
                //~ IGNCR	Ignore CR
                //~ ICRNL	Map CR to NL
                //~ IUCLC	Map uppercase to lowercase
                //~ IMAXBEL	Echo BEL on input line too long
                //~ To disable software flow control simply mask those bits: com_settings.c_iflag &= ~(IXON | IXOFF | IXANY);
                //~ Raw output is selected by resetting the OPOST option in the c_oflag member:
                //~ com_settings.c_oflag &= ~OPOST;
                //~ When the OPOST option is disabled, all other option bits in c_oflag are ignored.
                //~ The c_cc character array contains control character definitions as well as timeout parameters. Constants are defined for every element of this array.
                
            int init(const PinoutConfig& config)
            {
                Baud = config.Baudrate;
                strncpy(Device, config.device, DeviceMaxLen);
                RtsCts = config.UseRtsCts;
                OddParity = config.UseOddParity;
                
                termios com_settings;

                //close if previously opened
                if (-1 != ComFileDescriptor) { close(ComFileDescriptor); }

                //open port
                ComFileDescriptor = open(config.device, config.OpenFlags); //O_NDELAY makes open, write, and read non-blocking; since we have dataready() this is uneccesary & dangerous...
                if (ComFileDescriptor < 0)
                {
                    int err = errno;
					std::ostringstream oss;
					oss << "\nlinux_pinout_uart::init(): Can't open COM-Port " << config.device << " ! (Error: " << err << " d (0x " << err << "))\n";
					MagAOXAppT::log<software_error>({__FILE__, __LINE__, oss.str()});
                    return(err);
                }

                //put port into non-blocking mode (returns '\0' if no chars waiting)...
                //~ fcntl(ComFileDescriptor, F_SETFL, 0);

                //get current port settings (initializes struct correctly for us)
                tcgetattr(ComFileDescriptor, &com_settings);

        //~ ifneq ($(findstring Darwin,$(OSTYPE)),)
        //~ CFLAGS+=-D__APPLE__
        //~ else
        //~ CFLAGS		+= -static
        //~ endif
        //~ #ifdef __APPLE__
        //~ #define NEWTERMIOS_SETBAUDARTE(bps) IspEnvironment->newtio.c_ispeed = IspEnvironment->newtio.c_ospeed = bps;
        //~ #else
        //~ #define NEWTERMIOS_SETBAUDARTE(bps) IspEnvironment->newtio.c_cflag |= bps;
        //~ #endif

                switch(Baud)
                {
                    case 1152000: com_settings.c_cflag |= B1152000; cfsetispeed(&com_settings, B1152000); cfsetospeed(&com_settings, B1152000); break;
                    case  921600: com_settings.c_cflag |= B921600; cfsetispeed(&com_settings, B921600); cfsetospeed(&com_settings, B921600); break;
                    case  460800: com_settings.c_cflag |= B460800; cfsetispeed(&com_settings, B460800); cfsetospeed(&com_settings, B460800); break;
                    case  230400: com_settings.c_cflag |= B230400; cfsetispeed(&com_settings, B230400); cfsetospeed(&com_settings, B230400); break;
                    case  115200: com_settings.c_cflag |= B115200; cfsetispeed(&com_settings, B115200); cfsetospeed(&com_settings, B115200); break;
                    case   57600: com_settings.c_cflag |= B57600; cfsetispeed(&com_settings, B57600); cfsetospeed(&com_settings, B57600); break;
                    case   38400: com_settings.c_cflag |= B38400; cfsetispeed(&com_settings, B38400); cfsetospeed(&com_settings, B38400); break;
                    case   19200: com_settings.c_cflag |= B19200; cfsetispeed(&com_settings, B19200); cfsetospeed(&com_settings, B19200); break;
                    case    9600: com_settings.c_cflag |= B9600; cfsetispeed(&com_settings, B9600); cfsetospeed(&com_settings, B9600); break;
                    case    4800: com_settings.c_cflag |= B4800; cfsetispeed(&com_settings, B4800); cfsetospeed(&com_settings, B4800); break;
                    default: 
                    { 
                        bool useterm2 = false;
                        
                        std::ostringstream oss;
                        oss << "\nlinux_pinout_uart::init(): non-standard baudrate " << Baud << "; strap in we're goin' off-roadin'\n";
                        MagAOXAppT::log<text_log>(oss.str());
                            
                        serial_struct serial;
                        
                        if (ioctl(ComFileDescriptor, TIOCGSERIAL, &serial))
                        {
                            MagAOXAppT::log<software_error>({__FILE__, __LINE__, "\nlinux_pinout_uart::init(): non-standard baudrate ioctl(TIOCGSERIAL) failed."});
                            useterm2 = true;
                        }
                        
                        serial.flags &= ~ASYNC_SPD_MASK;
                        serial.flags |= ASYNC_SPD_CUST;
                        serial.custom_divisor = serial.baud_base / Baud;
                        
                        //ugly hack:
                        if (0 == serial.baud_base)
                        {
                            useterm2 = true;
                        }
                        else
                        {
                            if (ioctl(ComFileDescriptor, TIOCSSERIAL, &serial))
                            {
                                MagAOXAppT::log<software_error>({__FILE__, __LINE__, "\nlinux_pinout_uart::init(): non-standard baudrate ioctl(TIOCSSERIAL) failed."});
                                useterm2 = true;
                            }
                        }
                        
                        if (useterm2)
                        {
                            MagAOXAppT::log<text_log>("\nlinux_pinout_uart::init(): non-standard baudrate clock of zero detected; assuming Nvidia Orin 408MHz clock...");
                            
                            serial.baud_base = 408000000;
                            
                            //~ termios2 tty;
                            struct termios2
                            {
                            tcflag_t c_iflag;
                            tcflag_t c_oflag;
                            tcflag_t c_cflag;
                            tcflag_t c_lflag;
                            cc_t c_cc[NCCS];
                            cc_t c_line;
                            speed_t c_ispeed;
                            speed_t c_ospeed;
                            } tty;

                            // Read in the terminal settings using ioctl instead
                            // of tcsetattr (tcsetattr only works with termios, not termios2)
                            if (ioctl(ComFileDescriptor, TCGETS2, &tty))
                            {
                                perror("\nlinux_pinout_uart::init(): non-standard baudrate, ioctl(fd, TCGETS2, &tty) failed: ");
                            }

                            // Set everything but baud rate as usual
                            // ...
                            // ...

                            // Set custom baud rate
                            tty.c_cflag &= ~CBAUD;
                            tty.c_cflag |= CBAUDEX;
                            // On the internet there is also talk of using the "BOTHER" macro here:
                            // tty.c_cflag |= BOTHER;
                            // I never had any luck with it, so omitting in favour of using
                            // CBAUDEX
                            tty.c_ispeed = Baud; // What a custom baud rate!
                            tty.c_ospeed = Baud;

                            // Write terminal settings to file descriptor
                            if (ioctl(ComFileDescriptor, TCSETS2, &tty))
                            {
                                perror("\nlinux_pinout_uart::init(): non-standard baudrate, ioctl(fd, TCSETS2, &tty) failed: ");
                            }
                        }

                        oss.str("");
                        oss << "\nlinux_pinout_uart::init(): non-standard baudrate settings complete; Clock: " << serial.baud_base << ", divider: " << serial.custom_divisor << ", actual baud: " << (double)serial.baud_base / (double)serial.custom_divisor << ".\n";
                        MagAOXAppT::log<text_log>(oss.str());

                    }
                }
                
                //raw input mode:
                //~ com_settings.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
                com_settings.c_lflag = 0;

                //raw output mode:
                //~ com_settings.c_oflag &= ~OPOST;
                com_settings.c_oflag = 0;
                        
                //enable single-byte input & output:
                cfmakeraw(&com_settings);

                com_settings.c_cflag |= (CLOCAL | CREAD);
                com_settings.c_cflag &= ~CSIZE; /* Mask the character size bits */
                com_settings.c_cflag |= CS8;    /* Select 8 data bits */
                com_settings.c_cflag &= ~CSTOPB;
                
                if (RtsCts) { com_settings.c_cflag |= CRTSCTS; }
                else { com_settings.c_cflag &= ~CRTSCTS; }
                
                if (OddParity) 
                {
                    MagAOXAppT::log<text_log>("\nlinux_pinout_uart::init(): Odd parity!\n");
                    com_settings.c_iflag &= ~IGNPAR;
                    com_settings.c_cflag |= PARODD; 
                    com_settings.c_cflag |= PARENB; 
                }
                else 
                { 
                    com_settings.c_iflag |= IGNPAR;
                    com_settings.c_cflag &= ~PARENB; 
                    com_settings.c_cflag &= ~PARODD;
                }
                
                //write settings to the port:
                if(tcsetattr(ComFileDescriptor, TCSANOW, &com_settings))
                {
                    int err = errno;
                    std::ostringstream oss;
					oss << "nlinux_pinout_uart::init(): Could not change serial port (" << config.device << ") behaviour (wrong baudrate?) (Error: " << err << "d (0x" << err << "))\n";
					MagAOXAppT::log<software_error>({__FILE__, __LINE__, oss.str()});
                    return(err);
                }

                //clear input & output buffers
                tcflush(ComFileDescriptor, TCOFLUSH);
                tcflush(ComFileDescriptor, TCIFLUSH);
                
                return(IUartOK);
            }

            virtual void deinit()
            {
                if (-1 == ComFileDescriptor) { return; }
                
                tcflush(ComFileDescriptor, TCOFLUSH);
                tcflush(ComFileDescriptor, TCIFLUSH);
                close(ComFileDescriptor);
            }
            
            virtual bool Echo() const { return(echo); }
            virtual void Echo(const bool e) { echo = e; }

                //~ In Chapter 2, Configuring the Serial Port we used the tcgetattr and tcsetattr functions to configure the serial port. Under UNIX these functions use the ioctl(2) system call to do their magic.
                //~ The ioctl system call takes three arguments:
                //~ int ioctl(int ComFileDescriptor, int request, ...);
                //~ The ComFileDescriptor argument specifies the serial port file descriptor. The request argument is a constant defined in the <termios.h> header file and is typically one of the constants listed in Table 10.
                //~ Table 10 - IOCTL Requests for Serial Ports Request	Description	POSIX Function
                //~ TCGETS	Gets the current serial port settings.	tcgetattr
                //~ TCSETS	Sets the serial port settings immediately.	tcsetattr(ComFileDescriptor, TCSANOW, &options)
                //~ TCSETSF	Sets the serial port settings after flushing the input and output buffers.	tcsetattr(ComFileDescriptor, TCSAFLUSH, &options)
                //~ TCSETSW	Sets the serial port settings after allowing the input and output buffers to drain/empty.	tcsetattr(ComFileDescriptor, TCSADRAIN, &options)
                //~ TCSBRK	Sends a break for the given time.	tcsendbreak, tcdrain
                //~ TCXONC	Controls software flow control.	tcflow
                //~ TCFLSH	Flushes the input and/or output queue.	tcflush
                //~ TIOCMGET	Returns the state of the "MODEM" bits.	None
                //~ TIOCMSET	Sets the state of the "MODEM" bits.	None
                //~ FIONREAD	Returns the number of bytes in the input buffer.	None
                //~ Getting the Number of Bytes Available
                //~ The FIONREAD ioctl gets the number of bytes in the serial port input buffer. As with TIOCMGET you pass in a pointer to an integer to hold the number of bytes, as shown in Listing 7.
                //~ Listing 7 - Getting the number of bytes in the input buffer.
                    //~ #include <unistd.h>
                    //~ #include <termios.h>
                    //~ int ComFileDescriptor;
                    //~ int bytes;
                    //~ ioctl(ComFileDescriptor, FIONREAD, &bytes);
                //~ This can be useful when polling a serial port for data, as your program can determine the number of bytes in the input buffer before attempting a read.

            virtual bool dataready() const
            {
                if (-1 == ComFileDescriptor) {
                    MagAOXAppT::log<text_log>("\nlinux_pinout_uart::dataready(): ioctl on uninitialized port; please open port!\n");
                    return(false);
                }

                //Didn't work under cygwin:
                //~ {
                    //~ int BytesWaitingToRead = 0;
                    //~ int err = ioctl(ComFileDescriptor, FIONREAD, &BytesWaitingToRead); //get number of bytes in input buffer
                    //~ if (0 != err) { printf("linux_pinout_uart::dataready(): ioctl err: %u, errno: %u\n", err, errno); return(false); }
                    //~ return(0 != BytesWaitingToRead);
                //~ }
                
                struct timeval tvptr;
                fd_set rset;
                int status = -1;
                int rtnval = 0;

                if (ComFileDescriptor != -1)
                {
                    /* Zero out the seconds and microseconds so we don't block */

                    tvptr.tv_sec = 0;
                    tvptr.tv_usec = 0;

                    /* Initialize the read set to zero */

                    FD_ZERO(&rset);

                    /* And now turn on the read set */

                    FD_SET(ComFileDescriptor,&rset);

                    status = select(ComFileDescriptor + 1,&rset,NULL,NULL,&tvptr);

                    if (status == -1) /* Error */
                    {
                        MagAOXAppT::log<software_error>({__FILE__, __LINE__, "\nlinux_pinout_uart::dataready(): select() returned -1\n"});
                        rtnval = 0;
                    }
                    else if (status > 0)
                    {
                        if (FD_ISSET(ComFileDescriptor,&rset))
                        {
                            rtnval = 1;
                        }
                    }
                }

                return(rtnval);
            }

                //~ Reading data from a port is a little trickier. When you operate the port in raw data mode, each read(2) system call will return the number of characters that are actually available in the serial input buffers. If no characters are available, the call will block (wait) until characters come in, an interval timer expires, or an error occurs. The read function can be made to return immediately by doing the following:
                //~ fcntl(ComFileDescriptor, F_SETFL, FNDELAY);
                //~ The FNDELAY option causes the read function to return 0 if no characters are available on the port. To restore normal (blocking) behavior, call fcntl() without the FNDELAY option:
                //~ fcntl(ComFileDescriptor, F_SETFL, 0);
                //~ This is also used after opening a serial port with the O_NDELAY option.

            virtual char getcqq()
            {
                char c = '\0';

                if (-1 == ComFileDescriptor) {
                    MagAOXAppT::log<software_error>({__FILE__, __LINE__, "\nlinux_pinout_uart::getcqq(): read on uninitialized port; please open port!\n"});
                    return('\0');
                }

                size_t len = read(ComFileDescriptor, &c, 1);

                if (len != 1)
                {
					std::ostringstream oss;
					oss << "\nlinux_pinout_uart::getcqq(): read failure (len: " << len << ", errno: " << errno << ")\n";
					MagAOXAppT::log<software_error>({__FILE__, __LINE__, oss.str()});
                    
                    if (autoreopen) 
                    {
                        MagAOXAppT::log<text_log>("\nlinux_pinout_uart::autoreopen.\n");
                        deinit();
                        PinoutConfig pinoutConfig = PinoutConfig::CreateSerialConfig(Baud, Device, RtsCts, OddParity);
                        init(pinoutConfig);
                    }		
                    
                    return('\0'); 
                }

                if (echo) { 
					std::ostringstream oss;
					oss << "<%.2x " << c;
					MagAOXAppT::log<text_log>(oss.str());
                }

                return(c);
            }

            virtual char putcqq(char c)
            {
                if (echo) {
					std::ostringstream oss;
					oss << ">%.2x " << c;
					MagAOXAppT::log<text_log>(oss.str());                    
                }
                
                if (-1 != ComFileDescriptor)
                {
                    size_t len = write(ComFileDescriptor, &c, 1);
                    if (len != 1) 
                    {
                        std::ostringstream oss;
                        oss << "\nlinux_pinout_uart::putcqq(): write failure (len: " << len << ", errno: " << errno << ")\n";
                        MagAOXAppT::log<software_error>({__FILE__, __LINE__, oss.str()});      
                        
                        if (autoreopen) 
                        {
                            MagAOXAppT::log<text_log>("\nlinux_pinout_uart::autoreopen.\n");
                            deinit();
                            PinoutConfig pinoutConfig = PinoutConfig::CreateSerialConfig(Baud, Device, RtsCts, OddParity);
                            init(pinoutConfig);
                        }		
                        
                        return('\0'); 
                    }		
                }
                else
                {
					MagAOXAppT::log<software_error>({__FILE__, __LINE__, "\nlinux_pinout_uart::putcqq(): read on uninitialized port; please open port!\n"});
                }

                return (c);
            }

            virtual void flushoutput()
            {
                if (-1 != ComFileDescriptor)
                {
                    fsync(ComFileDescriptor);
                    //~ int BytesWaitingToWrite;
                    //~ ioctl(ComFileDescriptor, TIOCOUTQ, &BytesWaitingToWrite); //get number of bytes in output buffer
                    //~ printf("linux_pinout_uart::flushoutput(): bytes in output buffer: %u\n", BytesWaitingToWrite);
                }
                else
                {
                    MagAOXAppT::log<software_error>({__FILE__, __LINE__, "\nlinux_pinout_uart::flushout(): fflush on uninitialized port; please open port!\n"});
                }
            }
            
            virtual void purgeinput()
            {
                if (-1 != ComFileDescriptor)
                {
                    tcflush(ComFileDescriptor, TCIFLUSH);
                }
                else
                {
                    MagAOXAppT::log<software_error>({__FILE__, __LINE__, "\nlinux_pinout_uart::flushout(): fflush on uninitialized port; please open port!\n"});
                }
            }
            
            virtual void flush()
            {
                flushoutput();
                purgeinput();
            }
            
            virtual bool isopen() const { return(-1 != ComFileDescriptor); }
            
            bool AutoReopen() { return(autoreopen); }
            
            void AutoReopen(const bool ar) { autoreopen = ar; }
            
            private:

                int ComFileDescriptor; //-1: not open
            
                bool echo;
            
                bool autoreopen;
            
                uint32_t Baud;
                static const size_t DeviceMaxLen = 256;
                char Device[DeviceMaxLen];
                bool RtsCts;
                bool OddParity;
        };

	} // namespace app
} // namespace MagAOX