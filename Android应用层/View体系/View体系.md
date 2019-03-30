### View与ViewGroup
ViewGroup是一种扩展的View，看一下ViewGroup类：

        public abstract class ViewGroup extends View implements ViewParent,ViewManager{}

### Android坐标系与View坐标系

![Android坐标系](https://github.com/xfmax/AndroidKnowledgeHierarchy/blob/master/Android%E5%BA%94%E7%94%A8%E5%B1%82/View%E4%BD%93%E7%B3%BB/image/android_view_1.png)

使用getRawX()和getRawY()来获取Android系统的坐标。

### View坐标系

    width = getRight()-getLeft()
    height = getBottom()-getTop()

这样使用起来有些麻烦，所以系统提供了封装：
    width = getWidth()
    height = getHeight()

MotionEvent除了有ACTION_DOWN、ACTION_UP等事件外，其中还有:

        getX():获取点击位置距控件左边的距离。
        getY():获取点击位置距控件上面的距离。
        getRawX():获取点击位置距屏幕最左边的距离，绝对坐标。
        getRawY():获取点击位置距屏幕最右边的距离，绝对坐标。
### View的滑动
View滑动的原理：记下触摸时的坐标，再记录移动时触摸的坐标算出偏移量，通过偏移量来移动View到达指定坐标点。有六种方式：

        layout()
        offsetLeftAndRight
        offsetTopAndBottom
        LayoutParams
        动画
        scrollTo
        scrollBy
        Scroller

### 事件分发机制

### View工作流程

### 自定义View

