Vendor 应用打包测试步骤

本文档介绍安装与部署 Vendor 应用的两种方式。

## 方式一：在 Android Studio 中使用 platform 签名，设定 APP 的 system 权限

**1：配置应用清单文件**

在 Android Studio 的 AndroidManifest.xml 中去掉：
android:sharedUserId="android.uid.system"


**2：准备系统签名文件**

确保拥有系统签名文件 platform.pk8 和 platform.x509.pem。这两种文件通常包含在 Keystore 中。

**3：对 APK 进行系统签名**

签名有以下两种方式：

**方式一：通过 Keytool 生成并使用 Keystore 签名**

1. 下载 keytool-importkeypair 工具：  
   https://github.com/getfatday/keytool-importkeypair

2. 切换到 Android 原生目录 build/target/product/security 下，执行命令：
   ./keytool-importkeypair -k ./platform.keystore -p ljs123 -pk8 platform.pk8 -cert platform.x509.pem -alias platform
   

3. 在 Android Studio 中使用生成的 platform.keystore 创建已签名的安装包。

**方式二：直接使用 apksigner 工具签名**

1. 切换到 Android SDK 的构建工具目录，例如：

   C:\Users\username\AppData\Local\Android\Sdk\build-tools\36.0.0
   

2. 执行命令：
```
   .\apksigner.bat sign --key platform.pk8 --cert platform.x509.pem --out output.apk input.apk
```   

   生成的 output.apk 即为最终签名完成的 APK 文件。

步骤 4：将签名后的 APK 部署到系统分区

>预备工作：如果bootloader没有解锁，需要优先解锁bootloader
- 打开设备的 开发者选项，进入 开发者选项 - OEM解锁（允许解锁引导加载程序）
- adb连接设备：adb reboot bootloader
- 连接设备：fastboot devices
- 解锁bootloader，进入Android SDK目录（C:\Users\用户名\AppData\Local\Android\Sdk\platform-tools）：fastboot flashing unlock
- 根据屏幕提示，选择“音量加/减键”确认执行bootloader的解锁，成功后会自动重启。

Android 10+ 动态分区 + dm-verity/AVB​ 下：

你就算用 root 把 APK cp进 /system/priv-app/XXX/，只要 verity 开着，重启后要么被还原要么直接进 dm-verity错误卡住。

所以“直接放 APK”能不能快速验证，取决于你是否满足前置条件。
>前置条件检查
```
adb root
adb shell getprop ro.boot.verifiedbootstate
adb shell getprop ro.boot.warranty_bit     # 有就看看
adb shell getprop service.adb.root          # 应该 1
adb shell mount | grep -E "system|vendor"
```

verifiedbootstate = orange（unlocked，verity 可关）✅

或​ system 挂载为 rw（非常少见，但 userdebug 可能）

如果是 green（锁死 + verity on）→ 此路不通。

如果上面前置检查通过，那么可以进行下一步：
```
adb root
adb enable-verity   # 有些版本要先执行一次（只设标记，不一定立即锁）
adb remount         # 关键：让 /system 或 overlay 可写
```
关Verify
```
adb root
adb disable-verity
adb reboot
# 重启后再
adb root
adb remount
```
执行如下指令：
```
adb shell mount | grep system
# rw  (或者 overlay)
```
推 APK（标准 system/priv-app 布局）
```
# 假设你要验证的是特权系统应用
APP=YourSystemApp
PKG=com.panasonic.yourapp
APK=YourSystemApp.apk

adb shell mkdir -p /system/priv-app/$APP
adb push $APK /system/priv-app/$APP/$APK

# 权限必须对
adb shell chmod 644 /system/priv-app/$APP/$APK
adb shell chown root:root /system/priv-app/$APP/$APK

```

重启设备后验证：
```
adb shell pm list packages -f | grep $PKG
adb shell dumpsys package $PKG | grep -A3 "userId"
# 期望看到 userId=1000 或 android.uid.system
```

4.1 挂载 product 分区为可读写
```
adb root
adb remount
adb reboot(操作remount后需要重启)
adb shell
su
# 查看 product 分区设备节点，示例：/dev/block/sda35
ls -la /dev/block/by-name/product
# 重新挂载为可读写
mount -o rw,remount /product
# 或使用设备节点挂载
mount -o rw,remount /dev/block/by-name/product /product
# 验证挂载状态（应显示 rw）
mount | grep product
```

4.2 创建应用目录并推送 APK 文件
```
# 创建应用目录（根据应用类型选择）
mkdir -p /product/app/YourAppName          # 普通系统应用
# 或
mkdir -p /product/priv-app/YourAppName     # 特权系统应用

# 设置目录权限
chmod 755 /product/app/YourAppName

# 退出 adb shell
exit

# 从电脑推送 APK
adb push your_app.apk /product/app/YourAppName/
```

4.3 设置 APK 文件权限
```
adb shell su -c "chmod 644 /product/app/YourAppName/your_app.apk"
adb shell su -c "chown root:root /product/app/YourAppName/your_app.apk"
```

