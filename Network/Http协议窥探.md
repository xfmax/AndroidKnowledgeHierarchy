task:表单提交的方式，内部包结构是什么样子的？
task：http的报文结构？

首先，我们来介绍一下http的包结构：

主要分为三个部分：

    1.状态行
    2.请求头
    3.请求体

    <method> <request-URL> <version>
    <headers>

    <entity-body>

[注]：这里headers和entity-body之间有一行空行来进行分割。
由于get请求比较简单，这里我们就以post请求为例来看看post请求的几种方式：

由于协议并没有严格的规定请求体的编码方式，但是一般服务器是根据headers中的content-type字段来获知请求中是用什么方式编码的，所以我们来了解一下都有那些content-type类型：

### applicaiton/x-www-form-urlencoded

浏览器的原生表单，如果不设置enctype字段，那么请求的内容大体如下：

    POST http://www.example.com HTTP/1.1
    Content-Type: application/x-www-form-urlencoded;charset=utf-8

    title=test&sub%5B%5D=1&sub%5B%5D=2&sub%5B%5D=3

此时，提交的数据格式为 key=val1&key=val2    

### mutipart/form-data

这种方式就是表单提交的方式，一般在上传图片的时候又要上传数据就会使用这种方式：

    POST http://www.example.com HTTP/1.1
    Content-Type:multipart/form-data; boundary=----WebKitFormBoundaryrGKCBY7qhFd3TrwA

    ------WebKitFormBoundaryrGKCBY7qhFd3TrwA
    Content-Disposition: form-data; name="text"

    title
    ------WebKitFormBoundaryrGKCBY7qhFd3TrwA
    Content-Disposition: form-data; name="file"; filename="chrome.png"
    Content-Type: image/png

    PNG ... content of chrome.png ...
    ------WebKitFormBoundaryrGKCBY7qhFd3TrwA--

可以看到每一部分都有一个boundary的分隔符来分割不同的字段，最后呢，使用--boundary--来作为结尾，如果输入的是文件还需要输入文件名和文件类型信息，可以看到状态行和header之间是没有分割的，而header和content之间是需要分割的。

上面提到的都是原生浏览器支持的post提交数据的方式，还要一种跟json有关的编码方式：

### application/json

先看发送格式：

    POST http://www.example.com HTTP/1.1 
    Content-Type: application/json;charset=utf-8

    {"title":"test","sub":[1,2,3]}

很简单，entity-content里面直接就是一个json格式的字符串，特别适合restful接口。

### text/xml

    POST http://www.example.com HTTP/1.1 
    Content-Type: text/xml

    <?xml version="1.0"?>
    <methodCall>
     <methodName>examples.getStateName</methodName>
        <params>
         <param>
              <value><i4>41</i4></value>
         </param>
     </params>
    </methodCall>

一种传输xml的数据格式，个人认为一般还是采用json的方式，xml的方式还是有些臃肿。    


####POST和GET
HTTP一共有８种请求，其中比较重要的就是POST和GET，其余的还有HEAD、PUT.
1.GET请求可以被缓存起来，收藏为书签，但是POST不行。
2.GET可以被保存到历史记录中，但是POST不会。
3.GET请求的长度有限制（根据浏览器的不同而不同，大约几kb），POST无限制
4.GET请求的参数在URL连接上，相对不安全，POST请求参数写在HTTP的请求头中，相对安全。

###首部(共74个，不能一一列出，只挑选相对重要一些的)
#### 通用首部
Cache-control：
     
     no-cache：强制向源服务器再次验证。
     no-store： 不缓存请求或响应的内容。      
Connection:

    1.控制不再转发给代理的首部字段（Connection：不再转发的首部字段名）。
    2.管理持久连接（Connection：close 关闭连接  Connection：Keep-Alive ）。 
    【注】：HTTP/1.1默认连接是持久连接，对于1.1之前的版本需要使用Connection：Keep-Alive来保持持久连接。
Upgrade:

     除了使用HTTP协议外，还能用此字段进行扩展协议
Via:
    
    为了追踪client和server之间的请求和响应的路径。
    例：Via：1.0 gw.hackr.jp     1.0表示http的版本，gw.hackr.jp表示当前的代理服务器信息。
    
Date:

    Date: Tue01 Jul 2012 04:40:59 GMT

Warning:

#### 请求首部
accept:
用于指定客户端接受那些请求的类型。
例：Accept：text/html，表明客户端希望接受html文本。

accept-encoding:
用于指定客户端可接受的编码内容。

accept-language:
用于指定客户端可接受的自然语言。

