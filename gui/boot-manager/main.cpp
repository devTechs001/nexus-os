#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>

#include "bootconfig-model.h"
#include "virtmode-manager.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("NEXUS Boot Manager");
    app.setOrganizationName("NEXUS-OS");
    app.setApplicationVersion("1.0.0");
    
    QQuickStyle::setStyle("Material");
    
    // Register custom types
    qmlRegisterType<BootConfigModel>("NexusOS", 1, 0, "BootConfigModel");
    qmlRegisterType<VirtModeManager>("NexusOS", 1, 0, "VirtModeManager");
    
    QQmlApplicationEngine engine;
    
    // Create managers
    BootConfigModel bootConfig;
    VirtModeManager virtManager;
    
    // Expose to QML
    engine.rootContext()->setContextProperty("bootConfig", &bootConfig);
    engine.rootContext()->setContextProperty("virtManager", &virtManager);
    
    const QUrl url(QStringLiteral("qrc:/gui/boot-manager/qml/Main.qml"));
    
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
