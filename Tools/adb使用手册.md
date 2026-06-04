adb,全称为Android debug bridge（安卓调试桥），既然是一座桥，一定是要连接两块区域的，那是哪两块区域呢，答案是：Android设备（或者虚拟机）与你使用的设备（如：pc、mac），那么我们先来了解一下原理：

## 连接原理

        客户端：该组件负责发送命令，例如你可以通过你的pc电脑使用adb来调用客户端。
        后台程序：该组件负责运行命令，以后台进程的形式跑在Android设备上。
        服务端：负责客户端和后台程序的通信，以后台进程的形式跑在开发计算机上。

[注]：你可以在android_sdk/platform-tools/目录下找到adb执行文件。

![adb-theory.jpg](https://github.com/xfmax/AndroidKnowledgeHierarchy/blob/master/%E5%BC%80%E5%8F%91%E5%B7%A5%E5%85%B7/image/adb-theory.jpg)

你可以看一下图片，更加直观。


工作方式：
启动一个adb客户端的时候，先检测是否有服务器进程存在，没有就启动服务器进程，服务端进程会检测本地机器的tcp的5037端口，如果没有人占用，则进行客户端与端口的绑定，以此来与客户端通信。之后在服务端进程会扫描5555-5585之间的奇数端口，大家会有疑问，那偶数端口干嘛去了？答案：偶数端口号是供对应的连接设备的控制台连接使用。


这一切都了解完，就开始了解一下如何连接吧：

两种连接方式：

        1.WLAN
        2.USB


方式1（WLAN）：

        1.开发计算机通过USB连接Android设备。
        2.保证Android设备与开发计算机处于同一个WLAN下。
        3.使用指令 adb tcp 5555
        4.拔掉Android设备与开发计算机的USB连接。
        5.找出Android设备的ip
        6.使用指令 adb connect ip
        7.你可以通过adb devices来查看是否连接成功。
   
[注]：你可以使用 adb kill-sever 杀死server进程，并通过 adb start-sever 开启server进程

方式2（USB）：这种方式好像没啥好说的，哈哈！

好了，我们来开始学习指令了：

#### 查询

      生成连接上的设备列表  adb devices

      结果格式如下：

            List of devices attached
            serial_number state

      有三种状态（state）：
    
            offline：未连接到设备或设备无响应
            device：连接上了设备
            on device：没有设备连接
[注]：如果同时连接多个设备，可以使用 adb -s serial_number 来制定要操作的设备是哪个。

例子：

       指定为serial_number为emulator-5556的设备安装helloWorld.apk的指令如下：

        adb -s emulator-5556 install helloWorld.apk 
        

#### 发送/复制

发送/复制的意思是说，让数据文件在客户端和服务端之间进行传输，其中分为两个指令 push 和 pull。

从Android设备获取数据，需要使用pull指令：

        adb pull remote local

从开发计算机向Android设备传输数据，需要使用push指令：

        adb push local remote

        例如：
            adb push app.apk /mnt/sdcard/app.apk



#### 安装应用

        adb install path_to_apk

如果想要安装的apk已经在被安装到了设备中，会提示安装失败，这时你可以使用如下指令覆盖安装：

        adb install -r path_to_apk


[注]：如果你在AndroidManifest.xml文件中添加了属性testOnly=true,此时你使用的apk的path是开发计算机的目录，想要直接安装到Android设备上的时候会报错（Failure [INSTALL_FAILED_TEST_ONLY]），你可以使用push指令将apk发送到Android设备上，在使用pm指令的安装命令（也可以直接使用 adb install -t path_to_apk），如下：

        adb push dev_path_apk android_path_apk
        adb shell pm install -t android_path_apk

AndroidStudio就是使用了上面的指令来进行安装（如果你没有开启instant run的情况下），其中push到的Android设备的目录是： /data/local/tmp/com.example.myapplication，熟悉Linux系统的朋友应该知道tmp文件夹意味着暂存目录，随时都可以被清理。

使用如下的格式来启动Activity：

        adb shell am start -n "com.example.myapplication/com.example.myapplication.MainActivity" -a android.intent.action.MAIN -c android.intent.category.LAUNCHER

        -n component（com.company.projectname.MainActivity）:启动哪个Activity
        -a intent中的action
        -c intent中的category
        -W 统计启动应用的时间

如果你开启了instant run的情况下，执行的是一个install-multiple安装器，再此不做过多解释，有兴趣的同学可以自行研究。
adb install-multiple -r -t /home/homer/AndroidStudioProjects/MyApplication/app/build/intermediates/split-apk/debug/slices/slice_9.apk /home/homer/AndroidStudioProjects/MyApplication/app/build/intermediates/split-apk/debug/slices/slice_7.apk /home/homer/AndroidStudioProjects/MyApplication/app/build/intermediates/split-apk/debug/slices/slice_8.apk /home/homer/AndroidStudioProjects/MyApplication/app/build/intermediates/split-apk/debug/slices/slice_0.apk /home/homer/AndroidStudioProjects/MyApplication/app/build/intermediates/split-apk/debug/slices/slice_5.apk /home/homer/AndroidStudioProjects/MyApplication/app/build/intermediates/split-apk/debug/slices/slice_6.apk /home/homer/AndroidStudioProjects/MyApplication/app/build/intermediates/split-apk/debug/slices/slice_4.apk /home/homer/AndroidStudioProjects/MyApplication/app/build/intermediates/split-apk/debug/slices/slice_1.apk /home/homer/AndroidStudioProjects/MyApplication/app/build/intermediates/split-apk/debug/dep/dependencies.apk /home/homer/AndroidStudioProjects/MyApplication/app/build/intermediates/split-apk/debug/slices/slice_2.apk /home/xufeng/AndroidStudioProjects/MyApplication/app/build/intermediates/split-apk/debug/slices/slice_3.apk /home/homer/AndroidStudioProjects/MyApplication/app/build/intermediates/instant-run-apk/debug/app-debug.apk 

#### Shell指令

        adb [-d|-e|-s serial_number] shell shell_command

执行过后，你会转换到Android设备的系统中的账户。

##### Activity Manager (am)

在shell中你可以使用am指令来启动activity、强制停止进程、发送广播等。

        adb shell am start -a android.intent.action.VIEW


        常用命令：

        start [option] intent 
                -D:启动调试
                -W:等待启动完成。
                --start-profiler file:启动分析器，将结果发送给file
                -P file:同上，只是在空闲进程时分析停止。
                -R count:重复activity启动count次。
                -S:启动前，强行停止目标应用。
                --opengl-trace:
                -user userid | current:指定要为哪个用户运行。

        startservice [option] intent

        force-stop package: 强行停止某个应用
        kill package:终止应用进程
        kill-all：终止所有进程
        broadcast [option] component：发送广播



##### 调用软件包管理器 (pm)

        adb shell pm uninstall com.example.MyApp


        list packages [options] filter：列出所有或者filter过滤后的软件包。
        install [options] path：安装apk文件
                -r：重新安装，保留数据。
                -i：指定安装程序软件包名称。
                -t：允许安装测试apk。
                -d：允许版本降级安装。
                -g：授予清单文件中列出的所有权限。
        uninstall [option] package：卸载应用
                -k：移除软件包的缓存文件与目录。

##### 进行屏幕截图

        格式：screencap filename
        例子：adb shell screencap /sdcard/screen.png

##### 其他 shell 命令

        dumpsys  ：将系统数据转储存到屏幕
        dumpstate ：将状态转储存到文件
        logcat [option]... [filter-spec]... :将日志文件输出到屏幕
        start ：启动（重启）Android设备实例
        stop ：停止Android设备实例

