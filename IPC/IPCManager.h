/******************************************************************************************
IPCManager.h
Copyright (c) Bit Software, Inc.(2015), All rights reserved.

Purpose: 实现命名管道的进程间通信

Author: xjh

Reviser: dongwei

Created Time: 2015-4-26
******************************************************************************************/

#pragma once

#ifdef _WIN32
	#include<Windows.h>
#else
	#include<string.h>
	#include<unistd.h>
	#include<sys/ipc.h>
	#include<sys/types.h>
	#include<sys/stat.h>
	#include<fcntl.h>
#endif

// 记录错误日志
static void RecordErrorLog(const char* errMsg, int line)
{
#ifdef _WIN32
	printf("%s. ErrorId:%d, Line:%d\n", errMsg, GetLastError(), line);
#else
	printf("%s. ErrorId:%d, Line:%d\n", errMsg, errno, line);
#endif
}

#define RECORD_ERROR_LOG(errMsg)	\
	RecordErrorLog(errMsg, __LINE__)

#ifdef _WIN32

static wchar_t* __MultiByteToWideChar(const char* cPtr)
{
	DWORD len = MultiByteToWideChar(CP_ACP, 0, cPtr, -1, NULL, 0);

	wchar_t* wPtr = new wchar_t[len + 2];
	DWORD nLen = MultiByteToWideChar(CP_ACP, 0, cPtr, -1, wPtr, len);
	if (nLen == 0)
	{
		RECORD_ERROR_LOG("MultiByteToWideChar Error\n");
	}
	wPtr[nLen] = 0;
	return wPtr;
}

// 管道消息发送者
class NamePipeSender
{
public:
	NamePipeSender(const char* pipeName)
		:_pipeName(pipeName)
		, _hPipe(INVALID_HANDLE_VALUE)
	{}

	~NamePipeSender()
	{
		ClosePipe();
	}

	bool Connect()
	{
		ClosePipe();

		wchar_t* wPipeName = __MultiByteToWideChar(_pipeName.c_str());
		BOOL ret = WaitNamedPipe(wPipeName, 1000);
		if (ret == FALSE)
		{
			delete[] wPipeName;
			return false;
		}

		_hPipe = CreateFile(wPipeName,
			GENERIC_READ | GENERIC_WRITE,
			0,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL);

		if (_hPipe == INVALID_HANDLE_VALUE)
		{
			delete[] wPipeName;
			return false;
		}

		return true;
	}

	bool SendMsg(const char* msg, size_t msgLen, size_t& realSize)
	{
		DWORD writeSize;
		if (!WriteFile(_hPipe, msg, msgLen, &writeSize, NULL))
		{
			return false;
		}

		realSize = writeSize;
		return true;
	}

	bool GetReplyMsg(char* msg, size_t msgLen, size_t realLen)
	{
		assert(_hPipe != INVALID_HANDLE_VALUE);

		DWORD readSize;
		if (!ReadFile(_hPipe, msg, msgLen, &readSize, NULL))
		{
			return false;
		}
		msg[readSize] = '\0';

		realLen = readSize;

		return true;
	}

	void ClosePipe()
	{
		if (_hPipe != INVALID_HANDLE_VALUE)
		{
			CloseHandle(_hPipe);
		}
	}

private:
	HANDLE _hPipe;
	string _pipeName;
};

// 管道消息接收者
class NamePipeReceiver
{
public:
	NamePipeReceiver(const char* pipeName)
		:_pipeName(pipeName)
		, _hPipe(INVALID_HANDLE_VALUE)
	{}

	~NamePipeReceiver()
	{
		ClosePipe();
	}

	bool Listen()
	{
		ClosePipe();

		wchar_t* wPipeName = __MultiByteToWideChar(_pipeName.c_str());

		_hPipe = CreateNamedPipe(wPipeName, PIPE_ACCESS_DUPLEX,
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
			PIPE_UNLIMITED_INSTANCES,
			0,
			0,
			NMPWAIT_WAIT_FOREVER,
			0);

		if (_hPipe == INVALID_HANDLE_VALUE)
		{
			delete[] wPipeName;
			return false;
		}

		delete[] wPipeName;
		return true;
	}

	bool ReceiverMsg(char* msg, size_t msgLen, size_t& realLen)
	{
		assert(_hPipe != INVALID_HANDLE_VALUE);
		BOOL ret = ConnectNamedPipe(_hPipe, NULL);

		if (!ret)
		{
			return false;
		}

		DWORD readSize;
		if (!ReadFile(_hPipe, msg, msgLen, &readSize, NULL))
		{
			return false;
		}
		msg[readSize] = '\0';

		realLen = readSize;

		return true;
	}

