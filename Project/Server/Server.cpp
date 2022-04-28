// Server.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "../Client/NetLib.h"

void usage() {
	printf("server.exe <tcp_server_ip> <tcp_listen_port> <udp_bind_port> <udp_broadcast_addr> <udp_broadcast_port>\n");
	printf("For example\n");
	printf("server.exe 192.168.10.2 4444 5555 192.168.255.255 6666\n");
}

#define MAX_CLIENT_NUMBER	30
#define MAX_CLIENT_NAME_LEN	0x40

SOCKET g_asClientSockets[MAX_CLIENT_NUMBER] = {};
char g_aszClientNames[MAX_CLIENT_NUMBER][MAX_CLIENT_NAME_LEN] = {};

void addLog(char* szContent, int nLen) {
	FILE* pfLogFile = NULL;
	if (NULL == szContent || /*strlen(szContent) == 0 || */0 == nLen)
		return;

	int err = fopen_s(&pfLogFile, LOG_FILE_NAME, "ab");
	if (NULL == pfLogFile) {
		return;
	}
	fwrite(szContent, sizeof(char), nLen, pfLogFile);
	fwrite("\n", sizeof(char), 1, pfLogFile);
	fclose(pfLogFile);
}

void removeClient(SOCKET s) {
	int i;
	for (i = 0; i < MAX_CLIENT_NUMBER; i++) {
		if (0 != g_asClientSockets[i] && g_asClientSockets[i] == s) {
			g_asClientSockets[i] = 0;
			memset(g_aszClientNames[i], 0, MAX_CLIENT_NAME_LEN);
			closesocket(s);
		}
	}
}

char* getConnectedClientList() {
	static char s_szBuffer[MAX_CLIENT_NUMBER * MAX_CLIENT_NAME_LEN] = {};
	int i;
	memset(s_szBuffer, 0, sizeof(s_szBuffer));
	bool bFirst = true;
	for (i = 0; i < MAX_CLIENT_NUMBER; i++) {
		if (0 != g_asClientSockets[i]) {
			if (bFirst) {
				bFirst = false;
			}
			else {
				strcat_s(s_szBuffer, ", ");
			}
			strcat_s(s_szBuffer, g_aszClientNames[i]);
		}
	}
	return s_szBuffer;
}

int getConnectedClientCount() {
	int i, nRet = 0;
	for (i = 0; i < MAX_CLIENT_NUMBER; i++) {
		if (0 != g_asClientSockets[i]) {
			nRet++;
		}
	}
	return nRet;
}