accept-charset:
用于指定客户端可接受的字符集。

Authorization:
用于证明客户端有权限查看某些资源。

Host:
用于指定被请求资源的Internet主机和端口号。

If-Match:
告诉服务器匹配资源所用的实体标记值（ETag）。

If-Range：
告诉服务器若指定的If-Range字段值和请求资源的ETag值或时间相一致时，则作为范围请求处理。

Max-Forwards:
通过TRACE方法或OPTIONS方法。

User-Agent:
它的操作系统、浏览器和其它属性告诉服务器。

#### 响应首部
Accept-Ranges:
用来告知客户端服务器是否能处理范围请求，以指定获取服务器端某个部分的资源。

Age:
告知客户端，源服务器在多久前创建了响应。字段值的单位为秒。

ETag:
能告知客户端实体标识，一种可将资源以字符串形式做唯一标识的方式，服务器会为每份资源分配对应的ETag值。

强ETag值和弱ETag值
强ETag值，不论实体发生多么细微的变化都会改变其值。
弱ETag值，只用于提示资源是否相同，只有资源发生了根本改变，产生差异时才会改变ETag值。这时会在字段值最开始处附加W/，ETag：W/"usagi-1234"

Location:
使用首部字段Location可以将响应接收方引导至某个与请求URI位置不同的资源。

Retry-After:
告知客户端应该在多久之后再次发送请求。

Server:
告知客户端当前服务器上安装的HTTP服务器应用程序的信息。
例子：Server：Apache/2.2.6（Unix）PHP/5.2.5

Vary:
对缓存进行控制，源服务器会向代理服务器传达关于本地缓存使用方法的命令。

WWW-Authenticate:
首部字段WWW-Authenticateu用于HTTP访问认证。告知客户端适用于访问请求URI所指定资源的认证方案和带参数提示的质询。

####实体首部
Allow:
告知客户端能够支持Request-URI指定资源的所有HTTP方法。

Content-Encoding:
通知客户端服务器对实体的主体部分选用的内容编码方式（gzip、compress、deflate、identity）。

Content-Language:
通知客户端服务器对实体的主体部分使用的自然语言。

Content-Location:
Content-Location:http://www.baidu.com
表示报文主体返回资源对应的URI。

Content-MD5:
客户端会对接收的报文主体执行相同的MD5算法，然后与首部字段Content-MD5的字段值比较。

Content-Range:
返回响应时使用的首部字段Content-Range，能告知客户端作为响应返回的实体的哪个部分符合范围请求。字段值以字节为单位，表达当前发送部分及整个实体大小。

Content-Type:
说明了实体主体内对象的媒体类型。

Expires:
将资源失效的日期告知客户端。

Last-Modified:
指明资源最终修改的时间。

####其他
Cookie：服务器接收到的Cookie信息
例子：
set-Coolie:开始状态管理所使用的Cookie信息
例子：set-Cookie:status=enable;expires=Tue,05 Jul 2016 08:00:00 GMT;path=/;domain=www.baidu.com
###加密机制

#### HTTP code
1XX:请求已被接收，需要继续处理。
2XX:请求已被接收，且处理完成。
3XX:重定向。
4XX:客户端请求错误。
5XX:服务器错误。

###HTTP与HTTPS
HTTP协议：使用HTTP协议时，客户端与服务端的80端口建立一个TCP连接，然后就在这个连接的基础上进行请求和应答，以及数据的交换。
[注]：HTTP1.0和HTTP1.1的区别是：1.0每次请求都要建立一个新的TCP，1.1可以运行在一个连接上发送多次命令和应答，提高效率。

HTTPS的优势：加密+认证+完整性保证。
HTTPS的劣势：

    1.加密需要占用大量cpu和内存
    2.多了一层SSL和TLS通信层，必然拖慢速度。

#### Cookie和Session
作用：因为HTTP是一种无状态的连接，所以无法记录上次传输的数据。
1.Cookie保存客户端，Session保存在服务器。
2.Cookie相对来说不安全，浏览器可以分析本地的Cookie，进行Cookie欺骗。
3.Session可以设置超时时间，超过这个时间就会失效，以免长期占用服务器内存。
4.单个Cookie大小限制（4kb），每个站点的Cookie数量一般也有限制（20个）。
5.客户端每次都会把Cookie发送到服务器，因此服务器可以知道Cookie，但是客户端不知道Session。

服务器收到Cookie后，会根据Cookie中的SessionID来找到客户的Session，如果没有，会生成一个新的SessionID随Cookie发送给客户端。