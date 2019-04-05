task：put、get、loadFactor、resize

此次HashMap分析基于Java 8的版本：

### put
```java
 public V put(K key, V value) {
        return putVal(hash(key), key, value, false, true);
    }
```
没啥可说的，直接看putVal方法：
```java
final V putVal(int hash, K key, V value, boolean onlyIfAbsent,
                   boolean evict) {
        Node<K,V>[] tab; Node<K,V> p; int n, i;
        if ((tab = table) == null || (n = tab.length) == 0)
            n = (tab = resize()).length;
        if ((p = tab[i = (n - 1) & hash]) == null)
            tab[i] = newNode(hash, key, value, null);
        else {
            Node<K,V> e; K k;
            if (p.hash == hash &&
                ((k = p.key) == key || (key != null && key.equals(k))))
                e = p;
            else if (p instanceof TreeNode)
                e = ((TreeNode<K,V>)p).putTreeVal(this, tab, hash, key, value);
            else {
                for (int binCount = 0; ; ++binCount) {
                    if ((e = p.next) == null) {
                        p.next = newNode(hash, key, value, null);
                        if (binCount >= TREEIFY_THRESHOLD - 1) // -1 for 1st
                            treeifyBin(tab, hash);
                        break;
                    }
                    if (e.hash == hash &&
                        ((k = e.key) == key || (key != null && key.equals(k))))
                        break;
                    p = e;
                }
            }
            if (e != null) { // existing mapping for key
                V oldValue = e.value;
                if (!onlyIfAbsent || oldValue == null)
                    e.value = value;
                afterNodeAccess(e);
                return oldValue;
            }
        }
        ++modCount;
        if (++size > threshold)
            resize();
        afterNodeInsertion(evict);
        return null;
    }
```
我们要了解的是put的流程，首先判断table是否为null，如果为null，则调用resize方法为table进行初始化，如果不为null，则使用hash值（h^(h>>>16)）与（table.length-1）进行与（&）操作计算出该Node在table中的下标（index），判断table[index]是否为null，如果为空，那么直接为table[index]赋值new Node(hash,key,value),如果不为null，那么再判断table[index].next是否为null，如果为null，说明这个Node还没有发生过碰撞，也就是没有形成链表或者红黑树，那么将table[index].next赋值new Node(hash,key,value)，如果table[index]不为null，则判断是否为红黑树，如果是，则调用红黑树的处理方法，如果不是，则就是一个数量没有达到8的链表，那么就是循环找到链表的尾部，插入即可，一下是伪代码：

    if(table == null){
        resize()
    }
    if(table[hash & (table.length -1 )] == null){
        table[hash & (table.length -1 )]  = new Node(hash,key,value);
    }else {
        Node node = table[hash & (table.length -1 )].next;
        if(node == null){
            node = new Node(hash,key,value);
        }else if(node instanceof TreeNode){//如果是红黑树
            putTreeVal();
        }else{//链表,使用循环找到链表尾部，添加新的Node即可
           Node e = table[hash & (table.length -1 )].next;
           Node next;
           do{
             next = e.next()
             if(next == null){
                next = New Node(hash,key,value);
             }
           }while((e = next) != null);
        }
    }

### get
```java
    public V get(Object key) {
        Node<K,V> e;
        return (e = getNode(hash(key), key)) == null ? null : e.value;
    }
```
接着看getNode方法：
```java
    final Node<K,V> getNode(int hash, Object key) {
        Node<K,V>[] tab; Node<K,V> first, e; int n; K k;
        if ((tab = table) != null && (n = tab.length) > 0 &&
            (first = tab[(n - 1) & hash]) != null) {
            if (first.hash == hash && // always check first node
                ((k = first.key) == key || (key != null && key.equals(k))))
                return first;
            if ((e = first.next) != null) {
                if (first instanceof TreeNode)
                    return ((TreeNode<K,V>)first).getTreeNode(hash, key);
                do {
                    if (e.hash == hash &&
                        ((k = e.key) == key || (key != null && key.equals(k))))
                        return e;
                } while ((e = e.next) != null);
            }
        }
        return null;
    }

```
其实getNode方法和putVal方法很像，直接上伪代码：

    if(table != null && table.length > 0){
        if(table[hash & (table - 1)] != null){
            if(table[hash & (table - 1)].next == null){
                return table[hash & (table - 1)];
            }else{
                Node node = table[hash & (table - 1)];
                if(node insanceof TreeNode){
                    return getTreeNode();
                }else{
                    Node next;
                    do{
                        next = node.next;
                        if(hash = node.hash && key != null && key = node.key){
                            return node;
                        }
                    }while((node = next) != null);
                }
            }
        }
    }
    return null;

