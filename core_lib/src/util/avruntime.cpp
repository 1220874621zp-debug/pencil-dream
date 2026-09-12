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

#include "avruntime.h"

#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QLibraryInfo>
#include <QSettings>
#include <memory>

#include "pencildef.h"

namespace
{
    // 61 代固定文件名(与 vendored 头 7.1.1 一一对应,勿混代;
    // 升级 ffmpeg 大版本须整体换头文件并同步这里)
#ifdef Q_OS_WIN
    const char* const kAvUtilFile = "avutil-59.dll";
    const char* const kSwscaleFile = "swscale-8.dll";
    const char* const kAvCodecFile = "avcodec-61.dll";
    const char* const kAvFormatFile = "avformat-61.dll";
#else
    const char* const kAvUtilFile = "libavutil.so.59";
    const char* const kSwscaleFile = "libswscale.so.8";
    const char* const kAvCodecFile = "libavcodec.so.61";
    const char* const kAvFormatFile = "libavformat.so.61";
#endif

    // 用户在首选项里配置的 ffmpeg.exe 所在目录(shared 发行版与 av*.dll 同目录)。
    // 与 util.cpp 的 customFFmpegPath 同源:直接读 QSettings。
    QString customFfmpegDir()
    {
        QSettings settings(PENCIL2D, PENCIL2D);
        const QString path = settings.value(SETTING_FFMPEG_PATH).toString();
        if (!path.isEmpty() && QFileInfo::exists(path))
        {
            return QFileInfo(path).absolutePath();
        }
        return QString();
    }

    QStringList candidateDirs()
    {
        QStringList dirs;
        const QString custom = customFfmpegDir();
        if (!custom.isEmpty()) { dirs << custom; }
        dirs << QCoreApplication::applicationDirPath();
        dirs << QCoreApplication::applicationDirPath() + "/plugins";
        // 部署态:BinariesPath 即 exe 旁;开发态:Qt 安装 bin。
        // 注意若 exe 旁放了部署版 Qt6Core.dll,前缀会重定向到本目录,
        // 此时靠下面的 PATH 扫描兜底命中 Qt 安装目录
        dirs << QLibraryInfo::path(QLibraryInfo::BinariesPath);
        // 显式扫描 PATH 各目录(裸名 LoadLibrary 在部分进程语境下不吃 PATH,
        // 必须 QFile 探测出绝对路径再加载)
        const QString pathEnv = qEnvironmentVariable("PATH");
        for (const QString& entry : pathEnv.split(';', Qt::SkipEmptyParts))
        {
            const QString clean = QDir::fromNativeSeparators(entry.trimmed());
            if (!clean.isEmpty() && !dirs.contains(clean))
            {
                dirs << clean;
            }
        }
        // 空目录 = 让系统加载器走默认搜索(最后兜底)
        dirs << QString();
        return dirs;
    }

    QLibrary* tryLoad(const QString& dir, const char* const file)
    {
        const QString candidate = dir.isEmpty()
            ? QString::fromLatin1(file)
            : QDir(dir).filePath(QString::fromLatin1(file));
        std::unique_ptr<QLibrary> lib = std::make_unique<QLibrary>(candidate);
        if (!lib->load())
        {
            return nullptr;
        }
        return lib.release();
    }

