task1:rxjava的背压机制？
task2：线程切换的方式与原理？
RxJava是一个扩展了的观察者模式，还具备了线程切换的功能，用以来完成一次异步操作，还提供了变换（核心功能）的操作，用来改变序列的顺序。

首先，我们要知道RxJava能够处理的问题，一个核心关键词是**异步**。

我们来了解一下RxJava链式调用的原理，到底是什么使得它可以进行链式调用，同时参看具体的细节。

```java
 Observable.create(new ObservableOnSubscribe<Integer>() {
            // 1. 创建被观察者 & 生产事件
            @Override
            public void subscribe(ObservableEmitter<Integer> emitter) throws Exception {

                for (int i = 0; ; i++) {
                    Log.d(TAG, "发送了事件"+ i );
                    Thread.sleep(10);
                    // 发送事件速度：10ms / 个 
                    emitter.onNext(i);

                }
                
            }
        }).subscribeOn(Schedulers.io()) // 设置被观察者在io线程中进行
                .observeOn(AndroidSchedulers.mainThread()) // 设置观察者在主线程中进行
             .subscribe(new Observer<Integer>() {
            // 2. 通过通过订阅（subscribe）连接观察者和被观察者
                 
            @Override
            public void onSubscribe(Disposable d) {
                Log.d(TAG, "开始采用subscribe连接");
            }

            @Override
            public void onNext(Integer value) {

                try {
                    // 接收事件速度：5s / 个 
                    Thread.sleep(5000);
                    Log.d(TAG, "接收到了事件"+ value  );
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }

            }

            @Override
            public void onError(Throwable e) {
                Log.d(TAG, "对Error事件作出响应");
            }

            @Override
            public void onComplete() {
                Log.d(TAG, "对Complete事件作出响应");
            }

        });
```

可以看到create方法接受一个ObservableOnSubscribe对象，而ObservableOnSubscribe是什么呢？

```java

```

task1:rxjava的背压机制？

首先我们要知道背压到底是什么？

背压就是在执行**异步**发送消息的时候，接收方因为发送方的发送速率过快而导致无法处理及时处理发送过来的消息的一种情况，而背压就是一种流速控制策略，来保证这种情况不会撑爆内存。    

背压的实现方式：

#### 在Java 2.0中提供了Flowable来处理背压问题

```java
//Flowable创建被观察者
Flowable.create(new FlowableOnSubscribe<Integer>() {
    @Override
    public void subscribe(@NonNull FlowableEmitter<Integer> emitter) throws Exception {
        emitter.onNext(1);
        emitter.onComplete();

    }
}, BackpressureStrategy.ERROR)//需要传入的被压参数
        .subscribeOn(Schedulers.io())//设置悲观者在子线程中
        .observeOn(AndroidSchedulers.mainThread())//设置观察者在主线程中
        //进行订阅
        .subscribe(new Subscriber<Integer>() {
            @Override
            public void onSubscribe(Subscription s) {

                //制定观察者接受事件的个数(响应式拉取),默认是最大
                s.request(Long.MAX_VALUE);

            }

            @Override
            public void onNext(Integer integer) {

                Log.d("TAG","接受数据:" + integer);

            }

            @Override
            public void onError(Throwable t) {

            }

            @Override
            public void onComplete() {

            }
        });
```

task2:线程的切换原理？
