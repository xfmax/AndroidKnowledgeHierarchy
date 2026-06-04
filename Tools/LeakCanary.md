task：Leakcanary原理分析？

LeakCanary是一款监控内存泄露的工具，github地址为：https://github.com/square/leakcanary

接下来进入主题，我们来分析一下LeakCanary的内部原理，在此之前，我们先来看一下它的使用方法：

在你的module模块里的build.gradle中定义：

    dependencies {

        debugImplementation 'com.squareup.leakcanary:leakcanary-android:1.6.3'
  
        releaseImplementation 'com.squareup.leakcanary:leakcanary-android-no-op:1.6.3'

        // Optional, if you use support library fragments:

        debugImplementation 'com.squareup.leakcanary:leakcanary-support-fragment:1.6.3'

    }
没啥好说的，就是依赖LeakCanary的包，然后我们看一下使用，一般推荐在Applicaiton的onCreate中调用如下代码[注意，在冷启动优化的时候，LeakCanary是可以被放在子线程进行延时加载的]：

```java

    @Override
    public void onCreate() {
        super.onCreate();

        if (LeakCanary.isInAnalyzerProcess(this)) {
            return;
        }
        LeakCanary.install(this);
    }

```

我们可以在看源码之前先分析一下这段代码，首先if语句是什么意思？看起来是判断是否在分析进程，如果是分析进程的话，不要进行onCreate中下面的代码，直接返回，我们可以在手机上看一下，发现安装到手机上除了我们自己的项目外，还有一个默认名为“Leaks”的快捷图标，我猜测“Leaks”是一个进程服务，因为在手机的应用程序列表中没有名为“Leaks”的应用程序，接下来看看isInanalyzerProcess方法：

```java
  public static boolean isInAnalyzerProcess(@NonNull Context context) {
    Boolean isInAnalyzerProcess = LeakCanaryInternals.isInAnalyzerProcess;
    // This only needs to be computed once per process.
    if (isInAnalyzerProcess == null) {
      isInAnalyzerProcess = isInServiceProcess(context,HeapAnalyzerService.class);
      LeakCanaryInternals.isInAnalyzerProcess = isInAnalyzerProcess;
    }
    return isInAnalyzerProcess;
  }
```

看一下isInServiceProcess方法，注意第二个参数 HeapAnalyzerService.class：
```java
  public static boolean isInServiceProcess(Context context, Class<? extends Service> serviceClass) {
    PackageManager packageManager = context.getPackageManager();
    PackageInfo packageInfo;
    try {
      packageInfo = packageManager.getPackageInfo(context.getPackageName(), GET_SERVICES);
    } catch (Exception e) {
      CanaryLog.d(e, "Could not get package info for %s", context.getPackageName());
      return false;
    }
    String mainProcess = packageInfo.applicationInfo.processName;

    ComponentName component = new ComponentName(context, serviceClass);
    ServiceInfo serviceInfo;
    try {
      serviceInfo = packageManager.getServiceInfo(component, PackageManager.GET_DISABLED_COMPONENTS);
    } catch (PackageManager.NameNotFoundException ignored) {
      // Service is disabled.
      return false;
    }

    if (serviceInfo.processName.equals(mainProcess)) {
      CanaryLog.d("Did not expect service %s to run in main process %s", serviceClass, mainProcess);
      // Technically we are in the service process, but we're not in the service dedicated process.
      return false;
    }

    int myPid = android.os.Process.myPid();
    ActivityManager activityManager =
        (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
    ActivityManager.RunningAppProcessInfo myProcess = null;
    List<ActivityManager.RunningAppProcessInfo> runningProcesses;
    try {
      runningProcesses = activityManager.getRunningAppProcesses();
    } catch (SecurityException exception) {
      // https://github.com/square/leakcanary/issues/948
      CanaryLog.d("Could not get running app processes %d", exception);
      return false;
    }
    if (runningProcesses != null) {
      for (ActivityManager.RunningAppProcessInfo process : runningProcesses) {
        if (process.pid == myPid) {
          myProcess = process;
          break;
        }
      }
    }
    if (myProcess == null) {
      CanaryLog.d("Could not find running process for %d", myPid);
      return false;
    }

    return myProcess.processName.equals(serviceInfo.processName);
  }
```

