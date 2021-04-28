# Reactor_DNSclient
使用Reactor框架libevent写的DNS客户端

1.	文件夹1：实现一个server和client，保证socket连接无误。
2.	文件夹2：扩展client，增加函数，实现gen_DNSrequest函数将域名打包成DNS请求；decode_DNSResponse函数解析DNS回复。
3.	文件夹3.将DNS代码放入Reactor框架，此处选择libevent框架。

编译运行方法都直接写在了代码注释中。
编写过程请见csdn，知识点包括**Reactor框架**，**socket同步编程**，**抓包解析报文**。


