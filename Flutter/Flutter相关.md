task1：flutter相对于原生的优点？
开发速度快，双端落地


task2：原生如何与flutter通信？

    消息使用channel（平台通道）
    BasicMessageChannel：用于传递字符串和半结构化的信息，持续通信，收到消息后可以回复此次消息，如：Native将遍历到的文件信息陆续传递到Dart，在比如：Flutter将从服务端陆陆续获取到信息交个Native加工，Native处理完返回等；

    MethodChannel：用于传递方法调用（method invocation）一次性通信：如Flutter调用Native拍照；

    EventChannel: 用于数据流（event streams）的通信，持续通信，收到消息后无法回复此次消息，通常用于Native向Dart的通信，如：手机电量变化，网络连接变化，陀螺仪，传感器等；

    这三种类型的类型的Channel都是全双工通信，即A <=> B，Dart可以主动发送消息给platform端，并且platform接收到消息后可以做出回应，同样，platform端可以主动发送消息给Dart端，dart端接收数后返回给platform端。


task3：如何避免flutter问号过多和嵌套过多
改为链式调用
拆分子组件


task4：flutter如何提高渲染速度

task5：flutter单线程模型

flutter是一个单线程的系统，里面有两个执行队列MicrotaskQueue任务和EventQueue,前者执行的系统的任务，比较少也，flutter中只有 7 处用到了而已（比如，手势识别、文本输入、滚动视图、保存页面效果等需要高优执行任务的场景），但是重要，所以现行执行，而EventQueue则是I/O，绘制、定时器。

task6:StatefulWidget 生命周期

1.初始化
//createState , initState
2.更新
//didChanegDependencies , bulid, didUpdateWidget
3.销毁
//deactivate , dispose
