/*
 * Pencil Dream - 脚本管理器实现
 */

#include "scriptmanager.h"
#include "scriptapi.h"

#include <QAction>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QMessageBox>
#include <QStandardPaths>
#include <QUrl>

#include "editor.h"
#include "scribblearea.h"

namespace
{
// 首次运行释放到脚本目录的内置示例（:/scripts/<名字> → scriptsPath()/<名字>，不覆盖）
const char* kBuiltinScripts[] = {
    "scaleKeyFramesToCanvas.js",
};
} // namespace

ScriptManager::ScriptManager(Editor* editor, QWidget* dialogParent, QObject* parent)
    : QObject(parent)
    , mEditor(editor)
    , mDialogParent(dialogParent)
{
    mMenu = new QMenu(tr("脚本"), dialogParent);
    mMenu->setSeparatorsCollapsible(false);

    QAction* outputAction = mMenu->addAction(tr("查看脚本输出"));
    outputAction->setStatusTip(tr("查看上一次脚本运行的 log 输出"));
    connect(outputAction, &QAction::triggered, this, [this]
    {
        // 未运行过脚本时给一条可操作的提示，而不是空框
        showOutput(tr("脚本输出"), { tr("还没有脚本输出。运行脚本后，脚本里 log() 的内容会显示在这里。") });
    });

    QAction* reloadAction = mMenu->addAction(tr("重新载入脚本"));
    reloadAction->setStatusTip(tr("重新扫描脚本文件夹（增删改 .js 后无需重启）"));
    connect(reloadAction, &QAction::triggered, this, &ScriptManager::reload);

    QAction* openAction = mMenu->addAction(tr("打开脚本文件夹"));
    openAction->setStatusTip(tr("把 .js 脚本放进该文件夹，即可出现在「脚本」菜单"));
    connect(openAction, &QAction::triggered, this, []
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(scriptsPath()));
    });

    mCommandsSeparator = mMenu->addSeparator();

    reload();
}

ScriptManager::~ScriptManager() = default;

QString ScriptManager::scriptsPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/scripts");
}

void ScriptManager::ensureScriptsDir()
{
    const QString path = scriptsPath();
    QDir dir(path);
    if (!dir.exists())
    {
        dir.mkpath(path);
    }
    for (const char* name : kBuiltinScripts)
    {
        const QString target = path + QLatin1Char('/') + QString::fromLatin1(name);
        if (QFile::exists(target)) { continue; }
        QFile src(QStringLiteral(":/scripts/") + QString::fromLatin1(name));
        if (src.open(QIODevice::ReadOnly))
        {
            QFile dst(target);
            if (dst.open(QIODevice::WriteOnly))
            {
                dst.write(src.readAll());
            }
        }
    }
}

void ScriptManager::reload()
{
    qDeleteAll(mHosts);
    mHosts.clear();
    mCommands.clear();

    ensureScriptsDir();

    const QDir dir(scriptsPath());
    const auto entries = dir.entryInfoList({ QStringLiteral("*.js") }, QDir::Files, QDir::Name);
    QStringList loadErrors;
    for (const QFileInfo& entry : entries)
    {
        auto* host = new ScriptHost(mEditor, mDialogParent, this);
        const QString error = host->loadScript(entry.absoluteFilePath());
        if (!error.isEmpty())
        {
            loadErrors.append(QStringLiteral("[%1] %2").arg(entry.fileName(), error));
            delete host;
            continue;
        }
        const QStringList labels = host->commandLabels();
        if (labels.isEmpty()) { continue; }
        mHosts.append(host);
        for (const QString& label : labels)
        {
            mCommands.append({ host, label, entry.completeBaseName() });
        }
    }
    if (!loadErrors.isEmpty())
    {
        QMessageBox::warning(mDialogParent, tr("脚本"),
                             tr("以下脚本载入失败：\n\n%1").arg(loadErrors.join(QLatin1Char('\n'))));
    }
    rebuildCommandActions();
}

void ScriptManager::rebuildCommandActions()
{
    // 清掉上一轮的命令项（分隔线之后的全部动作）
    const int separatorIndex = mMenu->actions().indexOf(mCommandsSeparator);
    const QList<QAction*> actions = mMenu->actions();
    for (int i = actions.size() - 1; i > separatorIndex; --i)
    {
        mMenu->removeAction(actions[i]);
        actions[i]->deleteLater();
    }

    if (mCommands.isEmpty())
    {
        QAction* empty = mMenu->addAction(tr("未发现脚本（将 .js 放入脚本文件夹）"));
        empty->setEnabled(false);
        return;
    }

    // 标签撞名时附加来源文件名消歧
    QHash<QString, int> labelCount;
    for (const ScriptCommandEntry& entry : mCommands)
    {
        labelCount[entry.label]++;
    }
    int index = 0;
    for (const ScriptCommandEntry& entry : mCommands)
    {
        QString text = entry.label;
        if (labelCount.value(entry.label) > 1)
        {
            text += QStringLiteral("（%1）").arg(entry.fileBase);
        }
        QAction* action = mMenu->addAction(text);
        const int commandIndex = index++;
        connect(action, &QAction::triggered, this, [this, commandIndex]
        {
            runCommand(commandIndex);
        });
    }
}

void ScriptManager::runCommand(int commandIndex)
{
    if (commandIndex < 0 || commandIndex >= mCommands.size()) { return; }
    ScriptHost* host = mCommands[commandIndex].host;

    const QString error = host->runCommand(mCommands[commandIndex].label);

    // 显示缓存失效：脚本改过的帧逐帧作废 + 广播多帧修改（时间轴缩略图等）
    const QList<int> touched = host->touchedFrames();
    if (!touched.isEmpty())
    {
        ScribbleArea* canvas = mEditor->getScribbleArea();
        for (int frame : touched)
        {
            if (canvas != nullptr) { canvas->onFrameModified(frame); }
        }
        emit mEditor->framesModified();
        host->clearTouchedFrames();
    }

    const QStringList output = host->takeOutput();
    if (!error.isEmpty())
    {
        QString text = error;
        if (!output.isEmpty())
        {
            text += QStringLiteral("\n\n——— 脚本输出 ———\n") + output.join(QLatin1Char('\n'));
        }
        QMessageBox::warning(mDialogParent, tr("脚本错误"), text);
    }
    else if (!output.isEmpty())
    {
        showOutput(tr("脚本输出"), output);
    }
}

void ScriptManager::showOutput(const QString& title, const QStringList& lines)
{
    QMessageBox box(mDialogParent);
    box.setWindowTitle(tr("脚本"));
    box.setIcon(QMessageBox::Information);
    box.setText(title);
    box.setDetailedText(lines.join(QLatin1Char('\n')));
    box.setTextInteractionFlags(Qt::TextSelectableByMouse);
    box.exec();
}