### loadFactor和Threshold、capacity
loadFactor是装载因子，负责控制一个table可以使用的最大容量，而Threshold就是table最大可使用容量，capacity是这个table的总容量，这三者计算公式为：

    Threshold = capacity × loadFactor

loadFactor的默认值是0.75，这应该是一个工程上的度量值，capacity的默认值是16，而且这个值都要取2的指数，因为这样在计算hash值的时候，capacity-1的二进制低位上会形成掩码1，这样再进行&运算计算index值的时候，会自动屏蔽掉高位，而留下低位，说起来可能有点费解，看一下具体的例子，如果capacity为16，计算index：

capacity-1 = 15，二进制为 0000000000000000 0000000000001111
那么任何数与这个数进行&操作，结果高28位都会被变为0，而只有低4位有可能为1，这样就把一个值限定在了整数0-15之间了

### resize
resize才是一个比较复杂的存在，看代码：
```java
    final Node<K,V>[] resize() {
        Node<K,V>[] oldTab = table;
        int oldCap = (oldTab == null) ? 0 : oldTab.length;
        int oldThr = threshold;
        int newCap, newThr = 0;
        if (oldCap > 0) {
            if (oldCap >= MAXIMUM_CAPACITY) {
                threshold = Integer.MAX_VALUE;
                return oldTab;
            }
            else if ((newCap = oldCap << 1) < MAXIMUM_CAPACITY &&
                     oldCap >= DEFAULT_INITIAL_CAPACITY)
                newThr = oldThr << 1; // double threshold
        }
        else if (oldThr > 0) // initial capacity was placed in threshold
            newCap = oldThr;
        else {               // zero initial threshold signifies using defaults
            newCap = DEFAULT_INITIAL_CAPACITY;
            newThr = (int)(DEFAULT_LOAD_FACTOR * DEFAULT_INITIAL_CAPACITY);
        }
        if (newThr == 0) {
            float ft = (float)newCap * loadFactor;
            newThr = (newCap < MAXIMUM_CAPACITY && ft < (float)MAXIMUM_CAPACITY ?
                      (int)ft : Integer.MAX_VALUE);
        }
        threshold = newThr;
        @SuppressWarnings({"rawtypes","unchecked"})
            Node<K,V>[] newTab = (Node<K,V>[])new Node[newCap];
        table = newTab;
        if (oldTab != null) {
            for (int j = 0; j < oldCap; ++j) {
                Node<K,V> e;
                if ((e = oldTab[j]) != null) {
                    oldTab[j] = null;
                    if (e.next == null)
                        newTab[e.hash & (newCap - 1)] = e;
                    else if (e instanceof TreeNode)
                        ((TreeNode<K,V>)e).split(this, newTab, j, oldCap);
                    else { // preserve order
                        Node<K,V> loHead = null, loTail = null;
                        Node<K,V> hiHead = null, hiTail = null;
                        Node<K,V> next;
                        do {
                            next = e.next;
                            if ((e.hash & oldCap) == 0) {
                                if (loTail == null)
                                    loHead = e;
                                else
                                    loTail.next = e;
                                loTail = e;
                            }
                            else {
                                if (hiTail == null)
                                    hiHead = e;
                                else
                                    hiTail.next = e;
                                hiTail = e;
                            }
                        } while ((e = next) != null);
                        if (loTail != null) {
                            loTail.next = null;
                            newTab[j] = loHead;
                        }
                        if (hiTail != null) {
                            hiTail.next = null;
                            newTab[j + oldCap] = hiHead;
                        }
                    }
                }
            }
        }
        return newTab;
    }
```
这么长的代码，到底干了什么，我给大家梳理一下，其实可以分为两步：

    1.确定新newTab的newCap和newThr,也就是新的table的capacity和Threshold。
    2.将旧表中的数据复制到新表中，这个过程需要重新计算index值。

别看代码这么长，其实就是这两步的实现，先看第1步：

    首先判断oldTab的capacity是否为大于0，如果大于0，就说明oldTable已经存在了，这种情况下，有两种情况，第一种oldTable的capacity已经超过了给定的最大值（1<<30），那么这时将OldThr改为Integer.Max_VALUE,然后返回oldTable，第二种情况就是正常的情况，那么newTab的newCap和newThr都要翻一倍。

    如果oldTab的capacity是否为不大于0，就判断oldThr是否大于0（也就是等于0，也就是还没有创建过table），言外之意，我们可以在new Hashmap，手动传入initialcapacity,由nitialcapacity来计算oldThr的初始值，我们来看一下Hashmap创建的代码：

 ```java
    public HashMap(int initialCapacity) {
        this(initialCapacity, DEFAULT_LOAD_FACTOR);
    }
```
接着看HashMap(int initialCapacity, float loadFactor):

