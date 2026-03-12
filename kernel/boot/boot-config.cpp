#include "boot-config.h"
#include <iostream>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/utsname.h>
#endif

namespace NexusOS {

BootConfigManager& BootConfigManager::instance() {
    static BootConfigManager instance;
    return instance;
}

bool BootConfigManager::loadConfig(const std::string& configPath) {
    std::ifstream file(configPath);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << configPath << std::endl;
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Parse configuration file
        // Format: key=value
        auto pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            if (key == "virt_mode") {
                m_config.virtMode = static_cast<BootConfig::VirtMode>(std::stoi(value));
            } else if (key == "secure_boot") {
                m_config.secureBoot = (value == "true");
            } else if (key == "fast_boot") {
                m_config.fastBoot = (value == "true");
            } else if (key == "num_cpus") {
                m_config.hardware.numCPUs = std::stou32(value);
            } else if (key == "memory_size") {
                m_config.hardware.memorySize = std::stou64(value);
            }
            // ... more options
        }
    }
    
    return true;
}

bool BootConfigManager::saveConfig(const std::string& configPath) {
    std::ofstream file(configPath);
    if (!file.is_open()) {
        return false;
    }
    
    file << "# NEXUS-OS Boot Configuration\n";
    file << "virt_mode=" << static_cast<int>(m_config.virtMode) << "\n";
    file << "secure_boot=" << (m_config.secureBoot ? "true" : "false") << "\n";
    file << "fast_boot=" << (m_config.fastBoot ? "true" : "false") << "\n";
    file << "num_cpus=" << m_config.hardware.numCPUs << "\n";
    file << "memory_size=" << m_config.hardware.memorySize << "\n";
    // ... more options
    
    return true;
}

BootConfig::Platform BootConfigManager::detectPlatform() {
#ifdef __ANDROID__
    return BootConfig::Platform::MOBILE;
#elif defined(_WIN32) || defined(__linux__)
    // Detect based on hardware characteristics
    int numCPUs = sysconf(_SC_NPROCESSORS_ONLN);
    uint64_t memory = sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGESIZE);
    
    if (numCPUs <= 2 && memory <= 2ULL * 1024 * 1024 * 1024) {
        return BootConfig::Platform::IOT;
    } else if (numCPUs >= 16 && memory >= 32ULL * 1024 * 1024 * 1024) {
        return BootConfig::Platform::SERVER;
    } else {
        return BootConfig::Platform::DESKTOP;
    }
#else
    return BootConfig::Platform::EMBEDDED;
#endif
}

void BootConfigManager::detectHardware() {
#ifdef _WIN32
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    m_config.hardware.numCPUs = sysInfo.dwNumberOfProcessors;
    
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    GlobalMemoryStatusEx(&memStatus);
    m_config.hardware.memorySize = memStatus.ullTotalPhys;
#else
    m_config.hardware.numCPUs = sysconf(_SC_NPROCESSORS_ONLN);
    m_config.hardware.memorySize = sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGESIZE);
#endif
}

void BootConfigManager::generateDefaultConfig(BootConfig::Platform platform) {
    switch (platform) {
    case BootConfig::Platform::MOBILE:
        m_config.virtMode = BootConfig::VirtMode::LIGHT;
        m_config.fastBoot = true;
        m_config.hardware.numCPUs = 0; // All
        m_config.userPrefs.defaultUI = "mobile";
        break;
        
    case BootConfig::Platform::DESKTOP:
        m_config.virtMode = BootConfig::VirtMode::LIGHT;
        m_config.fastBoot = true;
        m_config.secureBoot = true;
        m_config.userPrefs.defaultUI = "desktop";
        break;
        
    case BootConfig::Platform::SERVER:
        m_config.virtMode = BootConfig::VirtMode::FULL;
        m_config.fastBoot = false;
        m_config.secureBoot = true;
        m_config.virt.maxVMs = 256;
        m_config.userPrefs.defaultUI = "server";
        break;
        
    case BootConfig::Platform::IOT:
        m_config.virtMode = BootConfig::VirtMode::CONTAINER;
        m_config.fastBoot = true;
        m_config.security.enableEncryption = false;
        m_config.userPrefs.defaultUI = "minimal";
        break;
        
    case BootConfig::Platform::EMBEDDED:
        m_config.virtMode = BootConfig::VirtMode::NATIVE;
        m_config.fastBoot = true;
        m_config.hardware.enableGPU = false;
        m_config.userPrefs.defaultUI = "none";
        break;
        
    default:
        m_config.virtMode = BootConfig::VirtMode::LIGHT;
        m_config.userPrefs.defaultUI = "auto";
        break;
    }
}

bool BootConfigManager::validateConfig() const {
    // Validate virtualization mode
    if (m_config.virtMode > BootConfig::VirtMode::CUSTOM) {
        return false;
    }
    
    // Validate CPU count
    if (m_config.hardware.numCPUs > sysconf(_SC_NPROCESSORS_ONLN)) {
        return false;
    }
    
    // Validate memory size
    uint64_t totalMem = sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGESIZE);
    if (m_config.hardware.memorySize > totalMem) {
        return false;
    }
    
    return true;
}

bool BootConfigManager::applyConfig() {
    if (!validateConfig()) {
        std::cerr << "Invalid configuration" << std::endl;
        return false;
    }
    
    // Apply hardware limits
    // Apply security settings
    // Initialize virtualization
    
    m_configured = true;
    return true;
}

void BootConfigManager::showBootMenu() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║           NEXUS-OS Boot Configuration                    ║\n";
    std::cout << "╠══════════════════════════════════════════════════════════╣\n";
    std::cout << "║                                                          ║\n";
    std::cout << "║  Select Virtualization Mode:                             ║\n";
    std::cout << "║                                                          ║\n";
    std::cout << "║  [1] Native Mode (No Virtualization)                     ║\n";
    std::cout << "║      Direct hardware access, maximum performance         ║\n";
    std::cout << "║                                                          ║\n";
    std::cout << "║  [2] Light Virtualization (Default)                      ║\n";
    std::cout << "║      Process isolation, balanced security/performance    ║\n";
    std::cout << "║                                                          ║\n";
    std::cout << "║  [3] Full Virtualization                                 ║\n";
    std::cout << "║      Complete hardware virtualization, enterprise        ║\n";
    std::cout << "║                                                          ║\n";
    std::cout << "║  [4] Container Mode                                      ║\n";
    std::cout << "║      Lightweight containers, fast deployment             ║\n";
    std::cout << "║                                                          ║\n";
    std::cout << "║  [5] Secure Enclave Mode                                 ║\n";
    std::cout << "║      Maximum security isolation, hardware-backed         ║\n";
    std::cout << "║                                                          ║\n";
    std::cout << "║  [6] Compatibility Mode                                  ║\n";
    std::cout << "║      Legacy OS support, run Windows/Linux apps           ║\n";
    std::cout << "║                                                          ║\n";
    std::cout << "║  [7] Custom Configuration                                ║\n";
    std::cout << "║      Advanced user-defined settings                      ║\n";
    std::cout << "║                                                          ║\n";
    std::cout << "║  [0] Boot with Defaults                                  ║\n";
    std::cout << "║                                                          ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    std::cout << "Enter choice (default: 2): ";
}

} // namespace NexusOS
