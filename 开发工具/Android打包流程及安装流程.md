详细的打包流程：

![install-image](https://github.com/xfmax/android_know/blob/master/%E5%BC%80%E5%8F%91%E5%B7%A5%E5%85%B7/image/apk_build.png)

    1.aapt将各个模块的manifest文件、res文件夹下的文件、assets文件夹下的文件，执行合并，然后为res文件夹下的图片文件生成R.java、将res文件夹下的xml文件生成resource.arsc文件，raw和assets文件夹的内容保持原状，assets文件夹下的文件是不能被映射到R.java中的，所以使用AssetManager进行加载。

    2.aidl文件转换为java文件。

    3.将R.java和src源文件、aidl装换成的java文件通过javac工具编译为class文件。

    4.通过项目中的混淆配置文件，将class文件（项目文件、第三方库、jar包）生成一个混淆后的jar包

    5.使用dx工具将jar进一步编译并优化成为dex文件。

    5.使用apkbuilder工具将上面resource.arsc和dex文件、raw文件夹、assets文件夹、第三方库资源文件打包成一个未签名的apk文件。

    6.使用jarsigner工具对apk文件进行签名。

    7.使用zipalign工具对apk进行对齐处理，以4字节的倍数进行存放。


安装流程：

以系统中的packageInstall.apk安装应用程序为例子，当你点击apk文件的时候，会调用系统应用packageInstall.apk应用，展示出安装界面：

![install-image](https://github.com/xfmax/android_know/blob/master/%E5%BC%80%E5%8F%91%E5%B7%A5%E5%85%B7/image/install_process.png)

当你点击安装按钮时，会执行2步操作：

    1.将APK的信息通过IO流的形式写入到PackageInstaller.Session中。
    2.调用PackageInstaller.Session的commit方法，将APK的信息交由PMS处理。

整个的安装流程如图：

![install-image](https://github.com/xfmax/android_know/blob/master/%E5%BC%80%E5%8F%91%E5%B7%A5%E5%85%B7/image/apk_install_structure.png)

    1.复制apk文件到/data/app/pkg目录下。
    2.解析AndroidManifest文件，并建立/data/data/pkg目录。
    3.将apk文件中的dex优化并解析到/data/delvik-cache/pkg目录，如果是art虚拟机的话，则将优化过的dex文件保存在/data/app/pkg/oat目录。
    4.解析AndroidManifest文件，将其中的信息提取到/system/app/package.xml文件中。
    5.安装完成，发送广播。

源代码流程：PackageInstall会将apk信息交给PMS处理，PMS通过PackageHandler发送消息来驱动来控制一个新的进程中DefaultContainerService，来完成apk的复制，安装前的校验以及安装工作。