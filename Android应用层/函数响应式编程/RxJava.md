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