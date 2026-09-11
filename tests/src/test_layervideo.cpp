#include "catch.hpp"
#include "layervideo.h"

#include <QDomDocument>
#include <QtMath>
#include <QDomElement>
#include <QTemporaryFile>

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
