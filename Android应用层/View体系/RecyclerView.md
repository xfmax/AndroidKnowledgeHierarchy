一、UI上的区别

1.RecyclerView支持线性、网格、瀑布流等布局方式
2.RecyclerView支持局部刷新DiffUtil
3.更加完善的缓存机制


二、缓存机制

RecyclerView的四级缓存，ListView只具有二级缓存

1.屏幕内缓存：

    屏幕中显示的ViewHolder，这些ViewHodler会缓存到mAttachedScrap和mChangedScrap中，前者是屏幕内展示的ViewHolder列表，后者是数据已经改变的ViewHolder列表。
2.屏幕外缓存：

    当列表滑出了屏幕，ViewHolder会在mCachedViews中进行缓存，默认缓存量是2，并且可以通过setItemViewCacheSize方法动态设置。

3.自定义缓存：

    可以自己实现ViewCacheExtension类实现自定义缓存，可通过Recyclerview.setViewCacheExtension()设置，如无特殊情况不会使用。

4.缓存池：

    ViewHolder会优先缓存在mCachedViews中，超过默认大小之后，会存入mRecyclerPool,mRecyclerPool会根据ViewType将ViewHolder进行分类，每个分类最多存储5个ViewHolder。



使用RecyclerView的优化方案：

一、数据优化

    1.使用DiffUtil来进行局部的刷新
    2.升级到25.1.0以上，可以使用Prefetch功能进行预取

二、布局优化

    1.减少过度绘制
    2.较少inflate的时间，或者采取new View（）的方式创建视图。
    3.设计层面，扁平化UI，减少对象的创建
    4.如果高度固定，可以使用setHasFixedSize(true),以来避免requestLayout浪费资源
    5.滑动过程中停止加载
    6.如果不需要动画，可以把默认动画关闭来提升效率
    7.
