/*
 * Copyright (C) 2006 The Android Open Source Project
 * 版权声明：Android开源项目，2006年
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * 根据Apache License 2.0许可授权
 * you may not use this file except in compliance with the License.
 * 除非遵守该许可，否则不得使用本文件
 * You may obtain a copy of the License at
 * 你可以在以下网址获得许可副本
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * 除非适用法律要求或书面同意，软件
 * distributed under the License is distributed on an "AS IS" BASIS,
 * 按"原样"基础分发
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * 不附带任何明示或暗示的担保或条件
 * See the License for the specific language governing permissions and
 * 请参阅许可了解具体权利和限制
 * limitations under the License.
 */
#define LOG_TAG "JavaBinder" // 定义日志标签，用于log输出时标识来源
// #define LOG_NDEBUG 0 // 被注释掉的宏：若取消注释，将关闭所有日志（包括ALOGV等）

#include "android_util_Binder.h" // 引入该JNI模块对应的头文件，包含一些辅助声明

#include <android-base/stringprintf.h> // 提供base::StringPrintf，类似sprintf但更安全
#include <binder/BpBinder.h> // BpBinder：Binder代理端的native实现
#include <binder/IInterface.h> // IInterface：定义Binder服务接口基类
#include <binder/IPCThreadState.h> // 管理每个线程的Binder调用状态（PID/UID等）
#include <binder/IServiceManager.h> // ServiceManager的代理接口，用于获取系统服务
#include <binder/Parcel.h> // Parcel：用于Binder通信的数据封装容器
#include <binder/ProcessState.h> // 负责打开Binder驱动，管理线程池等
#include <binder/Stability.h> // 用于标记VINTF稳定性等
#include <binderthreadstate/CallerUtils.h> // 工具：获取当前调用状态（如是否是直接binder调用）
#include <fcntl.h> // 文件控制选项（如open标志）
#include <inttypes.h> // 定义跨平台的整数格式宏（如PRId32）
#include <log/log.h> // Android原生log库，提供ALOGV/ALOGD/ALOGE等宏
#include <nativehelper/JNIHelp.h> // 提供jniRegisterNativeMethods等工具函数
#include <nativehelper/ScopedLocalRef.h> // RAII类：自动管理JNI局部引用的生命周期
#include <nativehelper/ScopedUtfChars.h> // RAII类：将jstring转为UTF-8 C字符串
#include <stdio.h> // 标准C输入输出
#include <sys/stat.h> // 文件状态（stat）
#include <sys/types.h> // 基本系统数据类型
#include <unistd.h> // POSIX操作系统API（如getpid）
#include <utils/KeyedVector.h> // 键值对向量（早期实现，现代多用map）
#include <utils/List.h> // 双向链表
#include <utils/Log.h> // 旧的日志头文件（已基本被log/log.h替代）
#include <utils/String8.h> // Android的UTF-8字符串类
#include <utils/SystemClock.h> // 系统时钟工具
#include <utils/threads.h> // 线程、互斥锁等基础线程支持

#include <atomic> // 原子操作，用于无锁计数器
#include <mutex> // 互斥锁（虽然代码中使用的是AutoMutex，但可能某些地方用到）
#include <string> // C++标准字符串

#include "android_os_Parcel.h" // 提供parcelForJavaObject等Parcel操作辅助函数
#include "core_jni_helpers.h" // 提供FindClassOrDie、GetMethodIDOrDie等便捷JNI函数

//#undef ALOGV
//#define ALOGV(...) fprintf(stderr, __VA_ARGS__) // 如果启用，会把ALOGV输出到stderr

#define DEBUG_DEATH_FREEZE 0 // 调试开关：0表示关闭死亡/冻结回调的详细日志
#if DEBUG_DEATH_FREEZE
#define LOG_DEATH_FREEZE ALOGD // 若开启调试，输出到ALOGD级别
#else
#define LOG_DEATH_FREEZE ALOGV // 否则输出到ALOGV（verbose，通常不显示）
#endif

using namespace android; // 导入android命名空间，方便使用Binder相关类

// ----------------------------------------------------------------------------

static struct bindernative_offsets_t // 定义一个结构体，用于缓存Java类android.os.Binder的字段/方法ID
{
    // Class state.
    jclass mClass; // 类引用
    jmethodID mExecTransact; // 方法：execTransact
    jmethodID mGetInterfaceDescriptor; // 方法：getInterfaceDescriptor
    jmethodID mTransactionCallback; // 静态方法：transactionCallback
    jmethodID mGetExtension; // 方法：getExtension

    // Object state.
    jfieldID mObject; // 字段：mObject (存储native holder指针)
} gBinderOffsets; // 全局实例

// ----------------------------------------------------------------------------

static struct binderinternal_offsets_t // 缓存com.android.internal.os.BinderInternal的类/方法ID
{
    // Class state.
    jclass mClass;
    jmethodID mForceGc; // 方法：forceBinderGc
    jmethodID mProxyLimitCallback; // 静态方法：binderProxyLimitCallbackFromNative
    jmethodID mProxyWarningCallback; // 静态方法：binderProxyWarningCallbackFromNative
} gBinderInternalOffsets;

static struct sparseintarray_offsets_t // 缓存android.util.SparseIntArray的类/方法ID
{
    jclass classObject;
    jmethodID constructor; // 构造函数
    jmethodID put; // put方法
} gSparseIntArrayOffsets;

// ----------------------------------------------------------------------------

static struct error_offsets_t // 缓存几个Java Error类的引用
{
    jclass mError; // java.lang.Error
    jclass mOutOfMemory; // java.lang.OutOfMemoryError
    jclass mStackOverflow; // java.lang.StackOverflowError
} gErrorOffsets;

// ----------------------------------------------------------------------------

static struct binderproxy_offsets_t // 缓存android.os.BinderProxy的类/方法/字段ID
{
    jclass mClass;
    jmethodID mGetInstance; // 静态方法：getInstance (用于创建或获取BinderProxy)
    jmethodID mSendDeathNotice; // 静态方法：sendDeathNotice
    jmethodID mInvokeFrozenStateChangeCallback; // 静态方法：invokeFrozenStateChangeCallback
    jfieldID mNativeData; // 字段：mNativeData (存储BinderProxyNativeData指针)
} gBinderProxyOffsets;

static struct class_offsets_t // 缓存java.lang.Class的getName方法
{
    jmethodID mGetName;
} gClassOffsets;

// ----------------------------------------------------------------------------

static struct log_offsets_t // 缓存android.util.Log的类及e方法（Log.e）
{
    jclass mClass;
    jmethodID mLogE; // 静态方法：e(String, String, Throwable)
} gLogOffsets;

