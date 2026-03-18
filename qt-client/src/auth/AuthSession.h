#pragma once

#include <QNetworkAccessManager>
#include <QObject>

#include "models/DeviceModels.h"

class SQLiteCache;
class SecureCredentialStore;
class QNetworkReply;

class AuthSession : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString apiBaseUrl READ apiBaseUrl WRITE setApiBaseUrl NOTIFY apiBaseUrlChanged)
    Q_PROPERTY(QString username READ username NOTIFY sessionChanged)
    Q_PROPERTY(QString displayName READ displayName NOTIFY sessionChanged)
    Q_PROPERTY(QString role READ role NOTIFY sessionChanged)
    Q_PROPERTY(QString token READ token NOTIFY sessionChanged)
    Q_PROPERTY(bool authenticated READ authenticated NOTIFY sessionChanged)
    Q_PROPERTY(bool admin READ admin NOTIFY sessionChanged)
    Q_PROPERTY(bool guestMode READ guestMode NOTIFY sessionChanged)
    Q_PROPERTY(bool localFallback READ localFallback NOTIFY sessionChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit AuthSession(SQLiteCache *cache, QObject *parent = nullptr);

    QString apiBaseUrl() const;
    void setApiBaseUrl(const QString &url);

    QString username() const;
    QString displayName() const;
    QString role() const;
    QString token() const;
    bool authenticated() const;
    bool remoteAuthenticated() const;
    bool admin() const;
    bool guestMode() const;
    bool localFallback() const;
    bool busy() const;
    QString lastError() const;

    Q_INVOKABLE void login(const QString &username, const QString &password);
    Q_INVOKABLE void loginAsGuest();
    Q_INVOKABLE void logout();
    bool ensureFreshToken(int minRemainingSeconds = 60);
    void invalidateRemoteSession(const QString &message = QString());

signals:
    void apiBaseUrlChanged();
    void sessionChanged();
    void busyChanged();
    void lastErrorChanged();

private:
    void finishLogin(const UserSession &session);
    void setBusy(bool busy);
    void setLastError(const QString &message);
    void loadFromCache();
    void handleReply(QNetworkReply *reply, const QString &username, const QString &password);
    void persistSessionSecrets();
    void clearLegacySecretCache();
    void clearCachedSecrets();
    void loadSecureSession();
    void migrateLegacySecretCache();
    bool allowLocalFallback() const;
    bool refreshWithToken();

    SQLiteCache *m_cache = nullptr;
    SecureCredentialStore *m_secureStore = nullptr;
    QNetworkAccessManager m_network;
    UserSession m_session;
    QString m_apiBaseUrl;
    QString m_lastError;
    bool m_busy = false;
};
