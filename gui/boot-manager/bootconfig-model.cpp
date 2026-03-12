#include "bootconfig-model.h"
#include <QSysInfo>
#include <QStandardPaths>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

BootConfigModel::BootConfigModel(QObject *parent)
    : QObject(parent)
{
    // Initialize virtualization modes
    m_virtModes = [this]() {
        QVariantList modes;
        
        QVariantMap native;
        native["id"] = 0;
        native["name"] = "Native Mode";
        native["description"] = "Direct hardware access, maximum performance";
        native["icon"] = "⚡";
        native["overhead"] = "0%";
        native["security"] = "Low";
        native["recommended"] = "Gaming, HPC";
        modes.append(native);
        
        QVariantMap light;
        light["id"] = 1;
        light["name"] = "Light Virtualization";
        light["description"] = "Process isolation, balanced security/performance";
        light["icon"] = "🔷";
        light["overhead"] = "<1%";
        light["security"] = "Medium";
        light["recommended"] = "Desktop, Mobile (Default)";
        light["default"] = true;
        modes.append(light);
        
        QVariantMap full;
        full["id"] = 2;
        full["name"] = "Full Virtualization";
        full["description"] = "Complete hardware virtualization, enterprise grade";
        full["icon"] = "🖥️";
        full["overhead"] = "2-5%";
        full["security"] = "High";
        full["recommended"] = "Servers, Enterprise";
        modes.append(full);
        
        QVariantMap container;
        container["id"] = 3;
        container["name"] = "Container Mode";
        container["description"] = "Lightweight containers, fast deployment";
        container["icon"] = "📦";
        container["overhead"] = "<1%";
        container["security"] = "Medium";
        container["recommended"] = "Development, Microservices";
        modes.append(container);
        
        QVariantMap enclave;
        enclave["id"] = 4;
        enclave["name"] = "Secure Enclave";
        enclave["description"] = "Maximum security isolation, hardware-backed";
        enclave["icon"] = "🔒";
        enclave["overhead"] = "5-10%";
        enclave["security"] = "Maximum";
        enclave["recommended"] = "Financial, Healthcare";
        modes.append(enclave);
        
        QVariantMap compat;
        compat["id"] = 5;
        compat["name"] = "Compatibility Mode";
        compat["description"] = "Legacy OS support, run Windows/Linux apps";
        compat["icon"] = "🔄";
        compat["overhead"] = "10-30%";
        compat["security"] = "Medium";
        compat["recommended"] = "Legacy Applications";
        modes.append(compat);
        
        QVariantMap custom;
        custom["id"] = 6;
        custom["name"] = "Custom Configuration";
        custom["description"] = "Advanced user-defined settings";
        custom["icon"] = "⚙️";
        custom["overhead"] = "Variable";
        custom["security"] = "Custom";
        custom["recommended"] = "Advanced Users";
        modes.append(custom);
        
        return modes;
    }();
    
    // Detect hardware
    detectHardware();
}

void BootConfigModel::setSelectedVirtMode(int mode)
{
    if (m_selectedVirtMode != mode) {
        m_selectedVirtMode = mode;
        emit selectedVirtModeChanged();
        emit selectedVirtModeNameChanged();
    }
}

QString BootConfigModel::selectedVirtModeName() const
{
    for (const QVariant &mode : m_virtModes) {
        QVariantMap map = mode.toMap();
        if (map["id"].toInt() == m_selectedVirtMode) {
            return map["name"].toString();
        }
    }
    return "Unknown";
}

void BootConfigModel::setSecureBoot(bool enabled)
{
    if (m_secureBoot != enabled) {
        m_secureBoot = enabled;
        emit secureBootChanged();
    }
}

void BootConfigModel::setFastBoot(bool enabled)
{
    if (m_fastBoot != enabled) {
        m_fastBoot = enabled;
        emit fastBootChanged();
    }
}

void BootConfigModel::setVerboseBoot(bool enabled)
{
    if (m_verboseBoot != enabled) {
        m_verboseBoot = enabled;
        emit verboseBootChanged();
    }
}

void BootConfigModel::setCpuCount(int count)
{
    if (m_cpuCount != count) {
        m_cpuCount = count;
        emit cpuCountChanged();
    }
}

void BootConfigModel::setMemorySize(int size)
{
    if (m_memorySize != size) {
        m_memorySize = size;
        emit memorySizeChanged();
    }
}

void BootConfigModel::setSecurityProfile(const QString &profile)
{
    if (m_securityProfile != profile) {
        m_securityProfile = profile;
        emit securityProfileChanged();
    }
}

QVariantList BootConfigModel::getVirtModes() const
{
    return m_virtModes;
}

QVariantMap BootConfigModel::getVirtModeDetails(int mode) const
{
    for (const QVariant &m : m_virtModes) {
        QVariantMap map = m.toMap();
        if (map["id"].toInt() == mode) {
            return map;
        }
    }
    return QVariantMap();
}

void BootConfigModel::loadConfig(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit configError("Failed to open config file");
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject obj = doc.object();
    
    m_selectedVirtMode = obj["virt_mode"].toInt(1);
    m_secureBoot = obj["secure_boot"].toBool(true);
    m_fastBoot = obj["fast_boot"].toBool(true);
    m_verboseBoot = obj["verbose_boot"].toBool(false);
    m_cpuCount = obj["cpu_count"].toInt(0);
    m_memorySize = obj["memory_size"].toInt(0);
    m_securityProfile = obj["security_profile"].toString("balanced");
    
    emit selectedVirtModeChanged();
    emit secureBootChanged();
    emit fastBootChanged();
    emit verboseBootChanged();
    emit cpuCountChanged();
    emit memorySizeChanged();
    emit securityProfileChanged();
}

void BootConfigModel::saveConfig(const QString &path)
{
    QJsonObject obj;
    obj["virt_mode"] = m_selectedVirtMode;
    obj["secure_boot"] = m_secureBoot;
    obj["fast_boot"] = m_fastBoot;
    obj["verbose_boot"] = m_verboseBoot;
    obj["cpu_count"] = m_cpuCount;
    obj["memory_size"] = m_memorySize;
    obj["security_profile"] = m_securityProfile;
    
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        emit configError("Failed to save config file");
        return;
    }
    
    file.write(QJsonDocument(obj).toJson());
}

void BootConfigModel::applyConfig()
{
    // Apply configuration to system
    // This would interface with the boot configuration system
    saveConfig(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + 
               "/nexus/boot.conf");
    emit configApplied();
}

void BootConfigModel::resetToDefaults()
{
    m_selectedVirtMode = 1; // Light virtualization
    m_secureBoot = true;
    m_fastBoot = true;
    m_verboseBoot = false;
    m_cpuCount = 0;
    m_memorySize = 0;
    m_securityProfile = "balanced";
    
    emit selectedVirtModeChanged();
    emit secureBootChanged();
    emit fastBootChanged();
    emit verboseBootChanged();
    emit cpuCountChanged();
    emit memorySizeChanged();
    emit securityProfileChanged();
}

bool BootConfigModel::validateConfig() const
{
    if (m_selectedVirtMode < 0 || m_selectedVirtMode > 6) {
        return false;
    }
    
    if (m_cpuCount < 0) {
        return false;
    }
    
    if (m_memorySize < 0) {
        return false;
    }
    
    return true;
}

void BootConfigModel::detectHardware()
{
    m_cpuCount = QThread::idealThreadCount();
    m_memorySize = QSysInfo::totalPhysicalMemory() / (1024 * 1024); // MB
}
