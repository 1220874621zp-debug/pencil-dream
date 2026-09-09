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

#include "ocaexportdialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

OcaExportDialog::OcaExportDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("导出 OCA"));
    setMinimumWidth(380);

    auto* form = new QFormLayout(this);

    mDirEdit = new QLineEdit(this);
    mDirEdit->setPlaceholderText(tr("选择导出位置…"));
    auto* browseBtn = new QPushButton(tr("浏览..."), this);
    auto* dirRow = new QHBoxLayout();
    dirRow->addWidget(mDirEdit, 1);
    dirRow->addWidget(browseBtn);
    form->addRow(tr("导出目录："), dirRow);

    mStartSpin = new QSpinBox(this);
    mStartSpin->setMinimum(1);
    mStartSpin->setMaximum(99999);
    mStartSpin->setValue(1);
    mEndSpin = new QSpinBox(this);
    mEndSpin->setMinimum(1);
    mEndSpin->setMaximum(99999);
    mEndSpin->setValue(1);
    auto* rangeRow = new QHBoxLayout();
    rangeRow->addWidget(mStartSpin);
    rangeRow->addWidget(new QLabel(tr("至"), this));
    rangeRow->addWidget(mEndSpin);
    rangeRow->addStretch(1);
    form->addRow(tr("帧范围："), rangeRow);

    mCameraCombo = new QComboBox(this);
    form->addRow(tr("拍摄机："), mCameraCombo);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("导出"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("取消"));
    form->addRow(QString(), buttons);

    connect(browseBtn, &QPushButton::clicked, this, &OcaExportDialog::browseOutputDir);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void OcaExportDialog::setCameras(const QStringList& cameraNames)
{
    mCameraCombo->clear();
    mCameraCombo->addItems(cameraNames);
}

void OcaExportDialog::setRange(int startFrame, int endFrame)
{
    mStartSpin->setValue(qMax(1, startFrame));
    mEndSpin->setValue(qMax(mStartSpin->value(), endFrame));
}

QString OcaExportDialog::outputDir() const
{
    return mDirEdit->text().trimmed();
}

int OcaExportDialog::startFrame() const
{
    return mStartSpin->value();
}

int OcaExportDialog::endFrame() const
{
    return mEndSpin->value();
}

QString OcaExportDialog::cameraName() const
{
    return mCameraCombo->currentText();
}

void OcaExportDialog::browseOutputDir()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("选择导出目录"),
                                                           mDirEdit->text().trimmed());
    if (!dir.isEmpty())
    {
        mDirEdit->setText(dir);
    }
}