    // 依赖顺序:avutil 最先(被其余三者依赖),avformat 依赖 avcodec。
    // 依赖已进进程后,后加载 DLL 的依赖解析直接复用内存模块,目录一致性有保障。
    bool tryLoadDir(AvRuntime& rt, const QString& dir, QLibrary** outLibs)
    {
        outLibs[0] = tryLoad(dir, kAvUtilFile);
        outLibs[1] = tryLoad(dir, kSwscaleFile);
        outLibs[2] = tryLoad(dir, kAvCodecFile);
        outLibs[3] = tryLoad(dir, kAvFormatFile);
        if (!outLibs[0] || !outLibs[1] || !outLibs[2] || !outLibs[3])
        {
            return false;
        }
        QLibrary* const util = outLibs[0];
        QLibrary* const sws = outLibs[1];
        QLibrary* const codec = outLibs[2];
        QLibrary* const fmt = outLibs[3];

#define AV_RESOLVE(member, lib, symbol) \
        rt.member = reinterpret_cast<decltype(rt.member)>((lib)->resolve(symbol)); \
        if (rt.member == nullptr) { return false; }

        AV_RESOLVE(frame_alloc,               util,  "av_frame_alloc")
        AV_RESOLVE(frame_free,                util,  "av_frame_free")
        AV_RESOLVE(frame_unref,               util,  "av_frame_unref")
        // packet 三件在 libavcodec 导出(历史归属,勿挂到 avutil)
        AV_RESOLVE(packet_alloc,              codec, "av_packet_alloc")
        AV_RESOLVE(packet_free,               codec, "av_packet_free")
        AV_RESOLVE(packet_unref,              codec, "av_packet_unref")
        AV_RESOLVE(rescale_q,                 util,  "av_rescale_q")
        AV_RESOLVE(strerror,                  util,  "av_strerror")

        AV_RESOLVE(format_alloc_context,      fmt,   "avformat_alloc_context")
        AV_RESOLVE(format_open_input,         fmt,   "avformat_open_input")
        AV_RESOLVE(format_find_stream_info,   fmt,   "avformat_find_stream_info")
        AV_RESOLVE(format_close_input,        fmt,   "avformat_close_input")
        AV_RESOLVE(format_seek_file,          fmt,   "avformat_seek_file")
        AV_RESOLVE(read_frame,                fmt,   "av_read_frame")
        AV_RESOLVE(find_best_stream,          fmt,   "av_find_best_stream")

        AV_RESOLVE(find_decoder,              codec, "avcodec_find_decoder")
        AV_RESOLVE(codec_alloc_context3,      codec, "avcodec_alloc_context3")
        AV_RESOLVE(codec_parameters_to_context, codec, "avcodec_parameters_to_context")
        AV_RESOLVE(codec_open2,               codec, "avcodec_open2")
        AV_RESOLVE(codec_free_context,        codec, "avcodec_free_context")
        AV_RESOLVE(codec_send_packet,         codec, "avcodec_send_packet")
        AV_RESOLVE(codec_receive_frame,       codec, "avcodec_receive_frame")
        AV_RESOLVE(codec_flush_buffers,       codec, "avcodec_flush_buffers")

        AV_RESOLVE(sws_getContext,            sws,   "sws_getContext")
        AV_RESOLVE(sws_scale,                 sws,   "sws_scale")
        AV_RESOLVE(sws_freeContext,           sws,   "sws_freeContext")

#undef AV_RESOLVE
        return true;
    }

    AvRuntime loadOnce()
    {
        AvRuntime rt;
        QLibrary* libs[4] = { nullptr, nullptr, nullptr, nullptr };
        QStringList failures;
        for (const QString& dir : candidateDirs())
        {
            QLibrary* attempt[4] = { nullptr, nullptr, nullptr, nullptr };
            if (tryLoadDir(rt, dir, attempt))
            {
                for (int i = 0; i < 4; ++i) { libs[i] = attempt[i]; }
                rt.ok = true;
                rt.loadedFromDir = dir.isEmpty() ? QStringLiteral("(系统搜索路径)") : dir;
                break;
            }
            if (attempt[0] != nullptr)
            {
                // 已加载但符号解析失败:记录具体缺哪个
                failures << QStringLiteral("%1: 符号缺失").arg(dir);
            }
            for (QLibrary* lib : attempt) { delete lib; }
        }
        if (!rt.ok)
        {
            rt.errorMessage = QStringLiteral("未找到 ffmpeg 61 代运行库(avcodec-61/avformat-61/avutil-59/swscale-8.dll)。"
                                             "Qt Multimedia 部署包应自带;也可在首选项里指定 ffmpeg shared 版目录。");
            if (!failures.isEmpty())
            {
                rt.errorMessage += QStringLiteral(" 诊断:") + failures.join(';');
            }
            qCritical() << "[视频]" << rt.errorMessage;
        }
        else
        {
            // QLibrary 对象与进程同寿命:卸载 ffmpeg DLL 在有线程仍持有上下文时是未定义行为
            static QLibrary* sKeepAlive[4] = { nullptr, nullptr, nullptr, nullptr };
            for (int i = 0; i < 4; ++i) { sKeepAlive[i] = libs[i]; }
        }
        return rt;
    }
}

const AvRuntime& avRuntime()
{
    static const AvRuntime sRuntime = loadOnce();
    return sRuntime;
}
