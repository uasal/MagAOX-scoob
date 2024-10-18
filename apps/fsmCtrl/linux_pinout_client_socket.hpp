/// \file
/// $Source: /raincloud/src/projects/include/client_socket/linux_pinout_client_socket.hpp,v $
/// $Revision: 1.9 $
/// $Date: 2009/11/14 00:20:04 $
/// $Author: steve $

#pragma once

#include <stdio.h>	/* Standard input/output definitions */
#include <string.h> /* String function definitions */

#include <unistd.h>		/* UNIX standard function definitions */
#include <fcntl.h>		/* File control definitions */
#include <errno.h>		/* Error number definitions */
#include <termios.h>	/* POSIX terminal control definitions */
#include <sys/ioctl.h>	/* ioctl() */
#include <sys/socket.h> /* FIONREAD on cygwin */
#include <netinet/in.h> /* struct sockaddr_in */
#include <netinet/ip.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>

// #include "format/formatf.h"

#include "IUart.h"

#define HOST_NAME_SIZE 255

namespace MagAOX
{
	namespace app
	{

		class linux_pinout_client_socket : public IUart
		{
		public:
			linux_pinout_client_socket() : IUart(), hSocket(-1) {}
			virtual ~linux_pinout_client_socket() {}

			int init(const PinoutConfig& config)
			{
				struct hostent *pHostInfo;	/* holds info about a machine */
				struct sockaddr_in Address; /* Internet socket address stuct */
				long nHostAddress;
				char strHostName[HOST_NAME_SIZE];
				int nHostPort = 20339;

				if (-1 != hSocket)
				{
					deinit();
				}

				strcpy(strHostName, "localhost");

				if (NULL != config.HostName)
				{
					strncpy(strHostName, config.HostName, HOST_NAME_SIZE);
					strHostName[HOST_NAME_SIZE] = '\0';
				}
				if (0 != config.HostPort)
				{
					nHostPort = config.HostPort;
				}

				MagAOXAppT::log<text_log>("linux_pinout_client_socket::init(): Making a socket");
				/* make a socket */
				hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

				if (hSocket == -1)
				{
					MagAOXAppT::log<software_error>({__FILE__, __LINE__, "linux_pinout_client_socket::init(): Could not make a socket"});
					return (errno);
				}
				else
				{
					std::ostringstream oss;
					oss << "linux_pinout_client_socket::init(): Socket handle: " << hSocket;
					MagAOXAppT::log<text_log>(oss.str());
				}

				/* get IP address from name */
				pHostInfo = gethostbyname(strHostName);
				if (NULL == pHostInfo)
				{
					std::ostringstream oss;
					oss << "linux_pinout_client_socket::init(): Could not gethostbyname(): " << strerror(errno);
					MagAOXAppT::log<software_error>({__FILE__, __LINE__, oss.str()});
					return (errno);
				}
				/* copy address into long */
				memcpy(&nHostAddress, pHostInfo->h_addr, pHostInfo->h_length);

				/* fill address struct */
				Address.sin_addr.s_addr = nHostAddress;
				Address.sin_port = htons(nHostPort);
				Address.sin_family = AF_INET;

				std::ostringstream oss;
				oss << "linux_pinout_client_socket::init(): Connecting to " << strHostName << " on port " << nHostPort;
				MagAOXAppT::log<text_log>(oss.str());

				int isConnected = connect(hSocket, (struct sockaddr *)&Address, sizeof(Address));
				/* connect to host */
				if (isConnected == -1)
				{
					std::ostringstream oss;
					oss << "linux_pinout_client_socket::init(): Could not connect to host: " << strerror(errno);
					MagAOXAppT::log<software_error>({__FILE__, __LINE__, oss.str()});
					return (errno);
				}
				else
				{
					MagAOXAppT::log<text_log>("linux_pinout_client_socket::init(): Connected.");
				}

				return (IUartOK);
			}

			virtual void deinit()
			{
				if (-1 != hSocket)
				{
					MagAOXAppT::log<text_log>("linux_pinout_client_socket::deinit(): Closing socket");

					close(hSocket);

					hSocket = -1;
				}
			}

			virtual bool dataready() const
			{
				fd_set sockset;
				struct timeval nowait;
				memset((char *)&nowait, 0, sizeof(nowait));
				if (-1 == hSocket)
				{
					return (false);
				} // open?
				FD_ZERO(&sockset);
				FD_SET(hSocket, &sockset);
				int result = select(hSocket + 1, &sockset, NULL, NULL, &nowait);
				if (result < 0)
				{
				} //"You have an error"
				else if (result == 1)
				{
					if (FD_ISSET(hSocket, &sockset)) // The socket has data. For good measure, it's not a bad idea to test further
					{
						return (true);
					}
				}
				return (false);
			}

			virtual char getcqq()
			{
				char c = 0;

				if (-1 != hSocket)
				{
					int numbytes = recv(hSocket, &c, 1, 0);
					if (1 != numbytes)
					{
						std::ostringstream oss;
						oss << "linux_pinout_client_socket::getcqq(): read from socket error (" << numbytes << " bytes gave: " << errno << ")";
						MagAOXAppT::log<software_error>({__FILE__, __LINE__, oss.str()});

						deinit();

						return (0);
					}
				}
				else
				{
					MagAOXAppT::log<text_log>("linux_pinout_client_socket::getcqq(): read on uninitialized socket; please open socket!");
				}

				return (c);
			}

			virtual char putcqq(char c)
			{
				// std::ostringstream oss;
				// oss << "Char: " << std::setw(2) << std::setfill('0') << std::hex << static_cast<unsigned>(c);
				// MagAOXAppT::log<text_log>(oss.str());

				if (-1 != hSocket)
				{
					int numbytes = send(hSocket, &c, 1, 0);

					if (1 != numbytes)
					{
						std::ostringstream oss;
						oss << "linux_pinout_client_socket::putcqq(): write from socket error (" << numbytes << " bytes gave: " << errno << ")";
						MagAOXAppT::log<software_error>({__FILE__, __LINE__, oss.str()});

						deinit();

						return (0);
					}
				}
				else
				{
					MagAOXAppT::log<software_warning>({__FILE__, __LINE__, "linux_pinout_client_socket::putcqq(): write on uninitialized socket; please open socket!"});
				}

				return (c);
			}

			virtual void flushoutput()
			{
				if (-1 != hSocket)
				{
					fsync(hSocket);
				}
				else
				{
					MagAOXAppT::log<software_warning>({__FILE__, __LINE__, "linux_pinout_client_socket::putcqq(): fflush on uninitialized socket; please open socket!"});
				}
			}

			virtual void purgeinput()
			{
				//~ if(dataready)
			}

			virtual bool connected() const
			{
				return (-1 != hSocket);
			}

			virtual bool isopen() const { return (connected()); }

		public:
			int hSocket; /* handle to socket */
		};

	} // namespace app
} // namespace MagAOX