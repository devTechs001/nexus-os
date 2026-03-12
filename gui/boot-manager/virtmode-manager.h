/*
 * NEXUS OS - Boot Manager GUI
 * Copyright (c) 2026 NEXUS Development Team
 *
 * virtmode-manager.h - Virtualization Mode Manager
 */

#ifndef VIRTMODE_MANAGER_H
#define VIRTMODE_MANAGER_H

#include <QObject>
#include <QAbstractListModel>
#include <QFileInfo>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

/*===========================================================================*/
/*                         VIRT MODE INFO STRUCT                             */
/*===========================================================================*/

struct VirtModeInfo {
    QString id;
    QString name;
    QString description;
    QString icon;
    int securityLevel;
    int performanceLevel;
    bool isAvailable;
    bool requiresHardwareSupport;
    QString requirements;
    QString recommendedFor;
    
    VirtModeInfo() : securityLevel(0), performanceLevel(0), 
                     isAvailable(true), requiresHardwareSupport(false) {}
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["id"] = id;
        obj["name"] = name;
        obj["description"] = description;
        obj["icon"] = icon;
        obj["securityLevel"] = securityLevel;
        obj["performanceLevel"] = performanceLevel;
        obj["isAvailable"] = isAvailable;
        obj["requiresHardwareSupport"] = requiresHardwareSupport;
        obj["requirements"] = requirements;
        obj["recommendedFor"] = recommendedFor;
        return obj;
    }
};

Q_DECLARE_METATYPE(VirtModeInfo)

/*===========================================================================*/
/*                         VIRT MODE MANAGER                                 */
============================================================================*/

class VirtModeManager : public QAbstractListModel
{
    Q_OBJECT
    
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString selectedMode READ selectedMode WRITE setSelectedMode NOTIFY selectedModeChanged)
    Q_PROPERTY(bool hardwareVirtSupported READ hardwareVirtSupported NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool nestedVirtSupported READ nestedVirtSupported NOTIFY capabilitiesChanged)
    
public:
    enum VirtRoles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        DescriptionRole,
        IconRole,
        SecurityLevelRole,
        PerformanceLevelRole,
        IsAvailableRole,
        RequiresHardwareSupportRole,
        RequirementsRole,
        RecommendedForRole
    };
    
    explicit VirtModeManager(QObject *parent = nullptr);
    ~VirtModeManager() override;
    
    // QAbstractListModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    
    // Properties
    Q_INVOKABLE int count() const { return m_modes.size(); }
    Q_INVOKABLE QString selectedMode() const { return m_selectedMode; }
    Q_INVOKABLE void setSelectedMode(const QString &mode);
    Q_INVOKABLE bool hardwareVirtSupported() const { return m_hwVirtSupported; }
    Q_INVOKABLE bool nestedVirtSupported() const { return m_nestedVirtSupported; }
    
    // Capabilities detection
    Q_INVOKABLE void detectCapabilities();
    Q_INVOKABLE QString getCPUVendor() const { return m_cpuVendor; }
    Q_INVOKABLE QString getCPUFeatures() const { return m_cpuFeatures; }
    
    // Mode info
    Q_INVOKABLE VirtModeInfo getModeInfo(const QString &id) const;
    Q_INVOKABLE QString getModeDescription(const QString &id) const;
    Q_INVOKABLE bool isModeAvailable(const QString &id) const;
    
    // Validation
    Q_INVOKABLE bool validateMode(const QString &id) const;
    Q_INVOKABLE QStringList getModeWarnings(const QString &id) const;
    Q_INVOKABLE QStringList getModeRecommendations(const QString &id) const;
    
    // System info
    Q_INVOKABLE QString getSystemInfo() const;
    Q_INVOKABLE int getCPUCores() const { return m_cpuCores; }
    Q_INVOKABLE int getCPULogicalCores() const { return m_cpuLogicalCores; }
    Q_INVOKABLE quint64 getTotalMemory() const { return m_totalMemory; }
    Q_INVOKABLE QString getVirtualizationTech() const;

signals:
    void countChanged();
    void selectedModeChanged();
    void capabilitiesChanged();
    void modeAvailabilityChanged();

private:
    QList<VirtModeInfo> m_modes;
    QString m_selectedMode;
    bool m_hwVirtSupported;
    bool m_nestedVirtSupported;
    QString m_cpuVendor;
    QString m_cpuFeatures;
    int m_cpuCores;
    int m_cpuLogicalCores;
    quint64 m_totalMemory;
    
    void initializeModes();
    void detectCPUInfo();
    void detectMemoryInfo();
    bool checkVTxSupport() const;
    bool checkSVM_SUPPORT() const;
};

#endif // VIRTMODE_MANAGER_H
