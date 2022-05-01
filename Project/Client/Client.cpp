// Client.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "NetLib.h"

void usage() {
	printf("client <udp_broadcast_port>\n");
	printf("For example:\n");
	printf("client 5555\n");
}

int send_cmd_to_tcp_server(SOCKET sSocket, char* szContent) {
	if (INVALID_SOCKET == sSocket || NULL == szContent || 0 >= strlen(szContent)) {
		return INVALID_SOCKET;
	}
	int nLen = strlen(szContent);
	int nSentLen = send(sSocket, szContent, nLen, 0);
	if (SOCKET_ERROR == nSentLen) {
		printf("Failed to send to tcp server, errno = %d\n", WSAGetLastError());
		return SOCKET_ERROR;
	}
	return nSentLen;
}

int main(int argc, char* argv[])
{
	SOCKET  sCliTcpSocket = INVALID_SOCKET, sCliUdpSocket = INVALID_SOCKET;
	struct sockaddr_in tTcpSvrAddr = {}, tUdpSvrAddr = {}, tUdpClientAddr = {};
	char* szSvrAddress = NULL;
	char szUserName[0x40] = {};
	char szSendBuf[0x100] = {}, szRecvBuf[0x100] = {}, szCommand[0x100] = {};
	u_short usTcpSvrPort = 0, usBroadcastPort = 0;
	int nRecvLen = 0, nSentLen = 0, err = 0;
	char chBroadCastEnable = 1; 
	u_long nNonBlock = 1;
	int bReuseAddr = 1;
	int nFlag = 0;
	WSADATA wsaData = {};
	FD_SET readfds;
	struct timeval tSvrTimeOut = { 1, 0 };
	if (2 > argc) {
		usage();
		goto end;
	}

// 	szSvrAddress = argv[1];
// 	usTcpSvrPort = atoi(argv[2]);
	usBroadcastPort = atoi(argv[1]);

	err = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (0 != err) {
		printf("WSAStartup failed with errno = %d\n", err);
		goto end;
	}

	sCliUdpSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (INVALID_SOCKET == sCliUdpSocket) {
		printf("create udp client socket failed, errno = %d\n", WSAGetLastError());
		goto end;
	}
	tUdpClientAddr.sin_family = AF_INET;
	tUdpClientAddr.sin_port = htons(usBroadcastPort);
	tUdpClientAddr.sin_addr.s_addr = INADDR_ANY;
	if (SOCKET_ERROR == setsockopt(sCliUdpSocket, SOL_SOCKET, SO_BROADCAST, &chBroadCastEnable, sizeof(chBroadCastEnable))) {
		printf("setsockopt for udp broadcast socket failed, errno = %d\n", WSAGetLastError());
		goto end;
	}

	if (SOCKET_ERROR == setsockopt(sCliUdpSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)& bReuseAddr, sizeof(bReuseAddr))) {
		printf("setsockopt for udp SO_REUSEADDR socket failed, errno = %d\n", WSAGetLastError());
		goto end;
	}

	if (SOCKET_ERROR == bind(sCliUdpSocket, (struct sockaddr*) & tUdpClientAddr, sizeof(tUdpClientAddr))) {
		printf("Failed to bind UDP on %d port, errno = %d\n", usBroadcastPort, WSAGetLastError());
		goto end;
	}

	err = ioctlsocket(sCliUdpSocket, FIONBIO, &nNonBlock);
	if (NOERROR != err) {
		printf("set nonblocking failed, errno = %d\n", err);
		goto end;
	}

	while (TRUE) {
		FD_ZERO(&readfds);
		FD_SET(sCliUdpSocket, &readfds);
		err = select(sCliUdpSocket + 1, &readfds, NULL, NULL, &tSvrTimeOut);
		if (0 == err) {
			continue;
		}
		if ((SOCKET_ERROR == err) && (errno != EINTR))
		{
			printf("select error");
			break;
		}
		int nSvrAddrLen = sizeof(tUdpSvrAddr);
		nRecvLen = recvfrom(sCliUdpSocket, szRecvBuf, sizeof(szRecvBuf), 0, (struct sockaddr*)&tUdpSvrAddr, &nSvrAddrLen);
		if (SOCKET_ERROR == nRecvLen) {
			int nErrno = WSAGetLastError();
			if (WSAEWOULDBLOCK == nErrno)
				continue;
			else {
				printf("recvfrom failed, errno = %d\n", nErrno);
				break;
			}
		}
		else if (0 == nRecvLen) {
			continue;
		}
		else if (sizeof(struct sockaddr_in) == nRecvLen) {
			memcpy(&tTcpSvrAddr, szRecvBuf, sizeof(tTcpSvrAddr));
			printf("received server address on UDP broadcast [%s:%d]\n", my_inet_ntoa(tTcpSvrAddr.sin_addr), htons(tTcpSvrAddr.sin_port));
			break;
		}
	}

	closesocket(sCliUdpSocket);
	sCliUdpSocket = INVALID_SOCKET;

	sCliTcpSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (INVALID_SOCKET == sCliTcpSocket) {
		printf("create tcp client socket failed, errno = %d\n", WSAGetLastError());
		goto end;
	}

