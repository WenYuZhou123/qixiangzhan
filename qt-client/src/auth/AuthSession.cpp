#include "AuthSession.h"

#include <QDateTime>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

#include "data/SecureCredentialStore.h"
#include "data/SQLiteCache.h"

namespace
{
#ifdef QXZ_CLIENT_DEFAULT_API_BASE
constexpr auto kDefaultApiBase = QXZ_CLIENT_DEFAULT_API_BASE;
#else
constexpr auto kDefaultApiBase = "https://api.qixiangzhan.online/api/v1";
#endif
constexpr auto kLocalAdminUser = "admin";
constexpr auto kLocalAdminPass = "admin123";

QString secureStorageWarning()
{
    return QStringLiteral("Secure storage is unavailable. This login will not be remembered.");
}
}

AuthSession::AuthSession(SQLiteCache *cache, QObject *parent)
    : QObject(parent)
    , m_cache(cache)
    , m_secureStore(new SecureCredentialStore(this))
{
    loadFromCache();
}

QString AuthSession::apiBaseUrl() const
{
    return m_apiBaseUrl;
}

void AuthSession::setApiBaseUrl(const QString &url)
{
    const QString trimmed = url.trimmed();
    if (m_apiBaseUrl == trimmed || trimmed.isEmpty())
    {
        return;
    }

    m_apiBaseUrl = trimmed;
    if (m_cache != nullptr)
    {
        m_cache->setSetting(QStringLiteral("api.baseUrl"), m_apiBaseUrl);
    }
    emit apiBaseUrlChanged();
}

QString AuthSession::username() const
{
    return m_session.username;
}

QString AuthSession::displayName() const
{
    return m_session.displayName;
}

QString AuthSession::role() const
{
    return m_session.role;
}

QString AuthSession::token() const
{
    return m_session.token;
}

bool AuthSession::authenticated() const
{
    return !m_session.token.isEmpty();
}

bool AuthSession::remoteAuthenticated() const
{
    return !m_session.token.isEmpty() && !m_session.localFallback;
}

bool AuthSession::admin() const
{
    return m_session.role == QStringLiteral("admin");
}

bool AuthSession::guestMode() const
{
    return m_session.role == QStringLiteral("guest");
}

bool AuthSession::localFallback() const
{
    return m_session.localFallback;
}

bool AuthSession::busy() const
{
    return m_busy;
}

QString AuthSession::lastError() const
{
    return m_lastError;
}

void AuthSession::login(const QString &usernameValue, const QString &password)
{
    if (m_busy)
    {
        return;
    }

    const QString normalizedUser = usernameValue.trimmed();
    if (normalizedUser.isEmpty() || password.isEmpty())
    {
        setLastError(QStringLiteral("请输入用户名和密码"));
        return;
    }

    setBusy(true);
    setLastError(QString());

    QNetworkRequest request(QUrl(m_apiBaseUrl + QStringLiteral("/auth/login")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QJsonObject body;
    body.insert(QStringLiteral("username"), normalizedUser);
    body.insert(QStringLiteral("password"), password);
    QNetworkReply *reply = m_network.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply, normalizedUser, password]() {
        handleReply(reply, normalizedUser, password);
    });
}

void AuthSession::loginAsGuest()
{
    if (m_busy)
    {
        return;
    }

    clearCachedSecrets();

    UserSession session;
    session.username = QStringLiteral("guest");
    session.displayName = QStringLiteral("游客模式");
    session.role = QStringLiteral("guest");
    session.token = QStringLiteral("guest-preview");
    session.localFallback = true;

    finishLogin(session);
    setBusy(false);
    setLastError(QStringLiteral("游客模式仅浏览本地缓存与演示状态，不连接远程控制。"));
}

void AuthSession::logout()
{
    if (!m_session.localFallback && !m_session.refreshToken.isEmpty())
    {
        QNetworkRequest request(QUrl(m_apiBaseUrl + QStringLiteral("/auth/logout")));
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        const QJsonObject body{{QStringLiteral("refresh_token"), m_session.refreshToken}};
        m_network.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    }

    clearCachedSecrets();
    m_session = UserSession{};
    if (m_cache != nullptr)
    {
        m_cache->setSetting(QStringLiteral("auth.username"), QString());
        m_cache->setSetting(QStringLiteral("auth.displayName"), QString());
        m_cache->setSetting(QStringLiteral("auth.role"), QString());
    }
    setLastError(QString());
    emit sessionChanged();
}

bool AuthSession::ensureFreshToken(int minRemainingSeconds)
{
    if (m_session.localFallback)
    {
        return false;
    }

    if (m_session.token.isEmpty() || m_session.refreshToken.isEmpty())
    {
        return remoteAuthenticated();
    }

    const QDateTime accessExpiry = QDateTime::fromString(m_session.accessExpiresAt, Qt::ISODate);
    if (accessExpiry.isValid() && QDateTime::currentDateTimeUtc().secsTo(accessExpiry) > minRemainingSeconds)
    {
        return true;
    }

    return refreshWithToken();
}

