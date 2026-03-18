#include "UserAdminService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

#include "auth/AuthSession.h"
#include "data/ApiClient.h"

namespace
{
QString errorText(QNetworkReply *reply, const QByteArray &payload)
{
    const QJsonDocument json = QJsonDocument::fromJson(payload);
    if (json.isObject())
    {
        const QString detail = json.object().value(QStringLiteral("detail")).toString();
        if (!detail.isEmpty())
        {
            return detail;
        }
    }
    return reply != nullptr ? reply->errorString() : QStringLiteral("Network error");
}

QVariantMap userMapFromJson(const QJsonObject &object)
{
    QVariantMap map;
    map.insert(QStringLiteral("id"), object.value(QStringLiteral("id")).toInt());
    map.insert(QStringLiteral("username"), object.value(QStringLiteral("username")).toString());
    map.insert(QStringLiteral("display_name"), object.value(QStringLiteral("display_name")).toString());
    map.insert(QStringLiteral("role"), object.value(QStringLiteral("role")).toString());
    map.insert(QStringLiteral("is_active"), object.value(QStringLiteral("is_active")).toBool());
    map.insert(QStringLiteral("created_at"), object.value(QStringLiteral("created_at")).toString());
    map.insert(QStringLiteral("last_login_at"), object.value(QStringLiteral("last_login_at")).toString());

    QStringList deviceIds;
    for (const QJsonValue &value : object.value(QStringLiteral("device_ids")).toArray())
    {
        deviceIds.append(value.toString());
    }
    map.insert(QStringLiteral("device_ids"), deviceIds);
    return map;
}
}

UserAdminService::UserAdminService(ApiClient *apiClient, AuthSession *authSession, QObject *parent)
    : QObject(parent)
    , m_apiClient(apiClient)
    , m_authSession(authSession)
{
    connect(m_authSession, &AuthSession::sessionChanged, this, [this]() {
        if (m_authSession != nullptr && m_authSession->admin())
        {
            refreshUsers();
        }
        else if (!m_users.isEmpty())
        {
            m_users.clear();
            emit usersChanged();
        }
    });
}

QVariantList UserAdminService::users() const
{
    return m_users;
}

bool UserAdminService::busy() const
{
    return m_busy;
}

QString UserAdminService::lastError() const
{
    return m_lastError;
}

void UserAdminService::refreshUsers()
{
    if (m_apiClient == nullptr || !m_apiClient->ready() || m_authSession == nullptr || !m_authSession->admin())
    {
        return;
    }

    setBusy(true);
    QNetworkReply *reply = m_apiClient->get(QStringLiteral("/users"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleUserListReply(reply, QStringLiteral("GET /users"));
    });
}

void UserAdminService::createUser(const QString &username,
                                  const QString &displayName,
                                  const QString &password,
                                  const QString &role,
                                  const QString &deviceIdsCsv)
{
    if (m_apiClient == nullptr || !m_apiClient->ready())
    {
        setLastError(QStringLiteral("API is not ready"));
        return;
    }
    if (username.trimmed().isEmpty() || password.isEmpty())
    {
        setLastError(QStringLiteral("Username and password are required"));
        return;
    }

    setBusy(true);
    QJsonObject body;
    body.insert(QStringLiteral("username"), username.trimmed());
    body.insert(QStringLiteral("display_name"), displayName.trimmed().isEmpty() ? username.trimmed() : displayName.trimmed());
    body.insert(QStringLiteral("password"), password);
    body.insert(QStringLiteral("role"), role.trimmed().isEmpty() ? QStringLiteral("operator") : role.trimmed());

    QJsonArray ids;
    for (const QString &deviceId : parseDeviceIds(deviceIdsCsv))
    {
        ids.append(deviceId);
    }
    body.insert(QStringLiteral("device_ids"), ids);

    QNetworkReply *reply = m_apiClient->post(QStringLiteral("/users"), QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleUserListReply(reply, QStringLiteral("POST /users"), true);
    });
}

void UserAdminService::setUserActive(int userId, bool active)
{
    if (m_apiClient == nullptr || !m_apiClient->ready())
    {
        setLastError(QStringLiteral("API is not ready"));
        return;
    }

    setBusy(true);
    QJsonObject body;
    body.insert(QStringLiteral("is_active"), active);
    QNetworkReply *reply = m_apiClient->patch(
        QStringLiteral("/users/%1").arg(userId),
        QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleUserListReply(reply, QStringLiteral("PATCH /users/{id}"), true);
    });
}

void UserAdminService::assignDevices(int userId, const QString &deviceIdsCsv)
{
    if (m_apiClient == nullptr || !m_apiClient->ready())
    {
        setLastError(QStringLiteral("API is not ready"));
        return;
    }

    setBusy(true);
    QJsonObject body;
    QJsonArray ids;
    for (const QString &deviceId : parseDeviceIds(deviceIdsCsv))
    {
        ids.append(deviceId);
    }
    body.insert(QStringLiteral("device_ids"), ids);
    QNetworkReply *reply = m_apiClient->post(
        QStringLiteral("/users/%1/devices").arg(userId),
        QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleUserListReply(reply, QStringLiteral("POST /users/{id}/devices"), true);
    });
}

void UserAdminService::setBusy(bool busyValue)
{
    if (m_busy == busyValue)
    {
        return;
    }
    m_busy = busyValue;
    emit busyChanged();
}

void UserAdminService::setLastError(const QString &message)
{
    if (m_lastError == message)
    {
        return;
    }
    m_lastError = message;
    emit lastErrorChanged();
}

QStringList UserAdminService::parseDeviceIds(const QString &csv) const
{
    QStringList values;
    for (const QString &part : csv.split(',', Qt::SkipEmptyParts))
    {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty())
        {
            values.append(trimmed);
        }
    }
    values.removeDuplicates();
    return values;
}

void UserAdminService::handleUserListReply(QNetworkReply *reply, const QString &context, bool refreshAfterSuccess)
{
    const QByteArray payload = reply->readAll();
    if (reply->error() != QNetworkReply::NoError)
    {
        setLastError(QStringLiteral("%1 failed: %2").arg(context, errorText(reply, payload)));
        setBusy(false);
        reply->deleteLater();
        return;
    }

    if (refreshAfterSuccess)
    {
        setLastError(QString());
        setBusy(false);
        reply->deleteLater();
        refreshUsers();
        return;
    }

    const QJsonDocument json = QJsonDocument::fromJson(payload);
    if (!json.isArray())
    {
        setLastError(QStringLiteral("Invalid /users payload"));
        setBusy(false);
        reply->deleteLater();
        return;
    }

    QVariantList users;
    for (const QJsonValue &value : json.array())
    {
        if (value.isObject())
        {
            users.append(userMapFromJson(value.toObject()));
        }
    }

    m_users = users;
    emit usersChanged();
    setLastError(QString());
    setBusy(false);
    reply->deleteLater();
}