// 	tTcpSvrAddr.sin_family = AF_INET;
// 	inet_pton(AF_INET, szSvrAddress, &tTcpSvrAddr.sin_addr);
// 	tTcpSvrAddr.sin_port = htons(usTcpSvrPort);
	if (SOCKET_ERROR == connect(sCliTcpSocket, (struct sockaddr*) & tTcpSvrAddr, sizeof(tTcpSvrAddr))) {
		printf("connect to server failed errno = %d\n", WSAGetLastError());
		goto end;
	}
	printf("connected to server on %s:%d\n", my_inet_ntoa(tTcpSvrAddr.sin_addr), htons(tTcpSvrAddr.sin_port));

	while (TRUE) {
		printf("please input your name.\n");
		gets_s(szUserName);
		if (strlen(szUserName) > 0)
			break;
	}
	snprintf(szSendBuf, _countof(szSendBuf) - 1, "$register %s", szUserName);
	nSentLen = send_cmd_to_tcp_server(sCliTcpSocket, szSendBuf);
	if (SOCKET_ERROR == nSentLen) {
		printf("Failed to send $register command to server, errno = %d\n", WSAGetLastError());
		goto end;
	}
	 nRecvLen = recv(sCliTcpSocket, szRecvBuf, sizeof(szRecvBuf), 0);
	 if (SOCKET_ERROR == nRecvLen) {
		 printf("Failed to recv command from server, errno = %d\n", WSAGetLastError());
		 goto end;
	 }
	 printf("recv %d bytes from server, content is \n%s\n", nRecvLen, szRecvBuf);
	 if (strcmp(szRecvBuf, STATUS_SV_FULL) == 0) {
		 printf("Server is full, exiting client\n");
		 goto end;
	 }
	 else if (strcmp(szRecvBuf, STATUS_SV_SUCCESS)) {
		 printf("Unknown server command, exiting client...\n");
		 goto end;
	 }

	 while (TRUE) {
		 while (TRUE) {
			 printf("please input your command\n$exit, $getlist, and $getlog or chat message\n");
			 memset(szCommand, 0, sizeof(szCommand));
			 gets_s(szCommand);
			 if (strlen(szCommand) > 0)
				 break;
		 }
		 send_cmd_to_tcp_server(sCliTcpSocket, szCommand);
		 memset(szRecvBuf, 0, sizeof(szRecvBuf));
		 if (!strcmp(szCommand, CMD_GET_LOG)) {
			 long lFileSize = 0;
			 recv(sCliTcpSocket, (char*)&lFileSize, sizeof(lFileSize), 0);
			 if (0 < lFileSize) {
				 char szFileName[0x100] = {};
				 _snprintf_s(szFileName, _countof(szFileName) - 1, "%s.log", szUserName);
				 FILE* pfLog = NULL;
				 err = fopen_s(&pfLog, szFileName, "wb");
				 if (NULL == pfLog) {
					 printf("Failed to open log file %s, errno = %d\n", szFileName, err);
				 }
				 else {
					 long lOffset = 0;
					 while (true)
					 {
						 nRecvLen = recv(sCliTcpSocket, szRecvBuf, sizeof(szRecvBuf), 0);
						 if (0 >= nRecvLen)
							 break;
						 fwrite(szRecvBuf, sizeof(char), nRecvLen, pfLog);
						 lOffset += nRecvLen;
						 if (lOffset >= lFileSize)
							 break;
					 }
					 fclose(pfLog);
					 printf("received %d bytes of log file to %s.\n", lFileSize, szFileName);
				 }
			 }
		 }
		 else {
			 nRecvLen = recv(sCliTcpSocket, szRecvBuf, sizeof(szRecvBuf), 0);
			 if (SOCKET_ERROR == nRecvLen) {
				 printf("Failed to recv command from server, errno = %d\n", WSAGetLastError());
				 goto end;
			 }
			 else if (0 == nRecvLen) {
				 printf("server closed the client socket.\n");
				 goto end;
			 }
			 printf("recv %d bytes from server, content is \n%s\n", nRecvLen, szRecvBuf);
		 }
	 }
end:
	if (INVALID_SOCKET != sCliTcpSocket)
		closesocket(sCliTcpSocket);
	if (INVALID_SOCKET != sCliUdpSocket)
		closesocket(sCliUdpSocket);
	WSACleanup();
	printf("Client is now exiting...\n");
	return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