void AuthSession::invalidateRemoteSession(const QString &message)
{
    if (m_session.localFallback)
    {
        return;
    }

    clearCachedSecrets();
    m_session = UserSession{};
    if (m_cache != nullptr)
    {
        m_cache->setSetting(QStringLiteral("auth.username"), QString());
        m_cache->setSetting(QStringLiteral("auth.displayName"), QString());
        m_cache->setSetting(QStringLiteral("auth.role"), QString());
    }
    setBusy(false);
    setLastError(message);
    emit sessionChanged();
}

void AuthSession::finishLogin(const UserSession &session)
{
    m_session = session;
    if (m_cache != nullptr)
    {
        m_cache->setSetting(QStringLiteral("auth.username"), m_session.username);
        m_cache->setSetting(QStringLiteral("auth.displayName"), m_session.displayName);
        m_cache->setSetting(QStringLiteral("auth.role"), m_session.role);
    }

    if (!m_session.localFallback)
    {
        persistSessionSecrets();
    }

    emit sessionChanged();
}

void AuthSession::setBusy(bool busyValue)
{
    if (m_busy == busyValue)
    {
        return;
    }

    m_busy = busyValue;
    emit busyChanged();
}

void AuthSession::setLastError(const QString &message)
{
    if (m_lastError == message)
    {
        return;
    }

    m_lastError = message;
    emit lastErrorChanged();
}

void AuthSession::loadFromCache()
{
    if (m_cache == nullptr)
    {
        m_apiBaseUrl = QString::fromLatin1(kDefaultApiBase);
        return;
    }

    m_apiBaseUrl = m_cache->setting(QStringLiteral("api.baseUrl"), QString::fromLatin1(kDefaultApiBase));
    m_session.username = m_cache->setting(QStringLiteral("auth.username"));
    m_session.displayName = m_cache->setting(QStringLiteral("auth.displayName"));
    m_session.role = m_cache->setting(QStringLiteral("auth.role"));

    migrateLegacySecretCache();
    loadSecureSession();
}

void AuthSession::handleReply(QNetworkReply *reply, const QString &usernameValue, const QString &password)
{
    const QByteArray payload = reply->readAll();
    const QJsonDocument json = QJsonDocument::fromJson(payload);

    if (reply->error() == QNetworkReply::NoError && json.isObject())
    {
        const QJsonObject root = json.object();
        UserSession session;
        session.username = root.value(QStringLiteral("username")).toString(usernameValue);
        session.displayName = root.value(QStringLiteral("display_name")).toString(session.username);
        session.role = root.value(QStringLiteral("role")).toString(QStringLiteral("operator"));
        session.token = root.value(QStringLiteral("access_token")).toString();
        session.refreshToken = root.value(QStringLiteral("refresh_token")).toString();
        session.accessExpiresAt = root.value(QStringLiteral("access_expires_at")).toString();
        session.refreshExpiresAt = root.value(QStringLiteral("refresh_expires_at")).toString();
        if (!session.token.isEmpty())
        {
            finishLogin(session);
            if (m_lastError.isEmpty())
            {
                setLastError(QString());
            }
            setBusy(false);
            reply->deleteLater();
            return;
        }
    }

    if (allowLocalFallback() &&
        usernameValue == QLatin1String(kLocalAdminUser) &&
        password == QLatin1String(kLocalAdminPass))
    {
        UserSession session;
        session.username = usernameValue;
        session.displayName = QStringLiteral("本地管理员");
        session.role = QStringLiteral("admin");
        session.token = QStringLiteral("local-admin-token");
        session.localFallback = true;
        finishLogin(session);
        setLastError(QStringLiteral("后端未连接，已切换为本地管理员模式"));
    }
    else
    {
        const QString remoteError = json.isObject()
            ? json.object().value(QStringLiteral("detail")).toString()
            : reply->errorString();
        setLastError(remoteError.isEmpty() ? QStringLiteral("登录失败") : remoteError);
    }

    setBusy(false);
    reply->deleteLater();
}

bool AuthSession::allowLocalFallback() const
{
    if (m_cache == nullptr)
    {
        return false;
    }
    const QString engineeringMode = m_cache->setting(QStringLiteral("app.engineeringMode"), QStringLiteral("0"));
    const QString normalizedBase = m_apiBaseUrl.toLower();
    const bool localApi = normalizedBase.contains(QStringLiteral("127.0.0.1")) ||
                          normalizedBase.contains(QStringLiteral("localhost"));
    return engineeringMode == QStringLiteral("1") && localApi;
}

