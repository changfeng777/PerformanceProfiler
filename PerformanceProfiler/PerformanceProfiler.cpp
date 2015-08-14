#include "PerformanceProfiler.h"

//////////////////////////////////////////////////////////////
// 资源统计剖析
//
//static void RecordErrorLog(const char* errMsg, int line)
//{
//#ifdef _WIN32
//	printf("%s. ErrorId:%d, Line:%d\n", errMsg, GetLastError(), line);
//#else
//	printf("%s. ErrorId:%d, Line:%d\n", errMsg, errno, line);
//#endif
//}
//
//#define RECORD_ERROR_LOG(errMsg)		\
//	RecordErrorLog(errMsg, __LINE__);

void ResourceInfo::Update(LongType value)
{
	// value < 0时则直接返回不再更新。
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
	:_refCount(0)
	,_statisticsThread(&ResourceStatistics::_Statistics, this)
{
	// 初始化统计资源信息
#ifdef _WIN32
	_lastKernelTime = -1;
	_lastSystemTime = -1;
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
	//
	// 多个线程并行剖析一个代码段的场景下使用引用计数进行统计。
	// 第一个线程进入剖析段时开始统计，最后一个线程出剖析段时
	// 停止统计。
	//
	if (_refCount++ == 0)
	{
		unique_lock<std::mutex> lock(_lockMutex);
		_condVariable.notify_one();
	}
}

void ResourceStatistics::StopStatistics()
{
	if (_refCount > 0)
	{
		--_refCount;
		_lastKernelTime = -1;
		_lastSystemTime = -1;
	}
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
// ResourceStatistics

static const int CPU_TIME_SLICE_UNIT = 100;

void ResourceStatistics::_Statistics()
{
	while (1)
	{
		//
		// 未开始统计时，则使用条件变量阻塞。
		//
		if (_refCount == 0)
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
	::GetSystemInfo(&info);

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

	// 1.如果是剖析段重新开始的第一次统计，则更新最近的内核时间和系统时间
	if (_lastSystemTime == -1 && _lastKernelTime == -1)
	{
		_lastSystemTime = GetTickCount();
		_lastKernelTime = _GetKernelTime();
		return cpuRate;
	}

	LongType systemTimeInterval = GetTickCount() - _lastSystemTime;
	LongType kernelTimeInterval = _GetKernelTime() - _lastKernelTime;

	// 2.若耗费的系统时间值小于设定的时间片（CPU_TIME_SLICE_UNIT），则不计入统计。
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

	//
	// 将 "ps" 命令的输出 通过管道读取 ("pid" 参数) 到 FILE* stream
	// 将刚刚 stream 的数据流读取到buf中
	// http://www.cnblogs.com/caosiyang/archive/2012/06/25/2560976.html
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

//////////////////////////////////////////////////////////////////////
// IPCMonitorServer

int GetProcessId()
{
	int processId = 0;

#ifdef _WIN32
	processId = GetProcessId(GetCurrentProcess());
#else
	processId = getpid();
#endif

	return processId;
}

#ifdef _WIN32
const char* SERVER_PIPE_NAME = "\\\\.\\Pipe\\PPServerPipeName";
#else
const char* SERVER_PIPE_NAME = "tmp/performance_profiler/_fifo";
#endif

string GetServerPipeName()
{
	string name = SERVER_PIPE_NAME;
	char idStr[10];
	_itoa(GetProcessId(), idStr, 10);
	name += idStr;
	return name;
}

IPCMonitorServer::IPCMonitorServer()
	:_onMsgThread(&IPCMonitorServer::OnMessage, this)
{
	printf("%s IPC Monitor Server Start\n", GetServerPipeName().c_str());

	_cmdFuncsMap["state"] = GetState;
	_cmdFuncsMap["save"] = Save;
	_cmdFuncsMap["disable"] = Disable;
	_cmdFuncsMap["enable"] = Enable;
}

void IPCMonitorServer::Start()
{}

void IPCMonitorServer::OnMessage()
{
	const int IPC_BUF_LEN = 1024;
	char msg[IPC_BUF_LEN] = { 0 };
	IPCServer server(GetServerPipeName().c_str());

	while (1)
	{
		server.ReceiverMsg(msg, IPC_BUF_LEN);
		printf("Receiver Cmd Msg: %s\n", msg);

		string reply;
		string cmd = msg;
		CmdFuncMap::iterator it = _cmdFuncsMap.find(cmd);
		if (it != _cmdFuncsMap.end())
		{
			CmdFunc func = it->second;
			func(reply);
		}
		else
		{
			reply = "Invalid Command";
		}

		server.SendReplyMsg(reply.c_str(), reply.size());
	}
}

void IPCMonitorServer::GetState(string& reply)
{
	reply += "State:";
	int flag = ConfigManager::GetInstance()->GetOptions();

	if (flag == PPCO_NONE)
	{
		reply += "None\n";
		return;
	}

	if (flag & PPCO_PROFILER)
	{
		reply += "Performance Profiler \n";
	}

	if (flag & PPCO_SAVE_TO_CONSOLE)
	{
		reply += "Save To Console\n";
	}

	if (flag & PPCO_SAVE_TO_FILE)
	{
		reply += "Save To File\n";
	}
}

void IPCMonitorServer::Enable(string& reply)
{
	ConfigManager::GetInstance()->SetOptions(PPCO_PROFILER | PPCO_SAVE_TO_FILE);

	reply += "Enable Success";
}

void IPCMonitorServer::Disable(string& reply)
{
	ConfigManager::GetInstance()->SetOptions(PPCO_NONE);

	reply += "Disable Success";
}

void IPCMonitorServer::Save(string& reply)
{
	ConfigManager::GetInstance()->SetOptions(
		ConfigManager::GetInstance()->GetOptions() | PPCO_SAVE_TO_FILE);
	PerformanceProfiler::GetInstance()->OutPut();

	reply += "Save Success";
}

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

	// 序列化效率统计信息
	auto costTimeIt = _costTimeMap.begin();
	for (; costTimeIt != _costTimeMap.end(); ++costTimeIt)
	{
		LongType callCount = _callCountMap[costTimeIt->first];
		SA.Save("Thread Id:%d, Cost Time:%.2f, Call Count:%d\n",
			 costTimeIt->first, 
			(double)costTimeIt->second / CLOCKS_PER_SEC,
			callCount);
	}

	SA.Save("Total Cost Time:%.2f, Total Call Count:%d\n",
		(double)_totalCostTime / CLOCKS_PER_SEC, _totalCallCount);

	// 序列化资源统计信息
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

void PerformanceProfilerSection::Begin(int threadId)
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
	++_totalCallCount;
}

void PerformanceProfilerSection::End(int threadId)
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

			_totalCostTime += costTime;
		}

		// 停止资源统计
		if (_rsStatistics)
		{
			_rsStatistics->StopStatistics();
		}
	}
}

