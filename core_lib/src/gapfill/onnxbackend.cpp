#include "onnxbackend.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QLibrary>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>

#include <cstring>
#include <memory>

#include "onnxruntime/onnxruntime_c_api.h"

namespace gap_assist {
namespace {

using OrtGetApiBaseFn = const OrtApiBase* (*)(void);

// Runtime-loaded onnxruntime: a missing DLL must never be a startup
// failure, the dialog falls back to the rule-based predictor instead.
struct OrtLib {
    QLibrary mLib;
    const OrtApi* mApi = nullptr;
    bool mOk = false;

    OrtLib() : mLib(QStringLiteral("onnxruntime")) {
        if (!mLib.load()) { return; }
        const auto fn = reinterpret_cast<OrtGetApiBaseFn>(
                    mLib.resolve("OrtGetApiBase"));
        if (!fn) { return; }
        const OrtApiBase* const base = fn();
        if (!base) { return; }
        mApi = base->GetApi(ORT_API_VERSION);
        mOk = mApi != nullptr;
    }
};

OrtLib& ortLib()
{
    static OrtLib lib;
    return lib;
}

// One OrtEnv for the process; the session is owned by OrtOnnxBackend.
OrtEnv* sharedEnv(const OrtApi* const api, QString& errOut)
{
    static OrtEnv* env = nullptr;
    static QMutex mutex;
    QMutexLocker lock(&mutex);
    if (!env) {
        if (api->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "GapFill", &env) != nullptr) {
            errOut = QStringLiteral("onnxruntime CreateEnv failed");
            return nullptr;
        }
    }
    return env;
}

QString sha256File(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { return QString(); }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) { return QString(); }
    return QString::fromLatin1(hash.result().toHex());
}

QString tensorTypeString(const ONNXTensorElementDataType type)
{
    return type == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT
            ? QStringLiteral("tensor(float)") : QString();
}

// Reads names + static shapes of input 0 / output 0 into the contract.
bool fillContract(const OrtApi* const api, const OrtSession* const session,
                  ModelContract& contract, QString& errOut)
{
    contract.inputCount = 0;
    contract.outputCount = 0;
    size_t count = 0;
    if (api->SessionGetInputCount(session, &count) != nullptr || count != 1) {
        errOut = QStringLiteral("GapFill model must have exactly one input");
        return false;
    }
    contract.inputCount = count;
    if (api->SessionGetOutputCount(session, &count) != nullptr || count != 1) {
        errOut = QStringLiteral("GapFill model must have exactly one output");
        return false;
    }
    contract.outputCount = count;

    OrtAllocator* alloc = nullptr;
    if (api->GetAllocatorWithDefaultOptions(&alloc) != nullptr) {
        errOut = QStringLiteral("onnxruntime allocator failed");
        return false;
    }
    char* inNameC = nullptr;
    char* outNameC = nullptr;
    if (api->SessionGetInputName(session, 0, alloc, &inNameC) != nullptr ||
        api->SessionGetOutputName(session, 0, alloc, &outNameC) != nullptr) {
        errOut = QStringLiteral("cannot query model tensor names");
        return false;
    }
    contract.inputName = QString::fromLatin1(inNameC).toStdString();
    contract.outputName = QString::fromLatin1(outNameC).toStdString();
    api->AllocatorFree(alloc, inNameC);
    api->AllocatorFree(alloc, outNameC);

    const auto readShape = [&](const bool input,
                               std::array<std::int64_t, 4>& shape) {
        OrtTypeInfo* info = nullptr;
        const OrtStatus* status = input
                ? api->SessionGetInputTypeInfo(session, 0, &info)
                : api->SessionGetOutputTypeInfo(session, 0, &info);
        if (status != nullptr) {
            api->ReleaseStatus(const_cast<OrtStatus*>(status));
            errOut = QStringLiteral("cannot read model tensor info");
            return false;
        }
        const OrtTensorTypeAndShapeInfo* tensorInfo = nullptr;
        if (api->CastTypeInfoToTensorInfo(info, &tensorInfo) != nullptr ||
            tensorInfo == nullptr) {
            api->ReleaseTypeInfo(info);
            errOut = QStringLiteral("model tensor is not a plain tensor");
            return false;
        }
        ONNXTensorElementDataType type = ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED;
        api->GetTensorElementType(tensorInfo, &type);
        const QString typeString = tensorTypeString(type);
        size_t dimCount = 0;
        api->GetDimensionsCount(tensorInfo, &dimCount);
        bool ok = typeString.isEmpty() || dimCount != 4;
        std::vector<int64_t> dims(dimCount, 0);
        if (!ok) {
            api->GetDimensions(tensorInfo, dims.data(), dimCount);
            for (size_t i = 0; i < 4; ++i) { shape[i] = dims[i]; }
        }
        if (input) { contract.inputType = typeString.toStdString(); }
        else { contract.outputType = typeString.toStdString(); }
        api->ReleaseTypeInfo(info);
        if (ok) {
            errOut = QStringLiteral("model tensor must be float32 rank-4");
            return false;
        }
        return true;
    };

    return readShape(true, contract.inputShape) &&
           readShape(false, contract.outputShape);
}

} // namespace

