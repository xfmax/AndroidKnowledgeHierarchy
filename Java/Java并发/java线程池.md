task1:了解线程池的复用和超时机制。

task2:java提供的几种线程池

task3:线程池有几种状态？

我们先来了解一下线程池的构造都有哪几部分？

    1.核心线程
    2.最大的线程数
    3.阻塞队列
    4.饱和策略
    5.非核心线程闲置的超时时间

### 线程池的处理流程和原理

![thread-excutor](https://github.com/xfmax/android_know/blob/master/Java/Java%E5%B9%B6%E5%8F%91/image/thread_excute.png)

这个图片清晰地说明了线程池的流程。

接下来我们来聊聊线程池的复用和超时机制，先上代码：

```java
    public void execute(Runnable command) {
        if (command == null)
            throw new NullPointerException();
        /*
         * Proceed in 3 steps:
         *
         * 1. If fewer than corePoolSize threads are running, try to
         * start a new thread with the given command as its first
         * task.  The call to addWorker atomically checks runState and
         * workerCount, and so prevents false alarms that would add
         * threads when it shouldn't, by returning false.
         *
         * 2. If a task can be successfully queued, then we still need
         * to double-check whether we should have added a thread
         * (because existing ones died since last checking) or that
         * the pool shut down since entry into this method. So we
         * recheck state and if necessary roll back the enqueuing if
         * stopped, or start a new thread if there are none.
         *
         * 3. If we cannot queue task, then we try to add a new
         * thread.  If it fails, we know we are shut down or saturated
         * and so reject the task.
         */
        int c = ctl.get();
        if (workerCountOf(c) < corePoolSize) {
            if (addWorker(command, true))
                return;
            c = ctl.get();
        }
        if (isRunning(c) && workQueue.offer(command)) {
            int recheck = ctl.get();
            if (! isRunning(recheck) && remove(command))
                reject(command);
            else if (workerCountOf(recheck) == 0)
                addWorker(null, false);
        }
        else if (!addWorker(command, false))
            reject(command);
    }
```
源代码贴心地写上了注释，其实就是上面写的那个流程图，接着看代码 addWorker方法：

```java
    private boolean addWorker(Runnable firstTask, boolean core) {
        retry:
        for (int c = ctl.get();;) {
            // Check if queue empty only if necessary.
            if (runStateAtLeast(c, SHUTDOWN)
                && (runStateAtLeast(c, STOP)
                    || firstTask != null
                    || workQueue.isEmpty()))
                return false;

            for (;;) {
                if (workerCountOf(c)
                    >= ((core ? corePoolSize : maximumPoolSize) & COUNT_MASK))
                    return false;
                if (compareAndIncrementWorkerCount(c))
                    break retry;
                c = ctl.get();  // Re-read ctl
                if (runStateAtLeast(c, SHUTDOWN))
                    continue retry;
                // else CAS failed due to workerCount change; retry inner loop
            }
        }

        boolean workerStarted = false;
        boolean workerAdded = false;
        Worker w = null;
        try {
            w = new Worker(firstTask);
            final Thread t = w.thread;
            if (t != null) {
                final ReentrantLock mainLock = this.mainLock;
                mainLock.lock();
                try {
                    // Recheck while holding lock.
                    // Back out on ThreadFactory failure or if
                    // shut down before lock acquired.
                    int c = ctl.get();

                    if (isRunning(c) ||
                        (runStateLessThan(c, STOP) && firstTask == null)) {
                        if (t.isAlive()) // precheck that t is startable
                            throw new IllegalThreadStateException();
                        workers.add(w);
                        int s = workers.size();
                        if (s > largestPoolSize)
                            largestPoolSize = s;
                        workerAdded = true;
                    }
                } finally {
                    mainLock.unlock();
                }
                if (workerAdded) {
                    t.start();
                    workerStarted = true;
                }
            }
        } finally {
            if (! workerStarted)
                addWorkerFailed(w);
        }
        return workerStarted;
    }
```
关键的地方是从try...catch部分开始的，总体上可以看出就是将work创建出来并加入到Works中，works是一个HashSet，后面要用到。

```java
    final void runWorker(Worker w) {
        Thread wt = Thread.currentThread();
        Runnable task = w.firstTask;
        w.firstTask = null;
        w.unlock(); // allow interrupts
        boolean completedAbruptly = true;
        try {
            while (task != null || (task = getTask()) != null) {
                w.lock();
                // If pool is stopping, ensure thread is interrupted;
                // if not, ensure thread is not interrupted.  This
                // requires a recheck in second case to deal with
                // shutdownNow race while clearing interrupt
                if ((runStateAtLeast(ctl.get(), STOP) ||
                     (Thread.interrupted() &&
                      runStateAtLeast(ctl.get(), STOP))) &&
                    !wt.isInterrupted())
                    wt.interrupt();
                try {
                    beforeExecute(wt, task);
                    try {
                        task.run();
                        afterExecute(task, null);
                    } catch (Throwable ex) {
                        afterExecute(task, ex);
                        throw ex;
                    }
                } finally {
                    task = null;
                    w.completedTasks++;
                    w.unlock();
                }
            }
            completedAbruptly = false;
        } finally {
            processWorkerExit(w, completedAbruptly);
        }
    }
```

看这个Worker类中调用的方法，这个方法使用了getTask这个方法在判断while循环是否结束：

```java
    private Runnable getTask() {
        boolean timedOut = false; // Did the last poll() time out?

        for (;;) {
            int c = ctl.get();

            // Check if queue empty only if necessary.
            if (runStateAtLeast(c, SHUTDOWN)
                && (runStateAtLeast(c, STOP) || workQueue.isEmpty())) {
                decrementWorkerCount();
                return null;
            }

            int wc = workerCountOf(c);

            // Are workers subject to culling?
            boolean timed = allowCoreThreadTimeOut || wc > corePoolSize;

            if ((wc > maximumPoolSize || (timed && timedOut))
                && (wc > 1 || workQueue.isEmpty())) {
                if (compareAndDecrementWorkerCount(c))
                    return null;
                continue;
            }

            try {
                Runnable r = timed ?
                    workQueue.poll(keepAliveTime, TimeUnit.NANOSECONDS) :
                    workQueue.take();
                if (r != null)
                    return r;
                timedOut = true;
            } catch (InterruptedException retry) {
                timedOut = false;
            }
        }
    }
```
可以看到这个方法使用了一个workQueue，用这个队列的poll(等待时间，等待时间的单位)方法来控制非核心线程的等待，而如果是核心线程，直接调用take方法，在等待期间如果有任务被入队，那么这个线程就不会被回收，而是立即去执行任务，这就是线程池的复用和超时机制的原理所在。



task2：

    1.newFixedThreadPool，最大线程数定长，最大线程数==核心线程，非核心线程超时时间为0秒
    2.newCachedThreadPool,都是非核心线程，最大线程数为Integer.MAX_VALUE，非核心线程超时为60秒,[注]：该线程池使用的是SynchronousQueue队列，次队列集成blockingQueue,同时次队列中只允许存在一个元素，通过这个队列，使得newCachedThreadPool里的线程看上去是同步执行的，不存在并发，[注]：如果加入队列之后，判断出当前没有正在运行的线程，那么会启动非核心线程。
    3.newScheduledThreadPool,自定义核心线程数，最大线程数为Integer.MAX_VALUE,使用带有delay功能的工作队列，非核心线程超时为0秒
    4.newSingleThreadPool,核心线程数==最大线程数==1，非核心线程超时时间为0秒



[注]：核心线程与非核心线程只是一种叫法上的区别，在代码层面都是Thread，而且没有通过一个成员变量进行区分，只是通过 当前正在 运行的线程数与设定的核心线程数 的比较来判断当前这个要被处理的线程是那种类型，进入来确定需要不要超时。    


task3:

RUNNING：创建线程池之后，表示可以接收执行任务的状态
SHUTDOWN：调用了shutdown之后的状态
STOP：调用了shutdownNow之后的状态
TIDYING：线程池与工作队列中都没有需要执行的任务时，进入此状态
TERMINATED：调用terminate方法之后


线程池的异常处理：

1.try。。catch
2.捕获future的get方法抛出的异常
3.实例化线程池的时候，传入自己的ThreadFactory，使得每个Thread都被设置了setUnCaughtExceptionHandler
4.通过ThreadPoolExecutor的afterExecutor方法传递异常引用
