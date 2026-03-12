/*
 * NEXUS OS - Boot Manager GUI
 * Copyright (c) 2024 NEXUS Development Team
 *
 * virtmode-manager.cpp - Virtualization Mode Manager Implementation
 */

#include "virtmode-manager.h"
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QSysInfo>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QRegularExpression>

/*===========================================================================*/
/*                         CONSTRUCTOR/DESTRUCTOR                            */
/*===========================================================================*/

VirtModeManager::VirtModeManager(QObject *parent)
    : QAbstractListModel(parent)
    , m_hwVirtSupported(false)
    , m_nestedVirtSupported(false)
    , m_cpuCores(0)
    , m_cpuLogicalCores(0)
    , m_totalMemory(0)
{
    qRegisterMetaType<VirtModeInfo>("VirtModeInfo");
    initializeModes();
    detectCapabilities();
}

VirtModeManager::~VirtModeManager()
{
}

/*===========================================================================*/
/*                         QABSTRACTLISTMODEL INTERFACE                      */
/*===========================================================================*/

int VirtModeManager::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_modes.size();
}

QVariant VirtModeManager::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_modes.size()) {
        return QVariant();
    }
    
    const VirtModeInfo &mode = m_modes.at(index.row());
    
    switch (role) {
    case IdRole:
        return mode.id;
    case NameRole:
        return mode.name;
    case DescriptionRole:
        return mode.description;
    case IconRole:
        return mode.icon;
    case SecurityLevelRole:
        return mode.securityLevel;
    case PerformanceLevelRole:
        return mode.performanceLevel;
    case IsAvailableRole:
        return mode.isAvailable;
    case RequiresHardwareSupportRole:
        return mode.requiresHardwareSupport;
    case RequirementsRole:
        return mode.requirements;
    case RecommendedForRole:
        return mode.recommendedFor;
    case Qt::DisplayRole:
        return mode.name;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> VirtModeManager::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "modeId";
    roles[NameRole] = "modeName";
    roles[DescriptionRole] = "modeDescription";
    roles[IconRole] = "modeIcon";
    roles[SecurityLevelRole] = "securityLevel";
    roles[PerformanceLevelRole] = "performanceLevel";
    roles[IsAvailableRole] = "isAvailable";
    roles[RequiresHardwareSupportRole] = "requiresHardwareSupport";
    roles[RequirementsRole] = "requirements";
    roles[RecommendedForRole] = "recommendedFor";
    return roles;
}

/*===========================================================================*/
/*                         PROPERTIES                                        */
/*===========================================================================*/

void VirtModeManager::setSelectedMode(const QString &mode)
{
    if (m_selectedMode != mode) {
        m_selectedMode = mode;
        emit selectedModeChanged();
    }
}

/*===========================================================================*/
/*                         CAPABILITIES DETECTION                            */
/*===========================================================================*/

void VirtModeManager::detectCapabilities()
{
    detectCPUInfo();
    detectMemoryInfo();
    
    // Check virtualization support
    m_hwVirtSupported = checkVTxSupport() || checkSVM_SUPPORT();
    m_nestedVirtSupported = false; // Requires additional checks
    
    emit capabilitiesChanged();
    
    // Update mode availability based on capabilities
    for (VirtModeInfo &mode : m_modes) {
        bool wasAvailable = mode.isAvailable;
        
        if (mode.requiresHardwareSupport && !m_hwVirtSupported) {
            mode.isAvailable = false;
        } else {
            mode.isAvailable = true;
        }
        
        if (wasAvailable != mode.isAvailable) {
            emit modeAvailabilityChanged();
        }
    }
}

void VirtModeManager::detectCPUInfo()
{
    m_cpuCores = QThread::idealThreadCount();
    m_cpuLogicalCores = m_cpuCores;
    
    // Try to get CPU vendor from /proc/cpuinfo (Linux)
    QFile cpuInfo("/proc/cpuinfo");
    if (cpuInfo.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&cpuInfo);
        QString content = in.readAll();
        cpuInfo.close();
        
        // Extract vendor
        QRegularExpression vendorRe("vendor_id\\s*:\\s*(\\w+)");
        QMatch match = vendorRe.match(content);
        if (match.hasMatch()) {
            m_cpuVendor = match.captured(1);
        } else {
            m_cpuVendor = "Unknown";
        }
        
        // Extract features
        QRegularExpression featuresRe("flags\\s*:\\s*([^\n]+)");
        match = featuresRe.match(content);
        if (match.hasMatch()) {
            m_cpuFeatures = match.captured(1).trimmed();
        }
        
        // Count physical cores
        QRegularExpression coreRe("cpu cores\\s*:\\s*(\\d+)");
        match = coreRe.match(content);
        if (match.hasMatch()) {
            m_cpuCores = match.captured(1).toInt();
        }
    } else {
        m_cpuVendor = QSysInfo::currentCpuArchitecture();
        m_cpuFeatures = "";
    }
}

