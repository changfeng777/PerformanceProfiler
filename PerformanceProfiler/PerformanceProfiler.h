#pragma once

#include<stdarg.h>
#include<time.h>
#include<assert.h>

#include <string>
#include <map>

// C++11
#include <mutex>
using namespace std;

typedef long long LongType;

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
struct PerformanceNode
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
class PerformanceProfilerSection
{
	//typedef unordered_map<int, LongType> StatMap;
	typedef map<int, LongType> StatMap;

	friend class PerformanceProfiler;
public:
	PerformanceProfilerSection()
		:_beginTime(0)
		, _costTime(0)
	{}

	void begin(int id = 0);
	void end(int id = 0);

	void Serialize(SaveAdapter& SA);
private:
	LongType _beginTime;		// 开始时间
	LongType _costTime;			// 花费时间
};

class PerformanceProfiler : public Singleton<PerformanceProfiler>
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
	PPS_##sign->begin();								\

//
// 剖析效率结束
// @sign是剖析段唯一标识
//
#define PERFORMANCE_PROFILER_EE_END(sign)				\
	PPS_##sign->end();