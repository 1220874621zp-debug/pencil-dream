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

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

*/

#include "projectthumb.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "qminiz.h"

namespace
{
    // main.xml 中 layer type 的裸值（Layer::LAYER_TYPE，避免 app 依赖 layer 头）
    constexpr int XML_LAYER_BITMAP = 1;
    constexpr int XML_LAYER_COLORIZE = 6;

    QString cacheKey(const QFileInfo& info)
    {
        const QString native = QDir::toNativeSeparators(info.absoluteFilePath()).toLower();
        return QString::fromLatin1(QCryptographicHash::hash(native.toUtf8(), QCryptographicHash::Md5).toHex());
    }

    QString thumbsDirPath()
    {
        return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/thumbs");
    }

    // 解开的工程目录里找最上层位图类图层的首帧图 src（相对 data 目录）
    QString firstBitmapFrameSrc(const QString& projectDir)
    {
        QFile mainXml(QDir(projectDir).filePath(QStringLiteral("main.xml")));
        if (!mainXml.open(QIODevice::ReadOnly)) { return QString(); }

        QDomDocument doc;
        if (!doc.setContent(&mainXml)) { return QString(); }

        QDomElement root = doc.documentElement();
        // 现行格式 layer 挂 <object> 下；旧格式（<=0.4.3）直接挂根节点
        if (root.tagName() == QLatin1String("document"))
        {
            root = root.firstChildElement(QLatin1String("object"));
            if (root.isNull()) { return QString(); }
        }
        for (QDomNode ln = root.firstChild(); !ln.isNull(); ln = ln.nextSibling())
        {
            const QDomElement layerTag = ln.toElement();
            if (layerTag.isNull() || layerTag.tagName() != QLatin1String("layer")) { continue; }
            const int layerType = layerTag.attribute(QStringLiteral("type")).toInt();
            if (layerType != XML_LAYER_BITMAP && layerType != XML_LAYER_COLORIZE) { continue; }

            int bestFrame = INT_MAX;
            QString bestSrc;
            for (QDomNode kn = layerTag.firstChild(); !kn.isNull(); kn = kn.nextSibling())
            {
                const QDomElement keyTag = kn.toElement();
                if (keyTag.isNull() || keyTag.tagName() != QLatin1String("image")) { continue; }
                const int frame = keyTag.attribute(QStringLiteral("frame"), QStringLiteral("-1")).toInt();
                if (frame >= 0 && frame < bestFrame)
                {
                    bestFrame = frame;
                    bestSrc = keyTag.attribute(QStringLiteral("src"));
                }
            }
            if (!bestSrc.isEmpty()) { return bestSrc; }
        }
        return QString();
    }

    QImage makeCard(const QImage& src)
    {
        QImage card(ProjectThumb::CARD_W, ProjectThumb::CARD_H, QImage::Format_ARGB32_Premultiplied);
        card.fill(QColor(0x1E, 0x20, 0x26));
        if (src.isNull()) { return card; }

        QPainter painter(&card);
        const QSize scaled = src.size().scaled(card.width(), card.height(), Qt::KeepAspectRatio);
        painter.drawImage(QRect((card.width() - scaled.width()) / 2,
                                (card.height() - scaled.height()) / 2,
                                scaled.width(), scaled.height()), src);
        return card;
    }
}

namespace ProjectThumb
{
    QImage load(const QString& pclxPath)
    {
        const QFileInfo info(pclxPath);
        if (!info.exists() || info.suffix().compare(QLatin1String("pclx"), Qt::CaseInsensitive) != 0)
        {
            return QImage();
        }

        const QDir dir(thumbsDirPath());
        dir.mkpath(QStringLiteral("."));
        const QString key = cacheKey(info);
        const QString pngPath = dir.filePath(key + QStringLiteral(".png"));
        const QString metaPath = dir.filePath(key + QStringLiteral(".txt"));
        const QString stamp = QStringLiteral("%1|%2").arg(info.size()).arg(info.lastModified().toMSecsSinceEpoch());

        QFile meta(metaPath);
        if (meta.open(QIODevice::ReadOnly))
        {
            if (QString::fromUtf8(meta.readAll().trimmed()) == stamp)
            {
                QImage cached;
                if (cached.load(pngPath)) { return cached; }
            }
            meta.close();
        }

        // 解包整个工程到临时目录抽帧（缓存命中时不走这条，重生成仅发生在源变更后）
        QTemporaryDir temp;
        QImage src;
        if (temp.isValid() && MiniZ::uncompressFolder(info.absoluteFilePath(), temp.path()).ok())
        {
            const QString rel = firstBitmapFrameSrc(temp.path());
            if (!rel.isEmpty() && QFileInfo(rel).isRelative())
            {
                src.load(QDir(temp.filePath(QStringLiteral("data"))).filePath(rel));
            }
        }
        if (src.isNull()) { return QImage(); }

        const QImage card = makeCard(src);
        card.save(pngPath, "PNG");
        if (meta.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            meta.write(stamp.toUtf8());
        }
        return card;
    }
}
