# 基于 RTL8722DM MINI 的云语音识别

本项目旨在实现基于 Realtek RTL8722DM MINI 的语音识别，主要工作在于拓展官方网路库 HTTPClient，将音频数据上传至 HTTP 服务器。

项目文章：https://hazhuzhu.com/mcu/rtl8722dm-mini-speech_recognizer.html

演示视频：https://mbb.eet-china.com/forum/topic/113070_1_1.html

参考资料：[RTL8722DM MINI 文档](https://www.amebaiot.com.cn/cn/ameba-arduino-summary/)；[RTL8722DM MINI ARDUINO 示例程序](https://www.amebaiot.com.cn/cn/amebad-rtl8722dm-mini-arduino-peripherals-examples/)

主要依赖库：

1. HttpClient：支持http协议一些比较基本的操作。
2. FatFs_SD：读写 Fat 文件格式的 SD 卡。
3. RecordWav：能直接录制 wav 格式音频到 SD 卡上，也可以播放。

工作流程：

1. 按键检测，利用 RecordWav 录制音频到 SD 卡。
2. 利用 FatFs_SD 读取刚录制的音频。
3. 利用 HttpClient 将音频 POST 到服务器，服务器调用语音识别 api 后将识别结果返回给 Ameba。最后返回流程1。

目录结构：

```
├─device	            // MINI Arduino 代码
│  ├─FatfsSDIO          // 对 FatFs_SD 库的修改
│  ├─Http               // 对 HttpClient 库的修改
│  └─speech_recognizer  // 主程序
└─server                // 服务器端代码
```

