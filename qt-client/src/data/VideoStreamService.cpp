#include "VideoStreamService.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QUrlQuery>

#include "data/ApiClient.h"

VideoStreamService::VideoStreamService(ApiClient *apiClient, QObject *parent)
    : QObject(parent)
    , m_apiClient(apiClient)
{
}

QString VideoStreamService::viewerUrl() const { return m_viewerUrl; }
QString VideoStreamService::state() const { return m_state; }
QString VideoStreamService::lastError() const { return m_lastError; }
bool VideoStreamService::loading() const { return m_loading; }

void VideoStreamService::start(const QString &deviceId)
{
    stop();
    if (m_apiClient == nullptr || !m_apiClient->ready() || deviceId.trimmed().isEmpty())
    {
        setError(QStringLiteral("登录后选择一台设备再打开图传"));
        setState(QStringLiteral("Unavailable"));
        return;
    }

    m_loading = true;
    emit loadingChanged();
    setError({});
    setState(QStringLiteral("Requesting credentials"));
    QNetworkReply *reply = m_apiClient->post(QStringLiteral("/devices/%1/video/credentials").arg(deviceId.trimmed()), QByteArrayLiteral("{}"));
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (m_reply != reply)
        {
            reply->deleteLater();
            return;
        }
        m_reply = nullptr;
        m_loading = false;
        emit loadingChanged();
        if (reply == nullptr)
            return;

        const QByteArray body = reply->readAll();
        const QString networkError = reply->error() == QNetworkReply::NoError ? QString() : reply->errorString();
        reply->deleteLater();
        if (!networkError.isEmpty())
        {
            setError(networkError);
            setState(QStringLiteral("Error"));
            return;
        }

        const QJsonObject response = QJsonDocument::fromJson(body).object();
        const QString whepUrl = response.value(QStringLiteral("whep_url")).toString();
        const QString username = response.value(QStringLiteral("username")).toString();
        const QString password = response.value(QStringLiteral("password")).toString();
        if (whepUrl.isEmpty() || username.isEmpty() || password.isEmpty())
        {
            setError(QStringLiteral("服务器返回的图传凭据无效"));
            setState(QStringLiteral("Error"));
            return;
        }

        QUrl viewer(whepUrl);
        const QString whepPath = viewer.path();
        const int whepOffset = whepPath.lastIndexOf(QStringLiteral("/whep"));
        const int streamOffset = whepOffset > 0 ? whepPath.lastIndexOf(QLatin1Char('/'), whepOffset - 1) : -1;
        const QString viewerBasePath = streamOffset > 0 ? whepPath.left(streamOffset) : QString();
        viewer.setPath(viewerBasePath + QStringLiteral("/viewer.html"));
        viewer.setQuery(QUrlQuery());
        QUrlQuery fragment;
        fragment.addQueryItem(QStringLiteral("whep"), whepUrl);
        fragment.addQueryItem(QStringLiteral("username"), username);
        fragment.addQueryItem(QStringLiteral("password"), password);
        fragment.addQueryItem(QStringLiteral("turnUrl"), response.value(QStringLiteral("turn_url")).toString());
        fragment.addQueryItem(QStringLiteral("turnUsername"), response.value(QStringLiteral("turn_username")).toString());
        fragment.addQueryItem(QStringLiteral("turnPassword"), response.value(QStringLiteral("turn_password")).toString());
        viewer.setFragment(fragment.toString(QUrl::FullyEncoded));
        m_viewerUrl = viewer.toString(QUrl::FullyEncoded);
        emit viewerUrlChanged();
        setState(QStringLiteral("Connecting"));
    });
}

void VideoStreamService::stop()
{
    if (m_reply != nullptr)
    {
        m_reply->abort();
        m_reply = nullptr;
    }
    if (!m_viewerUrl.isEmpty())
    {
        m_viewerUrl.clear();
        emit viewerUrlChanged();
    }
    if (m_loading)
    {
        m_loading = false;
        emit loadingChanged();
    }
    setState(QStringLiteral("Idle"));
}

void VideoStreamService::setState(const QString &value)
{
    if (m_state == value)
        return;
    m_state = value;
    emit stateChanged();
}

void VideoStreamService::setError(const QString &value)
{
    if (m_lastError == value)
        return;
    m_lastError = value;
    emit lastErrorChanged();
}
