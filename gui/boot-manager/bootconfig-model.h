/*
 * NEXUS OS - Boot Manager GUI
 * Copyright (c) 2024 NEXUS Development Team
 *
 * bootconfig-model.h - Boot Configuration Data Model
 */

#ifndef BOOTCONFIG_MODEL_H
#define BOOTCONFIG_MODEL_H

#include <QObject>
#include <QAbstractListModel>
#include <QSettings>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QTimer>

/*===========================================================================*/
/*                         BOOT ENTRY STRUCT                                 */
/*===========================================================================*/

struct BootEntry {
    QString id;
    QString name;
    QString kernelPath;
    QString initrdPath;
    QString cmdline;
    QString uuid;
    bool isDefault;
    bool isEnabled;
    int timeout;
    QString virtMode;
    QString lastBooted;
    int bootCount;
    
    BootEntry() : isDefault(false), isEnabled(true), timeout(0), bootCount(0) {}
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["id"] = id;
        obj["name"] = name;
        obj["kernelPath"] = kernelPath;
        obj["initrdPath"] = initrdPath;
        obj["cmdline"] = cmdline;
        obj["uuid"] = uuid;
        obj["isDefault"] = isDefault;
        obj["isEnabled"] = isEnabled;
        obj["timeout"] = timeout;
        obj["virtMode"] = virtMode;
        obj["lastBooted"] = lastBooted;
        obj["bootCount"] = bootCount;
        return obj;
    }
    
    static BootEntry fromJson(const QJsonObject &obj) {
        BootEntry entry;
        entry.id = obj["id"].toString();
        entry.name = obj["name"].toString();
        entry.kernelPath = obj["kernelPath"].toString();
        entry.initrdPath = obj["initrdPath"].toString();
        entry.cmdline = obj["cmdline"].toString();
        entry.uuid = obj["uuid"].toString();
        entry.isDefault = obj["isDefault"].toBool();
        entry.isEnabled = obj["isEnabled"].toBool();
        entry.timeout = obj["timeout"].toInt();
        entry.virtMode = obj["virtMode"].toString();
        entry.lastBooted = obj["lastBooted"].toString();
        entry.bootCount = obj["bootCount"].toInt();
        return entry;
    }
};

Q_DECLARE_METATYPE(BootEntry)

/*===========================================================================*/
/*                         BOOT CONFIG MODEL                                 */
/*===========================================================================*/

class BootConfigModel : public QAbstractListModel
{
    Q_OBJECT
    
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int defaultIndex READ defaultIndex NOTIFY defaultChanged)
    Q_PROPERTY(int globalTimeout READ globalTimeout WRITE setGlobalTimeout NOTIFY timeoutChanged)
    Q_PROPERTY(bool isModified READ isModified NOTIFY modifiedChanged)
    
public:
    enum BootRoles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        KernelPathRole,
        InitrdPathRole,
        CmdlineRole,
        UuidRole,
        IsDefaultRole,
        IsEnabledRole,
        TimeoutRole,
        VirtModeRole,
        LastBootedRole,
        BootCountRole
    };
    
    explicit BootConfigModel(QObject *parent = nullptr);
    ~BootConfigModel() override;
    
    // QAbstractListModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    
    // Model management
    Q_INVOKABLE int count() const { return m_entries.size(); }
    Q_INVOKABLE int defaultIndex() const;
    Q_INVOKABLE int globalTimeout() const { return m_globalTimeout; }
    Q_INVOKABLE void setGlobalTimeout(int timeout);
    Q_INVOKABLE bool isModified() const { return m_modified; }
    
    // Configuration loading/saving
    Q_INVOKABLE bool loadConfiguration();
    Q_INVOKABLE bool saveConfiguration();
    Q_INVOKABLE void reloadConfiguration();
    
    // Entry management
    Q_INVOKABLE int addEntry(const QString &name, const QString &kernelPath,
                            const QString &initrdPath, const QString &cmdline);
    Q_INVOKABLE bool removeEntry(int index);
    Q_INVOKABLE bool removeEntryById(const QString &id);
    Q_INVOKABLE BootEntry getEntry(int index) const;
    Q_INVOKABLE BootEntry getEntryById(const QString &id) const;
    Q_INVOKABLE bool updateEntry(int index, const BootEntry &entry);
    Q_INVOKABLE void moveEntry(int from, int to);
    
    // Default entry
    Q_INVOKABLE void setDefaultEntry(int index);
    Q_INVOKABLE void setDefaultEntryById(const QString &id);
    
    // Entry state
    Q_INVOKABLE void setEntryEnabled(int index, bool enabled);
    Q_INVOKABLE bool isEntryEnabled(int index) const;
    
    // Boot tracking
    Q_INVOKABLE void recordBoot(int index);
    Q_INVOKABLE void clearBootHistory();
    
    // Validation
    Q_INVOKABLE bool validateEntry(int index) const;
    Q_INVOKABLE bool validatePath(const QString &path) const;
    Q_INVOKABLE QStringList scanForKernels(const QString &directory) const;
    
    // Configuration editor
    Q_INVOKABLE void showConfigurationEditor();
    
    // Import/Export
    Q_INVOKABLE bool importConfiguration(const QString &filePath);
    Q_INVOKABLE bool exportConfiguration(const QString &filePath);
    
    // Reset
    Q_INVOKABLE void resetToDefaults();
    Q_INVOKABLE void clearAll();

signals:
    void countChanged();
    void defaultChanged();
    void timeoutChanged();
    void modifiedChanged();
    void entryAdded(int index);
    void entryRemoved(int index);
    void entryModified(int index);

private:
    QList<BootEntry> m_entries;
    int m_globalTimeout;
    bool m_modified;
    QString m_configPath;
    
    void createDefaultEntries();
    QString generateUuid() const;
    QString findInitrdForKernel(const QString &kernelPath) const;
};

#endif // BOOTCONFIG_MODEL_H