这一段的重要意思是，获取PackageManager，通过PackageManager获取到PackageInfo，在PackageInfo中可以获取到这个package运行的进程mainProcess，注意该函数的第二个参数HeapAnalyzerService.class，它是什么呢？

```java
public final class HeapAnalyzerService extends ForegroundService
    implements AnalyzerProgressListener {
```
它是一个前台服务，为什么需要一个前台服务呢？当然是要提高dump进程内存泄露信息的前台服务，这个前台的服务是以一个独立进程的形式跑在packgeName:leakcanary进程中的，如果这个进程和主进程的名字一样，就返回false，一般不会出现这种情况，然后判断正在运行的进程列表中是否有当前进程（通过pid来判断），如果有，再与ServiceInfo.processName做判断，如果不等，就可以正常执行install的逻辑。

这一大长段就是在处理，**保证当前的进程不在前台服务所在的进程。**

到这，我产生了一个疑问，为什么当你安装apk完成后，在屏幕上还多了一个默认名为“Leaks”的应用程序快捷图标？带着这个疑问，我们来看看install方法：

```java
  public static @NonNull RefWatcher install(@NonNull Application application) {
    return refWatcher(application).listenerServiceClass(DisplayLeakService.class)
        .excludedRefs(AndroidExcludedRefs.createAppDefaults().build())
        .buildAndInstall();
  }
```

首先观察到这个方法的参数是Applicaiton类型的，第二返回值是一个RefWatcher，方法内容很简单，一串链式调用，让我们依次分解，目的是最后弄清楚refWatch到底是个什么玩意？

```java
  public static @NonNull AndroidRefWatcherBuilder refWatcher(@NonNull Context context) {
    return new AndroidRefWatcherBuilder(context);
  }
```
refWatcher方法返回了一个AndroidRefWatcherBuilder对象，接下来要调用的listenerServiceClass方法自然要到AndroidRefWatcherBuilder中查找：

```java
  public @NonNull AndroidRefWatcherBuilder listenerServiceClass(
      @NonNull Class<? extends AbstractAnalysisResultService> listenerServiceClass) {
      enableDisplayLeakActivity = DisplayLeakService.class.isAssignableFrom(listenerServiceClass);
      return heapDumpListener(new ServiceHeapDumpListener(context, listenerServiceClass));
  }
```
这个方法首先创建了一个DisplayLeakService对象（IntentService,其中有notification），其中isAssignableFrom方法是用来判断对象所表示的类或者接口与指定的类或者接口是否是同一类型或其超类，将boolean类型的结果记录在enableDisplayLeakActivity变量中，然后return一个heapDempListener对象，继续：

```java
  public final T heapDumpListener(HeapDump.Listener heapDumpListener) {
    this.heapDumpListener = heapDumpListener;
    return self();
  }
```
注意这个方法传入的参数是new ServiceHeapDumpListener(context, listenerServiceClass),在这个方法中会记录这个Listener，再继续看看self方法：

```java
  protected final T self() {
    return (T) this;
  }
```
很简单，返回自己，到此为止，refWatch方法和listenerServiceClass就分析完成了，它主要的工作就是new了一个listen（ServiceHeapDumpListener），并让RefWatchBuilder持有这个listener，接着，我们继续在RefWatcherBuilder中找到excludedRefs方法：

```java
  public final T excludedRefs(ExcludedRefs excludedRefs) {
    heapDumpBuilder.excludedRefs(excludedRefs);
    return self();
  }
```
可以看到这完全就是Builder设计模式了，将传入的ExcludedRefs值赋值给builder的成员变量，我们可以看一眼这个ExcludeRefs到底是什么：

```java
AndroidExcludedRefs.createAppDefaults().build()
```

