#include "catch.hpp"
#include "layervideo.h"

#include <QCoreApplication>
#include <iostream>
#include <QDebug>
#include <QDomDocument>
#include <QtMath>
#include <QDomElement>
#include <QEventLoop>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QThread>

TEST_CASE("LayerVideo type and source", "[LayerVideo]")
{
    LayerVideo layer(7);
    REQUIRE(layer.type() == Layer::MOVIE);

    SECTION("source sets a single clip with explicit length")
    {
        // 不存在的文件:不建解码器,但 clip 占位照常建立
            layer.setVideoSource("C:/no_such_video_ref.mp4", 29.97, 300, 5);
            REQUIRE(layer.keyFrameCount() == 1);
        KeyFrame* clip = layer.getKeyFrameAt(5);
        REQUIRE(clip != nullptr);
        REQUIRE(clip->length() == 300);
        REQUIRE(clip->isLengthExplicit());
        REQUIRE(layer.videoPath() == QString("C:/no_such_video_ref.mp4"));
        REQUIRE(layer.isFileMissing());

        // 换源:旧 clip 被替换,入点更新
        layer.setVideoSource("C:/no_such_other_ref.mp4", 24.0, 48, 10);
        REQUIRE(layer.keyFrameCount() == 1);
        REQUIRE(layer.getKeyFrameAt(10) != nullptr);
        REQUIRE(layer.getKeyFrameAt(5) == nullptr);
    }

    SECTION("clip clone keeps position and length")
    {
        layer.setVideoSource("C:/no_such_video_ref.mp4", 24.0, 100, 3);
        KeyFrame* cloned = layer.getKeyFrameAt(3)->clone();
        REQUIRE(cloned != nullptr);
        REQUIRE(cloned->pos() == 3);
        REQUIRE(cloned->length() == 100);
        delete cloned;
    }
}

TEST_CASE("LayerVideo XML round trip", "[LayerVideo]")
{
    LayerVideo src(1);
    src.setName("my ref");
    // 缺文件路径:确保测试不触碰解码器
    src.setVideoSource("C:/no_such_shot010_ref.mp4", 29.97, 450, 12);

    QDomDocument doc;
    QDomElement elem = src.createDomElement(doc);

    LayerVideo dst(2);
    dst.loadDomElement(elem, QString(), nullptr);

    REQUIRE(dst.videoPath() == QString("C:/no_such_shot010_ref.mp4"));
    REQUIRE(qAbs(dst.videoFps() - 29.97) < 0.001);
    REQUIRE(dst.keyFrameCount() == 1);
    KeyFrame* clip = dst.getKeyFrameAt(12);
    REQUIRE(clip != nullptr);
    REQUIRE(clip->length() == 450);
}

TEST_CASE("LayerVideo frame index mapping", "[LayerVideo]")
{
    // fps 一致=恒等;不一致按比例换算(工程 12fps 放视频 24fps → 两帧取一)
    REQUIRE(LayerVideo::videoFrameIndexForRel(10, 24.0, 24.0) == 10);
    REQUIRE(LayerVideo::videoFrameIndexForRel(10, 24.0, 12.0) == 20);
    REQUIRE(LayerVideo::videoFrameIndexForRel(10, 12.0, 24.0) == 5);
    // 非法 fps 退化恒等
    REQUIRE(LayerVideo::videoFrameIndexForRel(7, 24.0, 0.0) == 7);
}

TEST_CASE("LayerVideo decode pipeline end to end", "[LayerVideo][video]")
{
    // 依赖运行库与生成器:任一缺失则跳过(环境完备时才验证真实解码链)。
    // 注意不直接包含 avruntime.h:ffmpeg 公共头与 catch.hpp 同单元冲突(启动期 fail-fast),
    // 一律经 LayerVideo 的封装接口探测。
    LayerVideo probe(2);
    // 环境依赖只在本机/分发包存在(CI 无 plugins/ffmpeg.exe):真跳过而非失败
    if (!probe.isDecoderAvailable())
    {
        std::cout << "[LayerVideo] skip: ffmpeg DLLs unavailable: "
                  << probe.decoderHint().toStdString() << std::endl;
        SUCCEED("skipped: decoder unavailable");
        return;
    }
    const QString ffmpeg = QCoreApplication::applicationDirPath() + "/plugins/ffmpeg.exe";
    if (!QFileInfo::exists(ffmpeg))
    {
        std::cout << "[LayerVideo] skip: plugins/ffmpeg.exe not available" << std::endl;
        SUCCEED("skipped: plugins/ffmpeg.exe not available");
        return;
    }

    // 生成 1 秒 12fps 测试视频(彩色测试图,含帧间变化)
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString videoPath = tempDir.filePath("ref.mp4");
    QProcess ffmpegGen;
    ffmpegGen.start(ffmpeg, { "-y", "-loglevel", "error",
                              "-f", "lavfi", "-i", "testsrc2=duration=1:size=320x240:rate=12",
                              "-pix_fmt", "yuv420p", "-c:v", "libx264", videoPath });
    REQUIRE(ffmpegGen.waitForStarted(5000));
    REQUIRE(ffmpegGen.waitForFinished(30000));
    REQUIRE(QFileInfo::exists(videoPath));

    LayerVideo layer(1);
    layer.setVideoSource(videoPath, 12.0, 12, 1);

    // 泵事件循环等 openFinished(onDecoderOpened 内部会自动请求第一帧)
    bool opened = false;
    for (int i = 0; i < 300 && !opened; ++i)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(10); // processEvents 无事件时立即返回,必须真睡等 worker 线程
        if (!layer.decoderHint().isEmpty() && layer.currentFrameImage().isNull())
        {
            // 打开失败(给出错误)且无帧:提前失败
            break;
        }
        opened = !layer.currentFrameImage().isNull();
    }
    INFO("decoderHint: " << layer.decoderHint().toStdString());
    REQUIRE(opened);
    REQUIRE(layer.currentFrameImage().width() == 320);
    REQUIRE(layer.currentFrameImage().height() == 240);

    // 跳到中段帧(scrub 语义):按需解后应能取到新画面
    layer.syncToFrame(9, 12.0, false);
    for (int i = 0; i < 300; ++i)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(10);
        if (!layer.currentFrameImage().isNull()) { break; }
    }
    REQUIRE(!layer.currentFrameImage().isNull());
}

