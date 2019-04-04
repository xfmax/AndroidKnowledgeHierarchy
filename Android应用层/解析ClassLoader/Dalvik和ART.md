##### 首先看看与JVM的区别：
1.基于的架构不同。
JVM基于栈，DVM基于寄存器，速度更快。

2.执行的字节码不同。
JVM执行class文件，DVM执行dex文件。

3.DVM允许在有限的内存中同时运行多个进程。
Andorid中每一个应用都有一个DVM实例

4.DVM由Zygote创建和初始化。
每当应用程序进程需要被创建，都会fork Zygote，这样应用程序就从Zygote中获得了一份DVM实例的副本。

5.DVM有共享机制。
DVM支持在不同的进程的相同类实现共享机制。

6.DVM早期没有使用JIT编译器。

DVM与ART在功能上和JVM没有什么特别的区别，都是将字节码翻译成机器能够阅读的代码。

#### DVM GC REASON与日志
DVM:
GC_CONCURRENT:第一次填充堆的时候，触发gc释放内存，用以提供更大的堆空间。
GC_FOR_MALLOC:堆满时，gc释放空间已允许为新的对象分配内存空间。
GC_HPROF_DUMP_HEAP:请求创建HPROF文件以分析内存堆栈为目的的gc。
GC_EXPLICIT:手动操作gc，例如system.gc()；

##### DVM运行时堆的内容都有什么：
DVM：分为两个部分，zygote heap和allocation heap，zygote heap是zygote在启动的时候预加载与创建的各种对象需要使用的空间，在DVM启动后会将一块内存分为zygote heap和allocation heap两部分，所有DVM实例可以共享zygote heap部分，但是allocation heap部分都是各自独有的。
除了这两部分，还有：
Card Table：第一次GC垃圾标记后，用来记录GC后的垃圾信息的。
两个Heap Bitmap: 一个用来记录上一次的GC存活下来的对象，一个用来存储这次GC存活下来的对象。
Mark Stack：DVM使用的标记清除的方式进行GC的，Mark Stack用来标记需要被GC的对象的。

##### ART之后起之秀：
你要知道要取代DVM，一定是因为DVM有它自身的问题，而ART能够在一定程度上解决这些问题，首先来看看问题，其实像在移动设备上使用的虚拟机，无非就是性能与内存占用的问题，所以DVM的问题就是：

1.使用JIT，每次启动应用都要将字节码解释为机器码，这会很耗时，而ART的方案是预编译AOT，在安装应用的时候就将字节码解释成机器码，在运行应用的时候就不用在进行编译解释工作了，节省了时间，同时带来了问题是生成的机器码会延长安装的时长，同时占用存储空间，那么ART的解决办法是在7.0版本只针对热点代码进行AOT，其余的仍然使用JIT。

2.DVM只支持32位CPU，ART支持64位CPU同时兼容32位，这是淘汰DVM的主要原因。

3.ART在垃圾回收进行改进，更加频繁的执行并行的GC，将暂停次数由2次改为1次。

4.ART的运行时堆内存划分方式也不同。

##### ART运行时堆
ART采用多种垃圾收集方案，每个方案运行不同的垃圾收集器，默认采用CMS方案。划分区域在DVM的zygote space、allocation space的基础上增加了Image space 、 LargeObject space。
Image space用来存储一下预加载类，LargeObject space用来放一些大对象（默认12K）


##### ART的GC REASON与日志
ART会为主动请求GC和一些gc时长超过5mm的GC操作打印日志，其他不符合上述条件的情况就不打印了。

GC REASON
Concurrent:并发GC，不会暂停app线程，后台运行，不阻止内存分配。
Alloc：当堆内存满了，该gc会发生在请求分配内存的线程中。
Explircit：手动触发gc。
NativeAlloc：Native内存分配时，比如bitmap分配，会导致Native内存压力，从而触发gc。
CollectorTransition：由奇幻gc收集器而产生的gc操作。
HomogeneousSpaceCompact:堆内存碎片整理。

##### ART垃圾收集器的名称
1.Concurrent Mark Sweep（CMS）默认收集器，使用标记清除算法。释放除Image space外的所有空间。
2.Concurrent Partial Mark Sweep:释放除Image space和Zygote space外的所有空间。
3.Concurrent Sticky Mark Sweep:只能释放自上次GC以来分配的对象，扫描频繁，有更短的暂停时间。
4.Marksweep+Semispace:非并发GC，复制GC用于堆转换以及齐性空间压缩（堆碎片整理）。