void VirtModeManager::detectMemoryInfo()
{
    // Try to get memory from /proc/meminfo (Linux)
    QFile memInfo("/proc/meminfo");
    if (memInfo.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&memInfo);
        QString content = in.readAll();
        memInfo.close();
        
        QRegularExpression memRe("MemTotal:\\s*(\\d+)\\s*kB");
        QMatch match = memRe.match(content);
        if (match.hasMatch()) {
            m_totalMemory = match.captured(1).toULongLong() * 1024; // Convert to bytes
        }
    } else {
        // Fallback: estimate from system
        m_totalMemory = QSysInfo::totalMemory();
    }
}

bool VirtModeManager::checkVTxSupport() const
{
    // Check for Intel VT-x support
    QFile cpuInfo("/proc/cpuinfo");
    if (cpuInfo.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString content = cpuInfo.readAll();
        cpuInfo.close();
        
        // Check for vmx flag (Intel VT-x)
        if (content.contains("vmx")) {
            return true;
        }
    }
    
    // Check via KVM module (Linux)
    QFileInfo kvmDev("/dev/kvm");
    if (kvmDev.exists()) {
        return true;
    }
    
    return false;
}

bool VirtModeManager::checkSVM_SUPPORT() const
{
    // Check for AMD SVM support
    QFile cpuInfo("/proc/cpuinfo");
    if (cpuInfo.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString content = cpuInfo.readAll();
        cpuInfo.close();
        
        // Check for svm flag (AMD-V)
        if (content.contains("svm")) {
            return true;
        }
    }
    
    return false;
}

/*===========================================================================*/
/*                         MODE INFO                                         */
/*===========================================================================*/

VirtModeInfo VirtModeManager::getModeInfo(const QString &id) const
{
    for (const VirtModeInfo &mode : m_modes) {
        if (mode.id == id) {
            return mode;
        }
    }
    return VirtModeInfo();
}

QString VirtModeManager::getModeDescription(const QString &id) const
{
    VirtModeInfo mode = getModeInfo(id);
    return mode.description;
}

bool VirtModeManager::isModeAvailable(const QString &id) const
{
    VirtModeInfo mode = getModeInfo(id);
    return mode.isAvailable;
}

/*===========================================================================*/
/*                         VALIDATION                                        */
/*===========================================================================*/

bool VirtModeManager::validateMode(const QString &id) const
{
    VirtModeInfo mode = getModeInfo(id);
    if (!mode.isAvailable) {
        return false;
    }
    
    if (mode.requiresHardwareSupport && !m_hwVirtSupported) {
        return false;
    }
    
    return true;
}

QStringList VirtModeManager::getModeWarnings(const QString &id) const
{
    QStringList warnings;
    VirtModeInfo mode = getModeInfo(id);
    
    if (mode.requiresHardwareSupport && !m_hwVirtSupported) {
        warnings << "Hardware virtualization not supported on this system";
    }
    
    if (mode.securityLevel >= 4 && m_totalMemory < 4ULL * 1024 * 1024 * 1024) {
        warnings << "High security mode requires at least 4GB RAM";
    }
    
    return warnings;
}

QStringList VirtModeManager::getModeRecommendations(const QString &id) const
{
    QStringList recommendations;
    VirtModeInfo mode = getModeInfo(id);
    
    if (mode.performanceLevel >= 4) {
        recommendations << "Recommended for development and gaming";
    }
    
    if (mode.securityLevel >= 4) {
        recommendations << "Recommended for enterprise and sensitive workloads";
    }
    
    if (mode.id == "light") {
        recommendations << "Best balance for everyday use";
    }
    
    return recommendations;
}

/*===========================================================================*/
/*                         SYSTEM INFO                                       */
/*===========================================================================*/