QString defaultModelPath()
{
    const QString fileName = QStringLiteral("unet32.onnx");
    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/models/") + fileName,
    };
    for (const QString& candidate : candidates) {
        if (QFile::exists(candidate)) { return candidate; }
    }
    return QString();
}

OrtOnnxBackend::OrtOnnxBackend(const QString& modelPath, QString* errorOut)
{
    QString error;
    const OrtApi* const api = ortLib().mApi;
    if (api == nullptr) {
        error = QStringLiteral("未找到 onnxruntime.dll（可执行文件旁缺失）");
    } else if (!QFile::exists(modelPath)) {
        error = QStringLiteral("未找到模型文件 %1").arg(modelPath);
    } else {
        mContract.artifactSha256 = sha256File(modelPath).toStdString();
        OrtEnv* const env = sharedEnv(api, error);
        if (env != nullptr) {
            OrtSessionOptions* options = nullptr;
            if (api->CreateSessionOptions(&options) != nullptr) {
                error = QStringLiteral("onnxruntime CreateSessionOptions failed");
            } else {
                api->SetIntraOpNumThreads(
                            options, qBound(1, QThread::idealThreadCount(), 4));
                api->SetSessionGraphOptimizationLevel(options, ORT_ENABLE_ALL);
                OrtSession* session = nullptr;
                const std::wstring widePath = QDir::toNativeSeparators(modelPath).toStdWString();
                const OrtStatus* status = api->CreateSession(
                            env, widePath.c_str(), options, &session);
                api->ReleaseSessionOptions(options);
                if (status != nullptr) {
                    error = QString::fromLatin1(api->GetErrorMessage(status));
                    api->ReleaseStatus(const_cast<OrtStatus*>(status));
                } else {
                    mSession = session;
                    if (!fillContract(api, session, mContract, error)) {
                        api->ReleaseSession(session);
                        mSession = nullptr;
                    }
                }
            }
        }
    }
    if (errorOut != nullptr) { *errorOut = error; }
}

OrtOnnxBackend::~OrtOnnxBackend()
{
    if (mSession != nullptr) {
        if (const OrtApi* const api = ortLib().mApi) {
            api->ReleaseSession(static_cast<OrtSession*>(mSession));
        }
    }
}

ModelContract OrtOnnxBackend::contract() const
{
    return mContract;
}

std::vector<float> OrtOnnxBackend::run(std::span<const float> input) const
{
    if (mSession == nullptr) {
        throw std::runtime_error("GapFill ONNX backend is not loaded.");
    }
    const OrtApi* const api = ortLib().mApi;
    const std::int64_t dims[4] = {
        mContract.inputShape[0], mContract.inputShape[1],
        mContract.inputShape[2], mContract.inputShape[3]};
    OrtMemoryInfo* memoryInfo = nullptr;
    if (api->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault,
                                 &memoryInfo) != nullptr) {
        throw std::runtime_error("GapFill CreateCpuMemoryInfo failed.");
    }
    OrtValue* inputValue = nullptr;
    if (api->CreateTensorWithDataAsOrtValue(
                memoryInfo, const_cast<float*>(input.data()),
                input.size() * sizeof(float), dims, 4,
                ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
                &inputValue) != nullptr) {
        api->ReleaseMemoryInfo(memoryInfo);
        throw std::runtime_error("GapFill input tensor creation failed.");
    }
    const char* inputNames[1] = { mContract.inputName.c_str() };
    const char* outputNames[1] = { mContract.outputName.c_str() };
    OrtValue* outputValue = nullptr;
    const OrtStatus* runStatus = api->Run(
                static_cast<OrtSession*>(mSession), nullptr,
                inputNames, const_cast<const OrtValue* const*>(&inputValue),
                1, outputNames, 1, &outputValue);
    api->ReleaseValue(inputValue);
    api->ReleaseMemoryInfo(memoryInfo);
    if (runStatus != nullptr) {
        const std::string message(api->GetErrorMessage(runStatus));
        api->ReleaseStatus(const_cast<OrtStatus*>(runStatus));
        throw std::runtime_error("GapFill inference failed: " + message);
    }

    float* outputData = nullptr;
    if (api->GetTensorMutableData(outputValue,
                                  reinterpret_cast<void**>(&outputData)) != nullptr) {
        api->ReleaseValue(outputValue);
        throw std::runtime_error("GapFill output read failed.");
    }
    OrtTensorTypeAndShapeInfo* shapeInfo = nullptr;
    size_t elementCount = 0;
    if (api->GetTensorTypeAndShape(outputValue, &shapeInfo) == nullptr) {
        api->GetTensorShapeElementCount(shapeInfo, &elementCount);
        api->ReleaseTensorTypeAndShapeInfo(shapeInfo);
    }
    std::vector<float> result(outputData, outputData + elementCount);
    api->ReleaseValue(outputValue);
    return result;
}

} // namespace gap_assist
