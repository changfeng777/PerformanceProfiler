#include<iostream>
using namespace std;

// C++11
#include<thread>

#ifndef _IMPORT
	#define _IMPORT
#endif // !_IMPORT



#include "../PerformanceProfiler/PerformanceProfiler.h"

#ifdef _WIN32
#pragma comment(lib, "../Debug/PerformanceProfiler.lib")
#endif // _WIN32

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

// 3.��ƥ�䳡��
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

// 4.�ݹ�
void Test4()
{
	PERFORMANCE_PROFILER_EE_BEGIN(Fib1, "����");

	Fib(10);

	PERFORMANCE_PROFILER_EE_END(Fib1);
}

int main()
{
	//Test1();
	//Test2();
	Test3();
	//Test4();

	return 0;
}