static struct parcel_file_descriptor_offsets_t // 缓存android.os.ParcelFileDescriptor的构造函数
{
    jclass mClass;
    jmethodID mConstructor;
} gParcelFileDescriptorOffsets;

static struct strict_mode_callback_offsets_t // 缓存android.os.StrictMode的静态回调方法
{
    jclass mClass;
    jmethodID mCallback; // onBinderStrictModePolicyChange
} gStrictModeCallbackOffsets;

static struct thread_dispatch_offsets_t // 缓存java.lang.Thread的字段/方法
{
    jclass mClass;
    jmethodID mDispatchUncaughtException; // dispatchUncaughtException
    jmethodID mCurrentThread; // 静态方法：currentThread()
} gThreadDispatchOffsets;

// ****************************************************************************
// ****************************************************************************
// ****************************************************************************

static constexpr uint32_t GC_INTERVAL = 1000; // 每创建1000个引用就触发一次GC

// 计数器：已创建的JavaBBinder的GlobalRef数量
static std::atomic<uint32_t> gNumLocalRefsCreated(0);
// 计数器：已删除的JavaBBinder的GlobalRef数量
static std::atomic<uint32_t> gNumLocalRefsDeleted(0);
// 计数器：已创建的JavaDeathRecipient的GlobalRef数量
static std::atomic<uint32_t> gNumDeathRefsCreated(0);
// 计数器：已删除的JavaDeathRecipient的GlobalRef数量
static std::atomic<uint32_t> gNumDeathRefsDeleted(0);

// 记录上一次触发GC时的引用总数
static std::atomic<uint32_t> gCollectedAtRefs(0);

// 函数：若近期创建了较多引用，则触发Java层的GC
// 使用无符号整数溢出特性来实现模运算，检测是否跨过GC区间
__attribute__((no_sanitize("unsigned-integer-overflow"))) // 禁止UBSan检测无符号溢出
static void gcIfManyNewRefs(JNIEnv* env)
{
    // 计算当前总的引用创建数量（本地引用+死亡引用）
    uint32_t totalRefs = gNumLocalRefsCreated.load(std::memory_order_relaxed)
            + gNumDeathRefsCreated.load(std::memory_order_relaxed);
    uint32_t collectedAtRefs = gCollectedAtRefs.load(std::memory_order_relaxed);
    // MAX_RACING 是对同时增加计数器的线程数目的一个宽松上界
    static constexpr uint32_t MAX_RACING = 100000;

    // 模运算检查：如果 totalRefs - (collectedAtRefs + GC_INTERVAL) 小于 MAX_RACING，
    // 说明已经超过或接近下一个GC触发点（考虑多线程竞争）
    if (totalRefs - (collectedAtRefs + GC_INTERVAL) /* modular arithmetic! */ < MAX_RACING) {
        // 尝试用CAS将gCollectedAtRefs增加GC_INTERVAL，谁成功谁负责触发GC
        if (gCollectedAtRefs.compare_exchange_strong(collectedAtRefs,
                collectedAtRefs + GC_INTERVAL, std::memory_order_relaxed)) {
            ALOGV("Binder forcing GC at %u created refs", totalRefs);
            env->CallStaticVoidMethod(gBinderInternalOffsets.mClass,
                    gBinderInternalOffsets.mForceGc); // 调用BinderInternal.forceBinderGc()
        }  // 否则其他线程已经处理了本次GC
    } else {
        ALOGV("Now have %d binder ops", totalRefs - collectedAtRefs);
    }
}

// 工具函数：从JNIEnv获取JavaVM指针
static JavaVM* jnienv_to_javavm(JNIEnv* env)
{
    JavaVM* vm;
    return env->GetJavaVM(&vm) >= 0 ? vm : NULL;
}

// 工具函数：从JavaVM获取当前线程的JNIEnv指针（必须已附加）
static JNIEnv* javavm_to_jnienv(JavaVM* vm)
{
    JNIEnv* env;
    return vm->GetEnv((void **)&env, JNI_VERSION_1_4) >= 0 ? env : NULL;
}

// 根据jthrowable判断是哪种Error类型，返回其名称字符串
static const char* GetErrorTypeName(JNIEnv* env, jthrowable error) {
  if (env->IsInstanceOf(error, gErrorOffsets.mOutOfMemory)) {
    return "OutOfMemoryError";
  }
  if (env->IsInstanceOf(error, gErrorOffsets.mStackOverflow)) {
    return "StackOverflowError";
  }
  return nullptr; // 非特定Error
}

// 报告java.lang.Error并终止运行时（调用FatalError）
static void report_java_lang_error_fatal_error(JNIEnv* env, jthrowable error,
        const char* msg)
{
    // 尝试获取异常的toString信息，用于最终错误消息
    std::string exc_msg;
    {
        ScopedLocalRef<jclass> exc_class(env, env->GetObjectClass(error)); // 获取异常类
        jmethodID method_id = env->GetMethodID(exc_class.get(), "toString",
                "()Ljava/lang/String;"); // 获取toString方法
        ScopedLocalRef<jstring> jstr(
                env,
                reinterpret_cast<jstring>(
                        env->CallObjectMethod(error, method_id))); // 调用toString
        ScopedLocalRef<jthrowable> new_error(env, nullptr);
        bool got_jstr = false;
        if (env->ExceptionCheck()) { // 如果toString本身抛出异常，保存该异常
            new_error = ScopedLocalRef<jthrowable>(env, env->ExceptionOccurred());
            env->ExceptionClear();
        }
        if (jstr.get() != nullptr) {
            ScopedUtfChars jstr_utf(env, jstr.get()); // 转成UTF-8字符串
            if (jstr_utf.c_str() != nullptr) {
                exc_msg = jstr_utf.c_str();
                got_jstr = true;
            } else {
                new_error = ScopedLocalRef<jthrowable>(env, env->ExceptionOccurred());
                env->ExceptionClear();
            }
        }
        if (!got_jstr) { // 无法获取异常信息，构建默认消息
            exc_msg = "(Unknown exception message)";
            const char* orig_type = GetErrorTypeName(env, error);
            if (orig_type != nullptr) {
                exc_msg = base::StringPrintf("%s (Error was %s)", exc_msg.c_str(), orig_type);
            }
            const char* new_type =
                new_error == nullptr ? nullptr : GetErrorTypeName(env, new_error.get());
            if (new_type != nullptr) {
                exc_msg = base::StringPrintf("%s (toString() error was %s)",
                                             exc_msg.c_str(),
                                             new_type);
            }
        }
    }

    env->Throw(error); // 重新抛出原始异常（以便后续FatalError包含它）
    ALOGE("java.lang.Error thrown during binder transaction (stack trace follows) : ");
    env->ExceptionDescribe(); // 打印异常栈到logcat

    std::string error_msg = base::StringPrintf(
            "java.lang.Error thrown during binder transaction: %s",
            exc_msg.c_str());
    env->FatalError(error_msg.c_str()); // 终止进程
}

