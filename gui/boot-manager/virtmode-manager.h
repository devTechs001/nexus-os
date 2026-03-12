#ifndef VIRTMODE_MANAGER_H
#define VIRTMODE_MANAGER_H

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class VirtModeManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    
    Q_PROPERTY(bool hardwareVirtSupported READ hardwareVirtSupported CONSTANT)
    Q_PROPERTY(bool nestedVirtSupported READ nestedVirtSupported CONSTANT)
    Q_PROPERTY(QString currentMode READ currentMode NOTIFY currentModeChanged)
    Q_PROPERTY(QVariantList runningVMs READ runningVMs NOTIFY runningVMsChanged)
    
public:
    explicit VirtModeManager(QObject *parent = nullptr);
    
    bool hardwareVirtSupported() const { return m_hardwareVirtSupported; }
    bool nestedVirtSupported() const { return m_nestedVirtSupported; }
    QString currentMode() const { return m_currentMode; }
    QVariantList runningVMs() const { return m_runningVMs; }
    
    Q_INVOKABLE bool checkVirtualizationSupport();
    Q_INVOKABLE QVariantMap getModeRecommendation() const;
    Q_INVOKABLE bool switchMode(int mode);
    Q_INVOKABLE void refreshVMList();
    
signals:
    void currentModeChanged();
    void runningVMsChanged();
    void supportCheckCompleted(bool supported);
    
private:
    bool m_hardwareVirtSupported = true;
    bool m_nestedVirtSupported = false;
    QString m_currentMode = "Light Virtualization";
    QVariantList m_runningVMs;
};

#endif // VIRTMODE_MANAGER_H
