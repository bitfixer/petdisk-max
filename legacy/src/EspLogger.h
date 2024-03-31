#ifndef __esp_logger_h__
#define __esp_logger_h__

#include "EspConn.h"

class EspLogger {
public:
	EspLogger(EspConn* espConn)
	: _espConn(espConn) {

	}

	void init()
	{
		_espConn->startClient("192.168.0.110", 8080, 4, UDP_MODE);
	}

	void log(const char* str)
	{
		_espConn->sendData(4, (unsigned char*)str, strlen(str));
	}

	void log(unsigned char* data, int length)
	{
		_espConn->sendData(4, data, length);
	}

private:
	EspConn* _espConn;
};

#endif