// 报告java.lang.Error，尝试通过线程的UncaughtExceptionHandler处理，失败则调用上面的fatal版本
static void report_java_lang_error(JNIEnv* env, jthrowable error, const char* msg)
{
    jobject thread = env->CallStaticObjectMethod(gThreadDispatchOffsets.mClass,
            gThreadDispatchOffsets.mCurrentThread); // 获取当前Java线程对象
    if (thread != nullptr) {
        env->CallVoidMethod(thread, gThreadDispatchOffsets.mDispatchUncaughtException,
                error); // 调用线程的dispatchUncaughtException
        // 正常情况下不应返回
    }
    // 如果dispatchUncaughtException失败或自身出错，回退到fatal处理
    env->ExceptionClear();
    report_java_lang_error_fatal_error(env, error, msg);
}

namespace android {

// 通用异常报告函数：清除异常，记录日志，如果是Error则尝试上报并终止进程
void binder_report_exception(JNIEnv* env, jthrowable excep, const char* msg) {
    env->ExceptionClear(); // 清除当前异常

    // 准备调用Log.e(tag, msg, throwable)
    ScopedLocalRef<jstring> tagstr(env, env->NewStringUTF(LOG_TAG));
    ScopedLocalRef<jstring> msgstr(env);
    if (tagstr != nullptr) {
        msgstr.reset(env->NewStringUTF(msg));
    }

    if ((tagstr != nullptr) && (msgstr != nullptr)) {
        env->CallStaticIntMethod(gLogOffsets.mClass, gLogOffsets.mLogE,
                tagstr.get(), msgstr.get(), excep);
        if (env->ExceptionCheck()) { // 如果Log.e本身失败
            ALOGW("Failed trying to log exception, msg='%s'\n", msg);
            env->ExceptionClear();
        }
    } else { // 无法创建tag或message字符串（可能OOM）
        env->ExceptionClear();      /* assume exception (OOM?) was thrown */
        ALOGE("Unable to call Log.e()\n");
        ALOGE("%s", msg);
    }

    // 如果异常是java.lang.Error的子类，则执行Error报告流程
    if (env->IsInstanceOf(excep, gErrorOffsets.mError)) {
        report_java_lang_error(env, excep, msg);
    }
}

} // namespace android

// JavaBBinder类：继承BBinder，封装一个Java层Binder对象，将native调用转发到Java
class JavaBBinderHolder; // 前向声明

class JavaBBinder : public BBinder
{
public:
    JavaBBinder(JNIEnv* env, jobject /* Java Binder */ object)
        : mVM(jnienv_to_javavm(env)), mObject(env->NewGlobalRef(object)) // 保存VM，并创建全局引用持有Java对象
    {
        ALOGV("Creating JavaBBinder %p\n", this);
        gNumLocalRefsCreated.fetch_add(1, std::memory_order_relaxed); // 增加本地引用计数
        gcIfManyNewRefs(env); // 检查是否需要触发GC
    }

    // 检查子类标识，用于识别JavaBBinder
    bool    checkSubclass(const void* subclassID) const
    {
        return subclassID == &gBinderOffsets; // 如果传入的是gBinderOffsets地址，则认为是JavaBBinder
    }

    jobject object() const
    {
        return mObject; // 返回持有的Java Binder对象
    }

protected:
    virtual ~JavaBBinder()
    {
        ALOGV("Destroying JavaBBinder %p\n", this);
        gNumLocalRefsDeleted.fetch_add(1, std::memory_order_relaxed); // 增加删除计数
        JNIEnv* env = javavm_to_jnienv(mVM); // 获取JNI环境
        env->DeleteGlobalRef(mObject); // 删除全局引用
    }

    // 获取接口描述符，采用懒加载方式，仅第一次调用时从Java获取
    const String16& getInterfaceDescriptor() const override
    {
        call_once(mPopulateDescriptor, [this] {
            JNIEnv* env = javavm_to_jnienv(mVM);

            ALOGV("getInterfaceDescriptor() on %p calling object %p in env %p vm %p\n", this, mObject, env, mVM);

            jstring descriptor = (jstring)env->CallObjectMethod(mObject, gBinderOffsets.mGetInterfaceDescriptor); // 调用Java的getInterfaceDescriptor

            if (descriptor == nullptr) {
                return; // 如果返回null，保持mDescriptor为空
            }

            static_assert(sizeof(jchar) == sizeof(char16_t), ""); // 确保jchar与char16_t大小一致
            const jchar* descriptorChars = env->GetStringChars(descriptor, nullptr); // 获取字符串字符
            const char16_t* rawDescriptor = reinterpret_cast<const char16_t*>(descriptorChars);
            jsize rawDescriptorLen = env->GetStringLength(descriptor);
            mDescriptor = String16(rawDescriptor, rawDescriptorLen); // 存储为String16
            env->ReleaseStringChars(descriptor, descriptorChars); // 释放字符串字符
        });

        return mDescriptor;
    }

    // 核心：将Binder事务转发给Java层Binder对象的execTransact方法
    status_t onTransact(
        uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags = 0) override
    {
        JNIEnv* env = javavm_to_jnienv(mVM);

        LOG_ALWAYS_FATAL_IF(env == nullptr,
                            "Binder thread started or Java binder used, but env null. Attach JVM?");

        ALOGV("onTransact() on %p calling object %p in env %p vm %p\n", this, mObject, env, mVM);

        IPCThreadState* thread_state = IPCThreadState::self();
        const int32_t strict_policy_before = thread_state->getStrictModePolicy(); // 记录当前StrictMode策略

        // 调用Java Binder的execTransact方法，传入code、data、reply等，返回是否成功
        jboolean res = env->CallBooleanMethod(mObject, gBinderOffsets.mExecTransact,
            code, reinterpret_cast<jlong>(&data), reinterpret_cast<jlong>(reply), flags);

        if (env->ExceptionCheck()) {
            ScopedLocalRef<jthrowable> excep(env, env->ExceptionOccurred());

            auto state = IPCThreadState::self();
            String8 msg;
            msg.appendFormat("*** Uncaught remote exception! Exceptions are not yet supported "
                             "across processes. Client PID %d UID %d.",
                             state->getCallingPid(), state->getCallingUid());
            binder_report_exception(env, excep.get(), msg.c_str()); // 报告未捕获的远程异常
            res = JNI_FALSE; // 将结果置为失败
        }

        // 检查StrictMode策略是否在处理过程中改变，若有则恢复并通知Java层
        if (thread_state->getStrictModePolicy() != strict_policy_before) {
            set_dalvik_blockguard_policy(env, strict_policy_before);
        }

        if (env->ExceptionCheck()) {
            ScopedLocalRef<jthrowable> excep(env, env->ExceptionOccurred());
            binder_report_exception(env, excep.get(),
                                    "*** Uncaught exception in onBinderStrictModePolicyChange");
        }

        // 对于SYSPROPS_TRANSACTION命令，仍需调用父类处理
        if (code == SYSPROPS_TRANSACTION) {
            BBinder::onTransact(code, data, reply, flags);
        }

        // 根据Java层返回值确定结果：JNI_TRUE->NO_ERROR, 否则UNKNOWN_TRANSACTION
        return res != JNI_FALSE ? NO_ERROR : UNKNOWN_TRANSACTION;
    }