QString VirtModeManager::getSystemInfo() const
{
    QString info;
    info += QString("CPU: %1 (%2 cores, %3 threads)\n")
            .arg(m_cpuVendor)
            .arg(m_cpuCores)
            .arg(m_cpuLogicalCores);
    info += QString("Memory: %1 GB\n").arg(m_totalMemory / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
    info += QString("Virtualization: %1\n").arg(m_hwVirtSupported ? "Supported" : "Not Supported");
    info += QString("OS: %1 %2\n").arg(QSysInfo::productType(), QSysInfo::productVersion());
    return info;
}

QString VirtModeManager::getVirtualizationTech() const
{
    if (m_cpuVendor.contains("GenuineIntel", Qt::CaseInsensitive) ||
        m_cpuFeatures.contains("vmx")) {
        return "Intel VT-x";
    } else if (m_cpuVendor.contains("AuthenticAMD", Qt::CaseInsensitive) ||
               m_cpuFeatures.contains("svm")) {
        return "AMD-V";
    } else if (m_cpuFeatures.contains("hypervisor")) {
        return "Running in VM";
    }
    return "Unknown";
}

/*===========================================================================*/
/*                         INITIALIZE MODES                                  */
/*===========================================================================*/

void VirtModeManager::initializeModes()
{
    beginResetModel();
    m_modes.clear();
    
    // Native Mode
    VirtModeInfo native;
    native.id = "native";
    native.name = "Native Mode";
    native.description = "Direct hardware access with no virtualization overhead. Maximum performance for dedicated workstations.";
    native.icon = "qrc:/icons/native.svg";
    native.securityLevel = 1;
    native.performanceLevel = 5;
    native.isAvailable = true;
    native.requiresHardwareSupport = false;
    native.requirements = "None";
    native.recommendedFor = "Gaming, Development, Performance-critical tasks";
    m_modes.append(native);
    
    // Light Virtualization
    VirtModeInfo light;
    light.id = "light";
    light.name = "Light Virtualization";
    light.description = "Process isolation via micro-VMs with minimal overhead. Default mode for balanced security and performance.";
    light.icon = "qrc:/icons/light.svg";
    light.securityLevel = 3;
    light.performanceLevel = 4;
    light.isAvailable = true;
    light.requiresHardwareSupport = false;
    light.requirements = "None";
    light.recommendedFor = "Desktop, Laptop, Everyday use";
    m_modes.append(light);
    
    // Full Virtualization
    VirtModeInfo full;
    full.id = "full";
    full.name = "Full Virtualization";
    full.description = "Complete hardware virtualization with multiple isolated environments. Enterprise-grade security.";
    full.icon = "qrc:/icons/full.svg";
    full.securityLevel = 5;
    full.performanceLevel = 3;
    full.isAvailable = true;
    full.requiresHardwareSupport = true;
    full.requirements = "Intel VT-x or AMD-V";
    full.recommendedFor = "Enterprise, Server, Multi-tenant";
    m_modes.append(full);
    
    // Container Mode
    VirtModeInfo container;
    container.id = "container";
    container.name = "Container Mode";
    container.description = "Lightweight container isolation for fast application deployment. Development optimized.";
    container.icon = "qrc:/icons/container.svg";
    container.securityLevel = 2;
    container.performanceLevel = 5;
    container.isAvailable = true;
    container.requiresHardwareSupport = false;
    container.requirements = "None";
    container.recommendedFor = "Development, Testing, CI/CD";
    m_modes.append(container);
    
    // Secure Enclave
    VirtModeInfo enclave;
    enclave.id = "enclave";
    enclave.name = "Secure Enclave";
    enclave.description = "Maximum security isolation with hardware-backed encryption. For sensitive workloads.";
    enclave.icon = "qrc:/icons/enclave.svg";
    enclave.securityLevel = 5;
    enclave.performanceLevel = 2;
    enclave.isAvailable = true;
    enclave.requiresHardwareSupport = true;
    enclave.requirements = "Intel SGX or AMD SEV";
    enclave.recommendedFor = "Financial, Healthcare, Government";
    m_modes.append(enclave);
    
    // Compatibility Mode
    VirtModeInfo compat;
    compat.id = "compat";
    compat.name = "Compatibility Mode";
    compat.description = "Legacy OS compatibility layer to run Windows/Linux/macOS applications with translation.";
    compat.icon = "qrc:/icons/compat.svg";
    compat.securityLevel = 2;
    compat.performanceLevel = 2;
    compat.isAvailable = true;
    compat.requiresHardwareSupport = true;
    compat.requirements = "Hardware virtualization recommended";
    compat.recommendedFor = "Legacy applications, Cross-platform";
    m_modes.append(compat);
    
    // Custom Mode
    VirtModeInfo custom;
    custom.id = "custom";
    custom.name = "Custom Mode";
    custom.description = "Mix and match virtualization modes with per-application settings. For advanced users.";
    custom.icon = "qrc:/icons/custom.svg";
    custom.securityLevel = 3;
    custom.performanceLevel = 3;
    custom.isAvailable = true;
    custom.requiresHardwareSupport = false;
    custom.requirements = "Advanced configuration required";
    custom.recommendedFor = "Power users, Custom setups";
    m_modes.append(custom);
    
    endResetModel();
    
    // Set default selection
    m_selectedMode = "light";
}
