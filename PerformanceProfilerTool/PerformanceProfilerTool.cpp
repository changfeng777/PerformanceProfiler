/******************************************************************************************
PerformanceProfilerTool.cpp:
Copyright (c) Bit Software, Inc.(2015), All rights reserved.

Purpose: 实现性能剖析在线控制工具

Author: xjh

Reviser: yyy

Created Time: 2015-4-26
******************************************************************************************/

#include <iostream>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
using namespace std;

#include "../IPC/IPCManager.h"

#ifdef _WIN32
const char* SERVER_PIPE_NAME = "\\\\.\\Pipe\\PPServerPipeName";
#else
const char* SERVER_PIPE_NAME = "tmp/performance_profiler/_fifo";
#endif

void UsageHelp ()
{
	printf ("Performance ProfilerTool Tool. Bit Internal Tool\n");
	printf ("Usage: PerformanceProfilerTool -help\n");
	printf ("Usage: PerformanceProfilerTool -pid pid.\n");
	printf ("Example: PerformanceProfilerTool -pid 2345.\n");	

	exit (0);
}

void UsageHelpInfo ()
{
	printf ("    <exit>:    Exit.\n");
	printf ("    <help>:    Show Usage help Info.\n");
	printf ("    <state>:   Show the state of the Performance ProfilerTool.\n");
	printf ("    <enable>:  Force enable performance profiler.\n");
	printf ("    <disable>: Force disable performance profiler.\n");
	printf ("    <save>:    Save the results to file.\n");
}

void PerformanceProfilerToolClient(const string& idStr)
{
	char msg[1024] = {0};
	size_t msgLen = 0;

	string serverPipeName = SERVER_PIPE_NAME;
	serverPipeName += idStr;

	UsageHelpInfo();

	IPCClient client(serverPipeName.c_str());

	while (1)
	{
		printf("shell:>");
		scanf("%s", msg);

		if (strcmp(msg, "help") == 0)
		{
			UsageHelpInfo();
			continue;
		}
		if (strcmp(msg, "exit") == 0)
		{
			printf("Performance Profiler Tool Client Exit\n");
			break;
		}

		client.SendMsg(msg, strlen(msg));
		client.GetReplyMsg(msg, 1024);

		printf("%s\n\n", msg);
	}
}

int main(int argc, char** argv)
{
	string idStr;
	if (argc == 2 && !strcmp(argv[1], "-help"))
	{
		UsageHelpInfo();

	}
	else if (argc == 3 && !strcmp(argv[1], "-pid"))
	{
		idStr += argv[2];
	}
	else
	{
		UsageHelp();
	}

	PerformanceProfilerToolClient(idStr);

	return 0;
}