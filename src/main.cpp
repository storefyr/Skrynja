#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QCoreApplication>
#include "mistralclient.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    
    // Sæt applikationsnavn for korrekt QSettings gemning
    QCoreApplication::setApplicationName("skrynja");
    QCoreApplication::setOrganizationName("Harbour");

    QQmlApplicationEngine engine;
    
    MistralClient mistralClient;
    engine.rootContext()->setContextProperty("mistralClient", &mistralClient);

    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