int main(int argc, char* argv[])
{
	SOCKET  nSvrTcpSocket = INVALID_SOCKET, sSvrUdpSocket = INVALID_SOCKET, sCliTcpSocket = INVALID_SOCKET, sCliUdpSocket = INVALID_SOCKET;
	int i, err = 0;
	struct sockaddr_in tTcpSvrAddr = {}, tUdpSvrAddr = {}, tTcpClientAddr = {}, tUdpClientAddr = {};
	WSADATA wsaData = {};
	char* szServerAddr = NULL, *szBroadcastAddr = NULL;
	u_short nTcpSvrPort = 0, nUdpSvrPort = 0, nUdpCliPort = 0;
	int nTcpClientAddrLen = 0, nUdpClientAddrLen = 0;
	fd_set readfds;
	int max_sd, activity;
	struct timeval tSvrTimeOut = {1, 0};
	char szRecvBuffer[0x400] = {}, szSendBuffer[0x400] = {};
	int nRecvLen = 0, nSentLen = 0;
	FILE* pfLogFile = NULL;
	char chBroadCastEnable = 1;
	int bReuseAddr = 1;
	int nArgs = 1;

	if (6 > argc) {
		usage();
		goto end;
	}

	// truncate the log file.
	err = fopen_s(&pfLogFile, LOG_FILE_NAME, "w");
	if (NULL == pfLogFile) {
		printf("Failed to open log file in %s, errno = %d\n", LOG_FILE_NAME, err);
		goto end;
	}
	fclose(pfLogFile);

	// read paramters
	szServerAddr = argv[nArgs++];
	nTcpSvrPort = atoi(argv[nArgs ++]);
	nUdpSvrPort = atoi(argv[nArgs++]);
	szBroadcastAddr = argv[nArgs++];
	nUdpCliPort = atoi(argv[nArgs++]);
	
	//initialise all client_socket[] to 0 so not checked
	for (i = 0; i < MAX_CLIENT_NUMBER; i++)
	{
		g_asClientSockets[i] = 0;
	}
	err = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (0 != err) {
		printf("WSAStartup failed with errno = %d\n", err);
		goto end;
	}
	nSvrTcpSocket = socket(AF_INET, SOCK_STREAM, 0/*IPPROTO_TCP*/);
	if (INVALID_SOCKET == nSvrTcpSocket) {
		printf("create tcp server socket failed, errno = %d\n", WSAGetLastError());
		goto end;
	}
	tTcpSvrAddr.sin_family = AF_INET;
	//tTcpSvrAddr.sin_addr.s_addr = INADDR_ANY;
	inet_pton(AF_INET, szServerAddr, &tTcpSvrAddr.sin_addr);
	tTcpSvrAddr.sin_port = htons(nTcpSvrPort);
	if (0 != bind(nSvrTcpSocket, (struct sockaddr*) & tTcpSvrAddr, sizeof(tTcpSvrAddr))) {
		printf("bind TCP on %d port failed, errno = %d\n", nTcpSvrPort, WSAGetLastError());
		goto end;
	}

	if (0 != listen(nSvrTcpSocket, 5)) {
		printf("listen on tcp server socket failed. errno = %d\n", WSAGetLastError());
		goto end;
	}
	printf("TCP server is listening on address [%s:%d]\n", my_inet_ntoa(tTcpSvrAddr.sin_addr), nTcpSvrPort);

	// Phase 2
	sSvrUdpSocket = socket(AF_INET, SOCK_DGRAM, 0/*IPPROTO_UDP*/);
	if (INVALID_SOCKET == sSvrUdpSocket) {
		printf("create udp server socket failed, errno = %d\n", WSAGetLastError());
		goto end;
	}
	if (SOCKET_ERROR == setsockopt(sSvrUdpSocket, SOL_SOCKET, SO_BROADCAST, &chBroadCastEnable, sizeof(chBroadCastEnable))) {
		printf("setsockopt for udp SO_BROADCAST socket failed, errno = %d\n", WSAGetLastError());
		goto end;
	}
	if (SOCKET_ERROR == setsockopt(sSvrUdpSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)& bReuseAddr, sizeof(bReuseAddr))) {
		printf("setsockopt for udp SO_REUSEADDR socket failed, errno = %d\n", WSAGetLastError());
		goto end;
	}
	tUdpSvrAddr.sin_family = AF_INET;
	tUdpSvrAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	tUdpSvrAddr.sin_port = htons(nUdpSvrPort);
	if (0 != bind(sSvrUdpSocket, (struct sockaddr*) & tUdpSvrAddr, sizeof(tUdpSvrAddr))) {
		printf("bind UDP on %d port failed, errno = %d\n", nUdpSvrPort, WSAGetLastError());
		goto end;
	}

	memset(&tUdpClientAddr, 0, sizeof(tUdpClientAddr));
	tUdpClientAddr.sin_family = AF_INET;
	tUdpClientAddr.sin_port = htons(nUdpCliPort);
