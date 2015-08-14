#include<iostream>
using namespace std;

// C++11
#include<thread>

#ifdef _WIN32
//
// �����̬�⵼����̬���������⣬UMM�еĵ�����Ϊ��̬����
// ��̬�ⷽʽʹ��ʱ�����IMPORT����̬��ʱȥ���������޷������ӳ���
//
	#ifndef IMPORT
		#define IMPORT
	#endif // !IMPORT
	#pragma comment(lib, "../Debug/PerformanceProfiler.lib")
#endif // _WIN32

#include "../PerformanceProfiler/PerformanceProfiler.h"

// 1.���Ի�������
void Test1()
{
	PERFORMANCE_PROFILER_EE_BEGIN(PP1, "PP1");

	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	
	PERFORMANCE_PROFILER_EE_END(PP1);

	PERFORMANCE_PROFILER_EE_BEGIN(PP2, "PP2");

	std::this_thread::sleep_for(std::chrono::milliseconds(2000));

	PERFORMANCE_PROFILER_EE_END(PP2);
}

void MutilTreadRun(int count)
{
	while (count--)
	{
		PERFORMANCE_PROFILER_EE_BEGIN(PP1, "PP1");

		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		PERFORMANCE_PROFILER_EE_END(PP1);

		PERFORMANCE_PROFILER_EE_BEGIN(PP2, "PP2");

		std::this_thread::sleep_for(std::chrono::milliseconds(800));

		PERFORMANCE_PROFILER_EE_END(PP2);
	}
}

// 2.���Զ��̳߳���
void Test2()
{
	thread t1(MutilTreadRun, 5);
	thread t2(MutilTreadRun, 4);
	thread t3(MutilTreadRun, 3);

	t1.join();
	t2.join();
	t3.join();
}

// 3.���������β�ƥ�䳡��
void Test3()
{
	// 2.����ƥ��
	PERFORMANCE_PROFILER_EE_BEGIN(PP1, "ƥ��");

	for (int i = 0; i < 10; ++i)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}

	PERFORMANCE_PROFILER_EE_END(PP1);

	// 2.��ƥ��
	PERFORMANCE_PROFILER_EE_BEGIN(PP2, "��ƥ��");

	for (int i = 0; i < 10; ++i)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		PERFORMANCE_PROFILER_EE_END(PP2);
	}
}

int Fib(int n)
{
	PERFORMANCE_PROFILER_EE_BEGIN(Fib2, "�����ݹ�");

	std::this_thread::sleep_for(std::chrono::milliseconds(10));

	int ret;
	if (n <= 1)
	{
		ret = n;
	}
	else
	{
		ret = Fib(n - 1) + Fib(n - 2);
	}

	PERFORMANCE_PROFILER_EE_END(Fib2);

	return ret;
}

// 4.���������ݹ����
void Test4()
{
	PERFORMANCE_PROFILER_EE_BEGIN(Fib1, "����");

	Fib(10);

	PERFORMANCE_PROFILER_EE_END(Fib1);
}

// 5.���Ի�����Դͳ�����
void Test5()
{
	PERFORMANCE_PROFILER_EE_RS_BEGIN(PP1, "PP1");

	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	PERFORMANCE_PROFILER_EE_RS_END(PP1);

	PERFORMANCE_PROFILER_EE_RS_BEGIN(PP2, "PP2");

	std::this_thread::sleep_for(std::chrono::milliseconds(2000));

	PERFORMANCE_PROFILER_EE_RS_END(PP2);
}

//
// http://www.cnblogs.com/Ripper-Y/archive/2012/05/19/2508511.html
// 6.CPUռ��������������������Դͳ��
//
void Test6()
{
	SetThreadAffinityMask(GetCurrentProcess(), 0x00000001);
	const double SPLIT = 0.01;
	const int COUNT = 200;
	const double PI = 3.14159265;
	const int INTERVAL = 300;
	DWORD busySpan[COUNT]; //array of busy time  
	DWORD idleSpan[COUNT]; //array of idle time  
	int half = INTERVAL / 2;
	double radian = 0.0;
	for (int i = 0; i < COUNT; i++)
	{
		busySpan[i] = (DWORD)(half + (sin(PI*radian)*half));
		idleSpan[i] = INTERVAL - busySpan[i];
		radian += SPLIT;
	}
	DWORD startTime = 0;
	int j = 0;
	while (true)
	{
		PERFORMANCE_PROFILER_EE_RS_BEGIN(PERFORMANCE_PROFILER_EE_RS, "PERFORMANCE_PROFILER_EE_RS");

		j = j%COUNT;
		startTime = GetTickCount();
		while ((GetTickCount() - startTime) <= busySpan[j]);

		Sleep(idleSpan[j]);
		j++;

		PERFORMANCE_PROFILER_EE_RS_END(PERFORMANCE_PROFILER_EE_RS);
		PerformanceProfiler::GetInstance()->OutPut();

		this_thread::sleep_for(std::chrono::milliseconds(1000));
	}
}

//
// 7.�������߿����������
// ����Test7�Ժ�cmd������PerformanceProfilerTool.exe -pid ���̺� ���ɲ������߿���
//
void Test7()
{
	Test6();
}

int main()
{
	SET_PERFORMANCE_PROFILER_OPTIONS(PPCO_PROFILER | PPCO_SAVE_TO_CONSOLE);

	//Test1();
	//Test2();
	//Test3();
	//Test4();
	//Test5();
	//Test6();
	//Test7();

	return 0;
}