PerformanceProfiler::PerformanceProfiler()
{
	// 程序结束时输出剖析结果
	atexit(OutPut);

	time(&_beginTime);

	IPCMonitorServer::GetInstance()->Start();
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

bool PerformanceProfiler::CompareByCallCount(PerformanceProfilerMap::iterator lhs,
	PerformanceProfilerMap::iterator rhs)
{
	return lhs->second->_totalCallCount > rhs->second->_totalCallCount;
}

bool PerformanceProfiler::CompareByCostTime(PerformanceProfilerMap::iterator lhs,
	PerformanceProfilerMap::iterator rhs)
{
	return lhs->second->_totalCostTime > rhs->second->_totalCostTime;
}

void PerformanceProfiler::_OutPut(SaveAdapter& SA)
{
	SA.Save("=============Performance Profiler Report==============\n\n");
	SA.Save("Profiler Begin Time: %s\n", ctime(&_beginTime));

	unique_lock<mutex> Lock(_mutex);

	vector<PerformanceProfilerMap::iterator> vInfos;
	auto it = _ppMap.begin();
	for (; it != _ppMap.end(); ++it)
	{
		vInfos.push_back(it);
	}

	// 按配置条件对剖析结果进行排序输出
	int flag = ConfigManager::GetInstance()->GetOptions();
	if (flag & PPCO_SAVE_BY_COST_TIME)
		sort(vInfos.begin(), vInfos.end(), CompareByCostTime);
	else if (flag & PPCO_SAVE_BY_CALL_COUNT)
		sort(vInfos.begin(), vInfos.end(), CompareByCallCount);

	for (int index = 0; index < vInfos.size(); ++index)
	{
		SA.Save("NO%d. Description:%s\n", index + 1, vInfos[index]->first._desc.c_str());
		vInfos[index]->first.Serialize(SA);
		vInfos[index]->second->Serialize(SA);
		SA.Save("\n");
	}

	SA.Save("==========================end========================\n\n");
}

