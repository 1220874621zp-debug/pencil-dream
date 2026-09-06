/*
    GapFill ONNX Runtime backend for Pencil2D.

    Wraps the frozen GapFill unet32.onnx model behind gap_assist::
    InferenceBackend. onnxruntime.dll is loaded at runtime through
    QLibrary so a missing runtime degrades to the rule-based
    predictor instead of failing to start.

    Core algorithm: Copyright (c) 2026 marc2825 (GapFill, MIT).
*/
#ifndef GAPFILL_ONNXBACKEND_H
#define GAPFILL_ONNXBACKEND_H

#include <QString>
#include <QVector>

#include "predictors/onnx_predictor_stub.hpp"

namespace gap_assist {

// Default model search locations: <exe>/models/unet32.onnx and
// <AppData>/pencil2d/models/unet32.onnx.
QString defaultModelPath();

class OrtOnnxBackend final : public InferenceBackend {
public:
    // errorOut (optional) receives a human readable failure reason.
    explicit OrtOnnxBackend(const QString& modelPath, QString* errorOut = nullptr);
    ~OrtOnnxBackend() override;

    OrtOnnxBackend(const OrtOnnxBackend&) = delete;
    OrtOnnxBackend& operator=(const OrtOnnxBackend&) = delete;

    bool isValid() const { return mSession != nullptr; }

    ModelContract contract() const override;
    std::vector<float> run(std::span<const float> input) const override;

private:
    void* mSession = nullptr; // OrtSession*, released in destructor
    ModelContract mContract;
    QString mInputName;
    QString mOutputName;
};

} // namespace gap_assist

#endif // GAPFILL_ONNXBACKEND_H
