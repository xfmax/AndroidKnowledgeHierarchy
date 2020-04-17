task1：https和http的异同？
task2：https握手过程？
task3：HttpUrlConnection是如何实现https的？以及优化dns域名解析

https在http的基础上，在传输层（Tcp）上面建立了一个SSL/TLS安全协议层，加密数据包。

### 握手过程

    1.client-hello：浏览器完成域名解析后，获得ip地址，然后与host：443尝试连接，浏览器会将“支持的加密组件/尝试连接到host头”，并附上一份随机生成的session ticket1。

    2.server-hello：服务器会收到TLS的连接请求，存储浏览器session ticket1，根据发送来的host来寻找服务器证书，然后将服务器的证书和加密套件以及一份随机生成的session ticket2发送给浏览器。

    3.cipher-spec：浏览器收到服务器的证书后，进行证书校验，分为如下几步：
        验证证书有效期
        验证证书的域名和浏览器输入的是否相同
        验证证书的吊销状态
        验证证书的颁发机构
      以上步骤需要全部通过，否则会显示警告。  
      检查通过会随机生成一个session ticket3，并使用证书中的公钥进行加密，并发送给服务器，服务器端会通过私钥解密出session ticket3，然后通过session ticket1（浏览器）+session ticket2（服务器）+session ticket3（浏览器）组合成session ticket。

如图：
![https](./image/https.jpg)


### httpurlConnection如何支持https，并优化DNS

一、IP直连

阿里云出品的HttpDNS

实现原理：将域名发送给httpdns服务器，服务器返回域名对应的IP地址，然后客户端通过IP地址直接进行网络访问。

```java
public static HttpURLConnection getHttpURLConnection(String urlString)
        throws IOException {
    URL url = new URL(urlString);
    String originHost = url.getHost();
    HttpURLConnection conn;
 
    String dstIp = httpdnsService.getIpByHost(url.getHost()); //这里得到了IP地址
    if (dstIp != null) {
        Log.d("HttpDNS Demo", "Get IP from HttpDNS, " + url.getHost() + ": " + dstIp);
        urlString = urlString.replaceFirst(url.getHost(), dstIp);
        url = new URL(urlString);
        conn = (HttpURLConnection) url.openConnection();
        // 设置HTTP请求头Host域
        conn.setRequestProperty("Host", originHost); // 这里是设置head中的host
        return conn;
    } else {
        Log.d("HttpDNS Demo", "Degrade to local DNS.");
        return (HttpURLConnection) url.openConnection();
    }
}
```

https://mp.weixin.qq.com/s?__biz=MzA3ODgyNzcwMw==&mid=201837080&idx=1&sn=b2a152b84df1c7dbd294ea66037cf262&scene=2&from=timeline&isappinstalled=0&utm_source=tuicool