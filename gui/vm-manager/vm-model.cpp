#include "vm-model.h"
#include <QDateTime>
#include <algorithm>

VMModel::VMModel(QObject *parent)
    : QAbstractListModel(parent)
{
    // Add sample VMs for demonstration
    VMInfo vm1;
    vm1.id = "vm-001";
    vm1.name = "Development Environment";
    vm1.status = "running";
    vm1.type = "process";
    vm1.cpuCount = 4;
    vm1.memorySize = 4096;
    vm1.diskSize = 64;
    vm1.uptime = "2h 34m";
    vm1.cpuUsage = 23.5;
    vm1.memoryUsage = 67.3;
    vm1.networkIP = "192.168.1.101";
    vm1.osType = "NEXUS Desktop";
    vm1.createdAt = "2024-01-15";
    m_vms.append(vm1);
    
    VMInfo vm2;
    vm2.id = "vm-002";
    vm2.name = "Production Server";
    vm2.status = "running";
    vm2.type = "hardware";
    vm2.cpuCount = 8;
    vm2.memorySize = 16384;
    vm2.diskSize = 256;
    vm2.uptime = "15d 4h";
    vm2.cpuUsage = 45.2;
    vm2.memoryUsage = 82.1;
    vm2.networkIP = "192.168.1.102";
    vm2.osType = "NEXUS Server";
    vm2.createdAt = "2024-01-01";
    m_vms.append(vm2);
    
    VMInfo vm3;
    vm3.id = "vm-003";
    vm3.name = "Test Container";
    vm3.status = "stopped";
    vm3.type = "container";
    vm3.cpuCount = 2;
    vm3.memorySize = 2048;
    vm3.diskSize = 32;
    vm3.uptime = "0h 0m";
    vm3.cpuUsage = 0;
    vm3.memoryUsage = 0;
    vm3.networkIP = "";
    vm3.osType = "Ubuntu 22.04";
    vm3.createdAt = "2024-02-01";
    m_vms.append(vm3);
    
    VMInfo vm4;
    vm4.id = "vm-004";
    vm4.name = "Secure Enclave";
    vm4.status = "paused";
    vm4.type = "enclave";
    vm4.cpuCount = 2;
    vm4.memorySize = 4096;
    vm4.diskSize = 64;
    vm4.uptime = "1h 23m";
    vm4.cpuUsage = 0;
    vm4.memoryUsage = 100;
    vm4.networkIP = "10.0.0.50";
    vm4.osType = "NEXUS Secure";
    vm4.createdAt = "2024-02-10";
    m_vms.append(vm4);
    
    m_filteredVMs = m_vms;
}

int VMModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_filteredVMs.size();
}

QVariant VMModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_filteredVMs.size())
        return QVariant();
    
    const VMInfo &vm = m_filteredVMs.at(index.row());
    
    switch (role) {
    case IdRole: return vm.id;
    case NameRole: return vm.name;
    case StatusRole: return vm.status;
    case TypeRole: return vm.type;
    case CPUCountRole: return vm.cpuCount;
    case MemorySizeRole: return vm.memorySize;
    case DiskSizeRole: return vm.diskSize;
    case UptimeRole: return vm.uptime;
    case CPUUsageRole: return vm.cpuUsage;
    case MemoryUsageRole: return vm.memoryUsage;
    case NetworkIPRole: return vm.networkIP;
    case OSTypeRole: return vm.osType;
    case CreatedAtRole: return vm.createdAt;
    default: return QVariant();
    }
}

QHash<int, QByteArray> VMModel::roleNames() const
{
    return {
        {IdRole, "vmId"},
        {NameRole, "vmName"},
        {StatusRole, "vmStatus"},
        {TypeRole, "vmType"},
        {CPUCountRole, "vmCPU"},
        {MemorySizeRole, "vmMemory"},
        {DiskSizeRole, "vmDisk"},
        {UptimeRole, "vmUptime"},
        {CPUUsageRole, "vmCPUUsage"},
        {MemoryUsageRole, "vmMemoryUsage"},
        {NetworkIPRole, "vmNetworkIP"},
        {OSTypeRole, "vmOS"},
        {CreatedAtRole, "vmCreatedAt"}
    };
}

