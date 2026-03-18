#include <QGuiApplication>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>

#include "app/AppController.h"

static void appendStartupWarnings(const QList<QQmlError> &warnings)
{
    const QString logPath = QCoreApplication::applicationDirPath() + QStringLiteral("/startup_errors.log");
    QFile logFile(logPath);
    if (!logFile.open(QIODevice::Append | QIODevice::Text))
    {
        return;
    }

    QTextStream stream(&logFile);
    stream << '[' << QDateTime::currentDateTime().toString(Qt::ISODate) << "] QML startup warnings\n";
    for (const QQmlError &warning : warnings)
    {
        const QString line = warning.toString();
        stream << line << '\n';
        qWarning().noquote() << line;
    }
    stream << '\n';
}

static void appendStartupMessage(const QString &message)
{
    const QString logPath = QCoreApplication::applicationDirPath() + QStringLiteral("/startup_errors.log");
    QFile logFile(logPath);
    if (!logFile.open(QIODevice::Append | QIODevice::Text))
    {
        return;
    }

    QTextStream stream(&logFile);
    stream << '[' << QDateTime::currentDateTime().toString(Qt::ISODate) << "] " << message << '\n';
    qWarning().noquote() << message;
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qixiangzhan"));
    QCoreApplication::setApplicationName(QStringLiteral("qixiang_platform"));

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app, [](const QList<QQmlError> &warnings) {
        appendStartupWarnings(warnings);
    });
    AppController controller;

    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &controller);
    engine.loadFromModule(QStringLiteral("RelayClient"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty())
    {
        appendStartupMessage(QStringLiteral("Failed to load RelayClient.Main; see previous QML warnings for details."));
        return -1;
    }

    return app.exec();
}
