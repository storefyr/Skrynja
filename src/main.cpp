#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QCoreApplication>
#include "mistralclient.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    
    // Set application info for consistent QSettings storage
    QCoreApplication::setApplicationName("skrynja");
    QCoreApplication::setOrganizationName("Harbour");
    QCoreApplication::setApplicationVersion("1.0");

    QQmlApplicationEngine engine;
    
    MistralClient mistralClient;
    engine.rootContext()->setContextProperty("mistralClient", &mistralClient);

    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
