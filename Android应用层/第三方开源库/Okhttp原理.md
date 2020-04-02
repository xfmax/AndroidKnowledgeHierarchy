task1:okhttp的线程池设计

![okhttp_full_process](./image/okhttp_full_process.png)

okhttp主要关注两点：

        1.网络请求的流程
        2.连接池的复用

1.网络请求的流程

创建一个call，通过Dispatcher来进行选择进行异步的请求还是同步的请求，如果选择异步，有两个队列（runningQueue、readyQueue），runningQueue最大支持64个元素，如果超过了64个，则加入readyQueue，在执行完一个异步请求之后，会把这个元素从runningQueue中移除，并将readyQueue中的一个元素加入到runningQueue。

最后异步或者同步都是启动线程池，执行execute，会调用getResponseWithInterceptorChain,这个方法会生成一个ApplicationChain，并执行proceed方法，而proceed方法又会执行对应的interceptor.intercept()方法，通过一系列的递归调用，最后调用到httpEngine去发起请求。

2.连接池的复用

连接池使用了一个Deque，在put前会执行一个cleanup的Thread来对Deque进行清理操作，清理的原则是判断当前遍历出来的RealConection的引用计数是否为0（称为空闲连接）并且空闲时间5分钟，这个引用计数是通过StreamAllocation的一个List<WeakRefenerece<StreamAllocation>>的个数来判断的，也就是StreamAllocation的实例数。

get方法会判断address与request相同并且Deque的连接数小于5，就可以进行复用。