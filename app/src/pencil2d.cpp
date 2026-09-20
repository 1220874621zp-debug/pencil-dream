
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
#include "pencil2d.h"

#include <memory>

#include <QDebug>
#include <QFileOpenEvent>
#include <QIcon>
#include <QLibraryInfo>
#include <QSettings>
#include <QTranslator>
#include <QLockFile>
#include <QStandardPaths>
#include <QDir>

#include "commandlineexporter.h"
#include "commandlineparser.h"
#include "autoshadowdialog.h"
#include "mainwindow2.h"
#include "pencildef.h"
#include "platformhandler.h"
#include "theme.h"

#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QSlider>


#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
#include <clocale>
#endif

Pencil2D::Pencil2D(int& argc, char** argv) :
    QApplication(argc, argv)
{
    // Set organization and application name
    setOrganizationName("Pencil2D");
    setOrganizationDomain("pencil2d.org");
    setApplicationName("Pencil2D");
    setApplicationDisplayName("Pencil Dream");

    // Set application version
    setApplicationVersion(APP_VERSION);

    // Set application icon
    setWindowIcon(QIcon(":/icons/icon.png"));

    Theme::applyDarkTheme(*this);

#if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)
    // Associate the application with our desktop entry
    setDesktopFileName("org.pencil2d.Pencil2D");
#endif
    installTranslators();
}

Pencil2D::~Pencil2D()
{
    // with a std::unique_ptr member variable,
    // you need a non-default destructor to avoid compilation error.
}

Status Pencil2D::handleCommandLineOptions()
{
    CommandLineParser parser;
    parser.process(arguments());

    // TODO(remove): 临时对话框布局探针——ASHADOW_DIALOG_PROBE=1 时离线量几何
    if (qEnvironmentVariableIsSet("ASHADOW_DIALOG_PROBE"))
    {
        AutoShadowDialog dialog(nullptr);
        auto dumpGeometry = [&dialog](const char* tag) {
            const auto spins = dialog.findChildren<QDoubleSpinBox*>();
            const auto sliders = dialog.findChildren<QSlider*>();
            qDebug() << "[probe]" << tag << "size" << dialog.size() << "hint" << dialog.sizeHint() << "minHint" << dialog.minimumSizeHint();
            struct Item { QString name; QRect r; };
            QList<Item> items;
            for (auto* sp : spins)
                items.append({ QStringLiteral("spin"), QRect(sp->mapTo(&dialog, QPoint(0, 0)), sp->size()) });
            for (auto* sl : sliders)
                items.append({ QStringLiteral("slider"), QRect(sl->mapTo(&dialog, QPoint(0, 0)), sl->size()) });
            int overlaps = 0;
            for (int i = 0; i < items.size(); ++i)
                for (int j = i + 1; j < items.size(); ++j)
                    if (items[i].r.intersects(items[j].r))
                    {
                        ++overlaps;
                        qDebug() << "[probe] OVERLAP" << items[i].name << items[i].r << "vs" << items[j].name << items[j].r;
                    }
            qDebug() << "[probe]" << tag << "overlaps=" << overlaps << "spins=" << spins.size() << "sliders=" << sliders.size();
        };
        dialog.show();
        for (int i = 0; i < 8; ++i)
            QCoreApplication::processEvents();
        dumpGeometry("natural");
        for (auto* b : dialog.findChildren<QGroupBox*>())
            qDebug() << "[probe] groupbox" << b->title() << QRect(b->mapTo(&dialog, QPoint(0, 0)), b->size());
        for (auto* sp : dialog.findChildren<QDoubleSpinBox*>())
            qDebug() << "[probe] spin parent=" << sp->parentWidget()->metaObject()->className()
                     << QRect(sp->mapTo(&dialog, QPoint(0, 0)), sp->size())
                     << "suffix=" << sp->suffix();
        dialog.resize(960, 660);
        for (int i = 0; i < 8; ++i)
            QCoreApplication::processEvents();
        dumpGeometry("squeezed660");
        return Status::SAFE;
    }

#ifndef QT_DEBUG
    if (isInstanceOpen()) {
        return Status::SAFE;
    }
#endif

    QString inputPath = parser.inputPath();
    QStringList outputPaths = parser.outputPaths();

    if (outputPaths.isEmpty())
    {
        prepareGuiStartup(inputPath);
        return Status::OK;
    }

    // Can't construct the editor directly, need to make a main window instead because... reasons
    mainWindow.reset(new MainWindow2);
    CommandLineExporter exporter(mainWindow->mEditor);
    if (exporter.process(inputPath,
                         outputPaths,
                         parser.camera(),
                         parser.width(),
                         parser.height(),
                         parser.startFrame(),
                         parser.endFrame(),
                         parser.transparency()))
    {
        return Status::SAFE;
    }
    return Status::FAIL;
}