4.4 （可选）部署 Native 库文件
```
adb shell su -c "mkdir -p /product/app/YourAppName/lib/arm64"
adb shell su -c "mkdir -p /product/app/YourAppName/lib/arm"
adb push libs/arm64-v8a/*.so /product/app/YourAppName/lib/arm64/
adb push libs/armeabi-v7a/*.so /product/app/YourAppName/lib/arm/
adb shell su -c "chmod 644 /product/app/YourAppName/lib/arm64/*.so"
adb shell su -c "chmod 644 /product/app/YourAppName/lib/arm/*.so"
```

如果需要制作system应用，需要执行以下步骤：



## 方式二：将 Android Studio 编译出的 APK 放入 Android 系统源码中，参与系统编译烧录至Vendor镜像中

**1：配置应用清单文件**

在 Android Studio 的 AndroidManifest.xml 中去掉 android:sharedUserId="android.uid.system"。

**2：将 APK 集成到 AOSP 源码树中**
```
2.1 使用 Android Studio 的 Build → Generate App Bundles or APKs 生成 APK 文件（注意：使用默认的 build 项目配置，生成的是默认签名的包，不必再签名，因为在与系统编译时会重新签名，否则可能因签名版本 V2、V3、V4 等与 Android 产生异常）。 

2.2 在 Android 源码目录 vendor/panasonic/proprietary/apps/ 下创建您的应用目录（例如 AccessController）。

2.3 将 Android Studio 编译出的 APK（例如 AccessController.apk）和所需的文件放入该目录。

2.4 在该目录下创建 Android.mk 文件，并添加 LOCAL_CERTIFICATE := platform 以使用系统平台签名。文件内容示例如下：
```
```
LOCAL_PATH := $(call my-dir)

######################################################
# Target for AccessController.apk
######################################################

include $(CLEAR_VARS)

LOCAL_MODULE := AccessController
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := APPS
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/vendor/app
LOCAL_SRC_FILES := AccessController.apk
LOCAL_MODULE_SUFFIX := $(COMMON_ANDROID_PACKAGE_SUFFIX)
LOCAL_CERTIFICATE := platform

include $(BUILD_PREBUILT)

############################################################
# Target for libpayment.so
############################################################

include $(CLEAR_VARS)

LOCAL_MODULE := libpayment
LOCAL_SRC_FILES := lib/$(LOCAL_MODULE).so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/vendor/lib64
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so

include $(BUILD_PREBUILT)
```

如果 mk 文件不适配当前的编译系统或编译工具，也可以使用 bp 文件，内容如下：
```
android_app_import {
    name: "AccessController",
    apk: "AccessController.apk",
    certificate: "platform",
    vendor: true,
    presigned: false,
}

// Prebuilt shared library: libpayment (64-bit only)
cc_prebuilt_library_shared {
    name: "libpayment",
    vendor: true,
    arch: {
        arm64: {
            srcs: ["lib/libpayment.so"],
        },
    },
    shared_libs: [
        "liblog",
    ],
    check_elf_files: false,
}
```

**3：在系统编译配置中启用该项目模块**

找到目录 device/mediateksample/aiot8391p2_64_bsp/device-vext.mk，在其中加入：
```
PRODUCT_PACKAGES += AccessController
```

为了便于管理每个项目的启用配置，可以在项目的目录（例如 /home/code_reader/mtk/U0/alps/vendor/panasonic/proprietary/apps/AccessController/）创建 access_controller_product.mk 文件，并在该文件中写入：
```
PRODUCT_PACKAGES += AccessController
```

如果项目里存在 so 库，还需要添加：
```
PRODUCT_PACKAGES += libpayment   
```
libpayment 为 so 库的名称，该名称在 Android.mk 或 Android.bp 文件中定义。


最后，将如下指令追加到 device-vext.mk 文件内容里面：
```
$(call inherit-product-if-exists, vendor/panasonic/proprietary/apps/AccessController/access_controller_product.mk)
```

**4：重新编译系统并刷机**

4.1 执行完整的 Android 系统编译（MTK 平台）

4.1.1 切换目录（脚本所在目录）：
```
   cd ${project-root}/B0/alps
```   

4.1.2 执行指令（MTK 提供）：
```
   python vendor/mediatek/proprietary/scripts/releasetools/split_build_helper.py --run full_aiot8391p2_64_bsp-next-userdebug --vf-path ../../U0/alps/
```

4.1.3 得到刷机产物：
```
   /home/code_reader/mtk/U0/alps/out/target/product/aiot8391p2_64_bsp/merged
```   

4.1.4 将 MTK 提供的 preloader_aiot8391p2_64_bsp.bin 文件覆盖刷机产物目录下的同名文件。

4.2 将生成的系统镜像刷入设备
```
4.2.1 使用 FlashToolSelector 工具，点击按钮弹出对话框，选择刷机产物下的 download_agent/flash.xml 文件。

4.2.2 将电脑与设备通过 USB 连接在一起，点击FlashToolSelector工具中 Download 按钮，同时快速按压设备上的 RST 键，等待刷入完成即可。
```
注意：上述步骤中的路径和产品名（aiot8391p2_64_bsp）为示例，请根据实际项目替换。