// 	inet_addr(szBroadcastAddr);
	inet_pton(AF_INET, szBroadcastAddr, &tUdpClientAddr.sin_addr);

	while (TRUE) {
		FD_ZERO(&readfds);
		FD_SET(nSvrTcpSocket, &readfds);
		max_sd = nSvrTcpSocket;
		for (i = 0; i < MAX_CLIENT_NUMBER; i++) {
			//socket descriptor
			sCliTcpSocket = g_asClientSockets[i];

			//if valid socket descriptor then add to read list
			if (sCliTcpSocket > 0)
				FD_SET(sCliTcpSocket, &readfds);

			//highest file descriptor number, need it for the select function
			if (sCliTcpSocket > max_sd)
				max_sd = sCliTcpSocket;
		}

		activity = select(max_sd + 1, &readfds, NULL, NULL, &tSvrTimeOut);

		if (0 == activity) {
			// Send UDP broadcast message
			sendto(sSvrUdpSocket, (const char*)& tTcpSvrAddr, sizeof(tTcpSvrAddr), 0, (struct sockaddr*)& tUdpClientAddr, sizeof(tUdpClientAddr));
			continue;
		}
		if ((SOCKET_ERROR == activity) && (errno != EINTR))
		{
			printf("select error");
			break;
		}


		//If something happened on the master socket , then its an incoming connection
		if (FD_ISSET(nSvrTcpSocket, &readfds))
		{
			nTcpClientAddrLen = sizeof(tTcpClientAddr);
			sCliTcpSocket = accept(nSvrTcpSocket, (struct sockaddr*) & tTcpClientAddr, &nTcpClientAddrLen);
			if (INVALID_SOCKET == sCliTcpSocket) {
				printf("accept tcp socket failed, errno = %d\n", WSAGetLastError());
			}
			else {
				//inform user of socket number - used in send and receive commands
				printf("New connection , socket fd is %d , ip is : %s , port : %d \n", sCliTcpSocket, my_inet_ntoa(tTcpClientAddr.sin_addr), ntohs(tTcpClientAddr.sin_port));
				//add new socket to array of sockets
				for (i = 0; i < MAX_CLIENT_NUMBER; i++)
				{
					//if position is empty
					if (g_asClientSockets[i] == 0)
					{
						g_asClientSockets[i] = sCliTcpSocket;
						printf("Adding to list of sockets as %d\n", i);
						break;
					}
				}
			}
		}

		//else its some IO operation on some other socket :)
		for (i = 0; i < MAX_CLIENT_NUMBER; i++)
		{
			sCliTcpSocket = g_asClientSockets[i];
			if (0 == sCliTcpSocket)
				continue;
			if (FD_ISSET(sCliTcpSocket, &readfds))
			{
				//Check if it was for closing , and also read the incoming message
				memset(szRecvBuffer, 0, sizeof(szRecvBuffer));
				nRecvLen = recv(sCliTcpSocket, szRecvBuffer, sizeof(szRecvBuffer), 0);
				if (0 == nRecvLen || SOCKET_ERROR == nRecvLen)
				{
					//Somebody disconnected , get his details and print
					getpeername(sCliTcpSocket, (struct sockaddr*) & tTcpClientAddr, (socklen_t*)& nTcpClientAddrLen);
					printf("Host disconnected , ip %s , port %d \n", my_inet_ntoa(tTcpClientAddr.sin_addr), ntohs(tTcpClientAddr.sin_port));
					//Close the socket and mark as 0 in list for reuse
					removeClient(sCliTcpSocket);
				}
				else
				{
					getpeername(sCliTcpSocket, (struct sockaddr*) & tTcpClientAddr, (socklen_t*)& nTcpClientAddrLen);
					printf("received %d bytes from client [%s:%d], content is \n%s\n", nRecvLen, my_inet_ntoa(tTcpClientAddr.sin_addr), ntohs(tTcpClientAddr.sin_port), szRecvBuffer);
					addLog(szRecvBuffer, nRecvLen);
					if (0 == strncmp(szRecvBuffer, CMD_REGISTER, strlen(CMD_REGISTER))) {
						if (getConnectedClientCount() >= MAX_CLIENT_NUMBER) {
							send(sCliTcpSocket, STATUS_SV_FULL, strlen(STATUS_SV_FULL), 0);
						}
						else {
							send(sCliTcpSocket, STATUS_SV_SUCCESS, strlen(STATUS_SV_SUCCESS), 0);
							strncpy_s(g_aszClientNames[i], szRecvBuffer + strlen(CMD_REGISTER) + 1, MAX_CLIENT_NAME_LEN - 1);
							printf("Registered new user with name [%s]\n", g_aszClientNames[i]);
						}
					}
					else if (!strcmp(szRecvBuffer, CMD_EXIT)) {
						removeClient(sCliTcpSocket);
					}
					else if (!strcmp(szRecvBuffer, CMD_GET_LIST)) {
						if (getConnectedClientCount() == 0) {
							send(sCliTcpSocket, STATUS_EMPTY, strlen(STATUS_EMPTY), 0);
						}
						else {
							char* szList = getConnectedClientList();
							send(sCliTcpSocket, szList, strlen(szList), 0);
						}
					}
					else if (!strcmp(szRecvBuffer, CMD_GET_LOG)) {
						err = fopen_s(&pfLogFile, LOG_FILE_NAME, "rb");
						if (NULL != pfLogFile) {
							fseek(pfLogFile, 0L, SEEK_END);
							long lFileSize = ftell(pfLogFile);
							long lOffset = 0;
							send(sCliTcpSocket, (const char*)&lFileSize, sizeof(lFileSize), 0);
							fseek(pfLogFile, 0L, SEEK_SET);
							while (TRUE) {
								memset(szSendBuffer, 0, sizeof(szSendBuffer));
								int nReadLen = fread(szSendBuffer, sizeof(char), _countof(szSendBuffer), pfLogFile);
								if (0 >= nReadLen)
									break;
								send(sCliTcpSocket, szSendBuffer, nReadLen, 0);
							}
							fclose(pfLogFile);
						}
					}
					else {
						//printf(szRecvBuffer);
						//set the string terminating NULL byte on the end of the data read
						//szRecvBuffer[nRecvLen] = '\0';
						// simply echo to client.
						send(sCliTcpSocket, szRecvBuffer, strlen(szRecvBuffer), 0);
					}
				}
			}
		}
	}


end:
	if (INVALID_SOCKET != nSvrTcpSocket)
		closesocket(nSvrTcpSocket);
	if (INVALID_SOCKET != sSvrUdpSocket)
		closesocket(sSvrUdpSocket);
	WSACleanup();

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
