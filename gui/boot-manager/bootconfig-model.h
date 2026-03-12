#ifndef BOOTCONFIG_MODEL_H
#define BOOTCONFIG_MODEL_H

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QSettings>

class BootConfigModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    
    Q_PROPERTY(int selectedVirtMode READ selectedVirtMode WRITE setSelectedVirtMode NOTIFY selectedVirtModeChanged)
    Q_PROPERTY(QString selectedVirtModeName READ selectedVirtModeName NOTIFY selectedVirtModeChanged)
    Q_PROPERTY(bool secureBoot READ secureBoot WRITE setSecureBoot NOTIFY secureBootChanged)
    Q_PROPERTY(bool fastBoot READ fastBoot WRITE setFastBoot NOTIFY fastBootChanged)
    Q_PROPERTY(bool verboseBoot READ verboseBoot WRITE setVerboseBoot NOTIFY verboseBootChanged)
    Q_PROPERTY(int cpuCount READ cpuCount WRITE setCpuCount NOTIFY cpuCountChanged)
    Q_PROPERTY(int memorySize READ memorySize WRITE setMemorySize NOTIFY memorySizeChanged)
    Q_PROPERTY(QString securityProfile READ securityProfile WRITE setSecurityProfile NOTIFY securityProfileChanged)
    
public:
    explicit BootConfigModel(QObject *parent = nullptr);
    
    int selectedVirtMode() const { return m_selectedVirtMode; }
    void setSelectedVirtMode(int mode);
    
    QString selectedVirtModeName() const;
    
    bool secureBoot() const { return m_secureBoot; }
    void setSecureBoot(bool enabled);
    
    bool fastBoot() const { return m_fastBoot; }
    void setFastBoot(bool enabled);
    
    bool verboseBoot() const { return m_verboseBoot; }
    void setVerboseBoot(bool enabled);
    
    int cpuCount() const { return m_cpuCount; }
    void setCpuCount(int count);
    
    int memorySize() const { return m_memorySize; }
    void setMemorySize(int size);
    
    QString securityProfile() const { return m_securityProfile; }
    void setSecurityProfile(const QString &profile);
    
    Q_INVOKABLE QVariantList getVirtModes() const;
    Q_INVOKABLE QVariantMap getVirtModeDetails(int mode) const;
    Q_INVOKABLE void loadConfig(const QString &path);
    Q_INVOKABLE void saveConfig(const QString &path);
    Q_INVOKABLE void applyConfig();
    Q_INVOKABLE void resetToDefaults();
    Q_INVOKABLE bool validateConfig() const;
    
signals:
    void selectedVirtModeChanged();
    void secureBootChanged();
    void fastBootChanged();
    void verboseBootChanged();
    void cpuCountChanged();
    void memorySizeChanged();
    void securityProfileChanged();
    void configApplied();
    void configError(const QString &error);
    
private:
    void detectHardware();
    
    int m_selectedVirtMode = 1; // Light virtualization (default)
    bool m_secureBoot = true;
    bool m_fastBoot = true;
    bool m_verboseBoot = false;
    int m_cpuCount = 0;
    int m_memorySize = 0;
    QString m_securityProfile = "balanced";
    
    QVariantList m_virtModes;
};

#endif // BOOTCONFIG_MODEL_H