```java
  public static @NonNull ExcludedRefs.Builder createAppDefaults() {
    return createBuilder(EnumSet.allOf(AndroidExcludedRefs.class));
  }
```
```java
  public static @NonNull ExcludedRefs.Builder createBuilder(EnumSet<AndroidExcludedRefs> refs) {
    ExcludedRefs.Builder excluded = ExcludedRefs.builder();
    for (AndroidExcludedRefs ref : refs) {
      if (ref.applies) {
        ref.add(excluded);
        ((ExcludedRefs.BuilderWithParams) excluded).named(ref.name());
      }
    }
    return excluded;
  }
```
流程很长，细节也很多，这一部分主要的功能就是剔除掉一些引用，就当这些引用不会发生内存泄露，在install里面主要是初始化这个引用的数组，应该之后还有方法要往这个数组中添加需要被剔除的引用，了解到这里就可以了，之后是最后一步：

```java
  public @NonNull RefWatcher buildAndInstall() {
    if (LeakCanaryInternals.installedRefWatcher != null) {
      throw new UnsupportedOperationException("buildAndInstall() should only be called once.");
    }
    RefWatcher refWatcher = build();
    if (refWatcher != DISABLED) {
      if (enableDisplayLeakActivity) {
        LeakCanaryInternals.setEnabledAsync(context, DisplayLeakActivity.class, true);
      }
      if (watchActivities) {
        ActivityRefWatcher.install(context, refWatcher);
      }
      if (watchFragments) {
        FragmentRefWatcher.Helper.install(context, refWatcher);
      }
    }
    LeakCanaryInternals.installedRefWatcher = refWatcher;
    return refWatcher;
  }
```
也就是建造起模式里的build方法，注意这里代码保证了此方法只能被调用一次，然后可以分为以下几步：

  1.enableDisplayLeakActivity为true，看源码应该是为true,除非你单独设置。然后调用setEnableAsync方法，也就是想要在出现内存泄露的时候，可以调用DisplayLeakActivity来展示具体的堆栈信息，这里要先将这个Activity组件设置为enable，才能供后续使用：
  ```java
 public static void setEnabledAsync(Context context, final Class<?> componentClass,
      final boolean enabled) {
    final Context appContext = context.getApplicationContext();
    AsyncTask.THREAD_POOL_EXECUTOR.execute(new Runnable() {
      @Override public void run() {
        setEnabledBlocking(appContext, componentClass, enabled);
      }
    });
  }
  ```
```java
  public static void setEnabledBlocking(Context appContext, Class<?> componentClass,
      boolean enabled) {
    ComponentName component = new ComponentName(appContext, componentClass);
    PackageManager packageManager = appContext.getPackageManager();
    int newState = enabled ? COMPONENT_ENABLED_STATE_ENABLED : COMPONENT_ENABLED_STATE_DISABLED;
    // Blocks on IPC.
    packageManager.setComponentEnabledSetting(component, newState, DONT_KILL_APP);
  }
```
这段代码主要的目的就是使得DisplayLeakActivity的状态变为enable，看源码注释说setComponentEnabledSetting会启用IPC，所以阻塞线程，所以使用AsyncTask中的线程池开启了一个子线程来完成这个工作。

