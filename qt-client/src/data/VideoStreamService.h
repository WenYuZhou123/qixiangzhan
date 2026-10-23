#pragma once

#include <QObject>
#include <QUrl>

class ApiClient;
class QNetworkReply;

class VideoStreamService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString viewerUrl READ viewerUrl NOTIFY viewerUrlChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)

public:
    explicit VideoStreamService(ApiClient *apiClient, QObject *parent = nullptr);

    QString viewerUrl() const;
    QString state() const;
    QString lastError() const;
    bool loading() const;

    Q_INVOKABLE void start(const QString &deviceId);
    Q_INVOKABLE void stop();

signals:
    void viewerUrlChanged();
    void stateChanged();
    void lastErrorChanged();
    void loadingChanged();

private:
    void setState(const QString &value);
    void setError(const QString &value);

    ApiClient *m_apiClient = nullptr;
    QNetworkReply *m_reply = nullptr;
    QString m_viewerUrl;
    QString m_state = QStringLiteral("Idle");
    QString m_lastError;
    bool m_loading = false;
};
