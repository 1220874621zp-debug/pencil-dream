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

#ifndef OCAEXPORTDIALOG_H
#define OCAEXPORTDIALOG_H

#include <QDialog>

class QComboBox;
class QLineEdit;
class QSpinBox;
class QCheckBox;

/** OCA 导出设置弹窗（主工具栏 OCA 按钮入口） */
class OcaExportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OcaExportDialog(QWidget* parent);

    void setCameras(const QStringList& cameraNames);
    void setRange(int startFrame, int endFrame);

    QString outputDir() const;
    int startFrame() const;
    int endFrame() const;
    QString cameraName() const;

private slots:
    void browseOutputDir();

private:
    QLineEdit* mDirEdit = nullptr;
    QSpinBox* mStartSpin = nullptr;
    QSpinBox* mEndSpin = nullptr;
    QComboBox* mCameraCombo = nullptr;
};

#endif // OCAEXPORTDIALOG_H
