#pragma once

#include <QObject>
#include <QString>

#include "models/DeviceModels.h"

class SecureCredentialStore : public QObject
{
    Q_OBJECT

public:
    struct LoadResult
    {
        bool found = false;
        bool backendAvailable = true;
        QString errorMessage;
        QString accessToken;
        QString refreshToken;
        QString accessExpiresAt;
        QString refreshExpiresAt;
    };

    explicit SecureCredentialStore(QObject *parent = nullptr);

    LoadResult loadSession(const QString &apiBaseUrl) const;
    bool saveSession(const QString &apiBaseUrl, const UserSession &session, QString *errorMessage = nullptr) const;
    bool clearSession(const QString &apiBaseUrl, QString *errorMessage = nullptr) const;

private:
    QString serviceName() const;
    QString keyForApiBase(const QString &apiBaseUrl) const;
    bool isBackendMissingError(int errorCode) const;
    bool shouldBypassSecureStore(QString *reason = nullptr) const;
};
