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

#include <memory>
#include <QDomDocument>
#include <QDomElement>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QPainter>
#include <QImage>
#include <QPixmap>
#include "filemanager.h"
#include "layercamera.h"
#include "object.h"
#include "layerbitmap.h"
#include "bitmapimage.h"
#include "layersound.h"
#include "canvaspainter.h"


TEST_CASE("Object::addXXXLayer()")
{
    Object* obj = new Object;

    SECTION("Init an Object")
    {
        obj->init();
        REQUIRE(obj->getLayerCount() == 0);
        REQUIRE(obj->getColorCount() > 0);
    }

    SECTION("Add a bitmap layer")
    {
        REQUIRE(obj->getLayerCount() == 0);
        obj->addNewBitmapLayer();
        REQUIRE(obj->getLayerCount() == 1);

        REQUIRE(obj->getLayer(0)->type() == Layer::BITMAP);
    }

    SECTION("Add a camera layer")
    {
        REQUIRE(obj->getLayerCount() == 0);
        obj->addNewCameraLayer();
        REQUIRE(obj->getLayerCount() == 1);

        REQUIRE(obj->getLayer(0)->type() == Layer::CAMERA);
    }

    SECTION("Add a sound layer")
    {
        REQUIRE(obj->getLayerCount() == 0);
        obj->addNewSoundLayer();
        REQUIRE(obj->getLayerCount() == 1);
        REQUIRE(obj->getLayer(0)->type() == Layer::SOUND);
    }

    SECTION("Add 3 layers")
    {
        REQUIRE(obj->getLayerCount() == 0);

        obj->addNewSoundLayer();
        REQUIRE(obj->getLayerCount() == 1);
        REQUIRE(obj->getLayer(0)->type() == Layer::SOUND);

        obj->addNewCameraLayer();
        REQUIRE(obj->getLayerCount() == 2);
        REQUIRE(obj->getLayer(1)->type() == Layer::CAMERA);

        obj->addNewBitmapLayer();
        REQUIRE(obj->getLayerCount() == 3);
        REQUIRE(obj->getLayer(2)->type() == Layer::BITMAP);
    }

    SECTION("Add 500 layers")
    {
        REQUIRE(obj->getLayerCount() == 0);
        for (int i = 0; i < 500; ++i)
        {
            obj->addNewBitmapLayer();
        }
        REQUIRE(obj->getLayerCount() == 500);
    }

    delete obj;
}



TEST_CASE("Object::getUniqueLayerID()")
{
    SECTION("getUniqueLayerID")
    {
        std::unique_ptr<Object> obj(new Object);

        Layer* bitmapLayer = obj->addNewBitmapLayer();
        REQUIRE(bitmapLayer->id() == 1);
        REQUIRE(obj->getUniqueLayerID() == 2);

        auto* cameraLayer = obj->addNewCameraLayer();
        REQUIRE(cameraLayer->id() == 2);
        REQUIRE(obj->getUniqueLayerID() == 3);
    }
}

TEST_CASE("Object: sound key survives save-load after modification")
{
    FileManager fm;

    QTemporaryDir testDir("PENCIL_TEST_XXXXXXXX");
    REQUIRE(testDir.isValid());

    const QString sourceAudioPath = testDir.filePath("input_sound.wav");
    {
        QFile sourceAudio(sourceAudioPath);
        REQUIRE(sourceAudio.open(QIODevice::WriteOnly));
        // LayerSound only requires a file to exist, so fixture bytes can be minimal.
        REQUIRE(sourceAudio.write("PENCIL_TEST_AUDIO") > 0);
    }

    const QString animationPath = testDir.filePath("sound-modified-roundtrip.pclx");

    {
        std::unique_ptr<Object> objectToCreate(new Object);
        objectToCreate->init();

        LayerSound* soundLayer = objectToCreate->addNewSoundLayer();
        REQUIRE(soundLayer != nullptr);
        REQUIRE(soundLayer->loadSoundClipAtFrame("test-clip", sourceAudioPath, 2).ok());

        REQUIRE(fm.save(objectToCreate.get(), animationPath).ok());
    }

    {
        std::unique_ptr<Object> loadedObject(fm.load(animationPath));
        REQUIRE(loadedObject != nullptr);
        REQUIRE(fm.error().ok());

        LayerSound* loadedSoundLayer = nullptr;
        for (int i = 0; i < loadedObject->getLayerCount(); ++i)
        {
            if (loadedObject->getLayer(i)->type() == Layer::SOUND)
            {
                loadedSoundLayer = static_cast<LayerSound*>(loadedObject->getLayer(i));
                break;
            }
        }

        REQUIRE(loadedSoundLayer != nullptr);

        KeyFrame* soundKey = loadedSoundLayer->getKeyFrameAt(2);
        REQUIRE(soundKey != nullptr);

        // Simulate editing workflow where sound key gets marked modified before saving again.
        soundKey->modification();

        REQUIRE(fm.save(loadedObject.get(), animationPath).ok());
    }

    {
        std::unique_ptr<Object> reloadedObject(fm.load(animationPath));
        REQUIRE(reloadedObject != nullptr);
        REQUIRE(fm.error().ok());

        LayerSound* reloadedSoundLayer = nullptr;
        for (int i = 0; i < reloadedObject->getLayerCount(); ++i)
        {
            if (reloadedObject->getLayer(i)->type() == Layer::SOUND)
            {
                reloadedSoundLayer = static_cast<LayerSound*>(reloadedObject->getLayer(i));
                break;
            }
        }

        REQUIRE(reloadedSoundLayer != nullptr);

        KeyFrame* reloadedSoundKey = reloadedSoundLayer->getKeyFrameAt(2);
        REQUIRE(reloadedSoundKey != nullptr);
        REQUIRE_FALSE(reloadedSoundKey->fileName().isEmpty());
        REQUIRE(QFileInfo::exists(reloadedSoundKey->fileName()));
    }
}