    // dump方法暂未实现，返回0
    status_t dump(int fd, const Vector<String16>& args) override
    {
        return 0;
    }

private:
    JavaVM* const   mVM;         // 关联的JavaVM
    jobject const   mObject;     // Java Binder对象的全局引用

    mutable std::once_flag mPopulateDescriptor; // 确保只初始化一次描述符
    mutable String16 mDescriptor; // 缓存的接口描述符
};

// ----------------------------------------------------------------------------

// JavaBBinderHolder类：管理JavaBBinder的弱引用，提供线程安全的获取/创建方法
class JavaBBinderHolder
{
public:
    sp<JavaBBinder> get(JNIEnv* env, jobject obj)
    {
        sp<JavaBBinder> b;
        {
            AutoMutex _l(mLock);
            // 尝试从弱引用提升为强引用
            b = mBinder.promote();
        }

        if (b) return b; // 如果已存在且有效，直接返回

        // b/360067751: 构造函数可能触发GC，所以在锁外创建
        b = sp<JavaBBinder>::make(env, obj);

        {
            AutoMutex _l(mLock);
            // 如果另一线程同时创建了，使用那个，丢弃当前新建的
            if (sp<JavaBBinder> b2 = mBinder.promote(); b2) return b2;

            // 应用之前设置的VINTF标记
            if (mVintf) {
                ::android::internal::Stability::markVintf(b.get());
            }
            // 如果已经调用过setExtension，将扩展设置到新的JavaBBinder上
            if (mSetExtensionCalled) {
                jobject javaIBinderObject = env->CallObjectMethod(obj, gBinderOffsets.mGetExtension);
                sp<IBinder> extensionFromJava = ibinderForJavaObject(env, javaIBinderObject);
                if (extensionFromJava != nullptr) {
                    b.get()->setExtension(extensionFromJava);
                }
            }
            if (mInheritRt) {
                b.get()->setInheritRt(mInheritRt);
            }
            mBinder = b; // 存储弱引用
            ALOGV("Creating JavaBinder %p (refs %p) for Object %p, weakCount=%" PRId32 "\n",
                 b.get(), b->getWeakRefs(), obj, b->getWeakRefs()->getWeakCount());
        }

        return b;
    }

    // 获取现有的JavaBBinder，不创建新对象
    sp<JavaBBinder> getExisting()
    {
        AutoMutex _l(mLock);
        return mBinder.promote();
    }

    void markVintf() {
        AutoMutex _l(mLock);
        mVintf = true; // 标记为VINTF
    }

    void forceDowngradeToSystemStability() {
        AutoMutex _l(mLock);
        mVintf = false; // 强制降级到系统稳定性级别
    }

    void setExtension(const sp<IBinder>& extension) {
        AutoMutex _l(mLock);
        mSetExtensionCalled = true;
        sp<JavaBBinder> b = mBinder.promote();
        if (b != nullptr) {
            b.get()->setExtension(extension); // 若JavaBBinder已存在，直接设置扩展
        }
    }

    void setInheritRt(bool inheritRt) {
        AutoMutex _l(mLock);
        mInheritRt = inheritRt;
        sp<JavaBBinder> b = mBinder.promote();
        if (b != nullptr) {
            b.get()->setInheritRt(inheritRt);
        }
    }

private:
    Mutex           mLock;       // 保护成员变量的互斥锁
    wp<JavaBBinder> mBinder;     // 弱引用持有的JavaBBinder

    // 以下属性在JavaBBinder未创建时被缓存，创建后应用
    bool            mVintf = false;
    bool            mSetExtensionCalled = false;
    bool            mInheritRt = false;
};

// ----------------------------------------------------------------------------

// 模板类：JavaRecipient 及其列表管理
// 用于处理Binder死亡通知和冻结状态变化回调
// 以下注释说明了继承/组合关系图

template <typename T>
class JavaRecipient;

template <typename T>
class RecipientList : public RefBase {
    List<sp<JavaRecipient<T>>> mInternalList; // 内部存储JavaRecipient的链表
    Mutex mLock;

public:
    RecipientList();
    virtual ~RecipientList();

    void add(const sp<JavaRecipient<T> >& recipient);     // 添加接收者
    void remove(const sp<JavaRecipient<T> >& recipient);  // 移除接收者
    sp<JavaRecipient<T> > find(jobject recipient);        // 根据Java对象查找对应的接收者

    Mutex& lock();  // 返回锁，用于外部同步（如死亡处理时）
};

// ----------------------------------------------------------------------------
// 根据SDK版本决定是否使用弱引用处理死亡接收
#ifdef BINDER_DEATH_RECIPIENT_WEAK_FROM_JNI
#if __BIONIC__
#include <android/api-level.h>
static bool target_sdk_is_at_least_vic() { // 判断目标SDK是否 >= V (VanillaIceCream?)
    return android_get_application_target_sdk_version() >= __ANDROID_API_V__;
}
#else
static constexpr bool target_sdk_is_at_least_vic() {
    return true; // 非Android平台（如主机测试）默认采用最新行为
}
#endif // __BIONIC__
#endif // BINDER_DEATH_RECIPIENT_WEAK_FROM_JNI

// 模板辅助函数：返回日志前缀，区分死亡和冻结
template <typename T>
constexpr const char* logPrefix();

template <>
constexpr const char* logPrefix<IBinder::DeathRecipient>() {
    return "[DEATH]";
}

template <>
constexpr const char* logPrefix<IBinder::FrozenStateChangeCallback>() {
    return "[FREEZE]";
}

template <typename T>
class JavaRecipient : public T {
public:
    // 构造函数：根据useWeakReference决定用全局引用还是弱全局引用持有Java对象
    JavaRecipient(JNIEnv* env, jobject recipient, const sp<RecipientList<T>>& list,
                  bool useWeakReference)
          : mVM(jnienv_to_javavm(env)), mRecipient(NULL), mRecipientWeak(NULL), mList(list) {
        if (useWeakReference) {
            mRecipientWeak = env->NewWeakGlobalRef(recipient); // 弱引用
        } else {
            mRecipient = env->NewGlobalRef(recipient); // 强全局引用
        }
    }