int VMModel::runningCount() const
{
    int count = 0;
    for (const VMInfo &vm : m_vms) {
        if (vm.status == "running") count++;
    }
    return count;
}

QVariantList VMModel::getAllVMs() const
{
    QVariantList list;
    for (const VMInfo &vm : m_vms) {
        QVariantMap map;
        map["id"] = vm.id;
        map["name"] = vm.name;
        map["status"] = vm.status;
        map["type"] = vm.type;
        map["cpu"] = vm.cpuCount;
        map["memory"] = vm.memorySize;
        map["disk"] = vm.diskSize;
        map["uptime"] = vm.uptime;
        map["cpuUsage"] = vm.cpuUsage;
        map["memoryUsage"] = vm.memoryUsage;
        map["networkIP"] = vm.networkIP;
        map["os"] = vm.osType;
        list.append(map);
    }
    return list;
}

QVariantMap VMModel::getVM(const QString &id) const
{
    for (const VMInfo &vm : m_vms) {
        if (vm.id == id) {
            QVariantMap map;
            map["id"] = vm.id;
            map["name"] = vm.name;
            map["status"] = vm.status;
            map["type"] = vm.type;
            map["cpu"] = vm.cpuCount;
            map["memory"] = vm.memorySize;
            map["disk"] = vm.diskSize;
            return map;
        }
    }
    return QVariantMap();
}

void VMModel::addVM(const QVariantMap &vm)
{
    beginInsertRows(QModelIndex(), m_vms.size(), m_vms.size());
    
    VMInfo info;
    info.id = vm["id"].toString();
    info.name = vm["name"].toString();
    info.status = vm["status"].toString();
    info.type = vm["type"].toString();
    info.cpuCount = vm["cpu"].toInt();
    info.memorySize = vm["memory"].toInt();
    info.diskSize = vm["disk"].toInt();
    info.createdAt = QDateTime::currentDate().toString("yyyy-MM-dd");
    
    m_vms.append(info);
    m_filteredVMs = m_vms;
    
    endInsertRows();
    emit countChanged();
    emit vmAdded(info.id);
}

void VMModel::removeVM(const QString &id)
{
    for (int i = 0; i < m_vms.size(); i++) {
        if (m_vms[i].id == id) {
            beginRemoveRows(QModelIndex(), i, i);
            m_vms.removeAt(i);
            m_filteredVMs = m_vms;
            endRemoveRows();
            
            emit countChanged();
            emit vmRemoved(id);
            return;
        }
    }
}

void VMModel::updateVM(const QString &id, const QVariantMap &data)
{
    for (int i = 0; i < m_vms.size(); i++) {
        if (m_vms[i].id == id) {
            if (data.contains("status")) m_vms[i].status = data["status"].toString();
            if (data.contains("cpuUsage")) m_vms[i].cpuUsage = data["cpuUsage"].toDouble();
            if (data.contains("memoryUsage")) m_vms[i].memoryUsage = data["memoryUsage"].toDouble();
            if (data.contains("uptime")) m_vms[i].uptime = data["uptime"].toString();
            
            QModelIndex idx = index(i);
            emit dataChanged(idx, idx);
            emit vmUpdated(id);
            return;
        }
    }
}

void VMModel::refresh()
{
    // Refresh VM data from hypervisor
    // For demo, just emit data changed
    emit dataChanged(index(0), index(rowCount() - 1));
}

void VMModel::filterByStatus(const QString &status)
{
    beginResetModel();
    
    if (status.isEmpty() || status == "all") {
        m_filteredVMs = m_vms;
    } else {
        m_filteredVMs.clear();
        for (const VMInfo &vm : m_vms) {
            if (vm.status == status) {
                m_filteredVMs.append(vm);
            }
        }
    }
    
    endResetModel();
}

void VMModel::sortBy(const QString &field)
{
    std::sort(m_filteredVMs.begin(), m_filteredVMs.end(),
        [&field](const VMInfo &a, const VMInfo &b) {
            if (field == "name") return a.name < b.name;
            if (field == "cpu") return a.cpuCount < b.cpuCount;
            if (field == "memory") return a.memorySize < b.memorySize;
            if (field == "uptime") return a.uptime < b.uptime;
            return a.name < b.name;
        });
    
    emit dataChanged(index(0), index(rowCount() - 1));
}
