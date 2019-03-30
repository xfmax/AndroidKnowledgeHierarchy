### 如何开启多进程

在Androidmanifest中可以为四大组件添加process属性，就可以为此组件开启进程。

        <Activity name="com.example.activity"
        process=":reomte"/>

         <Activity name="com.example.activity"
        process="reomte"/>

第一种方式是以私有方式创建进程，也就是这个创建的进程归属于当前应用进程。
第二种方式是以全局独立的创建一个新的进程，可以与别的应用程序进程通信。


### IPC传输的文件格式

序列化-将文件或者数据以二进制的方式存入内存或者硬盘上。
1.Serializable：可以存如内存，也可以是硬盘

        实现Serializable接口
        在类中创建一个long型的成员变量serialVersionUID，以供在反序列化的时候检测是否为同一个类，以及数据是否有效。

2.Parcelable：Parcelable是Android提供的一种序列号方式，它的诞生是为了解决Serializable在序列化的时候会有大量的io操作，进而特别占用内存的问题，而且Parcelable被设计用来把对象序列化到内存，不建议序列化到硬盘（不代表不能，只是稍微麻烦）,Parcelable是Binder机制的传输文件格式。

### 进程通信Binder
接下来，我们来了解一下Binder的原理：

话说Android是基于Linux内核的，那么我们来看看Linux的进程间通讯手段：共享内存、管道、消息队列和socket。
那么既然Linux有这么多种的选择，为啥Android自己还要自己写个新的呢？

#### Binder是如何诞生的？

移动设备上，当然主要就是因为性能与稳定性，其次是安全性。

性能：

        socket传输效率低，开销大，适合于网络传输。
        内存共享控制复制，难以使用。
        管道和队列都是存储-转发模式，需要从发送方的缓冲区拷贝到内核开辟的缓冲区，再从核心的缓冲区拷贝回发送方的缓冲区，一次数据交换需要拷贝两次，开销也有点大。

        Binder只需要一次数据拷贝，性能仅次于共享内存。

稳定性：

        使用c/s架构，架构清晰，职责明确，独立。

安全性：

        传统的ipc都没有任何安全措施，安全方面的保障都交给了上层协议处理，身份识别是个问题，而binder采用在内核层分配uid，保证了身份安全。
        传统的ipc机制，只要能够拿到连接点信息，就可以建立接连。

总结一下优点：1.只需要拷贝一次 2.c/s架构，结构清晰，独立 3.为每个app分配uid，进程uid是鉴别进程身份的重要标识。

#### Linux的通信原理





### 能够进行进程间通信的方式
1.contentprovider
2.broadcast
3.aidl
4.messager