    void onFirstRef() override {
        T::onFirstRef();
        sp<RecipientList<T>> list = mList.promote();
        // 将自己添加到列表中，列表持有强引用
        LOG_DEATH_FREEZE("%s Adding JavaRecipient %p to RecipientList %p", logPrefix<T>(), this,
                         list.get());
        list->add(sp<JavaRecipient>::fromExisting(this));
    }

    // 清理引用，从列表中移除
    void clearReference() {
        sp<RecipientList<T> > list = mList.promote();
        if (list != NULL) {
            LOG_DEATH_FREEZE("%s Removing JavaRecipient %p from RecipientList %p", logPrefix<T>(),
                             this, list.get());
            list->remove(sp<JavaRecipient>::fromExisting(this));
        } else {
            LOG_DEATH_FREEZE("%s clearReference() on JavaRecipient %p but RecipientList wp purged",
                             logPrefix<T>(), this);
        }
    }

    bool matches(jobject obj) { // 检查是否与给定的Java对象匹配
        bool result;
        JNIEnv* env = javavm_to_jnienv(mVM);

        if (mRecipient != NULL) {
            result = env->IsSameObject(obj, mRecipient);
        } else {
            ScopedLocalRef<jobject> me(env, env->NewLocalRef(mRecipientWeak));
            result = env->IsSameObject(obj, me.get());
        }
        return result;
    }

    void warnIfStillLive() { // 如果仍持有引用，发出警告（可能泄漏）
        if (mRecipient != NULL) {
            JNIEnv* env = javavm_to_jnienv(mVM);
            ScopedLocalRef<jclass> objClassRef(env, env->GetObjectClass(mRecipient));
            ScopedLocalRef<jstring> nameRef(env,
                                            (jstring)env->CallObjectMethod(objClassRef.get(),
                                                                           gClassOffsets.mGetName));
            ScopedUtfChars nameUtf(env, nameRef.get());
            if (nameUtf.c_str() != NULL) {
                ALOGW("BinderProxy is being destroyed but the application did not call "
                      "unlinkToDeath to unlink all of its death recipients beforehand.  "
                      "Releasing leaked death recipient: %s",
                      nameUtf.c_str());
            } else {
                ALOGW("BinderProxy being destroyed; unable to get DR object name");
                env->ExceptionClear();
            }
        }
    }

protected:
    virtual ~JavaRecipient() {
        JNIEnv* env = javavm_to_jnienv(mVM);
        if (mRecipient != NULL) {
            env->DeleteGlobalRef(mRecipient); // 删除全局引用
        } else {
            env->DeleteWeakGlobalRef(mRecipientWeak); // 删除弱全局引用
        }
    }

    JavaVM* const mVM;
    jobject mRecipient;       // 强全局引用（可能为NULL）
    jweak mRecipientWeak;     // 弱全局引用（可能为NULL）
    wp<RecipientList<T> > mList; // 指向所属列表的弱引用
};

// JavaDeathRecipient：专门处理死亡通知
class JavaDeathRecipient : public JavaRecipient<IBinder::DeathRecipient> {
public:
    static bool useWeakReference() {
#ifdef BINDER_DEATH_RECIPIENT_WEAK_FROM_JNI
        return target_sdk_is_at_least_vic(); // 新SDK使用弱引用避免内存泄漏
#else
        return false;
#endif
    }

    JavaDeathRecipient(JNIEnv* env, jobject recipient,
                       const sp<RecipientList<IBinder::DeathRecipient>>& list)
          : JavaRecipient(env, recipient, list, useWeakReference()) {
        gNumDeathRefsCreated.fetch_add(1, std::memory_order_relaxed);
        gcIfManyNewRefs(env);
    }

    virtual ~JavaDeathRecipient() {
        gNumDeathRefsDeleted.fetch_add(1, std::memory_order_relaxed);
    }

    void binderDied(const wp<IBinder>& who) override // 收到死亡通知时回调
    {
        LOG_DEATH_FREEZE("Receiving binderDied() on JavaDeathRecipient %p\n", this);
        if (mRecipient == NULL && mRecipientWeak == NULL) {
            return;
        }
        JNIEnv* env = javavm_to_jnienv(mVM);
        ScopedLocalRef<jobject> jBinderProxy(env, javaObjectForIBinder(env, who.promote()));

        // 获取死亡接收者Java对象的局部引用
        ScopedLocalRef<jobject> jRecipient(env,
                                           env->NewLocalRef(mRecipient != NULL ? mRecipient
                                                                               : mRecipientWeak));
        if (jRecipient.get() == NULL) {
            ALOGW("Binder died, but death recipient is already garbage collected. ...");
            return;
        }

        if (mFired) { // 已经触发过，防止重复通知
            ALOGW("Received multiple death notices for the same binder object. Binder driver bug?");
            return;
        }
        mFired = true;

        // 调用BinderProxy的静态方法sendDeathNotice，将通知派发给Java层
        env->CallStaticVoidMethod(gBinderProxyOffsets.mClass, gBinderProxyOffsets.mSendDeathNotice,
                                  jRecipient.get(), jBinderProxy.get());
        if (env->ExceptionCheck()) {
            jthrowable excep = env->ExceptionOccurred();
            binder_report_exception(env, excep,
                                    "*** Uncaught exception returned from death notification!");
        }

        // 死亡通知已发送，将强引用降级为弱引用（如果存在），允许GC回收
        if (mRecipient != NULL) {
            auto list = mList.promote();
            if (list != NULL) {
                AutoMutex _l(list->lock());

                mRecipientWeak = env->NewWeakGlobalRef(mRecipient);
                env->DeleteGlobalRef(mRecipient);
                mRecipient = NULL;
            }
        }
    }

private:
    bool mFired = false; // 标记是否已触发
};

// JavaFrozenStateChangeCallback：处理冻结状态变化
class JavaFrozenStateChangeCallback : public JavaRecipient<IBinder::FrozenStateChangeCallback> {
public:
    JavaFrozenStateChangeCallback(JNIEnv* env, jobject recipient,
                                  const sp<RecipientList<IBinder::FrozenStateChangeCallback>>& list)
          : JavaRecipient(env, recipient, list, /*useWeakReference=*/true) {}

    virtual ~JavaFrozenStateChangeCallback() {}

