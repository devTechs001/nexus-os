#include "virtmode-manager.h"
#include <QSysInfo>
#include <QFile>
#include <QProcess>

VirtModeManager::VirtModeManager(QObject *parent)
    : QObject(parent)
{
    checkVirtualizationSupport();
}

bool VirtModeManager::checkVirtualizationSupport()
{
    // Check for hardware virtualization support
#ifdef __linux__
    QFile cpuinfo("/proc/cpuinfo");
    if (cpuinfo.open(QIODevice::ReadOnly)) {
        QByteArray content = cpuinfo.readAll();
        m_hardwareVirtSupported = content.contains("vmx") || content.contains("svm");
        m_nestedVirtSupported = content.contains("nested");
    }
#elif defined(_WIN32)
    // Check via systeminfo or registry
    m_hardwareVirtSupported = true; // Simplified
#endif
    
    emit supportCheckCompleted(m_hardwareVirtSupported);
    return m_hardwareVirtSupported;
}

QVariantMap VirtModeManager::getModeRecommendation() const
{
    QVariantMap recommendation;
    
    // Detect platform type
    QString platform = "desktop";
    int cpuCount = QThread::idealThreadCount();
    quint64 memory = QSysInfo::totalPhysicalMemory() / (1024 * 1024); // MB
    
    if (cpuCount <= 4 && memory <= 4096) {
        platform = "mobile";
        recommendation["mode"] = 1; // Light
        recommendation["reason"] = "Optimized for mobile/low-resource devices";
    } else if (cpuCount >= 16 && memory >= 32768) {
        platform = "server";
        recommendation["mode"] = 2; // Full
        recommendation["reason"] = "Enterprise features recommended for server hardware";
    } else {
        recommendation["mode"] = 1; // Light
        recommendation["reason"] = "Best balance for desktop use";
    }
    
    recommendation["platform"] = platform;
    recommendation["cpuCount"] = cpuCount;
    recommendation["memory"] = memory;
    
    return recommendation;
}

bool VirtModeManager::switchMode(int mode)
{
    // Switch virtualization mode (requires reboot)
    // This would interface with the boot configuration
    QString modeName;
    switch (mode) {
    case 0: modeName = "Native"; break;
    case 1: modeName = "Light"; break;
    case 2: modeName = "Full"; break;
    case 3: modeName = "Container"; break;
    case 4: modeName = "Secure Enclave"; break;
    case 5: modeName = "Compatibility"; break;
    case 6: modeName = "Custom"; break;
    }
    
    m_currentMode = modeName;
    emit currentModeChanged();
    
    return true;
}

void VirtModeManager::refreshVMList()
{
    // Get list of running VMs from hypervisor
    m_runningVMs.clear();
    
    // Example VM data
    QVariantMap vm1;
    vm1["id"] = "vm-001";
    vm1["name"] = "Development VM";
    vm1["status"] = "running";
    vm1["cpu"] = 4;
    vm1["memory"] = 4096;
    vm1["uptime"] = "2h 34m";
    m_runningVMs.append(vm1);
    
    QVariantMap vm2;
    vm2["id"] = "vm-002";
    vm2["name"] = "Test Environment";
    vm2["status"] = "paused";
    vm2["cpu"] = 2;
    vm2["memory"] = 2048;
    vm2["uptime"] = "0h 0m";
    m_runningVMs.append(vm2);
    
    emit runningVMsChanged();
}
