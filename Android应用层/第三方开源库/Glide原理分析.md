Glide分析工作主要要针对两方面：

    1.内存缓存的设置（对象池）与Bitmap解码与复用
    2.掌握生命周期，再配合生命周期来判断要不要进行数据的更新

### 内存缓存

聊这个，我们就要知道LruCache这个内存缓存的类，LruCache的内部实现是LinkedHashMap，在创建LruCache的时候，需要给一个空间大小，这个大小默认是总容量的八分之一。

![lrucache](./image/lrucache_structure.png)

整体流程：

当我们把对象从LruCache中删除后，会把对象加入到softReference中，当需要进行查找时，先去LruCache中找，找不到，就去softReference中寻找，如果找到了，就将对象重新加入到LruCache中，如果还找不到，就去文件系统中找，如果找到了就把对象放到LruCache中，如果找不到，就去网络下载，之后保存到文件系统，并放入LruCache。

LinkedHashMap内部就是一个HashMap，然后另外保存一个链表，具体看代码，首先看一下put方法：

```java
    public final V put(K key, V value) {
        if (key == null || value == null) {
            throw new NullPointerException("key == null || value == null");
        }

        V previous;
        synchronized (this) {
            putCount++;
            size += safeSizeOf(key, value);
            previous = map.put(key, value);
            if (previous != null) {
                size -= safeSizeOf(key, previous);
            }
        }

        if (previous != null) {
            entryRemoved(false, key, previous, value);
        }

        trimToSize(maxSize);
        return previous;
    }

```
一般一个图片的key会使用图片的url，value一般会是Bitmap，这个put方法使用synchroized来保证多线程下的使用是安全的，重点在trimToSize方法：
```java
 private void trimToSize(int maxSize) {
        while (true) {
            K key;
            V value;
            synchronized (this) {
                if (size < 0 || (map.isEmpty() && size != 0)) {
                    throw new IllegalStateException(getClass().getName()
                            + ".sizeOf() is reporting inconsistent results!");
                }

                if (size <= maxSize) {
                    break;
                }

                // BEGIN LAYOUTLIB CHANGE
                // get the last item in the linked list.
                // This is not efficient, the goal here is to minimize the changes
                // compared to the platform version.
                Map.Entry<K, V> toEvict = null;
                for (Map.Entry<K, V> entry : map.entrySet()) {
                    toEvict = entry;
                }
                // END LAYOUTLIB CHANGE

                if (toEvict == null) {
                    break;
                }

                key = toEvict.getKey();
                value = toEvict.getValue();
                map.remove(key);
                size -= safeSizeOf(key, value);
                evictionCount++;
            }

            entryRemoved(true, key, value, null);
        }
    }
```
这里代码也比较简单，就是判断当前的map是否大于最大容量，如果大于了，循环删除存储对象。

```java
    public final V get(K key) {
        if (key == null) {
            throw new NullPointerException("key == null");
        }

        V mapValue;
        synchronized (this) {
            mapValue = map.get(key);
            if (mapValue != null) {
                hitCount++;
                return mapValue;
            }
            missCount++;
        }

        /*
         * Attempt to create a value. This may take a long time, and the map
         * may be different when create() returns. If a conflicting value was
         * added to the map while create() was working, we leave that value in
         * the map and release the created value.
         */

        V createdValue = create(key);
        if (createdValue == null) {
            return null;
        }

        synchronized (this) {
            createCount++;
            mapValue = map.put(key, createdValue);

            if (mapValue != null) {
                // There was a conflict so undo that last put
                map.put(key, mapValue);
            } else {
                size += safeSizeOf(key, createdValue);
            }
        }

        if (mapValue != null) {
            entryRemoved(false, key, createdValue, mapValue);
            return mapValue;
        } else {
            trimToSize(maxSize);
            return createdValue;
        }
    }
```

到此LruCache就分析完成，接着，我们来看看Glide中是如何使用LruCache。

1.with方法

with方法传入的Context参数分两种：Application和非Applicaton

如果是Application的话，那么就不用处理，因为此时的生命周期是整个应用的生命周期。

如果是非Application的话，那么就会创建一个隐藏的Fragment来获取生命周期

with方法最终会返回一个RequestManager

2.load方法

这个方法返回一个DrawableTypeRequest对象，这个对象的父类是DrawableRequestBuilder，这个类承包了Glide的大部分api

3.into方法

    1.初始化参数，做好网络请求的准备
    2.使用原始的HttpConnect，读取文件流
    3.根据文件判断是gif或者png
    4.通过复杂的逻辑将图片提取出来，交给ImageView控件


对象池：通过减少大对象的内存分配开销来达到节省资源的目的。
