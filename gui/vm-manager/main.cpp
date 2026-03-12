#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "vm-model.h"
#include "vm-controller.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("NEXUS VM Manager");
    app.setOrganizationName("NEXUS-OS");
    app.setApplicationVersion("1.0.0");
    
    QQuickStyle::setStyle("Material");
    
    qmlRegisterType<VMModel>("NexusOS", 1, 0, "VMModel");
    qmlRegisterType<VMController>("NexusOS", 1, 0, "VMController");
    
    QQmlApplicationEngine engine;
    
    VMModel vmModel;
    VMController vmController;
    
    engine.rootContext()->setContextProperty("vmModel", &vmModel);
    engine.rootContext()->setContextProperty("vmController", &vmController);
    
    const QUrl url(QStringLiteral("qrc:/gui/vm-manager/qml/Main.qml"));
    
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated,
        &app, [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);
    
    engine.load(url);
    
    return app.exec();
}
