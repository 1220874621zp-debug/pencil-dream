# libav 公共头文件（ffmpeg 7.1.1）

来源：https://ffmpeg.org/releases/ffmpeg-7.1.1.tar.xz 的
libavcodec / libavformat / libavutil / libswscale 目录下的 `.h` 文件
（`libavutil/avconfig.h` 为按 x86-64 Windows 手写的等价生成物）。
许可证 LGPLv2.1（见 COPYING.LGPLv2.1）。

用途：**仅用于编译期声明**，运行时通过 `util/avruntime` 以 QLibrary
动态加载 avcodec-61 / avformat-61 / avutil-59 / swscale-8.dll
（Qt 6.8 Multimedia 自带同代 DLL，随 windeployqt 部署）。
ABI 钉死 61 代（ffmpeg 7.x）：代数不匹配的 DLL 拒绝加载，勿只改文件名后缀。

改动 ffmpeg 7.x → 8.x 等升级时：整体替换本目录 + 同步 avruntime.cpp
里的 DLL 文件名与函数签名。
