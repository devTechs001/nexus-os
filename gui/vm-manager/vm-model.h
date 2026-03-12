#ifndef VM_MODEL_H
#define VM_MODEL_H

#include <QObject>
#include <QAbstractListModel>
#include <QVariantList>
#include <QVariantMap>

struct VMInfo {
    QString id;
    QString name;
    QString status;        // running, stopped, paused, error
    QString type;          // hardware, process, container, enclave
    int cpuCount;
    int memorySize;        // MB
    int diskSize;          // GB
    QString uptime;
    double cpuUsage;
    double memoryUsage;
    QString networkIP;
    QString osType;
    QString createdAt;
};

class VMModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    
    Q_PROPERTY(int totalCount READ totalCount NOTIFY countChanged)
    Q_PROPERTY(int runningCount READ runningCount NOTIFY countChanged)
    
public:
    enum VMRoles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        StatusRole,
        TypeRole,
        CPUCountRole,
        MemorySizeRole,
        DiskSizeRole,
        UptimeRole,
        CPUUsageRole,
        MemoryUsageRole,
        NetworkIPRole,
        OSTypeRole,
        CreatedAtRole
    };
    
    explicit VMModel(QObject *parent = nullptr);
    
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    
    int totalCount() const { return m_vms.size(); }
    int runningCount() const;
    
    Q_INVOKABLE QVariantList getAllVMs() const;
    Q_INVOKABLE QVariantMap getVM(const QString &id) const;
    Q_INVOKABLE void addVM(const QVariantMap &vm);
    Q_INVOKABLE void removeVM(const QString &id);
    Q_INVOKABLE void updateVM(const QString &id, const QVariantMap &data);
    
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void filterByStatus(const QString &status);
    Q_INVOKABLE void sortBy(const QString &field);
    
signals:
    void countChanged();
    void vmAdded(const QString &id);
    void vmRemoved(const QString &id);
    void vmUpdated(const QString &id);
    
private:
    QList<VMInfo> m_vms;
    QList<VMInfo> m_filteredVMs;
};

#endif // VM_MODEL_H
