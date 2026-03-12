#ifndef VM_CONTROLLER_H
#define VM_CONTROLLER_H

#include <QObject>
#include <QVariantMap>
#include <QVariantList>

class VMController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    
    Q_PROPERTY(bool isCreating READ isCreating NOTIFY isCreatingChanged)
    Q_PROPERTY(QString currentAction READ currentAction NOTIFY currentActionChanged)
    
public:
    explicit VMController(QObject *parent = nullptr);
    
    bool isCreating() const { return m_isCreating; }
    QString currentAction() const { return m_currentAction; }
    
    Q_INVOKABLE void createVM(const QVariantMap &config);
    Q_INVOKABLE void startVM(const QString &vmId);
    Q_INVOKABLE void stopVM(const QString &vmId);
    Q_INVOKABLE void pauseVM(const QString &vmId);
    Q_INVOKABLE void resumeVM(const QString &vmId);
    Q_INVOKABLE void restartVM(const QString &vmId);
    Q_INVOKABLE void deleteVM(const QString &vmId);
    
    Q_INVOKABLE void openConsole(const QString &vmId);
    Q_INVOKABLE void openSettings(const QString &vmId);
    Q_INVOKABLE void createSnapshot(const QString &vmId, const QString &name);
    Q_INVOKABLE void restoreSnapshot(const QString &vmId, const QString &name);
    
    Q_INVOKABLE void exportVM(const QString &vmId, const QString &path);
    Q_INVOKABLE void importVM(const QString &path);
    
    Q_INVOKABLE void cloneVM(const QString &vmId, const QString &newName);
    Q_INVOKABLE void migrateVM(const QString &vmId, const QString &targetHost);
    
signals:
    void isCreatingChanged();
    void currentActionChanged();
    void vmCreated(const QString &vmId);
    void vmStarted(const QString &vmId);
    void vmStopped(const QString &vmId);
    void vmPaused(const QString &vmId);
    void vmResumed(const QString &vmId);
    void vmDeleted(const QString &vmId);
    void actionCompleted(bool success, const QString &message);
    void errorOccurred(const QString &error);
    
private:
    void setAction(const QString &action);
    
    bool m_isCreating = false;
    QString m_currentAction;
};

#endif // VM_CONTROLLER_H
