/*
 * Pencil Dream - 脚本管理器
 * 扫描用户脚本目录（AppData/scripts）下的 *.js，载入后把脚本注册的
 * 命令列进「脚本」菜单。菜单固定项：查看脚本输出 / 重新载入 / 打开脚本文件夹。
 */

#ifndef SCRIPTMANAGER_H
#define SCRIPTMANAGER_H

#include <QObject>
#include <QList>
#include <QMenu>

class Editor;
class ScriptHost;

struct ScriptCommandEntry
{
    ScriptHost* host = nullptr;
    QString label;       // registerCommand 的标签（菜单文字）
    QString fileBase;    // 来源脚本文件名（标签撞名时消歧显示）
};

class ScriptManager : public QObject
{
    Q_OBJECT

public:
    explicit ScriptManager(Editor* editor, QWidget* dialogParent, QObject* parent = nullptr);
    ~ScriptManager() override;

    QMenu* menu() const { return mMenu; }

    /** 重扫脚本目录并重建菜单（构造时已调用一次）。 */
    void reload();

    /** 用户脚本目录（%APPDATA%/Pencil2D/Pencil2D/scripts，随设置自动创建）。 */
    static QString scriptsPath();

private:
    void ensureScriptsDir();       // 建目录 + 释放内置示例脚本（不覆盖已存在的文件）
    void rebuildCommandActions();
    void runCommand(int commandIndex);
    void showOutput(const QString& title, const QStringList& lines);

    Editor* mEditor = nullptr;
    QWidget* mDialogParent = nullptr;
    QMenu* mMenu = nullptr;
    QAction* mCommandsSeparator = nullptr;
    QList<ScriptHost*> mHosts;
    QList<ScriptCommandEntry> mCommands;
    QStringList mLastOutput;   // 最近一次脚本运行的 log（「查看脚本输出」回看）
};

#endif // SCRIPTMANAGER_H
