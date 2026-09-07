/*

    GapFill gap-fill assistant for Pencil2D bitmap layers.
    Detects small uncolored enclosed regions ("塗り残し") on the
    current coloring layer and fills them with a recommended color.

    Core algorithm: Copyright (c) 2026 marc2825 (GapFill, MIT).

*/
#ifndef GAPFILLDIALOG_H
#define GAPFILLDIALOG_H

#include <QDialog>
#include <QRect>
#include <memory>
#include <vector>

#include "gapfill/onnxbackend.h"
#include "gapfill/core/image_types.hpp"

class Editor;

namespace Ui {
class GapFillDialog;
}

class GapFillDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GapFillDialog(QWidget* parent = nullptr);
    ~GapFillDialog() override;

    void setCore(Editor* editor);
    void initUI();

private slots:
    void detectGaps();
    void checkHighConfidence();
    void clearChecks();
    void applySelected();
    void gapSizeChanged(int index);
    void cellChanged(int row, int column);

private:
    void refreshLineLayerCombo();
    void setStatus(const QString& text);
    int appliedCheckColumnWidth() const;

    Ui::GapFillDialog* ui = nullptr;
    Editor* mEditor = nullptr;

    std::vector<gap_assist::GapCandidate> mGaps;
    QRect mCanvasRect;      // detection canvas, document coordinates
    int mColoringLayerId = -1;
    int mColoringKeyPos = 0;
    bool mSuppressCellChanged = false;
    std::vector<bool> mApplied;

    std::unique_ptr<gap_assist::OrtOnnxBackend> mBackend;
};

#endif // GAPFILLDIALOG_H