	bool SendReplyMsg(const char* msg, size_t msgLen, size_t& realSize)
	{
		DWORD writeSize;
		if (!WriteFile(_hPipe, msg, msgLen, &writeSize, NULL))
		{
			return false;
		}

		realSize = writeSize;
		return true;
	}

	void ClosePipe()
	{
		if (_hPipe != INVALID_HANDLE_VALUE)
		{
			DisconnectNamedPipe(_hPipe);
			CloseHandle(_hPipe);
		}
	}

private:
	HANDLE _hPipe;
	string _pipeName;
};

#else

class NamePipeSender
{
public:
	NamePipeSender(const char* pipeName)
		:_pipeName(pipeName)
	{}

	bool Connect()
	{
		return true;
	}

	bool SendMsg(const char* msg, size_t msgLen, size_t& realSize)
	{

		int pipe_fd = open(_pipeName.c_str(), O_WRONLY| O_NONBLOCK);
		int ret = write(pipe_fd, msg, msgLen);
		if (ret == 0)
		{
			return false;
		}

		realSize = ret;
		close(pipe_fd);
		return true;
	}

	bool GetReplyMsg(char* msg, size_t msgLen, size_t realLen)
	{
		int pipe_fd = open(_pipeName.c_str(), O_RDONLY);
		int ret = read(pipe_fd, msg, msgLen);
		if (ret == 0)
		{
			return false;
		}
		msg[ret] = '\0';

		realLen = ret;
		close(pipe_fd);
		return true;
	}
private:
	string _pipeName;
};

class NamePipeReceiver
{
public:
	NamePipeReceiver(const char* pipeName)
		:_pipeName(pipeName)
	{}

	bool Listen()
	{

		int ret= 0;
		ret = mkfifo(_pipeName.c_str(), 0777);
		if (ret != 0)
		{
			return false;
		}

		return true;
	}

	bool ReceiverMsg(char* msg, size_t msgLen, size_t &realLen)
	{

		int pipe_fd = open(_pipeName.c_str(), O_RDONLY);
		int ret = read(pipe_fd, msg, msgLen);
		if (ret == 0)
		{
			return false;
		}
		msg[ret] = '\0';

		realLen = ret;
		close(pipe_fd);
		return true;
	}

	bool SendReplyMsg(const char* msg, size_t msgLen, size_t& realSize)
	{

		int pipe_fd = open(_pipeName.c_str(), O_WRONLY| O_NONBLOCK);
		int ret = write(pipe_fd, msg, msgLen);
		if (ret == 0)
		{
			return false;
		}

		realSize = ret;
		close(pipe_fd);
		return true;
	}

private:
	string _pipeName;
};

#endif

// IPC客户端
class IPCClient
{
public:
	IPCClient(const char* serverName)
		:_sender(serverName)
	{}

	~IPCClient()
	{}

	// 发送消息给服务端
	void SendMsg(char* buf, size_t bufLen)
	{
		size_t realLen;
		if (!_sender.Connect())
		{
			RECORD_ERROR_LOG("Client Connect Error\n");
		}

		if (!_sender.SendMsg(buf, bufLen, realLen))
		{
			RECORD_ERROR_LOG("Client SendMsg Error\n");
		}
	}

	// 获取服务端回复消息
	void GetReplyMsg(char* buf, size_t bufLen)
	{
		size_t realLen = 0;
		if (!_sender.GetReplyMsg(buf, bufLen, realLen))
		{
			RECORD_ERROR_LOG("Client GetReplyMsg Error\n");
		}
	}

private:
	NamePipeSender _sender;			// 发送者
};

// IPC服务端
class IPCServer
{
public:
	IPCServer(const char* serverName)
		:_receiver(serverName)
	{}

	~IPCServer()
	{}

	// 接收客户端消息
	void ReceiverMsg(char* buf, size_t bufLen)
	{
		size_t realLen;
		if (!_receiver.Listen())
		{
			RECORD_ERROR_LOG("Server Listen Error\n");
		}

		if (!_receiver.ReceiverMsg(buf, bufLen, realLen))
		{
			RECORD_ERROR_LOG("Server ReceiverMsg Error\n");
		}
	}

	// 回复客户端消息
	void SendReplyMsg(const char* buf, size_t bufLen)
	{
		size_t realLen;
		if (!_receiver.SendReplyMsg(buf, bufLen, realLen))
		{
			RECORD_ERROR_LOG("Server SendReplyMsg Error\n");
		}
	}
private:
	NamePipeReceiver _receiver;		// 接收者
};