    void onStateChanged(const wp<IBinder>& who, State state) override {
        LOG_DEATH_FREEZE("Receiving onStateChanged() ...");
        if (mRecipientWeak == NULL) {
            return;
        }
        JNIEnv* env = javavm_to_jnienv(mVM);
        ScopedLocalRef<jobject> jBinderProxy(env, javaObjectForIBinder(env, who.promote()));

        ScopedLocalRef<jobject> jCallback(env, env->NewLocalRef(mRecipientWeak));
        if (jCallback.get() == NULL) {
            return;
        }
        env->CallStaticVoidMethod(gBinderProxyOffsets.mClass,
                                  gBinderProxyOffsets.mInvokeFrozenStateChangeCallback,
                                  jCallback.get(), jBinderProxy.get(), state);
        if (env->ExceptionCheck()) {
            jthrowable excep = env->ExceptionOccurred();
            binder_report_exception(env, excep,
                                    "*** Uncaught exception returned from frozen state change "
                                    "notification!");
        }
    }
};

// ----------------------------------------------------------------------------
// RecipientList 的实现

template <typename T>
RecipientList<T>::RecipientList() {
    LOG_DEATH_FREEZE("%s New RecipientList @ %p", logPrefix<T>(), this);
}

template <typename T>
RecipientList<T>::~RecipientList() {
    LOG_DEATH_FREEZE("%s Destroy RecipientList @ %p", logPrefix<T>(), this);
    AutoMutex _l(mLock);
    if (mInternalList.size() > 0) {
        for (auto iter = mInternalList.begin(); iter != mInternalList.end(); iter++) {
            (*iter)->warnIfStillLive(); // 列表销毁时仍有接收者未移除，发出警告
        }
    }
}

template <typename T>
void RecipientList<T>::add(const sp<JavaRecipient<T> >& recipient) {
    AutoMutex _l(mLock);
    LOG_DEATH_FREEZE("%s RecipientList @ %p : add JavaRecipient %p", logPrefix<T>(), this,
                     recipient.get());
    mInternalList.push_back(recipient);
}

template <typename T>
void RecipientList<T>::remove(const sp<JavaRecipient<T> >& recipient) {
    AutoMutex _l(mLock);
    for (auto iter = mInternalList.begin(); iter != mInternalList.end(); iter++) {
        if (*iter == recipient) {
            LOG_DEATH_FREEZE("%s RecipientList @ %p : remove JavaRecipient %p", logPrefix<T>(),
                             this, recipient.get());
            mInternalList.erase(iter);
            return;
        }
    }
}

template <typename T>
sp<JavaRecipient<T> > RecipientList<T>::find(jobject recipient) {
    AutoMutex _l(mLock);
    for (auto iter = mInternalList.begin(); iter != mInternalList.end(); iter++) {
        if ((*iter)->matches(recipient)) {
            return *iter;
        }
    }
    return NULL;
}

template <typename T>
Mutex& RecipientList<T>::lock() {
    return mLock;
}

// 实例化两种列表类型
using DeathRecipientList = RecipientList<IBinder::DeathRecipient>;
using FrozenStateChangeCallbackList = RecipientList<IBinder::FrozenStateChangeCallback>;

// ----------------------------------------------------------------------------

