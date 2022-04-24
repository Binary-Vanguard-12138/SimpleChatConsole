#pragma once
#include <WinSock2.h>
#include <WS2tcpip.h>


#define CMD_REGISTER	"$register"
#define CMD_GET_LIST	"$getlist"
#define CMD_GET_LOG		"$getlog"
#define CMD_EXIT		"$exit"

#define STATUS_SV_FULL	"sv_full"
#define STATUS_SV_SUCCESS	"sv_success"

char* my_inet_ntoa(IN_ADDR s);