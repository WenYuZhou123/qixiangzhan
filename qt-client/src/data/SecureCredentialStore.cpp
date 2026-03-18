#include "SecureCredentialStore.h"

#include <algorithm>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QTimer>

#include <keychain.h>

#if defined(Q_OS_ANDROID)
#include <QJniEnvironment>
#include <QJniObject>
#endif

namespace
{
constexpr auto kSessionKeyPrefix = "auth-session-";

QString normalizedApiBase(const QString &apiBaseUrl)
{
    return apiBaseUrl.trimmed().toLower();
}

template <typename JobType>
bool waitForJob(JobType *job, int timeoutMs)
{
    QEventLoop loop;
    QTimer timer;
    bool completed = false;
    timer.setSingleShot(true);
    timer.setInterval(timeoutMs);

    QObject::connect(job, &QKeychain::Job::finished, &loop, [&completed, &loop]() {
        completed = true;
        loop.quit();
    });
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    timer.start();
    job->start();
    loop.exec();

    if (!completed)
    {
        job->deleteLater();
        return false;
    }

    return true;
}

#if defined(Q_OS_ANDROID)
QStringList supportedAndroidAbis()
{
    QStringList abis;
    QJniEnvironment env;
    const QJniObject abiArrayObject = QJniObject::getStaticObjectField(
        "android/os/Build",
        "SUPPORTED_ABIS",
        "[Ljava/lang/String;");
    jobjectArray array = abiArrayObject.object<jobjectArray>();
    if (array == nullptr)
    {
        return abis;
    }

    const jsize length = env->GetArrayLength(array);
    for (jsize index = 0; index < length; ++index)
    {
        jstring item = static_cast<jstring>(env->GetObjectArrayElement(array, index));
        if (item != nullptr)
        {
            abis.push_back(QJniObject(item).toString().trimmed().toLower());
            env->DeleteLocalRef(item);
        }
    }
    return abis;
}

QString javaSystemProperty(const QString &name)
{
    const QJniObject key = QJniObject::fromString(name);
    const QJniObject value = QJniObject::callStaticObjectMethod(
        "java/lang/System",
        "getProperty",
        "(Ljava/lang/String;)Ljava/lang/String;",
        key.object<jstring>());
    return value.toString().trimmed().toLower();
}
#endif
}

SecureCredentialStore::SecureCredentialStore(QObject *parent)
    : QObject(parent)
{
}

SecureCredentialStore::LoadResult SecureCredentialStore::loadSession(const QString &apiBaseUrl) const
{
    LoadResult result;
    QString bypassReason;
    if (shouldBypassSecureStore(&bypassReason))
    {
        result.backendAvailable = false;
        result.errorMessage = bypassReason;
        return result;
    }

    QKeychain::ReadPasswordJob *job = new QKeychain::ReadPasswordJob(serviceName());
    job->setAutoDelete(false);
    job->setInsecureFallback(false);
    job->setKey(keyForApiBase(apiBaseUrl));

    if (!waitForJob(job, 5000))
    {
        result.backendAvailable = false;
        result.errorMessage = QStringLiteral("Secure storage read timed out.");
        delete job;
        return result;
    }

    const int errorCode = static_cast<int>(job->error());
    if (errorCode == static_cast<int>(QKeychain::NoError))
    {
        const QJsonDocument json = QJsonDocument::fromJson(job->textData().toUtf8());
        if (!json.isObject())
        {
            result.backendAvailable = false;
            result.errorMessage = QStringLiteral("Secure storage returned invalid session data.");
        }
        else
        {
            const QJsonObject root = json.object();
            result.found = true;
            result.accessToken = root.value(QStringLiteral("access_token")).toString();
            result.refreshToken = root.value(QStringLiteral("refresh_token")).toString();
            result.accessExpiresAt = root.value(QStringLiteral("access_expires_at")).toString();
            result.refreshExpiresAt = root.value(QStringLiteral("refresh_expires_at")).toString();
        }
    }
    else if (errorCode == static_cast<int>(QKeychain::EntryNotFound))
    {
        result.found = false;
    }
    else
    {
        result.backendAvailable = !isBackendMissingError(errorCode);
        result.errorMessage = job->errorString();
        if (result.errorMessage.isEmpty())
        {
            result.errorMessage = QStringLiteral("Secure storage read failed.");
        }
    }

    delete job;
    return result;
}