2.监控Activity对象
```java
  public static void install(@NonNull Context context, @NonNull RefWatcher refWatcher) {
    Application application = (Application) context.getApplicationContext();
    ActivityRefWatcher activityRefWatcher = new ActivityRefWatcher(application, refWatcher);

    application.registerActivityLifecycleCallbacks(activityRefWatcher.lifecycleCallbacks);
  }
```
竟然使用Android系统提供的api注册Activity的生命周期回调，再看看lifecycleCallbacks：
```java
  private final Application.ActivityLifecycleCallbacks lifecycleCallbacks =
      new ActivityLifecycleCallbacksAdapter() {
        @Override public void onActivityDestroyed(Activity activity) {
          refWatcher.watch(activity);
        }
      };
```
当activity被销毁的时候，会将activity引用加入到refWatcher中，看一下如何在refWatcher中处理：
```java
  public void watch(Object watchedReference, String referenceName) {
    if (this == DISABLED) {
      return;
    }
    checkNotNull(watchedReference, "watchedReference");
    checkNotNull(referenceName, "referenceName");
    final long watchStartNanoTime = System.nanoTime();
    String key = UUID.randomUUID().toString();
    retainedKeys.add(key);
    final KeyedWeakReference reference =
        new KeyedWeakReference(watchedReference, key, referenceName, queue);

    ensureGoneAsync(watchStartNanoTime, reference);
  }
```
判空操作可以直接略过，首先生成一个随机的key，并加入到retainedKeys这个set集合中，之后我们就通过这个key来判断引用对象是否还被引用着，我们看创建了一个KeyedWeakReference对象，很明显这是一个弱引用，将此弱引用交给了ensureGoneAsync方法，我猜测这里肯定要开始gc了：
```java
  private void ensureGoneAsync(final long watchStartNanoTime, final KeyedWeakReference reference) {
    watchExecutor.execute(new Retryable() {
      @Override public Retryable.Result run() {
        return ensureGone(reference, watchStartNanoTime);
      }
    });
  }
```
开了个线程执行，继续ensureGone：
```java
 Retryable.Result ensureGone(final KeyedWeakReference reference, final long watchStartNanoTime) {
    long gcStartNanoTime = System.nanoTime();
    long watchDurationMs = NANOSECONDS.toMillis(gcStartNanoTime - watchStartNanoTime);

    removeWeaklyReachableReferences();

    if (debuggerControl.isDebuggerAttached()) {
      // The debugger can create false leaks.
      return RETRY;
    }
    if (gone(reference)) {
      return DONE;
    }
    gcTrigger.runGc();
    removeWeaklyReachableReferences();
    if (!gone(reference)) {
      long startDumpHeap = System.nanoTime();
      long gcDurationMs = NANOSECONDS.toMillis(startDumpHeap - gcStartNanoTime);

      File heapDumpFile = heapDumper.dumpHeap();
      if (heapDumpFile == RETRY_LATER) {
        // Could not dump the heap.
        return RETRY;
      }
      long heapDumpDurationMs = NANOSECONDS.toMillis(System.nanoTime() - startDumpHeap);

      HeapDump heapDump = heapDumpBuilder.heapDumpFile(heapDumpFile).referenceKey(reference.key)
          .referenceName(reference.name)
          .watchDurationMs(watchDurationMs)
          .gcDurationMs(gcDurationMs)
          .heapDumpDurationMs(heapDumpDurationMs)
          .build();

      heapdumpListener.analyze(heapDump);
    }
    return DONE;
  }

```
果然我猜测的没错（也没啥骄傲的），这里要首先使用gone方法判断一次引用是否已经被回收，gone方法就是判断retainedKeys这个set集合中有没有当前ref的key在其中，如果这个set集合中已经不包含这个ref的key了，就表示这个引用被回收了，直接return DONE，否则进行GC操作gcTrigger.runGc()，再接着看代码removeWeaklyReachableReferences方法：
```java
  private void removeWeaklyReachableReferences() {
    // WeakReferences are enqueued as soon as the object to which they point to becomes weakly
    // reachable. This is before finalization or garbage collection has actually happened.
    KeyedWeakReference ref;
    while ((ref = (KeyedWeakReference) queue.poll()) != null) {
      retainedKeys.remove(ref.key);
    }
  }
```
这里面涉及到了一个queue，是refWatch的成员变量ReferenceQueue<Object> queue，在refWatch的构造方法中初始化，removeWeaklyReachableReferences方法的作用是将对象已经回收的引用从retainKeys中移除。

之后再调用gone方法判断一次，如果这时候对象的引用还在，就证明出现了内存泄露，随之使用之前定义的listener进行dump分析，也就是调用heapdumpListener.analyze(heapDump)函数，接着看ServiceHeapDumpListener类的analyze方法：

```java
public void analyze(@NonNull HeapDump heapDump) {
    checkNotNull(heapDump, "heapDump");
    HeapAnalyzerService.runAnalysis(context, heapDump, listenerServiceClass);
  }
```
继续跳转HeapAnalyzerService类的runAnalysis方法：
```java
public static void runAnalysis(Context context, HeapDump heapDump,
      Class<? extends AbstractAnalysisResultService> listenerServiceClass) {
    setEnabledBlocking(context, HeapAnalyzerService.class, true);
    setEnabledBlocking(context, listenerServiceClass, true);
    Intent intent = new Intent(context, HeapAnalyzerService.class);
    intent.putExtra(LISTENER_CLASS_EXTRA, listenerServiceClass.getName());
    intent.putExtra(HEAPDUMP_EXTRA, heapDump);
    ContextCompat.startForegroundService(context, intent);
  }

```
HeapAnalyzerService是一个IntentService类，让我们看一个IntentService里的onHandleIntent方法：

