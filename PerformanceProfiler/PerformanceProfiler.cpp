#include "PerformanceProfiler.h"

//////////////////////////////////////////////////////////////
// 资源统计剖析

static void RecordErrorLog(const char* errMsg, int line)
{
#ifdef _WIN32
	printf("%s. ErrorId:%d, Line:%d\n", errMsg, GetLastError(), line);
#else
	printf("%s. ErrorId:%d, Line:%d\n", errMsg, errno, line);
#endif
}

#define RECORD_ERROR_LOG(errMsg)		\
	RecordErrorLog(errMsg, __LINE__);

void ResourceInfo::Update(LongType value)
{
	if (value < 0)
		return;

	if (value > _peak)
		_peak = value;

	//
	// 计算不准确，平均值受变化影响不明显
	// 需优化，可参考网络滑动窗口反馈调节计算。
	//
	_total += value;
	_avg = _total / (++_count);
}

ResourceStatistics::ResourceStatistics()
	:_isStatistics(false)
	,_statisticsThread(&ResourceStatistics::_Statistics, this)
{
	// 初始化统计资源信息
#ifdef _WIN32
	_lastKernelTime = 0;
	_lastSystemTime = 0;
	_processHandle = ::GetCurrentProcess();
	_cpuCount = _GetCpuCount();
#else
	_pid = getpid();
#endif
}

ResourceStatistics::~ResourceStatistics()
{
#ifdef _WIN32
	CloseHandle(_processHandle);
#endif
}

void ResourceStatistics::StartStatistics()
{
	if (_isStatistics == false)
	{
		_isStatistics.exchange(true);

		unique_lock<std::mutex> lock(_lockMutex);
		_condVariable.notify_one();
	}
}

void ResourceStatistics::StopStatistics()
{
	_isStatistics.exchange(false);
}

const ResourceInfo& ResourceStatistics::GetCpuInfo()
{
	return _cpuInfo;
}

const ResourceInfo& ResourceStatistics::GetMemoryInfo()
{
	return _memoryInfo;
}

///////////////////////////////////////////////////
// private

void ResourceStatistics::_Statistics()
{
	while (1)
	{
		//
		// 未开始统计，则等待
		//
		if (_isStatistics == false)
		{
			unique_lock<std::mutex> lock(_lockMutex);
			_condVariable.wait(lock);
		}

		// 更新统计信息
		_UpdateStatistics();

		// 每个CPU时间片单元统计一次
		this_thread::sleep_for(std::chrono::milliseconds(CPU_TIME_SLICE_UNIT));
	}
}

#ifdef _WIN32
// FILETIME->long long
static LongType FileTimeToLongType(const FILETIME& fTime)
{
	LongType time = 0;

	time = fTime.dwHighDateTime;
	time <<= 32;
	time += fTime.dwLowDateTime;

	return time;
}

// 获取CPU个数
int ResourceStatistics::_GetCpuCount()
{
	SYSTEM_INFO info;
	GetSystemInfo(&info);

	return info.dwNumberOfProcessors;
}

// 获取内核时间
LongType ResourceStatistics::_GetKernelTime()
{
	FILETIME createTime;
	FILETIME exitTime;
	FILETIME kernelTime;
	FILETIME userTime;

	if (false == GetProcessTimes(GetCurrentProcess(),
		&createTime, &exitTime, &kernelTime, &userTime))
	{
		RECORD_ERROR_LOG("GetProcessTimes Error");
	}

	return (FileTimeToLongType(kernelTime) + FileTimeToLongType(userTime)) / 10000;
}

// 获取CPU占用率
LongType ResourceStatistics::_GetCpuUsageRate()
{
	LongType cpuRate = -1;

	if (_lastSystemTime == 0 && _lastKernelTime == 0)
	{
		_lastSystemTime = GetTickCount();
		_lastKernelTime = _GetKernelTime();
		return cpuRate;
	}

	LongType systemTimeInterval = GetTickCount() - _lastSystemTime;
	LongType kernelTimeInterval = _GetKernelTime() - _lastKernelTime;

	if (systemTimeInterval > CPU_TIME_SLICE_UNIT)
	{
		cpuRate = kernelTimeInterval * 100 / systemTimeInterval;
		cpuRate /= _cpuCount;

		_lastSystemTime = GetTickCount();
		_lastKernelTime = _GetKernelTime();
	}

	return cpuRate;
}

// 获取内存使用信息
LongType ResourceStatistics::_GetMemoryUsage()
{
	PROCESS_MEMORY_COUNTERS PMC;
	if (false == GetProcessMemoryInfo(_processHandle, &PMC, sizeof(PMC)))
	{
		RECORD_ERROR_LOG("GetProcessMemoryInfo Error");
	}

	return PMC.PagefileUsage;
}

