task1：实现线程安全同步的方式？
task2：synchronized是可重入锁吗？java如何实现synchronized的可重入？
task3：ReentrantLock如何实现可重入？
task4:Sychronized底层实现
task5:Voltile底层实现
task6:java的内存模型
task7:锁优化

并发需要掌握的几个并发特性：

* 原子性
* 可见性
* 不变性

原子性，顾名思义，原子在化学里是不可以在进行分割的一种元素，所以原子性描述的就是一件事情不能被再细分为另外的两件事情。

死锁：如果一个任务等待另一个任务的完成才能继续，而另一个任务也在等待其他的任务，并形成了一个等待的链条，而这个链条形成了一个环，那么就被成为死锁。

task1：实现线程安全同步的方式？

    1.synchronize
    2.voltile
    3.wait/notify
    4.ReentrantLock
    5.阻塞队列
    6.automicInteger等原子类
    7.使用final来完成

task2：synchronized是可重入锁吗？java如何实现synchronized的可重入？
是可重入锁，可重入锁的表现就是可以支持循环或者递归调用锁调用，java想要实现重入的方式，就是有一个int型的变量来记录，对象持有锁的情况，默认是0，如果有一个锁持有，就+1，再次重入，就再+1，也就变成了2，以此类推，当synchronized执行完会释放锁，也就会-1，当有一个线程想要持有这个对象的锁的时候会判断这个int值是否为0，只有为0，才可以对这个对象加锁。

task3：ReentrantLock如何实现可重入？

按照原理说，ReentrantLock和synchronized实现可重入的原理应该是相似的，但是synchronized是关键字，会直接翻译成jvm能识别的class文件中的指令。
详细讲解就是AQS(一组同步器使用同一个voltile修改的int对象来进行枷锁过程的判断)+CAS（通过比较“需要被修改的值是否被改变”来决定“是否要进行更新或者报错”，注意CAS存在的问题可以使用Version来解决）

task4：在对象的头文件里会持有monitor这个对象的引用，这个对象会记录这个对象被其他对象持有锁的信息，以及等待队列等信息。

task5：首先voltile起到的作用有两个：

        1.保证内存的可见性
        2.禁止JVM虚拟机的指令重排序

底层实现，通过观察为变量添加与删除voltile关键字的字节码发现，其实在字节码中没有任何不同，所以voltile关键字不是作用在字节码上，在汇编代码上看到voltile修饰的变量前面有一个lock前缀，这是一个原子操作，在多线程的情况下内存地址保持互斥，而这个lock前缀相当于一个内存屏障，内存屏障的意思是一组cpu处理指令，实现对内存操作的顺序限制，同时内存屏障可以强制更新内存缓存。


task6:
首先Java的内存模型是规定了JVM与主存是如何协调工作的。

每一个线程都有自己私有的一个存储空间，这个私有的存储空间何时与主存进行同步，就是内存模型关注的事情，正常情况下JVM对这个同步是比较松散的标准，因为不是每次变量的写操作都是具有多线程性的，所以针对多线程自己，可以使用voltile或者synchronized等关键字在底层保证变量修改的同步性。

task7：
锁优化：
- 缩短锁时间
- 锁的粒度化 - ConcurrentHashMap
- 锁的粗化 - 多个串行的锁是否可以合并为一个
- 读写分离