#pragma once

#include <iostream>
#include<stdarg.h>
#include<time.h>
#include<assert.h>
#include <string>
#include <map>

// C++11
#include <mutex>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif // _WIN32

using namespace std;

typedef long long LongType;

#if(defined(_WIN32) && defined(_IMPORT))
	#define API_EXPORT _declspec(dllimport)
#elif _WIN32
	#define API_EXPORT _declspec(dllexport)
#else
	#define API_EXPORT
#endif

//
// 获取当前线程id
//
static int GetThreadId()
{
#ifdef _WIN32
	return ::GetCurrentThreadId();
#else
	return ::thread_self();
#endif
}

// 保存适配器抽象基类
class SaveAdapter
{
public:
	virtual int Save(char* format, ...) = 0;
};

// 控制台保存适配器
class ConsoleSaveAdapter : public SaveAdapter
{
public:
	virtual int Save(char* format, ...)
	{
		va_list argPtr;
		int cnt;

		va_start(argPtr, format);
		cnt = vfprintf(stdout, format, argPtr);
		va_end(argPtr);

		return cnt;
	}
};

// 文件保存适配器
class FileSaveAdapter : public SaveAdapter
{
public:
	FileSaveAdapter(const char* path)
		:_fOut(0)
	{
		_fOut = fopen(path, "w");
	}

	~FileSaveAdapter()
	{
		if (_fOut)
		{
			fclose(_fOut);
		}
	}

	virtual int Save(char* format, ...)
	{
		if (_fOut)
		{
			va_list argPtr;
			int cnt;

			va_start(argPtr, format);
			cnt = vfprintf(_fOut, format, argPtr);
			va_end(argPtr);

			return cnt;
		}

		return 0;
	}
private:
	FileSaveAdapter(const FileSaveAdapter&);
	FileSaveAdapter& operator==(const FileSaveAdapter&);

private:
	FILE* _fOut;
};

// 单例基类
template<class T>
class Singleton
{
public:
	static T* GetInstance()
	{
		return _sInstance;
	}
protected:
	Singleton()
	{}

	static T* _sInstance;
};

// 静态对象指针初始化，保证线程安全。 
template<class T>
T* Singleton<T>::_sInstance = new T();

//
// 性能剖析节点
//
struct API_EXPORT PerformanceNode
{
	string _fileName;	// 文件名
	string _function;	// 函数名
	int	   _line;		// 行号
	string _desc;		// 附加描述

	PerformanceNode(const char* fileName, const char* function,
		int line, const char* desc);

	// 做map键值，所以需要重载operator<
	bool operator<(const PerformanceNode& p) const;

	// 序列化节点信息到容器
	void Serialize(SaveAdapter& SA) const;
};

//
// 性能剖析段
//
class API_EXPORT PerformanceProfilerSection
{
	//typedef unordered_map<thread::id, LongType> StatisMap;
	typedef map<int, LongType> StatisMap;

	friend class PerformanceProfiler;
public:
	PerformanceProfilerSection()
		:_totalRef(0)
	{}

	void begin(int threadId);
	void end(int threadId);

	void Serialize(SaveAdapter& SA);
private:
	mutex _mutex;					// 互斥锁
	StatisMap _beginTimeMap;		// 开始时间统计
	StatisMap _costTimeMap;			// 花费时间统计

	StatisMap _refCountMap;			// 引用计数(解决剖析段首尾不匹配，递归测试)
	LongType _totalRef;				// 总的引用计数

	StatisMap _callCountMap;		// 调用次数统计
};

class API_EXPORT PerformanceProfiler : public Singleton<PerformanceProfiler>
{
	friend class Singleton<PerformanceProfiler>;
	typedef map<PerformanceNode, PerformanceProfilerSection*> PerformanceProfilerMap;

public:
	//
	// 创建剖析段
	//
	PerformanceProfilerSection* CreateSection(const char* fileName,
		const char* funcName, int line, const char* desc);

	static void OutPut();

protected:
	PerformanceProfiler()
	{
		// 程序结束时输出剖析结果
		atexit(OutPut);
	}

	// 输出序列化信息
	void _OutPut(SaveAdapter& SA);
private:
	mutex _mutex;
	PerformanceProfilerMap _ppMap;
};

//
// 剖析效率开始
// @sign是剖析段唯一标识，构造出唯一的剖析段变量
// @desc是剖析段描述
//
#define PERFORMANCE_PROFILER_EE_BEGIN(sign, desc)		\
	PerformanceProfilerSection* PPS_##sign = NULL;		\
	PPS_##sign = PerformanceProfiler::GetInstance()->CreateSection(__FILE__, __FUNCTION__, __LINE__, desc);\
	PPS_##sign->begin(GetThreadId());					\

//
// 剖析效率结束
// @sign是剖析段唯一标识
//
#define PERFORMANCE_PROFILER_EE_END(sign)				\
	PPS_##sign->end(GetThreadId());