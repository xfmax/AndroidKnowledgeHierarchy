今天，我们来聊聊Android系统的启动流程，了解了系统的启动流程，才能更好的理解系统的各个模块、服务是如何有效地组织并运行的。

首先，从你按下手机上的电源按钮开始，会执行固化在rom上的一段程序，然后就会启动引导程序，接着引导程序会启动底层Linux系统，Linux系统会启动第一个进程-init进程。

# init进程启动
init进程干了四件事
* 创建和挂载文件目录
* 属性服务初始化
* 解析init.rc文件（*）
* 如果Zygote死亡，则重启Zygote进程

#### 解析init.rc文件

##### zygote创建
主要的功能聚焦在解析init.rc文件上，这个文件是以一种Android初始化语言书写，这个文件会通过command指令启动service.cpp，在Service中通过fork方法创建出zygote，并启动zygote，也就是启动路径为/system/bin/app_process64进程，对应的文件是app_main.cpp，并进入其main方法。

##### Dalvik或ART创建
app_main.cpp在main方法中又会调用AndroidRuntime.start()方法，代码接着就进入到了AndroidRuntime.cpp，到目前为止，还在c++的世界里，然后创建并启动Davlik或者ART虚拟机，注册JNI函数，接着通过JNI方法，调用Zygote的main方法，到这就由Native世界转到了JAVA世界，紧接着在Zygote的main方法中执行了4步操作：

    1.创建server端的socket，用来处理与AMS请求创建应用程序进程交互。
    2.预加载类和资源。
    3.启动system_server进程。
    4.执行runselectLoop循环等待AMS请求创建新的应用程序进程。

其中比较重要的是第3步，让我们来细细地看一下：
##### SystemServer创建与启动
首先Zygote中会调用startSystemServer方法，先fork出SystemServer进程，接着调用handleSystemServerProcess来启动线程，我们继续看，首先这个方法会创建一个pathClassLoader，在调用ZygoteInit.zygoteInit方法，zygote方法干两件事：
1.创建binder线程池
2.调用RuntimeInit.applicationInit方法

1会创建一个通过Native方法创建一个binder线程池，用来跟其他进程通信使用。

再来看2，跳转很多次，最终在RuntimeInit的invokeStaticMain方法中使用**反射**创建一个SystemServer对象。
具体步骤：通过反射的getMethod方法拿到main方法的Method，并通过throw Zygote.MethodAndArgsCaller抛出一个异常，这个异常在ZygoteInit的main方法中有被捕获，然后执行mMethod.invoke调用SystemServer的main，这样SystemServer进程就进入了SystemServer的main方法中。

##### 解析SystemServer进程
这里就要开始介绍SystenServer进程都干了些什么，那么自然就要从SystemServer中的main方法开始看起，里面只有一句new SystemServer().run()，我们看看这个run方法做了什么吧：

    1.System.loadLibrary("android_servers")
    
    2.创建 SystemServiceManager，它负责对系统服务进程创建、启动和生命周期管理。

    3.启动引导服务startBootstrapServices(ActivityManagerService、PowerManagerService、PackageManagerService)

    4.启动核心服务startCoreServices(DropBoxManagerService[用于生成与管理系统运行时的一些日志]、BatteryService、UsageStatsService[收集用户的使用频率与时长]、WebViewUpdateService)

    5.启动其他服务startOtherServices（CameraService、AlarmManagerService、WindowManagerService、LocationManagerService、InputManagerService、NotificationManagerService、启动Laucher）

[注]：SystemServerManager会调用onStart方法，执行相应的Service（例如ActivityManagerService）的main方法，并在main方法中将自己注册到ServiceManager中，ServiceManager
用于binder通信。

##### Launcher启动过程
刚刚在SystemServer进程中启动其他服务的过程中，通过SystemReady方法调用ActivityManagerService中的StartHomeActivityLocked方法，此方法中会创建一个Intent对象，用来启动Launcher，

##### Launcher中应用图标的显示过程
首先是Launcher中的onCreate函数，创建一个LauncherState单例对象，通过setLauncher方法获取到LauncherModel，其中LauncherModel持有一个Launcher对象的弱引用，LauncherModel内部有一个消息循环的线程HandlerThread对象，通过内部类LoadTask作为Message，post给HandlerThread，而LoadTask本身是一个Runnable，而LoadTask中的run方法执行内容如下：

1.loadWorkspace 加载工作区信息
2.bindWorkspace 绑定工作区信息
3.loadAllApps 加载系统已经安装的应用程序信息

工作区用来描述一个抽象桌面的，它由n个屏幕组成，每个屏幕又分为n个单元格，每个单元格用来显示一个应用程序的快捷图标。

重点在第3步，加载系统已经安装的应用程序，问题点在于
1.如何获取到系统已经加载的应用程序信息？
2.如何加载到工作区中

问题1：从loadAllApps开始里面会调用Launcher的bindAllApplications方法，传入的参数就是系统安装的应用程序的ArrayList集合，继续追，找到LauncherApps中的getActivityList方法，该方法会调用一个aidl方法，持有的对象是使用getSystemService(Context.LAUNCHER_APPS_SERVICE)得到的，也就是说使用aidl从PackageManagerService中获取到已安装到手机上的应用程序信息。[此处有个疑问，Launcher应该是属于SystemServer进程的，而PackageManagerService作为一种服务也是属于SystemServer进程的，为何还要使用aidl呢？]

问题2：获取到已安装到手机上的应用程序信息后，通过setApps方法，使用RecycleView来展示快捷图标。

疑问：看起来AMS、WMS、PMS都是运行在SystemServer进程？

最后我们来总结一下Android系统启动的流程，首先启动电源，引导芯片代码从预定义的地方（固话在ROM）上加载引导程序到RAM，引导程序会启动Linux内核，而Linux内核启动会创建和启动init进程，init进程会首先fork出zygote进程，然后zygote进程也会fork出SystemServer进程，同时启动Dalvik或者ART虚拟机，然后通过一个socket等待AMS创建应用程序的请求，而SystemServer进程则启动了很多服务如我们常见的AMS、WMS、PMS等，最后启动Launcher作为桌面，到此Android系统就完整地启动了。