bool Pencil2D::isInstanceOpen()
{
    QDir appDir = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("Pencil2D");
    appDir.mkpath(".");
    mProcessLock.reset(new QLockFile(appDir.absoluteFilePath("pencil2d-process.lock")));
    if (!mProcessLock->tryLock(10))
    {
        qint64 pid = 0;
        QString lockAppName, lockHost;
        mProcessLock->getLockInfo(&pid, &lockAppName, &lockHost);
        if (pid > 0 && PlatformHandler::raiseWindowsOfProcessIfNamed(pid, QStringLiteral("pencil2d.exe")))
        {
            // 确有存活实例：把它的窗口拉到前台后安静退出。旧版在这里弹
            // 「不推荐多开」警告框且默认按钮=关闭——快速连点快捷方式时，
            // 弹窗被当成「点了没反应」，按默认键则整个静默退出
            return true;
        }
        // 锁里的进程已不在（崩溃/强杀残留）或 PID 被别的程序复用：
        // 清掉陈旧锁重试一次，别让用户永远点不开
        mProcessLock->removeStaleLockFile();
        if (mProcessLock->tryLock(100))
        {
            return false;
        }
        return true;
    }
    return false;
}

bool Pencil2D::event(QEvent* event)
{
    if (event->type() == QEvent::FileOpen)
    {
        auto fileOpenEvent = dynamic_cast<QFileOpenEvent*>(event);
        Q_ASSERT(fileOpenEvent);
        emit openFileRequested(fileOpenEvent->file());
        return true;
    }
    return QApplication::event(event);
}

void Pencil2D::installTranslators()
{
    QSettings setting(PENCIL2D, PENCIL2D);
    QString userLocale = setting.value(SETTING_LANGUAGE).toString();
    QLocale locale = userLocale.isEmpty() ? QLocale::system() : QLocale(userLocale);
    QLocale::setDefault(locale);

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    // In versions prior to 5.14, Qt's DOM implementation erroneously used
    // locale-dependent string conversion for double attributes (QTBUG-80068).
    // To work around this, we override the numeric locale category to use the
    // C locale.
    std::setlocale(LC_NUMERIC, "C");
#endif

    std::unique_ptr<QTranslator> qtTranslator(new QTranslator(this));
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (qtTranslator->load(locale, "qt", "_", QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
#else
    if (qtTranslator->load(locale, "qt", "_", QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
#endif
    {
        installTranslator(qtTranslator.release());
    }

    std::unique_ptr<QTranslator> pencil2DTranslator(new QTranslator(this));
    if (pencil2DTranslator->load(locale, "pencil", "_", ":/i18n/"))
    {
        installTranslator(pencil2DTranslator.release());
    }
}

void Pencil2D::prepareGuiStartup(const QString& inputPath)
{
    PlatformHandler::configurePlatformSpecificSettings();

    mainWindow.reset(new MainWindow2);
    connect(this, &Pencil2D::openFileRequested, mainWindow.get(), &MainWindow2::openFile);
    // if the saved session was maximized, show maximized from the first
    // frame: restoreGeometry() only sets the maximize flag on a hidden
    // window, and plain show() paints one normal-size frame before the
    // maximize is applied asynchronously (a visible small-then-full flash)
    if (mainWindow->isMaximized())
    {
        mainWindow->showMaximized();
    }
    else
    {
        mainWindow->show();
    }

    mainWindow->openStartupFile(inputPath);
}