/*
void TestObject::testMoveLayer()
{
    std::unique_ptr< Object > obj( new Object );

    obj->addNewBitmapLayer();
    obj->addNewBitmapLayer();
    QCOMPARE( obj->getLayer( 0 )->id(), 1 );
    QCOMPARE( obj->getLayer( 1 )->id(), 2 );

    obj->moveLayer( 0, 1 );
    QCOMPARE( obj->getLayer( 0 )->id(), 2 );
    QCOMPARE( obj->getLayer( 1 )->id(), 1 );

}

void TestObject::testLoadXML()
{
    std::unique_ptr< Object > obj( new Object );

    QString strXMLContent;
    QTextStream sout( &strXMLContent );
    sout << "<!DOCTYPE PencilDocument><object>";
    sout << "</object>";
    sout.flush();

    QDomDocument doc;
    doc.setContent( strXMLContent );
    QDomElement e = doc.firstChildElement( "object" );
    QVERIFY( !e.isNull() );

    QVERIFY( obj->loadXML( e ) );
    
}

void TestObject::testExportColorPalette()
{
    std::shared_ptr< Object > obj = std::make_shared<Object>();

    obj->addColor(ColorRef(QColor(255, 254, 253, 100), "TestColor1"));

    QTemporaryDir dir;
    if (dir.isValid())
    {
        QString sOutPath = dir.path() + "/testPalette.xml";
        QVERIFY(obj->exportPalette(sOutPath));
        QVERIFY(obj->importPalette(sOutPath));

        ColorRef c = obj->getColor(0);

        QVERIFY(c.name == "TestColor1");
        QCOMPARE(c.color.red(), 255);
        QCOMPARE(c.color.green(), 254);
        QCOMPARE(c.color.blue(), 253);
        QCOMPARE(c.color.alpha(), 100);
    }

}
*/

