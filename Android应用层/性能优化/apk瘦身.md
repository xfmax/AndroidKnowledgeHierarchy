### 了解一下apk的结构

    1.META—INF：存放签名文件，分别为 MANIFEST.MF、CERT.SH、CERT.RSA。
    2.AndroidManifest.xml:android四大组件注册信息。
    3.classes.dex:将混淆后的class文件优化并打包成dex文件。
    4.lib:包含特定cpu架构的软件库，例如armeabi、armeabi-v7a、armeabi-v8a,x86,x86_64,mips。
    5.assets:可以使用AssetMananger来加载资源文件。
    6.res:包含未编译到的资源，即图片文件。
    7.resources.arsc:可以被编译的资源文件，例如xml文件。

### 能够优化的目录或文件有：

    res、lib、resource.arsc、classes.dex

## classes.dex

定义：dex文件的数量由你的方法数而定，当你开启了multidex之后，系统会打包多个dex文件，命名规则为classes1.dex,classes2.dex...

措施：

    1.减少第三方库的使用
    2.避免使用枚举，而使用注解代替
    3.使用proGuard
    
## 优化res和assets目录下的资源文件

res/raw 和 assets的不同点：

    1.res/raw是会被映射到R.java文件的，你可以使用R.raw.id来访问res/raw目录下的文件，但是Assets目录下的文件，只能通过AssetManager来访问。
    2.res/raw目录下是不能有目录结构的，但是assets目录下还可以再建立目录。

图片优化策略：

    1.格式的选择：webP是最好的，比jpg省25%-35%,比png省80%左右。
    2.使用Tiny进行压缩。
    3.使用矢量图像，例如VectorDrawable，但是只能针对一些小图，否则系统加载的时间很长。
    4.剔除未被使用的资源，在build.gradle中设置参数shrinkResources为true；
    
例如，将下边的代码保存在 res/raw/keep.xml。构建不会将该文件打包到 APK 之中。

    <?xml version="1.0" encoding="utf-8"?>
    <resources xmlns:tools="http://schemas.android.com/tools"
    tools:keep="@layout/l_used*_c,@layout/l_used_a,@layout/l_used_b*"
    tools:discard="@layout/unused2" />
   
resources有以下属性:

    tools:keep 指出哪些资源会保留
    tools:discard 指定哪些资源需要剔除
    tools:shrinkMode 资源压缩模式,有两种取值strict和safe,默认为safe


### 移除未使用的备用资源

    defaultConfig {
    // 对于国际化支持只打包中文资源，
    resConfigs "zh-rCN"
    }

## lib中资源优化

    1.一般我们在开发过程中，使用的动态库只需要放入armeabi-v7a即可，它可以兼容armeabi库。
    2.有些库可以使用网络下载，具体根据业务场景来判断。
