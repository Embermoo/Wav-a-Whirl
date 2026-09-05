#ifndef AUDIONORMALIZER_H
#define AUDIONORMALIZER_H
#include <QObject>
#include <QHash>
#include <QTimer>
#include <QStringList>
class QAudioDecoder;
class AudioNormalizer : public QObject {
    Q_OBJECT
public:
    explicit AudioNormalizer(QObject *parent = nullptr);
    void analyze(const QString &path);
    void prepare(const QStringList &paths);
    void cancel();
signals:
    void completed(float gain, bool success);
    void prepared();
    void progress(int done, int total);
private:
    QAudioDecoder *decoder = nullptr;
    QTimer timeout;
    QHash<QString, double> cache;
    QStringList pending;
    int nextIndex = 0;
    bool preparing = false;
    quint64 generation = 0;
    double target = 0.063095734448;
    void next();
    void stopDecoder();
    void report(double rms, bool success);
    QString key;
    double sumSquares = 0;
    quint64 samples = 0;
    void finish(bool success);
};
#endif
