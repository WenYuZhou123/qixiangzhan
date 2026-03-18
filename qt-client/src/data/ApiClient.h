#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QUrlQuery>

class AuthSession;
class QNetworkReply;
class QNetworkRequest;

class ApiClient : public QObject
{
    Q_OBJECT

public:
    explicit ApiClient(AuthSession *authSession, QObject *parent = nullptr);

    bool ready() const;
    QString baseUrl() const;

    QNetworkReply *get(const QString &path, const QUrlQuery &query = QUrlQuery());
    QNetworkReply *post(const QString &path, const QByteArray &payload);
    QNetworkReply *patch(const QString &path, const QByteArray &payload);

private:
    QNetworkRequest buildRequest(const QString &path, const QUrlQuery &query = QUrlQuery()) const;

    AuthSession *m_authSession = nullptr;
    QNetworkAccessManager m_network;
};
