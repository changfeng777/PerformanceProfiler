#include "PerformanceProfiler.h"

//////////////////////////////////////////////////////////////
//获取路径中最后的文件名。
static string GetFileName(const string& path)
{
	char ch = '/';

#ifdef _WIN32
	ch = '\\';
#endif

	size_t pos = path.rfind(ch);
	if (pos == string::npos)
	{
		return path;
	}
	else
	{
		return path.substr(pos + 1);
	}
}

// PerformanceNode
PerformanceNode::PerformanceNode(const char* fileName, const char* function,
	int line, const char* desc)
	:_fileName(GetFileName(fileName))
	,_function(function)
	,_line(line)
	,_desc(desc)
{}

bool PerformanceNode::operator<(const PerformanceNode& p) const
{
	if (_line > p._line)
		return false;

	if (_line < p._line)
		return true;

	if (_fileName > p._fileName)
		return false;
	
	if (_fileName < p._fileName)
		return true;

	if (_function > p._function)
		return false;

	if (_function < p._function)
		return true;

	return false;
}

void PerformanceNode::Serialize(SaveAdapter& SA) const
{
	SA.Save("PerformanceDescribe:【%s】\nFileName:%s, Fuction:%s, Line:%d\n",
		_desc.c_str(), _fileName.c_str(), _function.c_str(), _line);
}

///////////////////////////////////////////////////////////////
//PerformanceProfilerSection
void PerformanceProfilerSection::Serialize(SaveAdapter& SA)
{
	// 若总的引用计数不等于0，则表示剖析段不匹配
	if (_totalRef)
		SA.Save("Performance Profiler Not Match!\n");

	LongType totalTime = 0;
	LongType totalCallCount = 0;
	auto costTimeIt = _costTimeMap.begin();
	for (; costTimeIt != _costTimeMap.end(); ++costTimeIt)
	{
		LongType callCount = _callCountMap[costTimeIt->first];
		SA.Save("Thread Id:%d, Cost Time:%.2f, Call Count:%d\n",
			 costTimeIt->first, 
			(double)costTimeIt->second / CLOCKS_PER_SEC,
			callCount);

		totalTime += costTimeIt->second;
		totalCallCount += callCount;
	}

	SA.Save("Total Cost Time:%.2f, Total Call Count:%d\n",
		(double)totalTime / CLOCKS_PER_SEC,
		totalCallCount);
}

//////////////////////////////////////////////////////////////
// PerformanceProfiler
PerformanceProfilerSection* PerformanceProfiler::CreateSection(const char* fileName,
	const char* function, int line, const char* extraDesc)
{
	PerformanceProfilerSection* section = NULL;
	PerformanceNode node(fileName, function, line, extraDesc);

	unique_lock<mutex> Lock(_mutex);
	auto it = _ppMap.find(node);
	if (it != _ppMap.end())
	{
		section = it->second;
	}
	else
	{
		section = new PerformanceProfilerSection;
		_ppMap[node] = section;
	}

	return section;
}

void PerformanceProfilerSection::begin(int threadId)
{
	unique_lock<mutex> Lock(_mutex);

	// 更新调用次数统计
	++_callCountMap[threadId];

	// 引用计数 == 0 时更新段开始时间统计，解决剖析递归程序的问题。
	auto& refCount = _refCountMap[threadId];
	if (refCount == 0)
		_beginTimeMap[threadId] = clock();

	// 更新剖析段开始结束的引用计数统计
	++refCount;
	++_totalRef;
}

void PerformanceProfilerSection::end(int threadId)
{
	unique_lock<mutex> Lock(_mutex);

	// 更新引用计数
	LongType refCount = --_refCountMap[threadId];
	--_totalRef;

	//
	// 引用计数 <= 0 时更新剖析段花费时间。
	// 解决剖析递归程序的问题和剖析段不匹配的问题
	//
	if (refCount <= 0)
	{
		auto it = _beginTimeMap.find(threadId);
		if (it != _beginTimeMap.end())
		{
			LongType costTime = clock() - it->second;
			if (refCount == 0)
				_costTimeMap[threadId] += costTime;
			else
				_costTimeMap[threadId] = costTime;
		}
	}
}

void PerformanceProfiler::OutPut()
{
	ConsoleSaveAdapter CSA;
	CSA.Save("=============Performance Profiler Report==============\n\n");

	PerformanceProfiler::GetInstance()->_OutPut(CSA);

	CSA.Save("\n========================end===========================\n");
}

void PerformanceProfiler::_OutPut(SaveAdapter& SA)
{
	int num = 1;
	auto it = _ppMap.begin();
	for (; it != _ppMap.end(); ++it)
	{
		SA.Save("NO%d.\n", num++);
		it->first.Serialize(SA);
		it->second->Serialize(SA);
		SA.Save("\n");
	}
}

