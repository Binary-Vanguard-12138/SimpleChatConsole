#include "NetLib.h"

char* my_inet_ntoa(IN_ADDR s) {
	static char szSample[0x100] = {  };
	inet_ntop(AF_INET, &s, szSample, sizeof(szSample));
	return szSample;
}