```java
  @Override protected void onHandleIntent(@Nullable Intent intent) {
    onHandleIntentInForeground(intent);
  }
```
```java
protected void onHandleIntentInForeground(@Nullable Intent intent) {
    if (intent == null) {
      CanaryLog.d("HeapAnalyzerService received a null intent, ignoring.");
      return;
    }
    String listenerClassName = intent.getStringExtra(LISTENER_CLASS_EXTRA);
    HeapDump heapDump = (HeapDump) intent.getSerializableExtra(HEAPDUMP_EXTRA);

    HeapAnalyzer heapAnalyzer =
        new HeapAnalyzer(heapDump.excludedRefs, this, heapDump.reachabilityInspectorClasses);

    AnalysisResult result = heapAnalyzer.checkForLeak(heapDump.heapDumpFile, heapDump.referenceKey,
        heapDump.computeRetainedHeapSize);
    AbstractAnalysisResultService.sendResultToListener(this, listenerClassName, heapDump, result);
  }
```
这个方法就是在组织dump出来的heap，最后调用了sendResultToListener方法：
```java
 public static void sendResultToListener(@NonNull Context context,
      @NonNull String listenerServiceClassName,
      @NonNull HeapDump heapDump,
      @NonNull AnalysisResult result) {
    Class<?> listenerServiceClass;
    try {
      listenerServiceClass = Class.forName(listenerServiceClassName);
    } catch (ClassNotFoundException e) {
      throw new RuntimeException(e);
    }
    Intent intent = new Intent(context, listenerServiceClass);

    File analyzedHeapFile = AnalyzedHeap.save(heapDump, result);
    if (analyzedHeapFile != null) {
      intent.putExtra(ANALYZED_HEAP_PATH_EXTRA, analyzedHeapFile.getAbsolutePath());
    }
    ContextCompat.startForegroundService(context, intent);
  }
```
这段代码也很简单，就是要启动这个listenerServiceClassName组件，那么它是什么东西呢？
答案是：DisplayLeakService，代码又转向了DisplayLeakService这个类，它本身也是一个IntentService，跟HeapAnalyzerService基本流程一样，最后会到onHeapAnalyzed方法中：
```java
  protected final void onHeapAnalyzed(@NonNull AnalyzedHeap analyzedHeap) {
    ...

    String contentTitle;
    if (resultSaved) {
      PendingIntent pendingIntent =
          DisplayLeakActivity.createPendingIntent(this, heapDump.referenceKey);
     ...
      String contentText = getString(R.string.leak_canary_notification_message);
      showNotification(pendingIntent, contentTitle, contentText);
    } else {
      onAnalysisResultFailure(getString(R.string.leak_canary_could_not_save_text));
    }

    afterDefaultHandling(heapDump, result, leakInfo);
  }
```
代码太长了，我省略了大部分，可以看到这个方法主要就是用来showNotificaiton的，点击notification可以启动DisplayLeakActivity，到此分析完毕，我们来总结一下：

  1.使用系统的api registerActivityLifecycleCallbacks来收集destory的activity引用，并将这个activity的应用放入一个weakreference队列。
  2.当一个对象被回收后，会将该对象的应用加入到weakReference队列中，所以判断weakReference队列中是否有引用对象，就能判断对象是否被回收
  3.如果没有被回收，则调用gc方法
  4.再次判断对象的引用是否被加入到weakReference队列中，如果加入，代表对象被销毁，如果还没有加入，那就证明出现了内存泄露。
  5.在内存泄露之后，就是处理heap，并最终通过notification进行展示。

3.监控Fragment对象
和Activity流程相似，直说不同点：


