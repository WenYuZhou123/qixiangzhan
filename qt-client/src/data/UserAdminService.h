#pragma once

#include <QVariantList>
#include <QObject>

class ApiClient;
class AuthSession;
class QNetworkReply;

class UserAdminService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList users READ users NOTIFY usersChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit UserAdminService(ApiClient *apiClient, AuthSession *authSession, QObject *parent = nullptr);

    QVariantList users() const;
    bool busy() const;
    QString lastError() const;

    Q_INVOKABLE void refreshUsers();
    Q_INVOKABLE void createUser(const QString &username,
                                const QString &displayName,
                                const QString &password,
                                const QString &role,
                                const QString &deviceIdsCsv);
    Q_INVOKABLE void setUserActive(int userId, bool active);
    Q_INVOKABLE void assignDevices(int userId, const QString &deviceIdsCsv);

signals:
    void usersChanged();
    void busyChanged();
    void lastErrorChanged();

private:
    void setBusy(bool busy);
    void setLastError(const QString &message);
    QStringList parseDeviceIds(const QString &csv) const;
    void handleUserListReply(QNetworkReply *reply, const QString &context, bool refreshAfterSuccess = false);

    ApiClient *m_apiClient = nullptr;
    AuthSession *m_authSession = nullptr;
    QVariantList m_users;
    bool m_busy = false;
    QString m_lastError;
};
