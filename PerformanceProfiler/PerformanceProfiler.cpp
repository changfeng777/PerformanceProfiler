#include "PerformanceProfiler.h"

//////////////////////////////////////////////////////////////
//��ȡ·���������ļ�����
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
	SA.Save("PerformanceDescribe:��%s��\nFileName:%s, Fuction:%s, Line:%d\n",
		_desc.c_str(), _fileName.c_str(), _function.c_str(), _line);
}

///////////////////////////////////////////////////////////////
//PerformanceProfilerSection
void PerformanceProfilerSection::Serialize(SaveAdapter& SA)
{
	// ���ܵ����ü���������0�����ʾ�����β�ƥ��
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

	// ���µ��ô���ͳ��
	++_callCountMap[threadId];

	// ���ü��� == 0 ʱ���¶ο�ʼʱ��ͳ�ƣ���������ݹ��������⡣
	auto& refCount = _refCountMap[threadId];
	if (refCount == 0)
		_beginTimeMap[threadId] = clock();

	// ���������ο�ʼ���������ü���ͳ��
	++refCount;
	++_totalRef;
}

void PerformanceProfilerSection::end(int threadId)
{
	unique_lock<mutex> Lock(_mutex);

	// �������ü���
	LongType refCount = --_refCountMap[threadId];
	--_totalRef;

	//
	// ���ü��� <= 0 ʱ���������λ���ʱ�䡣
	// ��������ݹ���������������β�ƥ�������
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

