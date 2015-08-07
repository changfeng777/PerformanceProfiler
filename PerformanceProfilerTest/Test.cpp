#include<iostream>
using namespace std;

// C++11
#include<thread>

#include "../PerformanceProfiler/PerformanceProfiler.h"

#ifdef _WIN32
#pragma comment(lib, "../Debug/PerformanceProfiler.lib")
#endif // _WIN32

// 1.测试基本功能
void Test1()
{
	PERFORMANCE_PROFILER_EE_BEGIN(PP1, "PP1");

	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	
	PERFORMANCE_PROFILER_EE_END(PP1);

	PERFORMANCE_PROFILER_EE_BEGIN(PP2, "PP2");

	std::this_thread::sleep_for(std::chrono::milliseconds(2000));

	PERFORMANCE_PROFILER_EE_END(PP2);
}

int main()
{
	Test1();

	return 0;
}