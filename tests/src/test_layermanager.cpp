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

#include "object.h"
#include "editor.h"
#include "layermanager.h"
#include "pencilerror.h"
#include "layerbitmap.h"
#include "bitmapimage.h"


TEST_CASE("LayerManager::init()")
{
    Object* object = new Object;
    Editor* editor = new Editor;
    editor->setObject(object);

    SECTION("Test initial state")
    {
        LayerManager* layerMgr = new LayerManager(editor);
        layerMgr->init();

        object->init();
        object->addNewCameraLayer();
        object->addNewBitmapLayer();
        REQUIRE(layerMgr->count() == 2);
        REQUIRE(layerMgr->currentLayerIndex() == 0);
        REQUIRE(layerMgr->getLayer(0)->type() == Layer::CAMERA);
        REQUIRE(layerMgr->getLayer(1)->type() == Layer::BITMAP);
    }
    delete editor;
}

TEST_CASE("LayerManager::deleteLayer()")
{
    Object* object = new Object;
    Editor* editor = new Editor;
    editor->setObject(object);

    SECTION("delete layers")
    {
        LayerManager* layerMgr = new LayerManager(editor);
        layerMgr->init();

        object->init();

        REQUIRE(layerMgr->count() == 0);
        layerMgr->createCameraLayer("Camera1");
        REQUIRE(layerMgr->count() == 1);
        layerMgr->createCameraLayer("Camera2");
        REQUIRE(layerMgr->count() == 2);
        layerMgr->createBitmapLayer("Bitmap3");
        REQUIRE(layerMgr->count() == 3);
        layerMgr->deleteLayer(2);
        REQUIRE(layerMgr->count() == 2);
        layerMgr->deleteLayer(1);
        REQUIRE(layerMgr->count() == 1);
    }

    SECTION("delete camera layers")
    {
        LayerManager* layerMgr = new LayerManager(editor);
        layerMgr->init();

        // create 2 camera layers
        REQUIRE(layerMgr->count() == 0);
        layerMgr->createCameraLayer("Camera1");
        REQUIRE(layerMgr->count() == 1);
        layerMgr->createCameraLayer("Camera2");
        REQUIRE(layerMgr->count() == 2);

        // delete one of them, ok.
        layerMgr->deleteLayer(1);
        REQUIRE(layerMgr->count() == 1);

        // delete the second, no, cant do it.
        Status st = layerMgr->deleteLayer(0);
        REQUIRE(layerMgr->count() == 1);
        REQUIRE((st == Status::ERROR_NEED_AT_LEAST_ONE_CAMERA_LAYER));
    }
    delete editor;
}

TEST_CASE("Layer::setCurrentLayer(index)") {
    Object* object = new Object;
    Editor* editor = new Editor;
    editor->setObject(object);

    SECTION("Deselect previous layer") {
        LayerManager* layerMgr = new LayerManager(editor);
        layerMgr->init();

        layerMgr->createBitmapLayer("Bitmap1");
        layerMgr->createBitmapLayer("Bitmap2");
        layerMgr->setCurrentLayer(0);

        Layer* currentLayer = layerMgr->currentLayer();
        currentLayer->addNewKeyFrameAt(1);
        currentLayer->addNewKeyFrameAt(2);

        currentLayer->setFrameSelected(1, true);
        currentLayer->setFrameSelected(2, true);

        REQUIRE(currentLayer->selectedKeyFrameCount() == 2);

        layerMgr->setCurrentLayer(1);

        // Make sure that previous layer has deselected all frames
        REQUIRE(layerMgr->getLayer(0)->selectedKeyFrameCount() == 0);
        REQUIRE(layerMgr->getLayer(1)->selectedKeyFrameCount() == 0);

    }
    delete editor;
}

TEST_CASE("LayerManager::mergeBitmapLayerDown()")
{
    Object* object = new Object;
    Editor* editor = new Editor;
    editor->setObject(object);

    SECTION("merge splits lower keys at upper exposure boundaries")
    {
        LayerManager* layerMgr = new LayerManager(editor);
        layerMgr->init();

        object->init();
        layerMgr->createCameraLayer("Camera");
        LayerBitmap* lower = layerMgr->createBitmapLayer("Lower");
        LayerBitmap* upper = layerMgr->createBitmapLayer("Upper");
        REQUIRE(layerMgr->count() == 3);

        // 下层：帧1红（自动曝光），帧10绿
        delete lower->takeKeyFrame(1); // new layers carry an empty auto key@1
        lower->addKeyFrame(1, new BitmapImage(QRect(0, 0, 4, 4), Qt::red));
        lower->addKeyFrame(10, new BitmapImage(QRect(0, 0, 4, 4), Qt::green));

        // 上层：帧5蓝，显式曝光 3 帧（5..7）
        BitmapImage* upperKey = new BitmapImage(QRect(0, 0, 4, 4), Qt::blue);
        upperKey->setLength(3);
        upperKey->setLengthExplicit(true);
        upper->addKeyFrame(5, upperKey);

        REQUIRE(layerMgr->mergeBitmapLayerDown(2).ok());

        // 上层被删除，当前层切到合并后的下层
        REQUIRE(layerMgr->count() == 2);
        REQUIRE(layerMgr->currentLayerIndex() == 1);
        REQUIRE(layerMgr->getLayer(1) == lower);

        // 下层按上层曝光边界拆出 1/5/8/10 四个 key
        REQUIRE(lower->keyExists(1));
        REQUIRE(lower->keyExists(5));
        REQUIRE(lower->keyExists(8));
        REQUIRE(lower->keyExists(10));

        // 逐段像素：1=红 5=红+蓝(蓝盖) 8=红(上层结束还原) 10=绿
        REQUIRE(lower->getBitmapImageAtFrame(1)->image()->pixel(1, 1) == QColor(Qt::red).rgba());
        REQUIRE(lower->getBitmapImageAtFrame(5)->image()->pixel(1, 1) == QColor(Qt::blue).rgba());
        REQUIRE(lower->getBitmapImageAtFrame(8)->image()->pixel(1, 1) == QColor(Qt::red).rgba());
        REQUIRE(lower->getBitmapImageAtFrame(10)->image()->pixel(1, 1) == QColor(Qt::green).rgba());
    }
    delete editor;
}
