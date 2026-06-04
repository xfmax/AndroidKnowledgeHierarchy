task：gradle哪一个环节最耗时，并如何优化？

### 最耗时的是：transformClassesWithDexForDebug

### 列出关键的task及其作用

    1.mergeDebugResources:解压所有的aar包，并将所有的资源文件合并到/app/build/intermediates/res/merged/debug

    2.processDebugManifest:把左右aar包中的AndroidManifest文件中的结点合并到项目的AndroidManifest文件中，输出到/app/build/intermediates/manifest/full/debug/Androidmanifest.xml

    3.complieDebugJavaWithJavac：将java文件编译成class文件。

    4.transformClassesWithJarMergingForDebug：将上一步的class文件和第三方库的aar文件合并。

    5.transformClassesWithMultidexlistForDebug：
        i：扫描AndroidManifest文件并分析类的依赖关系，确定哪些类会被放到第一个dex中，分析结果写到/app/build/intermediates/multi-dex/debug/
        maindexlist.txt
        ii：生成混淆配置项，输出到/app/build/intermediates/multi-dex/debug/manifest_keep.txt
        
    6.transformClassesWithDexForDebug：将所有jar文件转换为dex文件。