TEST_CASE("Object::paintImage renders loop-mode wrapped frames", "[Object]")
{
    Object obj;
    LayerBitmap* layer = obj.addNewBitmapLayer();
    delete layer->takeKeyFrame(1); // drop the auto-created empty key@1
    layer->addKeyFrame(1, new BitmapImage(QRect(0, 0, 10, 10), QColor(255, 0, 0)));
    layer->addKeyFrame(2, new BitmapImage(QRect(0, 0, 10, 10), QColor(0, 0, 255)));
    layer->addKeyFrame(3, new BitmapImage(QRect(0, 0, 10, 10), QColor(0, 255, 0)));

    auto paintAt = [&obj](int frame)
    {
        QImage img(32, 32, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter painter(&img);
        obj.paintImage(painter, frame, false, true);
        painter.end();
        return img.pixelColor(5, 5);
    };

    SECTION("Hold (default): tail frame persists")
    {
        REQUIRE(paintAt(1).red() > 200);
        REQUIRE(paintAt(5).green() > 200);
    }

    SECTION("Cycle: export renders wrapped frames")
    {
        layer->setLoopMode(Layer::LoopMode::Cycle);
        // cycle = [1,4): 4->red, 5->blue, 6->green
        REQUIRE(paintAt(4).red() > 200);
        REQUIRE(paintAt(5).blue() > 200);
        REQUIRE(paintAt(6).green() > 200);
    }

    SECTION("PingPong: export renders the triangle wave")
    {
        layer->setLoopMode(Layer::LoopMode::PingPong);
        // sequence 1,2,3,2,1,...: 4->blue, 5->red, 6->blue
        REQUIRE(paintAt(4).blue() > 200);
        REQUIRE(paintAt(5).red() > 200);
        REQUIRE(paintAt(6).blue() > 200);
    }
}

TEST_CASE("Object::paintImage clip mask uses nearest non-clip base (friction semantics)", "[Object]")
{
    // 栈(底→顶): farBelow 左半大块 | base 右半块 | top 全画布红(剪贴)
    // friction 保持透明度语义:蒙版=正下方最近非剪贴位图层(base),
    // 更下方 farBelow 的 alpha 不并入 → 红只出现在右半
    Object obj;
    auto addShapeLayer = [&obj](const QColor& c, int x0, int x1)
    {
        LayerBitmap* l = obj.addNewBitmapLayer();
        delete l->takeKeyFrame(1);
        QImage img(64, 64, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter p(&img);
        p.fillRect(QRect(x0, 0, x1 - x0, 64), c);
        p.end();
        l->addKeyFrame(1, new BitmapImage(QPoint(0, 0), img));
        return l;
    };

    addShapeLayer(QColor(0, 0, 255, 255), 0, 64);   // farBelow: 蓝全幅
    addShapeLayer(QColor(0, 255, 0, 255), 32, 64);  // base: 绿右半
    LayerBitmap* top = obj.addNewBitmapLayer();
    delete top->takeKeyFrame(1);
    QImage red(64, 64, QImage::Format_ARGB32_Premultiplied);
    red.fill(QColor(255, 0, 0, 255));
    top->addKeyFrame(1, new BitmapImage(QPoint(0, 0), red));
    top->setClipMask(true);

    QImage out(64, 64, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    QPainter painter(&out);
    obj.paintImage(painter, 1, false, true);
    painter.end();

    SECTION("clipped layer limited to adjacent base, not the union below")
    {
        // 左半: base 外(只有 farBelow)→ 剪贴层不可见,蓝显示
        const QColor left = out.pixelColor(10, 32);
        REQUIRE(left.blue() > 200);
        REQUIRE(left.red() < 60);
        // 右半: base 内 → 红(盖住绿)
        const QColor right = out.pixelColor(50, 32);
        REQUIRE(right.red() > 200);
        REQUIRE(right.green() < 60);
    }

    SECTION("run of clipped layers shares the base; no base below hides all")
    {
        // 再叠一层剪贴蓝,同样只应出现在右半(共享 base)
        LayerBitmap* top2 = obj.addNewBitmapLayer();
        delete top2->takeKeyFrame(1);
        QImage blue(64, 64, QImage::Format_ARGB32_Premultiplied);
        blue.fill(QColor(0, 0, 255, 255));
        top2->addKeyFrame(1, new BitmapImage(QPoint(0, 0), blue));
        top2->setClipMask(true);

        QImage out2(64, 64, QImage::Format_ARGB32_Premultiplied);
        out2.fill(Qt::transparent);
        QPainter p2(&out2);
        obj.paintImage(p2, 1, false, true);
        p2.end();

        const QColor left = out2.pixelColor(10, 32);
        REQUIRE(left.red() < 60);   // 左半无剪贴红
        const QColor right = out2.pixelColor(50, 32);
        REQUIRE(right.blue() > 200); // 顶层蓝盖红
    }
}

TEST_CASE("CanvasPainter applies layer opacity once (clip base edge coverage)", "[Object]")
{
    // o² 双重应用回归:底形 50% + 剪贴不透明红,圆心合成 alpha
    // 单次语义 SrcOver(0.5 over 0.5)=0.75(≈192);o² 旧bug≈112
    Object obj;
    LayerBitmap* base = obj.addNewBitmapLayer();
    delete base->takeKeyFrame(1);
    QImage baseImg(64, 64, QImage::Format_ARGB32_Premultiplied);
    baseImg.fill(Qt::transparent);
    {
        QPainter p(&baseImg);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 255));
        p.drawEllipse(QRect(12, 12, 40, 40));
    }
    base->addKeyFrame(1, new BitmapImage(QPoint(0, 0), baseImg));
    base->setOpacity(0.5);

    LayerBitmap* top = obj.addNewBitmapLayer();
    delete top->takeKeyFrame(1);
    QImage red(64, 64, QImage::Format_ARGB32_Premultiplied);
    red.fill(QColor(255, 0, 0, 255));
    top->addKeyFrame(1, new BitmapImage(QPoint(0, 0), red));
    top->setClipMask(true);

    CanvasPainterOptions opts;
    opts.eLayerVisibility = LayerVisibility::ALL;

    QPixmap canvas(64, 64);
    canvas.fill(Qt::transparent);
    {
        CanvasPainter cp(canvas);
        cp.setOptions(opts);
        cp.setViewTransform(QTransform(), QTransform());
        cp.setPaintSettings(&obj, 1, 1, nullptr);
        cp.paint(canvas.rect());
    }
    const QColor c = canvas.toImage().pixelColor(32, 32);
    REQUIRE(c.alpha() >= 185);
    REQUIRE(c.alpha() <= 200);
    REQUIRE(c.red() > 100);
}
