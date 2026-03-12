/*
 * NEXUS OS - Boot Manager GUI
 * Copyright (c) 2024 NEXUS Development Team
 *
 * main.cpp - Boot Manager Application Entry Point
 * 
 * Full-featured boot configuration and virtualization mode selector
 */

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QFont>
#include <QFontDatabase>
#include <QDir>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>

#include "bootconfig-model.h"
#include "virtmode-manager.h"

/*===========================================================================*/
/*                         APPLICATION GLOBALS                               */
/*===========================================================================*/

static BootConfigModel *g_bootConfig = nullptr;
static VirtModeManager *g_virtMode = nullptr;

/*===========================================================================*/
/*                         TRAY ICON SETUP                                   */
/*===========================================================================*/

static void setupSystemTray(QApplication &app)
{
    static QSystemTrayIcon trayIcon;
    static QMenu trayMenu;
    
    // Create tray icon
    QIcon icon(":icons/nexus-logo.png");
    if (icon.isNull()) {
        // Fallback: create simple icon
        QPixmap pixmap(64, 64);
        pixmap.fill(QColor("#1a73e8"));
        icon = QIcon(pixmap);
    }
    
    trayIcon.setIcon(icon);
    trayIcon.setToolTip("NEXUS OS Boot Manager");
    
    // Create menu actions
    QAction showAction("Show Boot Manager", &trayMenu);
    QObject::connect(&showAction, &QAction::triggered, [&]() {
        if (qApp->activeWindow()) {
            qApp->activeWindow()->showNormal();
            qApp->activeWindow()->raise();
            qApp->activeWindow()->activateWindow();
        }
    });
    
    QAction editConfigAction("Edit Boot Configuration", &trayMenu);
    QObject::connect(&editConfigAction, &QAction::triggered, [&]() {
        if (g_bootConfig) {
            g_bootConfig->showConfigurationEditor();
        }
    });
    
    QAction reloadAction("Reload Configuration", &trayMenu);
    QObject::connect(&reloadAction, &QAction::triggered, [&]() {
        if (g_bootConfig) {
            g_bootConfig->reloadConfiguration();
        }
    });
    
    trayMenu.addSeparator();
    
    QAction quitAction("Quit", &trayMenu);
    QObject::connect(&quitAction, &QAction::triggered, [&]() {
        qApp->quit();
    });
    
    trayMenu.addAction(&showAction);
    trayMenu.addAction(&editConfigAction);
    trayMenu.addAction(&reloadAction);
    trayMenu.addSeparator();
    trayMenu.addAction(&quitAction);
    
    trayIcon.setContextMenu(&trayMenu);
    trayIcon.setVisible(true);
    
    // Double-click to show
    QObject::connect(&trayIcon, &QSystemTrayIcon::activated,
                     [&](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick) {
            if (qApp->activeWindow()) {
                qApp->activeWindow()->showNormal();
                qApp->activeWindow()->raise();
                qApp->activeWindow()->activateWindow();
            }
        }
    });
}

/*===========================================================================*/
/*                         APPLICATION ENTRY POINT                           */
/*===========================================================================*/

int main(int argc, char *argv[])
{
    // Set application attributes
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    
    // Create application
    QApplication app(argc, argv);
    app.setApplicationName("NEXUS Boot Manager");
    app.setApplicationVersion(NEXUS_VERSION_STRING);
    app.setOrganizationName("NEXUS OS Project");
    app.setOrganizationDomain("nexus-os.dev");
    
    // Set application style
    QQuickStyle::setStyle("Material");
    QApplication::setStyle("Fusion");
    
    // Set dark palette
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(30, 30, 30));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Base, QColor(45, 45, 45));
    darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);
    app.setPalette(darkPalette);
    
    // Load custom fonts
    QFontDatabase::addApplicationFont(":fonts/Roboto-Regular.ttf");
    QFontDatabase::addApplicationFont(":fonts/Roboto-Medium.ttf");
    QFontDatabase::addApplicationFont(":fonts/Roboto-Bold.ttf");
    QFont appFont("Roboto", 11);
    app.setFont(appFont);
    
    // Initialize global objects
    g_bootConfig = new BootConfigModel();
    g_virtMode = new VirtModeManager();
    
    // Load configuration
    g_bootConfig->loadConfiguration();
    g_virtMode->detectCapabilities();
    
    // Setup QML engine
    QQmlApplicationEngine engine;
    
    // Register C++ types with QML
    qmlRegisterUncreatableType<BootEntry>("NEXUS.Boot", 1, 0, "BootEntry",
                                          "BootEntry is managed by BootConfigModel");
    qmlRegisterUncreatableType<VirtModeInfo>("NEXUS.Virt", 1, 0, "VirtModeInfo",
                                             "VirtModeInfo is managed by VirtModeManager");
    
    // Expose C++ objects to QML
    engine.rootContext()->setContextProperty("bootConfig", g_bootConfig);
    engine.rootContext()->setContextProperty("virtModeManager", g_virtMode);
    engine.rootContext()->setContextProperty("appVersion", NEXUS_VERSION_STRING);
    engine.rootContext()->setContextProperty("appCodename", NEXUS_CODENAME);
    
    // Set QML file
    const QUrl url(QStringLiteral("qrc:/qml/Main.qml"));
    
    // Load QML
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl) {
            qFatal("Failed to load QML");
            QCoreApplication::exit(-1);
        }
        
        // Setup system tray after window is created
        if (obj) {
            setupSystemTray(app);
        }
    }, Qt::QueuedConnection);
    
    engine.load(url);
    
    // Run application
    return app.exec();
}
