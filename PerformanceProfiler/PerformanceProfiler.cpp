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
	if (_fileName > p._fileName)
		return false;
	if (_function > p._function)
		return false;

	return true;
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
	SA.Save("CostTime:%.2f\n", (float)_costTime / CLOCKS_PER_SEC);
}

//////////////////////////////////////////////////////////////
// PerformanceProfiler
PerformanceProfilerSection* PerformanceProfiler::CreateSection(const char* fileName,
	const char* function, int line, const char* extraDesc)
{
	PerformanceProfilerSection* section = NULL;
	PerformanceNode node(fileName, function, line, extraDesc);
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

void PerformanceProfilerSection::begin(int id)
{
	_beginTime = clock();
}

void PerformanceProfilerSection::end(int id)
{
	_costTime += (clock() - _beginTime);
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
	auto it = _ppMap.begin();
	for (; it != _ppMap.end(); ++it)
	{
		it->first.Serialize(SA);
		it->second->Serialize(SA);
	}
}

