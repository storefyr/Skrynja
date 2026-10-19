#include <QGuiApplication>
#include <QQuickView>
#include <QQmlContext>
#include <sailfishapp.h>

#include "mistralclient.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    QScopedPointer<QQuickView> view(SailfishApp::createView());

    QCoreApplication::setApplicationName("skrynja");
    QCoreApplication::setOrganizationName("Harbour");
    QCoreApplication::setApplicationVersion("1.0");

    MistralClient mistralClient;
    view->rootContext()->setContextProperty("mistralClient", &mistralClient);

    view->setSource(SailfishApp::pathTo("qml/main.qml"));
    view->showFullScreen();

    return app->exec();
}