bool SecureCredentialStore::saveSession(const QString &apiBaseUrl,
                                        const UserSession &session,
                                        QString *errorMessage) const
{
    QString bypassReason;
    if (shouldBypassSecureStore(&bypassReason))
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = bypassReason;
        }
        return false;
    }

    QJsonObject root;
    root.insert(QStringLiteral("access_token"), session.token);
    root.insert(QStringLiteral("refresh_token"), session.refreshToken);
    root.insert(QStringLiteral("access_expires_at"), session.accessExpiresAt);
    root.insert(QStringLiteral("refresh_expires_at"), session.refreshExpiresAt);

    QKeychain::WritePasswordJob *job = new QKeychain::WritePasswordJob(serviceName());
    job->setAutoDelete(false);
    job->setInsecureFallback(false);
    job->setKey(keyForApiBase(apiBaseUrl));
    job->setTextData(QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));

    if (!waitForJob(job, 5000))
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = QStringLiteral("Secure storage write timed out.");
        }
        delete job;
        return false;
    }

    const bool ok = job->error() == QKeychain::NoError;
    if (!ok && errorMessage != nullptr)
    {
        *errorMessage = job->errorString().isEmpty()
            ? QStringLiteral("Secure storage write failed.")
            : job->errorString();
    }

    delete job;
    return ok;
}

bool SecureCredentialStore::clearSession(const QString &apiBaseUrl, QString *errorMessage) const
{
    QString bypassReason;
    if (shouldBypassSecureStore(&bypassReason))
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = bypassReason;
        }
        return false;
    }

    QKeychain::DeletePasswordJob *job = new QKeychain::DeletePasswordJob(serviceName());
    job->setAutoDelete(false);
    job->setInsecureFallback(false);
    job->setKey(keyForApiBase(apiBaseUrl));

    if (!waitForJob(job, 5000))
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = QStringLiteral("Secure storage delete timed out.");
        }
        delete job;
        return false;
    }

    const int errorCode = static_cast<int>(job->error());
    const bool ok = errorCode == static_cast<int>(QKeychain::NoError) ||
                    errorCode == static_cast<int>(QKeychain::EntryNotFound);
    if (!ok && errorMessage != nullptr)
    {
        *errorMessage = job->errorString().isEmpty()
            ? QStringLiteral("Secure storage delete failed.")
            : job->errorString();
    }

    delete job;
    return ok;
}

QString SecureCredentialStore::serviceName() const
{
    const QString organization = QCoreApplication::organizationName().isEmpty()
        ? QStringLiteral("qixiangzhan")
        : QCoreApplication::organizationName();
    const QString application = QCoreApplication::applicationName().isEmpty()
        ? QStringLiteral("qixiang_platform")
        : QCoreApplication::applicationName();
    return organization + QLatin1Char('.') + application + QStringLiteral(".auth");
}

QString SecureCredentialStore::keyForApiBase(const QString &apiBaseUrl) const
{
    const QByteArray digest = QCryptographicHash::hash(normalizedApiBase(apiBaseUrl).toUtf8(),
                                                       QCryptographicHash::Sha256)
                                  .toHex();
    return QString::fromLatin1(kSessionKeyPrefix) + QString::fromLatin1(digest);
}

bool SecureCredentialStore::isBackendMissingError(int errorCode) const
{
    return errorCode == static_cast<int>(QKeychain::NoBackendAvailable) ||
           errorCode == static_cast<int>(QKeychain::NotImplemented);
}

bool SecureCredentialStore::shouldBypassSecureStore(QString *reason) const
{
#if defined(Q_OS_ANDROID)
    const QStringList abis = supportedAndroidAbis();
    const QString osArch = javaSystemProperty(QStringLiteral("os.arch"));

    const bool x86Runtime = std::any_of(abis.cbegin(), abis.cend(), [](const QString &abi) {
        return abi.contains(QStringLiteral("x86"));
    }) || osArch.contains(QStringLiteral("x86"));

    if (x86Runtime)
    {
        if (reason != nullptr)
        {
            *reason = QStringLiteral("Secure storage is disabled on Android emulator x86/x86_64 runtimes.");
        }
        return true;
    }
#else
    Q_UNUSED(reason);
#endif
    return false;
}
