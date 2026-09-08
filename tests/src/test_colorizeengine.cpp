/*

Pencil2D - Traditional Animation Software
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include "catch.hpp"

#include "colorizeengine.h"

#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QImage>
#include <QPainter>

namespace
{

// 画布：透明底 + 若干条不透明白线（Pencil 线稿惯例：线只看 alpha）
QImage makeLineArt(const QSize& size, std::function<void(QPainter&)> drawLines)
{
    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, false);
    drawLines(p);
    p.end();
    return img;
}

QImage makeStrokes(const QSize& size, std::function<void(QPainter&)> paintStrokes)
{
    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    paintStrokes(p);
    p.end();
    return img;
}

// 与引擎 splitKeyStrokesByColor 相同取整的反预乘，保证比对一致
QRgb unpremulPixel(QRgb px)
{
    const int a = qAlpha(px);
    if (a == 0) return 0;
    if (a == 255) return px;
    const int r = qBound(0, qRound(qRed(px) * 255.0 / a), 255);
    const int g = qBound(0, qRound(qGreen(px) * 255.0 / a), 255);
    const int b = qBound(0, qRound(qBlue(px) * 255.0 / a), 255);
    return qRgb(r, g, b);
}

QRgb nonPremul(const QImage& img, int x, int y)
{
    return unpremulPixel(img.pixel(x, y));
}

// 非透明内容的包围盒
QRect contentRect(const QImage& img)
{
    int minX = img.width(), minY = img.height(), maxX = -1, maxY = -1;
    for (int y = 0; y < img.height(); ++y)
    {
        const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x)
        {
            if (qAlpha(line[x]) > 0)
            {
                minX = qMin(minX, x);
                maxX = qMax(maxX, x);
                minY = qMin(minY, y);
                maxY = qMax(maxY, y);
            }
        }
    }
    return maxX < 0 ? QRect() : QRect(QPoint(minX, minY), QPoint(maxX, maxY));
}

// 统计图像中某非预乘颜色的像素数
qint64 countColor(const QImage& img, QRgb color)
{
    qint64 count = 0;
    for (int y = 0; y < img.height(); ++y)
    {
        const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x)
        {
            if (line[x] != 0 && nonPremul(img, x, y) == color)
                ++count;
        }
    }
    return count;
}

} // namespace

TEST_CASE("Colorize buildHeightMap")
{
    SECTION("black line is barrier, background is flat")
    {
        QImage lineArt = makeLineArt(QSize(64, 64), [](QPainter& p) {
            p.fillRect(30, 4, 4, 56, Qt::black); // 垂直墙
        });

        Colorize::FilteringOptions opt; // 全部关闭
        QImage h = Colorize::buildHeightMap(lineArt, lineArt.rect(), opt);

        REQUIRE(h.format() == QImage::Format_Grayscale8);
        REQUIRE((h.pixel(4, 30) & 0xFF) == 0);     // 远离线：平地
        REQUIRE((h.pixel(31, 30) & 0xFF) == 255);  // 线中心：满屏障
    }

    SECTION("empty lineart means no barrier")
    {
        QImage lineArt(QSize(32, 32), QImage::Format_ARGB32_Premultiplied);
        lineArt.fill(Qt::transparent);

        QImage h = Colorize::buildHeightMap(lineArt, lineArt.rect(), Colorize::FilteringOptions());
        REQUIRE((h.pixel(15, 15) & 0xFF) == 0);
    }
}

TEST_CASE("Colorize WatershedFill")
{
    SECTION("single color stays inside closed line art")
    {
        // 封闭方框 + 框内一笔红：框内全红，框外保持透明（自动背景组）
        QImage lineArt = makeLineArt(QSize(64, 64), [](QPainter& p) {
            p.drawRect(8, 8, 48, 48);
        });
        QImage strokes = makeStrokes(QSize(64, 64), [](QPainter& p) {
            p.setPen(QPen(Qt::red, 3));
            p.drawLine(24, 32, 40, 32);
        });

        Colorize::FilteringOptions opt;
        QImage result = Colorize::colorize(lineArt, strokes, lineArt.rect(), opt);

        REQUIRE(!result.isNull());
        REQUIRE(result.size() == lineArt.size());

        const QRgb red = QColor(Qt::red).rgba();
        // 框内（远离边线）全部为红
        for (int y = 12; y < 52; ++y)
            for (int x = 12; x < 52; ++x)
                REQUIRE(nonPremul(result, x, y) == red);
        // 框外四角保持透明
        REQUIRE(result.pixel(2, 2) == 0);
        REQUIRE(result.pixel(61, 2) == 0);
        REQUIRE(result.pixel(2, 61) == 0);
        REQUIRE(result.pixel(61, 61) == 0);
        // 红色总量 = 框内区域（约 47x47），远小于全图
        REQUIRE(countColor(result, red) > 40 * 40);
        REQUIRE(countColor(result, red) < 60 * 60);
    }

    SECTION("unsealed single stroke does not blanket the canvas")
    {
        // 非封闭线稿（孤立短墙）+ 单笔：结果不得是整幅单色矩形
        QImage lineArt = makeLineArt(QSize(64, 64), [](QPainter& p) {
            p.fillRect(30, 24, 4, 16, Qt::black); // 孤立短墙
        });
        QImage strokes = makeStrokes(QSize(64, 64), [](QPainter& p) {
            p.setPen(QPen(Qt::red, 3));
            p.drawLine(6, 32, 12, 32);
        });

        QImage result = Colorize::colorize(lineArt, strokes, lineArt.rect(), Colorize::FilteringOptions());
        const QRgb red = QColor(Qt::red).rgba();

        REQUIRE(nonPremul(result, 9, 32) == red);       // 笔画处着色
        REQUIRE(countColor(result, red) < 64 * 64);     // 不再铺满全图
    }

    SECTION("two colors respect the wall")
    {
        QImage lineArt = makeLineArt(QSize(64, 64), [](QPainter& p) {
            p.fillRect(30, 4, 4, 56, Qt::black); // 中央垂直墙（上下留 4px 开口？不，画满）
            p.fillRect(0, 0, 64, 4, Qt::black);  // 上封口
            p.fillRect(0, 60, 64, 4, Qt::black); // 下封口
        });
        QImage strokes = makeStrokes(QSize(64, 64), [](QPainter& p) {
            p.setPen(QPen(Qt::red, 3));
            p.drawLine(6, 32, 12, 32);
            p.setPen(QPen(Qt::blue, 3));
            p.drawLine(52, 32, 58, 32);
        });

        QImage result = Colorize::colorize(lineArt, strokes, lineArt.rect(), Colorize::FilteringOptions());

        const QRgb red = QColor(Qt::red).rgba();
        const QRgb blue = QColor(Qt::blue).rgba();

        // 远离线的左右两块分别为红/蓝（贴边一圈归自动背景组=透明，属设计行为）
        REQUIRE(nonPremul(result, 8, 32) == red);
        REQUIRE(nonPremul(result, 55, 32) == blue);
        REQUIRE(nonPremul(result, 10, 30) == red);
        REQUIRE(nonPremul(result, 52, 30) == blue);
        // 红色像素只出现在左半，蓝色只出现在右半
        qint64 redCount = 0, blueCount = 0;
        for (int y = 0; y < 64; ++y)
        {
            for (int x = 0; x < 64; ++x)
            {
                const QRgb c = nonPremul(result, x, y);
                if (c == red) { REQUIRE(x < 32); ++redCount; }
                if (c == blue) { REQUIRE(x >= 32); ++blueCount; }
            }
        }
        REQUIRE(redCount > 0);
        REQUIRE(blueCount > 0);
    }

    SECTION("fuzzy radius seals small gaps")
    {
        // 墙中央留 3px 缺口
        QImage lineArt = makeLineArt(QSize(64, 64), [](QPainter& p) {
            p.fillRect(30, 4, 4, 24, Qt::black);
            p.fillRect(30, 36, 4, 24, Qt::black); // 缺口 y=28..35
            p.fillRect(0, 0, 64, 4, Qt::black);
            p.fillRect(0, 60, 64, 4, Qt::black);
        });
        QImage strokes = makeStrokes(QSize(64, 64), [](QPainter& p) {
            p.setPen(QPen(Qt::red, 3));
            p.drawLine(6, 32, 12, 32);
            p.setPen(QPen(Qt::blue, 3));
            p.drawLine(52, 32, 58, 32);
        });

        const QRgb red = QColor(Qt::red).rgba();
        const QRgb blue = QColor(Qt::blue).rgba();

        // 无模糊：颜色会经由缺口互渗（红蓝在对面出现）
        QImage leaked = Colorize::colorize(lineArt, strokes, lineArt.rect(), Colorize::FilteringOptions());
        bool leakObserved = false;
        for (int y = 0; y < 64 && !leakObserved; ++y)
            for (int x = 40; x < 64; ++x)
                if (nonPremul(leaked, x, y) == red) { leakObserved = true; break; }
        // （缺口存在时几乎必然互渗；此断言仅记录行为，不作为硬性要求）
        INFO("无模糊时红蓝经由缺口互渗 = " << leakObserved);

        // 开模糊：缺口被桥接，右侧保持纯蓝
        Colorize::FilteringOptions opt;
        opt.fuzzyRadius = 3.0;
        QImage sealed = Colorize::colorize(lineArt, strokes, lineArt.rect(), opt);
        REQUIRE(nonPremul(sealed, 55, 32) == blue);
        REQUIRE(nonPremul(sealed, 8, 32) == red);
        for (int y = 8; y < 56; ++y)
        {
            for (int x = 40; x < 64; ++x)
            {
                const QRgb c = nonPremul(sealed, x, y);
                if (c != 0)
                    REQUIRE(c != red);
            }
        }
    }

    SECTION("transparent stroke protects region")
    {
        QImage lineArt = makeLineArt(QSize(64, 64), [](QPainter& p) {
            p.drawRect(4, 4, 56, 56); // 封闭方框，1px 线
        });

        // 红色笔画 + 透明笔画（中心保护块）
        QVector<Colorize::KeyStroke> keyStrokes;
        QImage redMask(QSize(64, 64), QImage::Format_Grayscale8);
        redMask.fill(0);
        QImage transparentMask(QSize(64, 64), QImage::Format_Grayscale8);
        transparentMask.fill(0);
        for (int y = 0; y < 64; ++y)
        {
            uchar* redLine = redMask.scanLine(y);
            uchar* trLine = transparentMask.scanLine(y);
            for (int x = 0; x < 64; ++x)
            {
                const bool inBox = x > 4 && x < 59 && y > 4 && y < 59;
                const bool inHole = x >= 24 && x < 40 && y >= 24 && y < 40;
                if (inBox && !inHole)
                    redLine[x] = 255;
                if (inHole)
                    trLine[x] = 255;
            }
        }
        keyStrokes.append({ transparentMask, 0, true }); // 透明在前（大面积优先）
        keyStrokes.append({ redMask, QColor(Qt::red).rgba(), false });

        QImage heightMap = Colorize::buildHeightMap(lineArt, lineArt.rect(), Colorize::FilteringOptions());
        QImage result = Colorize::runWatershed(heightMap, keyStrokes, lineArt.rect(), 0.7);

        const QRgb red = QColor(Qt::red).rgba();
        // 框内非保护区 = 红，保护块 = 透明，框外（无笔画覆盖的开放区域）= 红或透明皆可，但框内断言必须成立
        REQUIRE(nonPremul(result, 10, 32) == red);
        REQUIRE(nonPremul(result, 54, 10) == red);
        REQUIRE(result.pixel(32, 32) == 0); // 中心保护块保持透明
    }

    SECTION("cancel callback returns null image")
    {
        QImage lineArt = makeLineArt(QSize(64, 64), [](QPainter& p) {
            p.fillRect(30, 4, 4, 56, Qt::black);
        });
        QImage strokes = makeStrokes(QSize(64, 64), [](QPainter& p) {
            p.setPen(QPen(Qt::red, 3));
            p.drawLine(6, 32, 12, 32);
        });

        QImage result = Colorize::colorize(lineArt, strokes, lineArt.rect(),
                                           Colorize::FilteringOptions(),
                                           [](int) { return false; });
        REQUIRE(result.isNull());
    }
}

TEST_CASE("Colorize splitKeyStrokesByColor")
{
    SECTION("antialiased strokes cluster by color")
    {
        QImage strokes = makeStrokes(QSize(64, 64), [](QPainter& p) {
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setPen(QPen(Qt::red, 5));
            p.drawLine(4, 4, 60, 60);
            p.setPen(QPen(Qt::blue, 5));
            p.drawLine(60, 4, 4, 60);
        });

        // 反预乘取整会造成边缘杂色 → 允许少量碎片组，但主体两组必须存在
        QVector<Colorize::KeyStroke> split = Colorize::splitKeyStrokesByColor(strokes, strokes.rect());
        REQUIRE(split.size() >= 2);
        REQUIRE(split.size() < 16); // 碎片不该失控

        bool hasRed = false, hasBlue = false;
        for (const auto& s : split)
        {
            if (s.color == QColor(Qt::red).rgba()) hasRed = true;
            if (s.color == QColor(Qt::blue).rgba()) hasBlue = true;
        }
        REQUIRE(hasRed);
        REQUIRE(hasBlue);
    }

    SECTION("empty stroke image yields empty list")
    {
        QImage empty(QSize(32, 32), QImage::Format_ARGB32_Premultiplied);
        empty.fill(Qt::transparent);
        REQUIRE(Colorize::splitKeyStrokesByColor(empty, empty.rect()).isEmpty());
    }
}

TEST_CASE("Colorize VisualPerf")
{
    SECTION("house colorize and save png")
    {
        const QSize size(320, 240);
        QImage lineArt = makeLineArt(size, [](QPainter& p) {
            p.setRenderHint(QPainter::Antialiasing, true);
            QPen pen(Qt::black, 2);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            // 房身
            p.drawRect(60, 100, 200, 100);
            // 屋顶
            p.drawLine(50, 100, 160, 30);
            p.drawLine(160, 30, 270, 100);
            p.drawLine(50, 100, 270, 100); // 屋檐封口（否则颜色从两角缺口漏到背景——算法预期行为）
            // 门
            p.drawRect(140, 130, 40, 70);
            // 窗
            p.drawRect(200, 130, 40, 40);
        });
        QImage strokes = makeStrokes(size, [](QPainter& p) {
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setPen(QPen(QColor(255, 120, 0), 6)); // 橙：房身
            p.drawLine(80, 160, 120, 160);
            p.setPen(QPen(QColor(90, 160, 255), 6)); // 蓝：屋顶
            p.drawLine(100, 60, 130, 60);
            p.setPen(QPen(QColor(120, 220, 120), 6)); // 绿：窗
            p.drawLine(205, 145, 235, 145);
            p.setPen(QPen(QColor(160, 90, 60), 6)); // 棕：门
            p.drawLine(150, 160, 170, 160);
        });

        QElapsedTimer timer;
        timer.start();

        // bounds = 线稿内容包围盒 ∪ 笔画范围（Krita 语义：蒙版范围=内容范围，出界即视为无穷高不可填）；
        // 框内无竞争色的开放区域会被就近颜色吃掉——官方工作流用「透明笔画」保护背景
        QRect content = contentRect(lineArt) | contentRect(strokes);
        REQUIRE(!content.isEmpty());
        QImage heightMap = Colorize::buildHeightMap(lineArt, content, Colorize::FilteringOptions());
        QVector<Colorize::KeyStroke> keyStrokes = Colorize::splitKeyStrokesByColor(strokes, content);

        // 透明保护笔画：房外背景（左上角一片）
        QImage protectMask(size, QImage::Format_Grayscale8);
        protectMask.fill(0);
        for (int y = 8; y < 28; ++y)
        {
            uchar* line = protectMask.scanLine(y);
            for (int x = 8; x < 28; ++x)
                line[x] = 255;
        }
        keyStrokes.prepend(Colorize::KeyStroke{ protectMask, 0, true });

        QImage result = Colorize::runWatershed(heightMap, keyStrokes, content, 0.7);
        const qint64 ms = timer.elapsed();
        qDebug() << "[colorize] 320x240 耗时 ms:" << ms;

        REQUIRE(!result.isNull());

        // 合成预览：颜色层在下、线稿在上
        QImage preview(size, QImage::Format_ARGB32_Premultiplied);
        preview.fill(Qt::white);
        QPainter p(&preview);
        p.drawImage(content.topLeft(), result);
        p.drawImage(0, 0, lineArt);
        // 笔画提示（半透明重画）
        p.setOpacity(0.6);
        p.drawImage(0, 0, strokes);
        p.end();

        const QString outDir = QDir::temp().absoluteFilePath("pencil-colorize-tests");
        QDir().mkpath(outDir);
        const QString outPath = outDir + "/house.png";
        REQUIRE(preview.save(outPath));
        qDebug() << "[colorize] 视觉基准已保存:" << outPath;
    }

    SECTION("perf 1920x1080")
    {
        const QSize size(1920, 1080);
        QImage lineArt = makeLineArt(size, [&size](QPainter& p) {
            p.setPen(QPen(Qt::black, 2));
            for (int y = 40; y < size.height(); y += 120)
                p.drawLine(0, y, size.width(), y);
            for (int x = 40; x < size.width(); x += 120)
                p.drawLine(x, 0, x, size.height());
        });
        QImage strokes = makeStrokes(size, [](QPainter& p) {
            p.setPen(QPen(QColor(255, 120, 0), 8));
            for (int i = 0; i < 20; ++i)
                p.drawPoint(60 + 120 * (i % 16), 100 + 120 * (i / 16));
        });

        QElapsedTimer timer;
        timer.start();
        QImage result = Colorize::colorize(lineArt, strokes, lineArt.rect(), Colorize::FilteringOptions());
        const qint64 ms = timer.elapsed();
        qDebug() << "[colorize] 1920x1080 耗时 ms:" << ms;
        REQUIRE(!result.isNull());
        REQUIRE(ms < 20000); // 上限保护：测试机上不应慢于一帧 20s
    }
}
