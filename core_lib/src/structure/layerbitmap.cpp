/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include "layerbitmap.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <algorithm>
#include "keyframe.h"
#include "bitmapimage.h"
#include "util/util.h"

LayerBitmap::LayerBitmap(int id, LAYER_TYPE type) : Layer(id, type)
{
    setName(tr("Bitmap Layer"));
}

LayerBitmap::~LayerBitmap()
{
}

BitmapImage* LayerBitmap::getBitmapImageAtFrame(int frameNumber)
{
    Q_ASSERT(frameNumber >= 1);
    return static_cast<BitmapImage*>(getKeyFrameAt(frameNumber));
}

BitmapImage* LayerBitmap::getLastBitmapImageAtFrame(int frameNumber)
{
    Q_ASSERT(frameNumber >= 1);
    return static_cast<BitmapImage*>(getLastKeyFrameAtPosition(displayFrameFor(frameNumber)));
}

void LayerBitmap::replaceKeyFrame(const KeyFrame* bitmapImage)
{
    *getBitmapImageAtFrame(bitmapImage->pos()) = *static_cast<const BitmapImage*>(bitmapImage);
}

void LayerBitmap::repositionFrame(QPoint point, int frame)
{
    BitmapImage* image = getBitmapImageAtFrame(frame);
    Q_ASSERT(image);
    image->moveTopLeft(point);
}

QRect LayerBitmap::getFrameBounds(int frame)
{
    BitmapImage* image = getBitmapImageAtFrame(frame);
    Q_ASSERT(image);
    return image->bounds();
}

std::vector<int> LayerBitmap::instanceGroupPositions(int keyPos) const
{
    KeyFrame* key = getKeyFrameAt(keyPos);
    if (key == nullptr) { return {}; }

    BitmapImage* bitmap = static_cast<BitmapImage*>(key);
    std::vector<int> positions;
    for (BitmapImage* member : bitmap->instanceMembers())
    {
        positions.push_back(member->pos());
    }
    std::sort(positions.begin(), positions.end());
    return positions;
}

void LayerBitmap::loadImageAtFrame(QString path, QPoint topLeft, int frameNumber, qreal opacity)
{
    BitmapImage* pKeyFrame = new BitmapImage(topLeft, path);
    pKeyFrame->enableAutoCrop(true);
    pKeyFrame->setPos(frameNumber);
    pKeyFrame->setOpacity(opacity);
    loadKey(pKeyFrame);
}

Status LayerBitmap::saveKeyFrameFile(KeyFrame* keyframe, QString path)
{
    QString strFilePath = filePath(keyframe, QDir(path));

    BitmapImage* bitmapImage = static_cast<BitmapImage*>(keyframe);

    bool needSave = needSaveFrame(keyframe, strFilePath);
    if (!needSave)
    {
        return Status::SAFE;
    }

    // 帧池可能已把图像逐出内存（bounds 仍在）；writeFile 对空图像且
    // bounds 非空会静默不写文件，XML 却照引 src -> 归档永久残缺。
    // 存盘前先从既有文件回载，保证需要落盘的帧真的落盘
    bitmapImage->loadFile();

    bitmapImage->setFileName(strFilePath);

    Status st = bitmapImage->writeFile(strFilePath);
    if (!st.ok())
    {
        bitmapImage->setFileName("");

        DebugDetails dd;
        dd << "LayerBitmap::saveKeyFrame";
        dd << QString("&nbsp;&nbsp;KeyFrame.pos() = %1").arg(keyframe->pos());
        dd << QString("&nbsp;&nbsp;strFilePath = %1").arg(strFilePath);
        dd << QString("Error: Failed to save BitmapImage");
        dd.collect(st.details());
        return Status(Status::FAIL, dd);
    }

    bitmapImage->setModified(false);
    return Status::OK;
}

KeyFrame* LayerBitmap::createKeyFrame(int position)
{
    BitmapImage* b = new BitmapImage;
    b->setPos(position);
    b->enableAutoCrop(true);
    return b;
}

Status LayerBitmap::presave(const QString& sDataFolder)
{
    QDir dataFolder(sDataFolder);
    // Handles keys that have been moved but not modified
    std::vector<BitmapImage*> movedOnlyBitmaps;
    foreachKeyFrame([&movedOnlyBitmaps,&dataFolder,this](KeyFrame* key)
    {
        auto bitmap = static_cast<BitmapImage*>(key);
        // (b->fileName() != fileName(b) && !modified => the keyframe has been moved, but users didn't draw on it.
        if (!bitmap->fileName().isEmpty()
            && !bitmap->isModified()
            && bitmap->fileName() != filePath(bitmap, dataFolder))
        {
            movedOnlyBitmaps.push_back(bitmap);
        }
    });

    for (BitmapImage* b : movedOnlyBitmaps)
    {
        // Move to temporary locations first to avoid overwritting anything we shouldn't be
        // Ex: Frame A moves from 1 -> 2, Frame B moves from 2 -> 3. Make sure A does not overwrite B
        QString tmpPath = dataFolder.filePath(QString::asprintf("t_%03d.%03d.png", id(), b->pos()));
        if (QFileInfo(b->fileName()).dir() != dataFolder) {
            // Copy instead of move if the data folder itself has changed
            QFile::copy(b->fileName(), tmpPath);
        }
        else {
            QFile::rename(b->fileName(), tmpPath);
        }
        b->setFileName(tmpPath);
    }

    for (BitmapImage* b : movedOnlyBitmaps)
    {
        QString dest = filePath(b, dataFolder);
        QFile::remove(dest);

        QFile::rename(b->fileName(), dest);
        b->setFileName(dest);
    }

    return Status::OK;
}