namespace android {

// BinderProxyNativeData结构体：聚合了BinderProxy的native数据
struct BinderProxyNativeData {
    sp<IBinder> mObject; // 实际的native binder代理
    sp<DeathRecipientList> mOrgue; // 死亡接收者列表
    sp<FrozenStateChangeCallbackList> mFrozenStateChangeCallbackList; // 冻结状态回调列表
};

// 从Java BinderProxy对象中取出BinderProxyNativeData指针
BinderProxyNativeData* getBPNativeData(JNIEnv* env, jobject obj) {
    return (BinderProxyNativeData *) env->GetLongField(obj, gBinderProxyOffsets.mNativeData);
}

// 核心函数：根据IBinder生成对应的Java对象
// 如果是JavaBBinder，返回其持有的Java对象；否则创建/复用BinderProxy对象
jobject javaObjectForIBinder(JNIEnv* env, const sp<IBinder>& val)
{
    if (val == NULL) return NULL;

    if (val->checkSubclass(&gBinderOffsets)) {
        // 是JavaBBinder，直接返回它封装的Java对象
        jobject object = static_cast<JavaBBinder*>(val.get())->object();
        LOG_DEATH_FREEZE("objectForBinder %p: it's our own %p!\n", val.get(), object);
        return object;
    }

    // 否则创建新的native数据结构和BinderProxy
    BinderProxyNativeData* nativeData = new BinderProxyNativeData();
    nativeData->mOrgue = sp<DeathRecipientList>::make();
    nativeData->mFrozenStateChangeCallbackList = sp<FrozenStateChangeCallbackList>::make();
    nativeData->mObject = val;

    // 调用BinderProxy的静态方法getInstance，传入native数据指针和原始binder指针
    jobject object = env->CallStaticObjectMethod(gBinderProxyOffsets.mClass,
            gBinderProxyOffsets.mGetInstance, (jlong) nativeData, (jlong) val.get());
    if (env->ExceptionCheck()) {
        // 发生异常时，getInstance已经负责释放nativeData
        return NULL;
    }
    BinderProxyNativeData* actualNativeData = getBPNativeData(env, object);
    if (actualNativeData != nativeData) {
        // 如果返回的对象中使用的不是新创建的nativeData，说明对象被复用了，删除多余的
        delete nativeData;
    }

    return object;
}

// 反向操作：根据Java对象获取IBinder
sp<IBinder> ibinderForJavaObject(JNIEnv* env, jobject obj)
{
    if (obj == NULL) return NULL;

    // 如果是Binder子类，从mObject字段取出JavaBBinderHolder，再获取JavaBBinder
    if (env->IsInstanceOf(obj, gBinderOffsets.mClass)) {
        JavaBBinderHolder* jbh = (JavaBBinderHolder*)
            env->GetLongField(obj, gBinderOffsets.mObject);
        if (jbh == nullptr) {
            ALOGE("JavaBBinderHolder null on binder");
            return nullptr;
        }
        return jbh->get(env, obj);
    }

    // 如果是BinderProxy，直接返回其native数据中的mObject
    if (env->IsInstanceOf(obj, gBinderProxyOffsets.mClass)) {
        return getBPNativeData(env, obj)->mObject;
    }

    ALOGW("ibinderForJavaObject: %p is not a Binder object", obj);
    return NULL;
}

// 创建一个ParcelFileDescriptor Java对象
jobject newParcelFileDescriptor(JNIEnv* env, jobject fileDesc)
{
    return env->NewObject(
            gParcelFileDescriptorOffsets.mClass, gParcelFileDescriptorOffsets.mConstructor, fileDesc);
}

// 同步StrictMode策略到Java层
void set_dalvik_blockguard_policy(JNIEnv* env, jint strict_policy)
{
    env->CallStaticVoidMethod(gStrictModeCallbackOffsets.mClass,
                              gStrictModeCallbackOffsets.mCallback,
                              strict_policy);
}

// 根据status_t错误码，抛出对应的Java异常
void signalExceptionForError(JNIEnv* env, jobject obj, status_t err,
        bool canThrowRemoteException, int parcelSize)
{
    switch (err) {
        case UNKNOWN_ERROR:
            jniThrowException(env, "java/lang/RuntimeException", "Unknown error");
            break;
        case NO_MEMORY: {
            String8 msg;
            msg.appendFormat("During transaction with Parcel size %d.", parcelSize);
            jniThrowException(env, "java/lang/OutOfMemoryError", msg.c_str());
            break;
        }
        // ... 其他case类似，映射到对应的Java异常
        case INVALID_OPERATION:
            jniThrowException(env, "java/lang/UnsupportedOperationException", NULL);
            break;
        case BAD_VALUE:
            jniThrowException(env, "java/lang/IllegalArgumentException", NULL);
            break;
        case BAD_INDEX:
            jniThrowException(env, "java/lang/IndexOutOfBoundsException", NULL);
            break;
        case BAD_TYPE:
            jniThrowException(env, "java/lang/IllegalArgumentException", NULL);
            break;
        case NAME_NOT_FOUND:
            jniThrowException(env, "java/util/NoSuchElementException", NULL);
            break;
        case PERMISSION_DENIED:
            jniThrowException(env, "java/lang/SecurityException", NULL);
            break;
        case NOT_ENOUGH_DATA:
            jniThrowException(env, "android/os/ParcelFormatException", "Not enough data");
            break;
        case NO_INIT:
            jniThrowException(env, "java/lang/RuntimeException", "Not initialized");
            break;
        case ALREADY_EXISTS:
            jniThrowException(env, "java/lang/RuntimeException", "Item already exists");
            break;
        case DEAD_OBJECT:
            // DeadObjectException是受检异常，根据canThrowRemoteException决定抛出
            jniThrowException(env, canThrowRemoteException
                    ? "android/os/DeadObjectException"
                    : "java/lang/RuntimeException", NULL);
            break;
        case UNKNOWN_TRANSACTION:
            jniThrowException(env, "java/lang/RuntimeException", "Unknown transaction code");
            break;
        case FAILED_TRANSACTION: {
            ALOGE("!!! FAILED BINDER TRANSACTION !!!  (parcel size = %d)", parcelSize);
            const char* exceptionToThrow;
            std::string msg;
            // 根据parcel大小猜测原因：大parcel抛TransactionTooLargeException，否则可能是远程进程死亡
            if (canThrowRemoteException && parcelSize > 200*1024) {
                exceptionToThrow = "android/os/TransactionTooLargeException";
                msg = base::StringPrintf("data parcel size %d bytes", parcelSize);
            } else {
                exceptionToThrow = (canThrowRemoteException)
                        ? "android/os/DeadObjectException"
                        : "java/lang/RuntimeException";
                msg = "Transaction failed on small parcel; remote process probably died...";
            }
            jniThrowException(env, exceptionToThrow, msg.c_str());
        } break;
        // ... 其他文件描述符相关错误
        case FDS_NOT_ALLOWED:
            jniThrowException(env, "java/lang/RuntimeException", "Not allowed to write file descriptors here");
            break;
        case UNEXPECTED_NULL:
            jniThrowNullPointerException(env, NULL);
            break;
        case -EBADF:
            jniThrowException(env, "java/lang/RuntimeException", "Bad file descriptor");
            break;
        // ... 剩余错误码类似，转化为RuntimeException
        default:
            ALOGE("Unknown binder error code. 0x%" PRIx32, err);
            String8 msg;
            msg.appendFormat("Unknown binder error code. 0x%" PRIx32, err);
            jniThrowException(env,
                              canThrowRemoteException ? "android/os/RemoteException"
                                                      : "java/lang/RuntimeException",
                              msg.c_str());
            break;
    }
}

}

// ----------------------------------------------------------------------------
// 以下为JNI原生方法实现，与android.os.Binder中的native方法一一对应
// 这些方法被标记为CRITICAL_JNI_PARAMS，表示关键原生方法，执行快速不阻塞

static jint android_os_Binder_getCallingPid(CRITICAL_JNI_PARAMS)
{
    return IPCThreadState::self()->getCallingPid(); // 获取调用者的PID
}

static jint android_os_Binder_getCallingUid(CRITICAL_JNI_PARAMS)
{
    return IPCThreadState::self()->getCallingUid(); // 获取调用者的UID
}

static jboolean android_os_Binder_isDirectlyHandlingTransactionNative(CRITICAL_JNI_PARAMS) {
    return getCurrentServingCall() == BinderCallType::BINDER; // 当前是否处理Binder调用（非hwbinder等）
}

static jlong android_os_Binder_clearCallingIdentity(CRITICAL_JNI_PARAMS)
{
    return IPCThreadState::self()->clearCallingIdentity(); // 清除调用者身份，返回恢复令牌
}

static void android_os_Binder_restoreCallingIdentity(CRITICAL_JNI_PARAMS_COMMA jlong token)
{
    IPCThreadState::self()->restoreCallingIdentity(token); // 恢复之前保存的身份
}

static jboolean android_os_Binder_hasExplicitIdentity(CRITICAL_JNI_PARAMS) {
    return IPCThreadState::self()->hasExplicitIdentity(); // 检查是否有显式设置的身份
}

static void android_os_Binder_setThreadStrictModePolicy(CRITICAL_JNI_PARAMS_COMMA jint policyMask)
{
    IPCThreadState::self()->setStrictModePolicy(policyMask); // 设置当前线程的StrictMode策略
}

static jint android_os_Binder_getThreadStrictModePolicy(CRITICAL_JNI_PARAMS)
{
    return IPCThreadState::self()->getStrictModePolicy(); // 获取策略
}

static jlong android_os_Binder_setCallingWorkSourceUid(CRITICAL_JNI_PARAMS_COMMA jint workSource)
{
    return IPCThreadState::self()->setCallingWorkSourceUid(workSource); // 设置工作源UID
}

static jlong android_os_Binder_getCallingWorkSourceUid(CRITICAL_JNI_PARAMS)
{
    return IPCThreadState::self()->getCallingWorkSourceUid();
}

static jlong android_os_Binder_clearCallingWorkSource(CRITICAL_JNI_PARAMS)
{
    return IPCThreadState::self()->clearCallingWorkSource();
}

static void android_os_Binder_restoreCallingWorkSource(CRITICAL_JNI_PARAMS_COMMA jlong token)
{
    IPCThreadState::self()->restoreCallingWorkSource(token);
}

