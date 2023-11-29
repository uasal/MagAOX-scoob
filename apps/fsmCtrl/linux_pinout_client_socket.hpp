/// \file
/// $Source: /raincloud/src/projects/include/client_socket/linux_pinout_client_socket.hpp,v $
/// $Revision: 1.9 $
/// $Date: 2009/11/14 00:20:04 $
/// $Author: steve $

#ifndef _linux_pinout_client_socket_h
#define _linux_pinout_client_socket_h
#pragma once

#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#ifdef WIN32
	//~ #include <windows.h>
	#include <io.h>
	#include <winsock.h>
#else
	#include <unistd.h>  /* UNIX standard function definitions */
	#include <fcntl.h>   /* File control definitions */
	#include <errno.h>   /* Error number definitions */
	#include <termios.h> /* POSIX terminal control definitions */
	#include <sys/ioctl.h> /* ioctl() */
	#include <sys/socket.h> /* FIONREAD on cygwin */
	#include <netinet/in.h> /* struct sockaddr_in */
	#include <netinet/ip.h>
	#include <netdb.h>
	#include <sys/types.h>
	#include <sys/stat.h>
#endif

// #include "format/formatf.h"

#include "IUart.h"

#define HOST_NAME_SIZE      255

class linux_pinout_client_socket : public IUart
{
public:

	linux_pinout_client_socket() : IUart(), hSocket(-1) { }
	virtual ~linux_pinout_client_socket() { }

	virtual int init(const int HostPort, const char* HostName)
	{
		struct hostent* pHostInfo;   /* holds info about a machine */
		struct sockaddr_in Address;  /* Internet socket address stuct */
		long nHostAddress;
		char strHostName[HOST_NAME_SIZE];
		int nHostPort = 20339;

		if (-1 != hSocket) { deinit(); }

		strcpy(strHostName, "localhost");

		if (NULL != HostName)
		{
			strncpy(strHostName, HostName, HOST_NAME_SIZE);
			strHostName[HOST_NAME_SIZE] = '\0';
		}
		if (0 != HostPort)
		{
			nHostPort = HostPort;
		}

		#ifdef WIN32
		formatf("\nStarting Winsock");
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2,0), &wsaData) != 0)
		{
			printf("\nZigSockServer: WSAStartup failed.\n");
			return 0;
		}
		#endif

		printf("\nlinux_pinout_client_socket::init(): Making a socket");
		/* make a socket */
		hSocket=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);

		if(hSocket == -1)
		{
			#ifdef WIN32
			printf("\nlinux_pinout_client_socket::init(): Could not make a socket: %d\n", WSAGetLastError());
			#else
			printf("\nlinux_pinout_client_socket::init(): Could not make a socket\n");
			#endif
			return(errno);
		}
		else
		{
			printf("\nlinux_pinout_client_socket::init(): Socket handle: %d\n", hSocket);
		}

		/* get IP address from name */
		pHostInfo=gethostbyname(strHostName);
		if (NULL == pHostInfo)
		{
			#ifdef WIN32
			printf("\nlinux_pinout_client_socket::init(): Could not gethostbyname(): %d\n", WSAGetLastError());
			#else
			printf("\nlinux_pinout_client_socket::init(): Could not gethostbyname(): %s", strerror(errno));
			#endif
			return(errno);
		}
		/* copy address into long */
		memcpy(&nHostAddress,pHostInfo->h_addr,pHostInfo->h_length);

		/* fill address struct */
		Address.sin_addr.s_addr=nHostAddress;
		Address.sin_port=htons(nHostPort);
		Address.sin_family=AF_INET;

		printf("\nlinux_pinout_client_socket::init(): Connecting to %s", strHostName);
		printf(" on port %d", nHostPort);

		int isConnected = connect(hSocket,(struct sockaddr*)&Address,sizeof(Address));
		/* connect to host */
		if(isConnected == -1)
		{
			#ifdef WIN32
			printf("\nlinux_pinout_client_socket::init(): Could not connect to host: %d\n", WSAGetLastError());
			#else
			printf("\nlinux_pinout_client_socket::init(): Could not connect to host: %s", strerror(errno));
			#endif
			return(errno);
		}
		else { printf("\nlinux_pinout_client_socket::init(): Connected.\n"); }

		return(IUartOK);
	}

	virtual void deinit()
	{
		if (-1 != hSocket)
		{
			printf("\nlinux_pinout_client_socket::deinit(): Closing socket\n");

			#ifdef WIN32
			closesocket(hSocket);
			#else
			close(hSocket);
			#endif

			hSocket = -1;
		}

		#ifdef WIN32
		WSACleanup();
		#endif
	}

	virtual bool dataready() const
	{
		fd_set sockset;
		struct timeval  nowait;
		memset((char *)&nowait,0,sizeof(nowait));
		if (-1 == hSocket) { return(false); } //open?
		FD_ZERO(&sockset);
		FD_SET(hSocket, &sockset);
		int result = select(hSocket + 1, &sockset, NULL, NULL, &nowait);
		if (result < 0)	{ } //"You have an error"
		else if (result == 1)
		{
			if (FD_ISSET(hSocket, &sockset)) // The socket has data. For good measure, it's not a bad idea to test further
			{
				return(true);
			}
		}
		return(false);
	}

	virtual char getcqq()
	{
		char c = 0;

		if (-1 != hSocket)
		{
			int numbytes = recv(hSocket,&c,1,0);
			if (1 != numbytes)
			{
				#ifdef WIN32
				formatf("linux_pinout_client_socket::getcqq(): read from socket error (%d bytes gave: %u [errno: %u])\n", numbytes, WSAGetLastError(), errno);
				#else
				cout << "linux_pinout_client_socket::getcqq(): read from socket error (" << numbytes << " bytes gave: " << errno << ")\n";
				#endif

				deinit();

				return(0);
			}
			//~ else { formatf("<-'x%.2X'", c); fflush(stdout); }
			//~ putchar(c);
			//~ formatf("%c", c);
		}
		else
		{
			printf("linux_pinout_client_socket::getcqq(): read on uninitialized socket; please open socket!\n");
		}

  		return(c);
	}

	virtual char putcqq(char c)
	{
		if (-1 != hSocket)
		{
			int numbytes = send(hSocket,&c,1,0);

			if (1 != numbytes)
			{
				#ifdef WIN32
				formatf("linux_pinout_client_socket::putcqq(): write from socket error (%d bytes gave: %u [errno: %u])\n", numbytes, WSAGetLastError(), errno);
				#else
				cout << "linux_pinout_client_socket::putcqq(): write from socket error (" << numbytes << " bytes gave: " << errno << ")\n";
				#endif

				deinit();

				return(0);
			}
		}
		else
		{
			printf("linux_pinout_client_socket::putcqq(): write on uninitialized socket; please open socket!\n");
		}

 		return (c);
	}

	virtual void flushoutput()
	{
		if (-1 != hSocket)
		{
			#ifdef WIN32
			#else
			fsync(hSocket);
			#endif
		}
		else
		{
			printf("linux_pinout_client_socket::flushout(): fflush on uninitialized socket; please open socket!\n");
		}
	}

	virtual void purgeinput()
	{
		//~ if(dataready)
	}

	virtual bool connected() const
	{
		return(-1 != hSocket);
	}

	virtual bool isopen() const { return(connected()); }

	public:

		int hSocket;                 /* handle to socket */
};

#endif //linux_pinout_client_socket_h
