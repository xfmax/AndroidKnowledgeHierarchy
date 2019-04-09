首先理解一下什么是MVC，因为MVP和MVVM都是在MVC的基础上发展而来。

### MVC

视图层(View) 

对应于xml布局文件和java代码动态view部分

控制层(Controller) 

MVC中Android的控制层是由Activity来承担的，Activity本来主要是作为初始化页面，展示数据的操作，但是因为XML视图功能太弱，所以Activity既要负责视图的显示又要加入控制逻辑，承担的功能过多。

模型层(Model) 

针对业务模型，建立的数据结构和相关的类，它主要负责网络请求，数据库处理，I/O的操作。

问题的关键是Activity承载了太多的控制和View的双重功能，所以需要一种新的架构方式来解放Activity,所以Android中的MVC像下图一样：

！[mvc]()

MVC的问题，衍生出了MVP模式：

![mvp]()
