首先理解一下什么是MVC，因为MVP和MVVM都是在MVC的基础上发展而来。

### MVC

视图层(View) 

对应于xml布局文件和java代码动态view部分

控制层(Controller) 

MVC中Android的控制层是由Activity来承担的，Activity本来主要是作为初始化页面，展示数据的操作，但是因为XML视图功能太弱，所以Activity既要负责视图的显示又要加入控制逻辑，承担的功能过多。

模型层(Model) 

针对业务模型，建立的数据结构和相关的类，它主要负责网络请求，数据库处理，I/O的操作。

问题的关键是Activity承载了太多的控制和View的双重功能，所以需要一种新的架构方式来解放Activity,所以Android中的MVC像下图一样：

![mvc](https://github.com/xfmax/android_know/blob/master/Android%E5%BA%94%E7%94%A8%E5%B1%82/%E6%9E%B6%E6%9E%84%E8%AE%BE%E8%AE%A1/image/mvc.jpg)

MVC的问题，衍生出了MVP模式：

![mvp](https://github.com/xfmax/android_know/blob/master/Android%E5%BA%94%E7%94%A8%E5%B1%82/%E6%9E%B6%E6%9E%84%E8%AE%BE%E8%AE%A1/image/mvp.jpg)

MVP在添加了一个presenter,用来和view进行通信，保证model和view层不进行交互，同时解放Activity。

代码结构图：
![mvp](https://github.com/xfmax/android_know/blob/master/Android%E5%BA%94%E7%94%A8%E5%B1%82/%E6%9E%B6%E6%9E%84%E8%AE%BE%E8%AE%A1/image/mvp_stucture.png)

先看BaseView：
```java
public interface BaseView<P extends BasePresenter> {
    void setPresenter(P presenter);
}
```
可以看出BaseView与Presenter关联。

```java
public interface BasePresenter {
    void onDestroy();
}
```
BasePresenter是一个接口，可以设置一些公共的函数。

```java

public class SampleModel {
    public static class UserInfo{
        private String uid;

        public UserInfo(String uid){
            this.uid = uid;
        }

        public String getUid(){
            return uid;
        }
    }
}
```

model就不是一个接口了，而是一个类，我这里使用了静态内部类的方式实现了UserInfo，其实也不一定非要用这种方式。

```java
public class SampleContract {

    public static class Presenter implements BasePresenter{

        public void getUserInfo(String uid, Callback<SampleModel.UserInfo> callback){

            SampleModel.UserInfo userInfo = new SampleModel.UserInfo(uid);
            callback.onCallBack(userInfo);
        }

        @Override
        public void onDestroy() {

        }
    }

    public interface View extends BaseView<Presenter>{
        void setDataToView(SampleModel.UserInfo userInfo);
    }
}
```
这个contract类是google提倡的MVP的写法，优点呢就是将presenter和view合并在一起，这样能让逻辑更加清晰一些。

```java
public interface Callback {
    void onCallBack(SampleModel.UserInfo userInfo);
}
```

```java
public class MainActivity extends AppCompatActivity implements SampleContract.View {
    private SampleContract.Presenter mPresenter;
    private TextView textView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        textView = findViewById(R.id.text);

        setPresenter(new SampleContract.Presenter());

        textView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mPresenter.getUserInfo("9527", new Callback<SampleModel.UserInfo>() {
                    @Override
                    public void onCallBack(SampleModel.UserInfo userInfo) {
                        setDataToView(userInfo);
                    }
                });
            }
        });
    }

    @Override
    public void setDataToView(SampleModel.UserInfo userInfo) {
        textView.setText(userInfo.getUid());
    }

    @Override
    public void setPresenter(SampleContract.Presenter presenter) {
        this.mPresenter = presenter;
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        mPresenter.onDestroy();
    }
}
```
可以看到在onCreate中需要创建Presenter对象，并通过setPresent来让view持有这个Presenter，同时这个Activity实现了view接口。

### MVVM
当业务变得越来越复杂，ui改变很多的情况下，会有非常多的跟ui相关的case，这样view的接口就会很庞大，MVVM就是为了解决这个问题的，通过双向绑定的机制，实现数据和ui内容，只要想改其中一方，另一方都能够及时更新的一种设计理念，省去view层中很多case的情况，只需要改变数据就可以了。

MVVM的核心：

    1.通过VM将[数据模型]传递给[视图模型]，使用的是 数据绑定。
    2.通过VM将[视图模型]传递给[数据模型]，使用的是 DOM事件监听。

