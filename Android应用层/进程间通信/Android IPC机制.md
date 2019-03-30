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

![ipc-theory](https://github.com/xfmax/android_know/blob/master/Android%E5%BA%94%E7%94%A8%E5%B1%82/%E8%BF%9B%E7%A8%8B%E9%97%B4%E9%80%9A%E4%BF%A1/image/ipc_theory_1.jpg)

如图，我们需要知道Linux系统的进程之间是有进程隔离的，也就是说进程之间是不共享内存的，所以才有IPC存在的必要。

可以看到Linux系统会划分用户空间和内核空间，如果是一个32位的操作系统，寻址空间有2的32次方，也就是4G，那么内核空间会分到最高的1G字节空间，剩下较低的3G字节空间留给用户空间，我们都知道内核是用来和硬件打交道的，同时可以访问受保护的内存空间，这样内核是绝对不可以被随便调用的，为了统一和谐，封装了很多操作，叫做系统调用，用来操作内核。

![ipc-theory](https://github.com/xfmax/android_know/blob/master/Android%E5%BA%94%E7%94%A8%E5%B1%82/%E8%BF%9B%E7%A8%8B%E9%97%B4%E9%80%9A%E4%BF%A1/image/ipc_theory_2.jpg)

传统的IPC传输针对Android这种移动端的设备有两个缺点：

1.拷贝两次，才能完成一次数据通信
2.无论是用户空间还是内核空间在开辟缓存区域的时，选择的大小是个问题，如果太大耗费内存，如果考虑在传输的数据加入消息头来获取数据大小后再开辟缓存空间，就会消耗一定的时间，无论哪种，不是耗空间就是耗时间。

#### Binder通信原理

接下来，我们来讲讲真正的主角，Binder通信原理。
binder驱动：驱动大家都顾名思义，就是用来和硬件打交道的程序，但是这个有点小区别，它也是为了能实现通信的，但是通信的对方是内核，也就是说Binder相当于是内核里的一个模块，供用户空间和内核空间进行交互的，你可以进行Android设备的根目录，查看/dev/binder,这就是存放binder驱动的位置，因为Linux可以实现动态内核可加载模块，所以binder可以被动态地加载进内核里。

然后要聊的就是通信的方式，肯定是有别于传统的方式了，binder使用的是：内存映射，mmap登场。
概念:将用户空间的一块内存区域映射到内核空间，同时内核空间也可以将自己的一块内存空间映射到用户空间

![ipc-theory](https://github.com/xfmax/android_know/blob/master/Android%E5%BA%94%E7%94%A8%E5%B1%82/%E8%BF%9B%E7%A8%8B%E9%97%B4%E9%80%9A%E4%BF%A1/image/ipc_theory_3.jpg)

一次完整的IPC通信过程：

        1.首先binder驱动在内核建立一块数据接收缓存区。
        2.内核还要再开辟一块内核缓存区，建立和数据接收缓存区之间的映射关系，以及内核数据接收缓存区与接收进程用户空间地址的映射。
        3.发送方进程通过copyfromuser系统调用将数据copy给内核缓存区，而内核缓存区由于和接收进程的用户空间地址有映射关系，所以数据会直接映射到接收进程的用户空间，这样就完成了一次IPC通信，只有一次拷贝。

![ipc-theory](https://github.com/xfmax/android_know/blob/master/Android%E5%BA%94%E7%94%A8%E5%B1%82/%E8%BF%9B%E7%A8%8B%E9%97%B4%E9%80%9A%E4%BF%A1/image/ipc_theory_4.jpg)


#### 通信模型

![ipc-theory](https://github.com/xfmax/android_know/blob/master/Android%E5%BA%94%E7%94%A8%E5%B1%82/%E8%BF%9B%E7%A8%8B%E9%97%B4%E9%80%9A%E4%BF%A1/image/ipc_theory_5.jpg)

Client、Server 和 ServiceManager 均是通过系统调用 open、mmap 和 ioctl 来访问设备文件 /dev/binder，从而实现与 Binder 驱动的交互来间接的实现跨进程通信。

client、Server、ServiceManager 存在于用户空间，而binder设备存在与内核空间。

#### 通信过程

1.一个进程过BINDERSETCONTEXT_MGR指令命令binder驱动将自己设置成ServiceManager。
2.Server向ServiceManager注册binder，表明自己对外提供服务，ServiceManager会存储名字和对应的引用。
3.client通过名字，在binder驱动的帮助下从ServiceManager中获取binder引用，从而与server通信。

![ipc-theory](https://github.com/xfmax/android_know/blob/master/Android%E5%BA%94%E7%94%A8%E5%B1%82/%E8%BF%9B%E7%A8%8B%E9%97%B4%E9%80%9A%E4%BF%A1/image/ipc_theory_6.jpg)


#### binder中的代理模式

通过上面的步骤，我产生一个疑问，如果A进程想要B进程的object对象，该怎么办？

binder的代理模式就应运而生，A进程通过binder驱动获取B进程的object对象的时候会进行一次转换，提供给A进程一个B进程种object对象的代理objectproxy,这个objectproxy拥有和object相同的接口，可是它并没有object的能力，所以当A进程调用objectproxy的某些方法是，就相当于通过binder驱动将这些调用的参数传递过去而已，最终还是要B进程的真正的object对象处理这个调用，并返结果。

#### 总结

从进程间通信的角度看，Binder 是一种进程间通信的机制；
从 Server 进程的角度看，Binder 指的是 Server 中的 Binder 实体对象；
从 Client 进程的角度看，Binder 指的是对 Binder 代理对象，是 Binder 实体对象的一个远程代理
从传输过程的角度看，Binder 是一个可以跨进程传输的对象；Binder 驱动会对这个跨越进程边界的对象对一点点特殊处理，自动完成代理对象和本地对象之间的转换。


### 能够进行进程间通信的方式
1.contentprovider
2.broadcast
3.aidl
4.messager