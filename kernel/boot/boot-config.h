#ifndef NEXUS_BOOT_CONFIG_H
#define NEXUS_BOOT_CONFIG_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace NexusOS {

// Boot Configuration Options
struct BootConfig {
    // Virtualization Mode
    enum class VirtMode {
        NATIVE,             // No virtualization
        LIGHT,              // Process isolation (default)
        FULL,               // Full hardware virtualization
        CONTAINER,          // Container-based
        SECURE_ENCLAVE,     // Maximum security
        COMPATIBILITY,      // Legacy support
        CUSTOM              // User-defined
    };
    
    VirtMode virtMode = VirtMode::LIGHT;
    
    // Boot Options
    bool secureBoot = true;
    bool fastBoot = true;
    bool verboseBoot = false;
    bool recoveryMode = false;
    bool safeMode = false;
    
    // Hardware Configuration
    struct HardwareConfig {
        uint32_t numCPUs = 0;         // 0 = all available
        uint64_t memorySize = 0;      // 0 = all available
        bool enableGPU = true;
        bool enableNetwork = true;
        bool enableAudio = true;
        std::vector<std::string> enabledDevices;
    } hardware;
    
    // Virtualization Settings
    struct VirtSettings {
        bool nestedVirt = false;
        bool hardwareAssist = true;
        bool iommuEnabled = true;
        bool srIovEnabled = false;
        uint32_t maxVMs = 64;
        std::string defaultVMType = "process";
    } virt;
    
    // Security Settings
    struct SecuritySettings {
        bool enableMAC = true;          // Mandatory Access Control
        bool enableEncryption = true;   // Disk encryption
        bool enableAudit = true;        // Security auditing
        bool enableFirewall = true;     // Network firewall
        std::string securityProfile = "balanced"; // minimal, balanced, maximum
    } security;
    
    // Platform Detection
    enum class Platform {
        UNKNOWN,
        MOBILE,
        DESKTOP,
        SERVER,
        IOT,
        EMBEDDED
    };
    
    Platform detectedPlatform = Platform::UNKNOWN;
    
    // Boot Arguments
    std::vector<std::string> kernelArgs;
    std::map<std::string, std::string> environment;
    
    // Boot Devices
    std::vector<std::string> bootDevices;
    std::string rootDevice;
    
    // Network Boot
    struct NetworkBoot {
        bool enabled = false;
        std::string serverIP;
        std::string bootFile;
    } networkBoot;
    
    // User Preferences
    struct UserPrefs {
        std::string defaultUI = "auto"; // auto, mobile, desktop, server
        std::string language = "en-US";
        std::string timezone = "UTC";
        std::string hostname = "nexus";
    } userPrefs;
};

// Boot Configuration Manager
class BootConfigManager {
public:
    static BootConfigManager& instance();
    
    // Load/Save configuration
    bool loadConfig(const std::string& configPath);
    bool saveConfig(const std::string& configPath);
    
    // Get current configuration
    const BootConfig& getConfig() const { return m_config; }
    
    // Platform detection
    BootConfig::Platform detectPlatform();
    
    // Hardware detection
    void detectHardware();
    
    // Generate default config based on platform
    void generateDefaultConfig(BootConfig::Platform platform);
    
    // Validate configuration
    bool validateConfig() const;
    
    // Apply configuration
    bool applyConfig();
    
    // Interactive configuration (for boot menu)
    void showBootMenu();
    
private:
    BootConfigManager() = default;
    ~BootConfigManager() = default;
    
    BootConfig m_config;
    bool m_configured = false;
};

} // namespace NexusOS

#endif // NEXUS_BOOT_CONFIG_H
