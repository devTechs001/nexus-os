#include "vm-controller.h"
#include <QTimer>
#include <QRandomGenerator>
#include <QDebug>

VMController::VMController(QObject *parent)
    : QObject(parent)
{
}

void VMController::setAction(const QString &action)
{
    m_currentAction = action;
    m_isCreating = (action == "Creating VM");
    emit currentActionChanged();
    emit isCreatingChanged();
}

void VMController::createVM(const QVariantMap &config)
{
    setAction("Creating VM");
    
    // Simulate VM creation
    QTimer::singleShot(2000, this, [this, config]() {
        QString vmId = "vm-" + QString::number(QRandomGenerator::global()->bounded(1000, 9999));
        qDebug() << "Created VM:" << vmId << config["name"];
        
        setAction("");
        emit vmCreated(vmId);
        emit actionCompleted(true, "VM created successfully: " + config["name"].toString());
    });
}

void VMController::startVM(const QString &vmId)
{
    setAction("Starting VM");
    
    QTimer::singleShot(1500, this, [this, vmId]() {
        qDebug() << "Started VM:" << vmId;
        setAction("");
        emit vmStarted(vmId);
        emit actionCompleted(true, "VM started successfully");
    });
}

void VMController::stopVM(const QString &vmId)
{
    setAction("Stopping VM");
    
    QTimer::singleShot(1000, this, [this, vmId]() {
        qDebug() << "Stopped VM:" << vmId;
        setAction("");
        emit vmStopped(vmId);
        emit actionCompleted(true, "VM stopped successfully");
    });
}

void VMController::pauseVM(const QString &vmId)
{
    setAction("Pausing VM");
    
    QTimer::singleShot(500, this, [this, vmId]() {
        qDebug() << "Paused VM:" << vmId;
        setAction("");
        emit vmPaused(vmId);
        emit actionCompleted(true, "VM paused successfully");
    });
}

void VMController::resumeVM(const QString &vmId)
{
    setAction("Resuming VM");
    
    QTimer::singleShot(500, this, [this, vmId]() {
        qDebug() << "Resumed VM:" << vmId;
        setAction("");
        emit vmResumed(vmId);
        emit actionCompleted(true, "VM resumed successfully");
    });
}

void VMController::restartVM(const QString &vmId)
{
    setAction("Restarting VM");
    
    QTimer::singleShot(2000, this, [this, vmId]() {
        qDebug() << "Restarted VM:" << vmId;
        setAction("");
        emit actionCompleted(true, "VM restarted successfully");
    });
}

void VMController::deleteVM(const QString &vmId)
{
    setAction("Deleting VM");
    
    QTimer::singleShot(1000, this, [this, vmId]() {
        qDebug() << "Deleted VM:" << vmId;
        setAction("");
        emit vmDeleted(vmId);
        emit actionCompleted(true, "VM deleted successfully");
    });
}

void VMController::openConsole(const QString &vmId)
{
    // Open VM console window
    qDebug() << "Opening console for VM:" << vmId;
    emit actionCompleted(true, "Console opened");
}

void VMController::openSettings(const QString &vmId)
{
    // Open VM settings dialog
    qDebug() << "Opening settings for VM:" << vmId;
    emit actionCompleted(true, "Settings opened");
}

void VMController::createSnapshot(const QString &vmId, const QString &name)
{
    setAction("Creating Snapshot");
    
    QTimer::singleShot(1500, this, [this, vmId, name]() {
        qDebug() << "Created snapshot:" << name << "for VM:" << vmId;
        setAction("");
        emit actionCompleted(true, "Snapshot created: " + name);
    });
}

void VMController::restoreSnapshot(const QString &vmId, const QString &name)
{
    setAction("Restoring Snapshot");
    
    QTimer::singleShot(2000, this, [this, vmId, name]() {
        qDebug() << "Restored snapshot:" << name << "for VM:" << vmId;
        setAction("");
        emit actionCompleted(true, "Snapshot restored: " + name);
    });
}

void VMController::exportVM(const QString &vmId, const QString &path)
{
    setAction("Exporting VM");
    
    QTimer::singleShot(3000, this, [this, vmId, path]() {
        qDebug() << "Exported VM:" << vmId << "to" << path;
        setAction("");
        emit actionCompleted(true, "VM exported successfully");
    });
}

void VMController::importVM(const QString &path)
{
    setAction("Importing VM");
    
    QTimer::singleShot(3000, this, [this, path]() {
        qDebug() << "Imported VM from" << path;
        setAction("");
        emit actionCompleted(true, "VM imported successfully");
    });
}

void VMController::cloneVM(const QString &vmId, const QString &newName)
{
    setAction("Cloning VM");
    
    QTimer::singleShot(2500, this, [this, vmId, newName]() {
        QString newId = "vm-" + QString::number(QRandomGenerator::global()->bounded(1000, 9999));
        qDebug() << "Cloned VM:" << vmId << "to" << newId << "(" << newName << ")";
        setAction("");
        emit vmCreated(newId);
        emit actionCompleted(true, "VM cloned: " + newName);
    });
}

void VMController::migrateVM(const QString &vmId, const QString &targetHost)
{
    setAction("Migrating VM");
    
    QTimer::singleShot(5000, this, [this, vmId, targetHost]() {
        qDebug() << "Migrated VM:" << vmId << "to" << targetHost;
        setAction("");
        emit actionCompleted(true, "VM migrated to " + targetHost);
    });
}
