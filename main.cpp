#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QDebug>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("Countdown"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("countdown.local"));
    QGuiApplication::setApplicationName(QStringLiteral("OffWorkCountdown"));
    QGuiApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::warnings,
        [](const QList<QQmlError> &warnings) {
            for (const QQmlError &error : warnings)
                qWarning().noquote() << error.toString();
        });

    const QUrl url(QStringLiteral("qrc:/qt/qml/countdown/main.qml"));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);
    engine.load(url);

    if (engine.rootObjects().isEmpty()) {
        qCritical() << "Failed to create root object from" << url;
        return -1;
    }

    return QGuiApplication::exec();
}
