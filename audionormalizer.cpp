#include "audionormalizer.h"
#include <QAudioDecoder>
#include <QAudioBuffer>
#include <QFileInfo>
#include <QDateTime>
#include <QUrl>
#include <cmath>

AudioNormalizer::AudioNormalizer(QObject *parent) : QObject(parent)
{
    timeout.setSingleShot(true);
    connect(&timeout, &QTimer::timeout, this, [this] { finish(false); });
}
void AudioNormalizer::cancel()
{
    ++generation;
    preparing = false;
    pending.clear();
    stopDecoder();
}
void AudioNormalizer::stopDecoder()
{
    timeout.stop();
    if (decoder) {
        decoder->disconnect(this);
        decoder->stop();
        decoder->deleteLater();
        decoder = nullptr;
    }
}
void AudioNormalizer::analyze(const QString &path)
{
    stopDecoder();
    const QFileInfo info(path);
    key = info.absoluteFilePath() + "|" + QString::number(info.size()) + "|" +
          QString::number(info.lastModified().toMSecsSinceEpoch());
    if (cache.contains(key)) {
        report(cache.value(key), true);
        return;
    }
    sumSquares = 0;
    samples = 0;
    decoder = new QAudioDecoder(this);
    connect(decoder, &QAudioDecoder::bufferReady, this, [this] {
        const auto buffer = decoder->read();
        if (!buffer.isValid()) return;
        for (qsizetype i = 0; i < buffer.sampleCount(); ++i) {
            double value = 0;
            switch (buffer.format().sampleFormat()) {
            case QAudioFormat::UInt8: value = (buffer.constData<quint8>()[i] - 128) / 128.0; break;
            case QAudioFormat::Int16: value = buffer.constData<qint16>()[i] / 32768.0; break;
            case QAudioFormat::Int32: value = buffer.constData<qint32>()[i] / 2147483648.0; break;
            case QAudioFormat::Float: value = buffer.constData<float>()[i]; break;
            default: finish(false); return;
            }
            if (!std::isfinite(value)) { finish(false); return; }
            sumSquares += value * value;
            ++samples;
        }
    });
    connect(decoder, &QAudioDecoder::finished, this, [this] { finish(true); });
    connect(decoder, QOverload<QAudioDecoder::Error>::of(&QAudioDecoder::error),
            this, [this](QAudioDecoder::Error) { finish(false); });
    timeout.start(15000);
    decoder->setSource(QUrl::fromLocalFile(path));
    if (decoder) decoder->start();
}
void AudioNormalizer::finish(bool success)
{
    success = success && samples > 0;
    const double rms = samples ? std::sqrt(sumSquares / samples) : 0;
    if (success) {
        if (cache.size() >= 256) cache.clear();
        cache.insert(key, rms);
    }
    stopDecoder();
    report(rms, success);
}
void AudioNormalizer::prepare(const QStringList &paths)
{
    cancel();
    pending = paths;
    nextIndex = 0;
    target = 0.063095734448;
    preparing = true;
    next();
}
void AudioNormalizer::next()
{
    if (!preparing) return;
    emit progress(nextIndex, pending.size());
    if (nextIndex == pending.size()) {
        preparing = false;
        pending.clear();
        emit prepared();
        return;
    }
    analyze(pending.at(nextIndex++));
}
void AudioNormalizer::report(double rms, bool success)
{
    if (preparing) {
        // Ignore silence and near-silence below -100 dBFS as reference material.
        if (success && rms > 0.00001) target = qMin(target, rms);
        const auto current = generation;
        QTimer::singleShot(0, this, [this, current] {
            if (current == generation) next();
        });
        return;
    }
    // Compare against the folder's quietest non-silent clip, capped at -24 dBFS.
    // Store RMS, not gain, so cached measurements work with different folders.
    const float gain = success && rms > 0 ? float(qMin(1.0, target / rms)) : 1.0f;
    emit completed(gain, success);
}
