#include "ApiClient.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "auth/AuthSession.h"

ApiClient::ApiClient(AuthSession *authSession, QObject *parent)
    : QObject(parent)
    , m_authSession(authSession)
{
}

bool ApiClient::ready() const
{
    return m_authSession != nullptr &&
           !m_authSession->apiBaseUrl().trimmed().isEmpty() &&
           m_authSession->remoteAuthenticated();
}

QString ApiClient::baseUrl() const
{
    return m_authSession != nullptr ? m_authSession->apiBaseUrl().trimmed() : QString();
}

QNetworkReply *ApiClient::get(const QString &path, const QUrlQuery &query)
{
    if (m_authSession != nullptr)
    {
        m_authSession->ensureFreshToken();
    }
    return m_network.get(buildRequest(path, query));
}

QNetworkReply *ApiClient::post(const QString &path, const QByteArray &payload)
{
    if (m_authSession != nullptr)
    {
        m_authSession->ensureFreshToken();
    }
    QNetworkRequest request = buildRequest(path);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    return m_network.post(request, payload);
}

QNetworkReply *ApiClient::patch(const QString &path, const QByteArray &payload)
{
    if (m_authSession != nullptr)
    {
        m_authSession->ensureFreshToken();
    }
    QNetworkRequest request = buildRequest(path);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    return m_network.sendCustomRequest(request, QByteArrayLiteral("PATCH"), payload);
}

QNetworkRequest ApiClient::buildRequest(const QString &path, const QUrlQuery &query) const
{
    QString normalizedBase = baseUrl();
    if (normalizedBase.endsWith('/'))
    {
        normalizedBase.chop(1);
    }

    QString normalizedPath = path.trimmed();
    if (!normalizedPath.startsWith('/'))
    {
        normalizedPath.prepend('/');
    }

    QUrl url(normalizedBase + normalizedPath);
    if (!query.isEmpty())
    {
        url.setQuery(query);
    }

    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/json");
    if (m_authSession != nullptr && !m_authSession->token().trimmed().isEmpty())
    {
        const QByteArray token = QByteArrayLiteral("Bearer ") + m_authSession->token().toUtf8();
        request.setRawHeader("Authorization", token);
    }
    return request;
}