// 标记Binder为VINTF稳定性
static void android_os_Binder_markVintfStability(JNIEnv* env, jobject clazz) {
    JavaBBinderHolder* jbh =
        (JavaBBinderHolder*) env->GetLongField(clazz, gBinderOffsets.mObject);
    jbh->markVintf();
}

static void android_os_Binder_forceDowngradeToSystemStability(JNIEnv* env, jobject clazz) {
    JavaBBinderHolder* jbh =
        (JavaBBinderHolder*) env->GetLongField(clazz, gBinderOffsets.mObject);
    jbh->forceDowngradeToSystemStability();
}

// 强制刷新Binder命令缓冲区
static void android_os_Binder_flushPendingCommands(JNIEnv* env, jobject clazz)
{
    IPCThreadState::self()->flushCommands();
}

// 创建JavaBBinderHolder并返回其指针
static jlong android_os_Binder_getNativeBBinderHolder(CRITICAL_JNI_PARAMS) {
    JavaBBinderHolder* jbh = new JavaBBinderHolder();
    return (jlong) jbh;
}

// 负责释放JavaBBinderHolder的回调
static void Binder_destroy(void* rawJbh)
{
    JavaBBinderHolder* jbh = (JavaBBinderHolder*) rawJbh;
    ALOGV("Java Binder: deleting holder %p", jbh);
    delete jbh;
}

JNIEXPORT jlong JNICALL android_os_Binder_getNativeFinalizer(JNIEnv*, jclass) {
    return (jlong) Binder_destroy; // 返回释放函数指针，供Java层注册
}

static void android_os_Binder_blockUntilThreadAvailable(JNIEnv* env, jobject clazz)
{
    return IPCThreadState::self()->blockUntilThreadAvailable();
}

// 设置扩展Binder
static void android_os_Binder_setExtension(JNIEnv* env, jobject obj, jobject extensionObject) {
    JavaBBinderHolder* jbh = (JavaBBinderHolder*) env->GetLongField(obj, gBinderOffsets.mObject);
    sp<IBinder> extension = ibinderForJavaObject(env, extensionObject);
    jbh->setExtension(extension);
}

// 设置是否继承实时优先级
static void android_os_Binder_setInheritRt(JNIEnv* env, jobject obj, jboolean inheritRt) {
    JavaBBinderHolder* jbh = (JavaBBinderHolder*) env->GetLongField(obj, gBinderOffsets.mObject);
    jbh->setInheritRt(inheritRt);
}

static void android_os_Binder_setGlobalInheritRt(CRITICAL_JNI_PARAMS_COMMA jboolean enabled) {
    BBinder::setGlobalInheritRt(enabled);
}

// ----------------------------------------------------------------------------

// Binder类的JNI方法注册表
static const JNINativeMethod gBinderMethods[] = {
    { "getCallingPid", "()I", (void*)android_os_Binder_getCallingPid },
    { "getCallingUid", "()I", (void*)android_os_Binder_getCallingUid },
    { "isDirectlyHandlingTransactionNative", "()Z", (void*)android_os_Binder_isDirectlyHandlingTransactionNative },
    { "clearCallingIdentity", "()J", (void*)android_os_Binder_clearCallingIdentity },
    { "restoreCallingIdentity", "(J)V", (void*)android_os_Binder_restoreCallingIdentity },
    // ... 其余方法注册
    { "setExtensionNative", "(Landroid/os/IBinder;)V", (void*)android_os_Binder_setExtension },
    { "setInheritRt", "(Z)V", (void*)android_os_Binder_setInheritRt },
};

const char* const kBinderPathName = "android/os/Binder"; // Java类全限定名

// 注册Binder类的native方法，并初始化相关JNI缓存
static int int_register_android_os_Binder(JNIEnv* env)
{
    jclass clazz = FindClassOrDie(env, kBinderPathName);

    gBinderOffsets.mClass = MakeGlobalRefOrDie(env, clazz);
    gBinderOffsets.mExecTransact = GetMethodIDOrDie(env, clazz, "execTransact", "(IJJI)Z");
    // ... 获取其他方法/字段ID
    gBinderOffsets.mObject = GetFieldIDOrDie(env, clazz, "mObject", "J");

    return RegisterMethodsOrDie(
        env, kBinderPathName,
        gBinderMethods, NELEM(gBinderMethods));
}

// ****************************************************************************
// 以下为BinderInternal、BinderProxy相关的JNI方法及注册
// 由于篇幅，仅列出关键注释，不再逐行翻译所有相似的函数

// BinderInternal: 获取上下文对象、加入线程池等
static jobject android_os_BinderInternal_getContextObject(JNIEnv* env, jobject clazz)
{
    sp<IBinder> b = ProcessState::self()->getContextObject(NULL);
    return javaObjectForIBinder(env, b);
}

static void android_os_BinderInternal_joinThreadPool(JNIEnv* env, jobject clazz)
{
    sp<IBinder> b = ProcessState::self()->getContextObject(NULL);
    android::IPCThreadState::self()->joinThreadPool();
}

// ... 其他BinderInternal方法注册

// BinderProxy: pingBinder, transact, linkToDeath, unlinkToDeath等
static jboolean android_os_BinderProxy_transact(JNIEnv* env, jobject obj,
        jint code, jobject dataObj, jobject replyObj, jint flags)
{
    // 将Java Parcel转为native Parcel，调用目标IBinder的transact
    // ...
    status_t err = target->transact(code, *data, reply, flags);
    // 根据返回值处理异常
    signalExceptionForError(env, obj, err, true, data->dataSize());
    return JNI_FALSE;
}

// linkToDeath: 为远程Binder注册死亡通知
static void android_os_BinderProxy_linkToDeath(JNIEnv* env, jobject obj,
        jobject recipient, jint flags)
{
    // 创建JavaDeathRecipient并调用target->linkToDeath
    // ...
}

// ... 其他方法及注册

// 最终的注册入口，在JNI_OnLoad中调用
int register_android_os_Binder(JNIEnv* env)
{
    if (int_register_android_os_Binder(env) < 0) return -1;
    if (int_register_android_os_BinderInternal(env) < 0) return -1;
    if (int_register_android_os_BinderProxy(env) < 0) return -1;

    // 初始化日志、ParcelFileDescriptor、StrictMode、Thread相关缓存
    jclass clazz = FindClassOrDie(env, "android/util/Log");
    gLogOffsets.mClass = MakeGlobalRefOrDie(env, clazz);
    gLogOffsets.mLogE = GetStaticMethodIDOrDie(env, clazz, "e", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)I");
    // ... 其他缓存初始化
    return 0;
}