#PerformanceProfiler
性能剖析器

详细的使用方式可参考PerformanceProfilerTest/Test.cpp下的测试用例。

###功能说明
    1：在待剖析的代码段后添加剖析段开始和结束的宏函数，支持剖析代码段的运行时间和调用次数。
    2：支持剖析代码段耗费的CPU占用率和内存等资源信息。
    3：支持多线程环境下的剖析，支持剖析递归程序。
    4：默认不开启剖析，不开启剖析时基本没有什么性能损耗。
    5：可通过宏接口设置/配置文件/在线工具控制等方式配置管理剖析功能。
    6：后台默认开始IPC的服务监控线程，可通过工具发命令消息控制剖析选项，生成剖析报告。
    7：兼容支持Windows和Linux。

框架设计说明：
###设计如下几个单例类
   能剖析类（负责剖析代码段的运行时间和运行次数）
   配置管理类（管理剖析选项）
   IPC监听服务（使用观察者模式实现，监听客户端工具发送的消息，根据不同的命令消息做相应的处理。）
###资源统计
   需要统计资源时，每个剖析段开始一个线程，每个时间片统计一个CPU/内存的占用。
   
UML类图
![image](https://github.com/changfeng777/PerformanceProfiler/raw/master/UML/PerformanceProfiler.png)

###代码结构：
    IPC目录下为命名管道的实现代码。
    PerformanceProfiler目录下为代码段剖析的实现代码。
    PerformanceProfilerTest目录下为性能剖析接口测试的实现代码。
    PerformanceProfilerTool目录下为IPC在线控制剖析功能的工具的实现代码。
    PerformanceProfiler/UML框架类图目录下为项目的框架类图文件，使用StartUML可以打开编辑。
   
ps：项目中使用了C++11部分库，当前在Windows环境下是使用vs2013开发，Linux环境需在gcc4.7以上版本编译器使用。