```java
    public HashMap(int initialCapacity, float loadFactor) {
        if (initialCapacity < 0)
            throw new IllegalArgumentException("Illegal initial capacity: " +
                                               initialCapacity);
        if (initialCapacity > MAXIMUM_CAPACITY)
            initialCapacity = MAXIMUM_CAPACITY;
        if (loadFactor <= 0 || Float.isNaN(loadFactor))
            throw new IllegalArgumentException("Illegal load factor: " +
                                               loadFactor);
        this.loadFactor = loadFactor;
        this.threshold = tableSizeFor(initialCapacity);
    }

 ```

 接着追tableSizeFor方法：

 ```java
     static final int tableSizeFor(int cap) {
        int n = cap - 1;
        n |= n >>> 1;
        n |= n >>> 2;
        n |= n >>> 4;
        n |= n >>> 8;
        n |= n >>> 16;
        return (n < 0) ? 1 : (n >= MAXIMUM_CAPACITY) ? MAXIMUM_CAPACITY : n + 1;
    }

 ```
这就算追到位置了，咋一看挺奇怪的，不知道在干什么，其实试几次随便穿几个cap的值，你就知道了，这里是想把你传进来的值变为2的幂数，例如你传入的cap为127，最后返回值是128，因为这样在计算index值的时候可以利用cap-1来进行将计算值限定在0到cap-1之中。

这一段过去，我们继续往下，如果上面的条件都不成立，我们就通过默认的capacity来创建newCap，并通过默认的loadFactor来计算newThr。

往下做了一下newThr的处理，如果等于0的情况下，为newThr赋值，到此位置，都是在处理newCap和newThr，再往下就是处理将oldTab的数据复制到newTab中，那么我们来看看究竟具体是怎么实现的：

如果oldTab不为null，那么首先根据oldCap进行循环，在循环中判断oldTab[j]不为null，再判断oldTab[j].next是否为null，如果为null的情况，直接赋值newTab[hash & (newCap-1)]，如果不为null，在进行红黑树和链表的判断，伪代码如下：

    if(oldTable != null){
        for(int j=0;j<oldCap;j++){
            Node e = oldTable[j];
            if(e != null){
                if(e.next == null){
                    newTab[hash & (newCap - 1)] = e;
                }else if(e instanceof TreeNode){
                    ((TreeNode<K,V>)e).split(this, newTab, j, oldCap);
                }else{
                    Node loHead,loTail；
                    Node hiHead，hiTail；
                    do{
                        if(hash & oldTab == 0){//index在newTable不变
                         if(loTail == null){
                                loHead = e;
                            }else{
                                loTail.next = e;
                            }
                            loTail = e;
                         }else{
                             if(hiTail == null){
                                 hiHead = e;
                             }else{
                                 hiTail.next = e;
                             }
                             hiTail = e;
                         }
                    }while((e = e.next)!=null);
                    
                    if(loTail != null){
                        loTail.next = null;
                        newTab[j] = loHead;
                    }
                    if(hiTail != null){
                        hiTail.next = null;
                        newTab[j+oldTab] = hiHead;
                    }

                }
            }
        }
    }

这一段只有在链表的处理时有些神奇，那么就让我们来看看这段神奇的代码，这段代码使用了loHead、loTail、hiHead、hiTail四个指针结点，一脸懵逼啊，为什么啊？带着疑问，我们往下看：

我们看到在do-while语句中第一个判断是hash & oldTab，请注意你没有看错，这里不是oldTab-1，而是oldTab，这里使用这个公式的目的是为了计算当前的Node在扩容一倍之后，是否还保持原来的下标，如果hash&oldTab == 0的时候就表示该Node在newTab和oldTab中的index是一样的，如果hash&oldTab ！= 0的时候就表示该Node在newTab中的下标为Node在oldTab中的index加上oldCap，即：

    在newTab中的下标 = 在oldTab中的下标 + oldCap。

回到刚才的那四个指针结点，我为什么叫它们指针的，因为这里是链表的处理么，loHead和loTail处理下标不变的情况，而hiHead和hiTail处理下标变为（oldTab的index+oldCap）的情况，head指向newTab中的Node，而tail指向链表的尾部，最后将head放入newTab中即可。

到这里，我们就差不过总结了一遍Java 8中的HashMap的重要使用原理了。
































