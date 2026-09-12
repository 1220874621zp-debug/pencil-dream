/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#ifndef AVRUNTIME_H
#define AVRUNTIME_H

#include <QString>

// ffmpeg 7.x 公共头(ABI 61 代),见 core_lib/src/external/libav/README.md。
// MSVC 对第三方头告警较多,统一在引入处压掉。
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4100 4180 4242 4244 4245 4267 4365 4456 4458 4459 4702 4706 4800 4996 5054)
#endif

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

// ffmpeg 共享库运行时加载器(参考 friction 的 VideoFrameLoader 架构,friction 为静态链接,
// pencil 保持"不链 ffmpeg"的传统,改为 QLibrary 动态加载,部署面与 Qt Multimedia 完全一致)。
//
// ABI 钉死 61 代 DLL(avcodec-61/avformat-61/avutil-59/swscale-8,Qt 6.8 Multimedia
// 自带同代,windeployqt 会复制到 exe 旁);头文件代数必须与 DLL 代数一致,否则结构体
// 布局错位=未定义行为,故不做多代探测,找不到就明确报缺。
//
// 线程安全:首次调用发生在 GUI 线程(建层时),之后函数局部静态只读;
// 解码工作线程按值使用该结构体是安全的。
struct AvRuntime
{
    bool ok = false;
    QString loadedFromDir; // 成功加载的目录(诊断用)
    QString errorMessage;  // 加载失败原因(占位提示用)

    // libavutil
    AVFrame* (*frame_alloc)() = nullptr;
    void (*frame_free)(AVFrame**) = nullptr;
    void (*frame_unref)(AVFrame*) = nullptr;
    AVPacket* (*packet_alloc)() = nullptr;
    void (*packet_free)(AVPacket**) = nullptr;
    void (*packet_unref)(AVPacket*) = nullptr;
    int64_t (*rescale_q)(int64_t, AVRational, AVRational) = nullptr;
    int (*strerror)(int, char*, size_t) = nullptr;

    // libavformat
    AVFormatContext* (*format_alloc_context)() = nullptr;
    int (*format_open_input)(AVFormatContext**, const char*, const AVInputFormat*, AVDictionary**) = nullptr;
    int (*format_find_stream_info)(AVFormatContext*, AVDictionary**) = nullptr;
    void (*format_close_input)(AVFormatContext**) = nullptr;
    int (*format_seek_file)(AVFormatContext*, int, int64_t, int64_t, int64_t, int) = nullptr;
    int (*read_frame)(AVFormatContext*, AVPacket*) = nullptr;
    int (*find_best_stream)(AVFormatContext*, AVMediaType, int, int, const AVCodec**, int) = nullptr;

    // libavcodec
    const AVCodec* (*find_decoder)(AVCodecID) = nullptr;
    AVCodecContext* (*codec_alloc_context3)(const AVCodec*) = nullptr;
    int (*codec_parameters_to_context)(AVCodecContext*, const AVCodecParameters*) = nullptr;
    int (*codec_open2)(AVCodecContext*, const AVCodec*, AVDictionary**) = nullptr;
    void (*codec_free_context)(AVCodecContext**) = nullptr;
    int (*codec_send_packet)(AVCodecContext*, const AVPacket*) = nullptr;
    int (*codec_receive_frame)(AVCodecContext*, AVFrame*) = nullptr;
    void (*codec_flush_buffers)(AVCodecContext*) = nullptr;

    // libswscale
    SwsContext* (*sws_getContext)(int, int, AVPixelFormat, int, int, AVPixelFormat, int,
                                  SwsFilter*, SwsFilter*, const double*) = nullptr;
    int (*sws_scale)(SwsContext*, const uint8_t* const*, const int*, int, int,
                     uint8_t* const*, const int*) = nullptr;
    void (*sws_freeContext)(SwsContext*) = nullptr;
};

// 进程级单例;失败时 ok=false 且 errorMessage 可用于界面提示。
const AvRuntime& avRuntime();

#endif // AVRUNTIME_H