QString LayerBitmap::filePath(KeyFrame* key, const QDir& dataFolder) const
{
    return dataFolder.filePath(fileName(key));
}

QString LayerBitmap::fileName(KeyFrame* key) const
{
    return QString::asprintf("%03d.%03d.png", id(), key->pos());
}

bool LayerBitmap::needSaveFrame(KeyFrame* key, const QString& savePath)
{
    if (key->isModified()) // keyframe was modified
        return true;
    if (QFile::exists(savePath) == false) // hasn't been saved before
        return true;
    if (key->fileName().isEmpty())
        return true;
    return false;
}

QDomElement LayerBitmap::createDomElement(QDomDocument& doc) const
{
    QDomElement layerElem = createBaseDomElement(doc);

    // 实例组编号：同一次遍历内按共享块身份现场分配（跨会话不要求稳定，
    // 载入侧只按属性值分组归并）
    QHash<const void*, int> instanceGroupIds;
    int nextInstanceId = 0;

    foreachKeyFrame([&](KeyFrame* pKeyFrame)
    {
        BitmapImage* pImg = static_cast<BitmapImage*>(pKeyFrame);

        QDomElement imageTag = doc.createElement("image");
        imageTag.setAttribute("frame", pKeyFrame->pos());
        imageTag.setAttribute("src", fileName(pKeyFrame));
        imageTag.setAttribute("topLeftX", pImg->topLeft().x());
        imageTag.setAttribute("topLeftY", pImg->topLeft().y());
        imageTag.setAttribute("opacity", pImg->getOpacity());
        if (pImg->isInstanceShared())
        {
            const void* groupId = pImg->sharedDataId();
            if (!instanceGroupIds.contains(groupId))
            {
                instanceGroupIds.insert(groupId, ++nextInstanceId);
            }
            imageTag.setAttribute("instance", instanceGroupIds.value(groupId));
        }
        if (pKeyFrame->isLengthExplicit())
        {
            imageTag.setAttribute("length", pKeyFrame->length());
        }
        if (!pKeyFrame->isKeyDrawing())
        {
            imageTag.setAttribute("keyDrawing", "0");
        }
        layerElem.appendChild(imageTag);

        if (!pKeyFrame->fileName().isEmpty()) {
            Q_ASSERT(QFileInfo(pKeyFrame->fileName()).fileName() == fileName(pKeyFrame));
        }
    });

    return layerElem;
}

void LayerBitmap::loadDomElement(const QDomElement& element, QString dataDirPath, ProgressCallback progressStep)
{
    this->loadBaseDomElement(element);

    // pos -> instance 组号（同组帧载入后归并回同一共享块，恢复全组同步）
    QHash<int, int> instanceGroupByPos;

    QDomNode imageTag = element.firstChild();
    while (!imageTag.isNull())
    {
        QDomElement imageElement = imageTag.toElement();
        if (!imageElement.isNull() && imageElement.tagName() == "image")
        {
            QString path = validateDataPath(imageElement.attribute("src"), dataDirPath);
            if (!path.isEmpty())
            {
                int position = imageElement.attribute("frame").toInt();
                int x = imageElement.attribute("topLeftX").toInt();
                int y = imageElement.attribute("topLeftY").toInt();
                qreal opacity = imageElement.attribute("opacity", "1.0").toDouble();
                loadImageAtFrame(path, QPoint(x, y), position, opacity);

                if (imageElement.hasAttribute("instance"))
                {
                    instanceGroupByPos.insert(position, imageElement.attribute("instance").toInt());
                }

                if (imageElement.hasAttribute("length"))
                {
                    int length = imageElement.attribute("length").toInt();
                    if (length >= 1)
                    {
                        KeyFrame* key = getKeyFrameAt(position);
                        if (key != nullptr)
                        {
                            key->setLength(length);
                            key->setLengthExplicit(true);
                        }
                    }
                }

                KeyFrame* loadedKey = getKeyFrameAt(position);
                if (loadedKey != nullptr)
                {
                    loadedKey->setKeyDrawing(imageElement.attribute("keyDrawing", "1").toInt() != 0);
                }
            }

            progressStep();
        }
        imageTag = imageTag.nextSibling();
    }

    // 实例组归并：每组首个成员的共享块作为组块，其余成员重绑过去
    // （各成员文件本就是同一内容的多份拷贝，取首个即正确）
    if (!instanceGroupByPos.isEmpty())
    {
        QHash<int, BitmapImage*> firstOfGroup;
        for (auto it = instanceGroupByPos.constBegin(); it != instanceGroupByPos.constEnd(); ++it)
        {
            BitmapImage* member = getBitmapImageAtFrame(it.key());
            if (member == nullptr) { continue; }

            const int groupId = it.value();
            BitmapImage* first = firstOfGroup.value(groupId, nullptr);
            if (first == nullptr)
            {
                firstOfGroup.insert(groupId, member);
            }
            else
            {
                member->shareDataFrom(first);
            }
        }
    }
}