// 获取IO使用信息
void ResourceStatistics::_GetIOUsage(LongType& readBytes, LongType& writeBytes)
{
	IO_COUNTERS IOCounter;
	if (false == GetProcessIoCounters(_processHandle, &IOCounter))
	{
		RECORD_ERROR_LOG("GetProcessIoCounters Error");
	}

	readBytes = IOCounter.ReadTransferCount;
	writeBytes = IOCounter.WriteTransferCount;
}

// 更新统计信息
void ResourceStatistics::_UpdateStatistics()
{
	_cpuInfo.Update(_GetCpuUsageRate());
	_memoryInfo.Update(_GetMemoryUsage());

	/*
	LongType readBytes, writeBytes;
	GetIOUsage(readBytes, writeBytes);
	_readIOInfo.Update(readBytes);
	_writeIOInfo.Update(writeBytes);
	*/
}

#else // Linux

void ResourceStatistics::_UpdateStatistics()
	{
		char buf[1024] = { 0 };
		char cmd[256] = { 0 };
		sprintf(cmd, "ps -o pcpu,rss -p %d | sed 1,1d", _pid);

		// 将 "ps" 命令的输出 通过管道读取 ("pid" 参数) 到 FILE* stream
		// 将刚刚 stream 的数据流读取到buf中
		FILE *stream = ::popen(cmd, "r");
		::fread(buf, sizeof (char), 1024, stream);
		::pclose(stream);

		double cpu = 0.0;
		int rss = 0;
		sscanf(buf, "%lf %d", &cpu, &rss);

		_cpuInfo.Update(cpu);
		_memoryInfo.Update(rss);
	}
#endif

//////////////////////////////////////////////////////////////
// 代码段效率剖析

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

bool PerformanceNode::operator == (const PerformanceNode& p) const
{
	return _fileName == p._fileName
		&&_function == p._function
		&&_line == p._line;
}


void PerformanceNode::Serialize(SaveAdapter& SA) const
{
	SA.Save("FileName:%s, Fuction:%s, Line:%d\n",
		_fileName.c_str(), _function.c_str(), _line);
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

	if (_rsStatistics)
	{
		ResourceInfo cpuInfo = _rsStatistics->GetCpuInfo();
		SA.Save("【Cpu】 Peak:%lld%%, Avg:%lld%%\n", cpuInfo._peak, cpuInfo._avg);

		ResourceInfo memoryInfo = _rsStatistics->GetMemoryInfo();
		SA.Save("【Memory】 Peak:%lldK, Avg:%lldK\n", memoryInfo._peak / 1024, memoryInfo._avg / 1024);
	}
}

//////////////////////////////////////////////////////////////
// PerformanceProfiler
PerformanceProfilerSection* PerformanceProfiler::CreateSection(const char* fileName,
	const char* function, int line, const char* extraDesc, bool isStatistics)
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
		if (isStatistics)
		{
			section->_rsStatistics = new ResourceStatistics();
		}
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
	{
		_beginTimeMap[threadId] = clock();

		// 开始资源统计
		if (_rsStatistics)
		{
			_rsStatistics->StartStatistics();
		}
	}

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

		// 停止资源统计
		if (_rsStatistics)
		{
			_rsStatistics->StopStatistics();
		}
	}
}

void PerformanceProfiler::OutPut()
{
	int flag = ConfigManager::GetInstance()->GetOptions();
	if (flag & PPCO_SAVE_TO_CONSOLE)
	{
		ConsoleSaveAdapter CSA;
		PerformanceProfiler::GetInstance()->_OutPut(CSA);
	}

	if (flag & PPCO_SAVE_TO_FILE)
	{
		FileSaveAdapter FSA("PerformanceProfilerReport.txt");
		PerformanceProfiler::GetInstance()->_OutPut(FSA);
	}
}

void PerformanceProfiler::_OutPut(SaveAdapter& SA)
{
	SA.Save("=============Performance Profiler Report==============\n\n");
	SA.Save("Profiler Begin Time: %s\n", ctime(&_beginTime));

	int num = 1;
	auto it = _ppMap.begin();
	for (; it != _ppMap.end(); ++it)
	{
		SA.Save("NO%d. Description:%s\n", num++, it->first._desc.c_str());
		it->first.Serialize(SA);
		it->second->Serialize(SA);
		SA.Save("\n");
	}

	SA.Save("==========================end========================\n\n");
}