还记得最开始的时候那个问题吗？为什么一个应用程序却可以有两个启动图标，这个问题就在于Androidmanifest.xml文件里，如果你回答不上这个问题说明你对Activity的启动模式的理解还是不那么深刻，这其中有两点需要你理解：

  1.Activity是跑在一个task栈中的，我们在学习Activity启动模式的时候会了解到taskAffinity这个参数，我们基本不太用这个参数，可是如果你不写，系统默认所有的Activity都在同一个task中，也就是以包名为taskAffinity的值的一个task栈中，但是如果我手动设置一个taskAffinity的话，被设置的Activity就会跑在我设置的这个taskAffinity中。

  2.平时开发的时候我们潜意识里会认为一个应用程序只有一个Activity的intent-fiflter会被设置为

    <intent-fiflter>
      <action name="android.intent.action.MAIN"/>
      <category name="android.intent.category.LAUNCHER"/>
    </intent-fiflter>

  但是，其实一个Applicaiton下可以有多个activity使用上述的intent-filter。
  所以综上两点，你都实现了，这个Activity就会在屏幕上创建一个icon。  
[注]：什么时候开始进行activity的检测？
这个问题需要查看AndroidWatchExecutor类，在调用watch函数的时候会使用这个Executor对象来创建线程处理任务
```java
  @Override protected @NonNull WatchExecutor defaultWatchExecutor() {
    return new AndroidWatchExecutor(DEFAULT_WATCH_DELAY_MILLIS);
  }
```
创建AndroidWatchExecutor的位置，参数为5秒。
```java
public AndroidWatchExecutor(long initialDelayMillis) {
    mainHandler = new Handler(Looper.getMainLooper());
    HandlerThread handlerThread = new HandlerThread(LEAK_CANARY_THREAD_NAME);
    handlerThread.start();
    backgroundHandler = new Handler(handlerThread.getLooper());
    this.initialDelayMillis = initialDelayMillis;
    maxBackoffFactor = Long.MAX_VALUE / initialDelayMillis;
  }
```
这个构造函数中创建了一个HandlerTread，并由handlerThread的looper创建一个子线程的handler，以及一个主线程的handler，接着我们看一下这个类的execute方法：

```java
  @Override public void execute(@NonNull Retryable retryable) {
    if (Looper.getMainLooper().getThread() == Thread.currentThread()) {
      waitForIdle(retryable, 0);
    } else {
      postWaitForIdle(retryable, 0);
    }
  }
```
分两种情况，如果在主线程，调用waitForIdle，如果在子线程会使用mainHandler回调到主线程最终还是调用waitForIdle：
```java
  private void waitForIdle(final Retryable retryable, final int failedAttempts) {
    // This needs to be called from the main thread.
    Looper.myQueue().addIdleHandler(new MessageQueue.IdleHandler() {
      @Override public boolean queueIdle() {
        postToBackgroundWithDelay(retryable, failedAttempts);
        return false;
      }
    });
  }
```
这个方法使用了idleHandler技术，这个技术是只指：正常情况下looper会一直从MessageQueue中取出message，但是当MessageQueue中没有message了，也就是空闲了的时候，就会执行IdleHandler中的queueIdle方法，言外之意，就是等着主线程空闲在执行这个方法：
```java
  private void postToBackgroundWithDelay(final Retryable retryable, final int failedAttempts) {
    long exponentialBackoffFactor = (long) Math.min(Math.pow(2, failedAttempts), maxBackoffFactor);
    long delayMillis = initialDelayMillis * exponentialBackoffFactor;
    backgroundHandler.postDelayed(new Runnable() {
      @Override public void run() {
        Retryable.Result result = retryable.run();
        if (result == RETRY) {
          postWaitForIdle(retryable, failedAttempts + 1);
        }
      }
    }, delayMillis);
  }
```
这就是开启一个子线程来执行retryable.run()，回到之前excute的调用处：
```java
private void ensureGoneAsync(final long watchStartNanoTime, final KeyedWeakReference reference) {
    watchExecutor.execute(new Retryable() {
      @Override public Retryable.Result run() {
        return ensureGone(reference, watchStartNanoTime);
      }
    });
  }
```
所以可以总结一下了，Leakcanary的开始检测的时机是**主线程空闲5秒后**开始执行检测。
