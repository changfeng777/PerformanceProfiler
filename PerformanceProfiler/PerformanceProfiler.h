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

// �����������������
class SaveAdapter
{
public:
	virtual int Save(char* format, ...) = 0;
};

// ����̨����������
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

// �ļ�����������
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

// ��������
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

// ��̬����ָ���ʼ������֤�̰߳�ȫ�� 
template<class T>
T* Singleton<T>::_sInstance = new T();

//
// ���������ڵ�
//
struct PerformanceNode
{
	string _fileName;	// �ļ���
	string _function;	// ������
	int	   _line;		// �к�
	string _desc;		// ��������

	PerformanceNode(const char* fileName, const char* function,
		int line, const char* desc);

	// ��map��ֵ��������Ҫ����operator<
	bool operator<(const PerformanceNode& p) const;

	// ���л��ڵ���Ϣ������
	void Serialize(SaveAdapter& SA) const;
};

//
// ����������
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
	LongType _beginTime;		// ��ʼʱ��
	LongType _costTime;			// ����ʱ��
};

class PerformanceProfiler : public Singleton<PerformanceProfiler>
{
	friend class Singleton<PerformanceProfiler>;
	typedef map<PerformanceNode, PerformanceProfilerSection*> PerformanceProfilerMap;

public:
	//
	// ����������
	//
	PerformanceProfilerSection* CreateSection(const char* fileName,
		const char* funcName, int line, const char* desc);

	static void OutPut();

protected:
	PerformanceProfiler()
	{
		// �������ʱ����������
		atexit(OutPut);
	}

	// ������л���Ϣ
	void _OutPut(SaveAdapter& SA);
private:
	PerformanceProfilerMap _ppMap;
};

//
// ����Ч�ʿ�ʼ
// @sign��������Ψһ��ʶ�������Ψһ�������α���
// @desc������������
//
#define PERFORMANCE_PROFILER_EE_BEGIN(sign, desc)		\
	PerformanceProfilerSection* PPS_##sign = NULL;		\
	PPS_##sign = PerformanceProfiler::GetInstance()->CreateSection(__FILE__, __FUNCTION__, __LINE__, desc);\
	PPS_##sign->begin();								\

//
// ����Ч�ʽ���
// @sign��������Ψһ��ʶ
//
#define PERFORMANCE_PROFILER_EE_END(sign)				\
	PPS_##sign->end();