bool AuthSession::refreshWithToken()
{
    if (m_session.refreshToken.isEmpty())
    {
        return false;
    }

    QNetworkRequest request(QUrl(m_apiBaseUrl + QStringLiteral("/auth/refresh")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    const QJsonObject body{{QStringLiteral("refresh_token"), m_session.refreshToken}};
    QNetworkReply *reply = m_network.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(5000);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start();
    loop.exec();

    if (!reply->isFinished())
    {
        reply->abort();
    }

    const QByteArray payload = reply->readAll();
    const QJsonDocument json = QJsonDocument::fromJson(payload);
    const bool success = reply->error() == QNetworkReply::NoError && json.isObject();
    if (success)
    {
        const QJsonObject root = json.object();
        m_session.token = root.value(QStringLiteral("access_token")).toString(m_session.token);
        m_session.refreshToken = root.value(QStringLiteral("refresh_token")).toString(m_session.refreshToken);
        m_session.accessExpiresAt = root.value(QStringLiteral("access_expires_at")).toString(m_session.accessExpiresAt);
        m_session.refreshExpiresAt = root.value(QStringLiteral("refresh_expires_at")).toString(m_session.refreshExpiresAt);
        m_session.username = root.value(QStringLiteral("username")).toString(m_session.username);
        m_session.displayName = root.value(QStringLiteral("display_name")).toString(m_session.displayName);
        m_session.role = root.value(QStringLiteral("role")).toString(m_session.role);
        m_session.localFallback = false;
        finishLogin(m_session);
        reply->deleteLater();
        return true;
    }

    invalidateRemoteSession(QStringLiteral("Session expired. Please log in again."));
    reply->deleteLater();
    return false;
}

void AuthSession::persistSessionSecrets()
{
    if (m_secureStore == nullptr || m_session.token.isEmpty())
    {
        return;
    }

    QString errorMessage;
    if (!m_secureStore->saveSession(m_apiBaseUrl, m_session, &errorMessage))
    {
        setLastError(errorMessage.isEmpty() ? secureStorageWarning() : secureStorageWarning() + QLatin1Char(' ') + errorMessage);
        return;
    }

    clearLegacySecretCache();
    if (m_lastError == secureStorageWarning() || m_lastError.startsWith(secureStorageWarning() + QLatin1Char(' ')))
    {
        setLastError(QString());
    }
}

void AuthSession::clearLegacySecretCache()
{
    if (m_cache != nullptr)
    {
        m_cache->removeSetting(QStringLiteral("auth.token"));
        m_cache->removeSetting(QStringLiteral("auth.refreshToken"));
        m_cache->removeSetting(QStringLiteral("auth.accessExpiresAt"));
        m_cache->removeSetting(QStringLiteral("auth.refreshExpiresAt"));
    }
}

void AuthSession::clearCachedSecrets()
{
    clearLegacySecretCache();

    if (m_secureStore != nullptr)
    {
        QString ignored;
        m_secureStore->clearSession(m_apiBaseUrl, &ignored);
    }
}

void AuthSession::loadSecureSession()
{
    if (m_secureStore == nullptr)
    {
        return;
    }

    const SecureCredentialStore::LoadResult loaded = m_secureStore->loadSession(m_apiBaseUrl);
    if (loaded.found)
    {
        m_session.token = loaded.accessToken;
        m_session.refreshToken = loaded.refreshToken;
        m_session.accessExpiresAt = loaded.accessExpiresAt;
        m_session.refreshExpiresAt = loaded.refreshExpiresAt;
        return;
    }

    if (!loaded.errorMessage.isEmpty())
    {
        setLastError(secureStorageWarning() + QLatin1Char(' ') + loaded.errorMessage);
    }
}

void AuthSession::migrateLegacySecretCache()
{
    if (m_cache == nullptr || m_secureStore == nullptr)
    {
        return;
    }

    UserSession legacySession;
    legacySession.token = m_cache->setting(QStringLiteral("auth.token"));
    legacySession.refreshToken = m_cache->setting(QStringLiteral("auth.refreshToken"));
    legacySession.accessExpiresAt = m_cache->setting(QStringLiteral("auth.accessExpiresAt"));
    legacySession.refreshExpiresAt = m_cache->setting(QStringLiteral("auth.refreshExpiresAt"));

    if (legacySession.token.isEmpty() && legacySession.refreshToken.isEmpty() &&
        legacySession.accessExpiresAt.isEmpty() && legacySession.refreshExpiresAt.isEmpty())
    {
        return;
    }

    QString errorMessage;
    if (!m_secureStore->saveSession(m_apiBaseUrl, legacySession, &errorMessage))
    {
        m_session.token = legacySession.token;
        m_session.refreshToken = legacySession.refreshToken;
        m_session.accessExpiresAt = legacySession.accessExpiresAt;
        m_session.refreshExpiresAt = legacySession.refreshExpiresAt;
        setLastError(errorMessage.isEmpty() ? secureStorageWarning() : secureStorageWarning() + QLatin1Char(' ') + errorMessage);
    }

    clearLegacySecretCache();
}
