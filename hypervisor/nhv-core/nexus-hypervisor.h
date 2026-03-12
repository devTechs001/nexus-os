#ifndef NEXUS_HYPERVISOR_H
#define NEXUS_HYPERVISOR_H

#include <cstdint>
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <functional>

namespace NexusOS {

// Virtualization Modes (User Selectable at Boot)
enum class VirtMode {
    NATIVE,             // No virtualization
    LIGHT,              // Process isolation (default)
    FULL,               // Full hardware virtualization
    CONTAINER,          // Container-based isolation
    SECURE_ENCLAVE,     // Maximum security isolation
    COMPATIBILITY,      // Legacy OS compatibility
    CUSTOM              // User-defined configuration
};

// VM Types
enum class VMType {
    HARDWARE_VM,        // Full hardware virtualization
    PROCESS_VM,         // Process-level virtualization
    CONTAINER_VM,       // Container-based VM
    SECURITY_ENCLAVE,   // Secure enclave VM
    COMPATIBILITY_VM    // Legacy compatibility VM
};

// VM State
enum class VMState {
    CREATED,
    STARTING,
    RUNNING,
    PAUSED,
    STOPPING,
    STOPPED,
    ERROR
};

// Virtual CPU Configuration
struct VCPUConfig {
    uint32_t numVCPU;
    uint64_t memorySize;    // MB
    bool hardwareAssist;
    bool nestedVirt;
    std::vector<uint32_t> cpuPin;
};

// Virtual Device Configuration
struct VDeviceConfig {
    std::string type;       // "network", "storage", "gpu", etc.
    std::map<std::string, std::string> properties;
    bool passthrough;
    std::string hostDevice;
};

// VM Configuration
struct VMConfig {
    std::string name;
    std::string uuid;
    VMType type;
    VCPUConfig cpu;
    std::vector<VDeviceConfig> devices;
    std::string bootImage;
    std::vector<std::string> bootArgs;
    bool autoStart;
    bool secureBoot;
    std::string securityProfile;
};

// VM Statistics
struct VMStats {
    double cpuUsage;
    uint64_t memoryUsed;
    uint64_t memoryTotal;
    uint64_t networkRx;
    uint64_t networkTx;
    uint64_t diskRead;
    uint64_t diskWrite;
    uint64_t uptime;
};

// Forward declarations
class VirtualMachine;
class VCPU;
class VMMemory;
class VDevice;

// ============================================================================
// NEXUS Hypervisor Core
// ============================================================================

class NEXUSHypervisor {
public:
    static NEXUSHypervisor& instance();
    
    // Initialization
    bool initialize(VirtMode mode);
    void shutdown();
    
    // Mode management
    VirtMode getCurrentMode() const { return m_mode; }
    bool isModeSupported(VirtMode mode) const;
    
    // VM lifecycle
    std::shared_ptr<VirtualMachine> createVM(const VMConfig& config);
    bool destroyVM(const std::string& vmId);
    std::shared_ptr<VirtualMachine> getVM(const std::string& vmId);
    std::vector<std::shared_ptr<VirtualMachine>> getAllVMs() const;
    
    // Resource management
    uint64_t getTotalMemory() const { return m_totalMemory; }
    uint64_t getAvailableMemory() const;
    uint64_t getTotalCPU() const { return m_numCPU; }
    
    // Statistics
    VMStats getHostStats() const;
    
    // Events
    using VMEventHandler = std::function<void(const std::string&, VMState)>;
    void registerVMHandler(VMEventHandler handler);
    
private:
    NEXUSHypervisor();
    ~NEXUSHypervisor();
    NEXUSHypervisor(const&) = delete;
    NEXUSHypervisor& operator=(const&) = delete;
    
    bool initializeNative();
    bool initializeLight();
    bool initializeFull();
    bool initializeContainer();
    bool initializeSecureEnclave();
    bool initializeCompatibility();
    bool initializeCustom();
    
    VirtMode m_mode;
    uint64_t m_totalMemory;
    uint32_t m_numCPU;
    bool m_initialized;
    
    std::map<std::string, std::shared_ptr<VirtualMachine>> m_vms;
    std::vector<VMEventHandler> m_vmHandlers;
    
    mutable std::mutex m_mutex;
};

// ============================================================================
// Virtual Machine
// ============================================================================

class VirtualMachine {
public:
    VirtualMachine(const VMConfig& config);
    ~VirtualMachine();
    
    // Lifecycle
    bool start();
    bool stop();
    bool pause();
    bool resume();
    bool reset();
    
    // State
    VMState getState() const { return m_state; }
    const VMConfig& getConfig() const { return m_config; }
    std::string getId() const { return m_config.uuid; }
    
    // VCPU management
    uint32_t getVCPUCount() const { return m_config.cpu.numVCPU; }
    std::shared_ptr<VCPU> getVCPU(uint32_t index);
    
    // Memory management
    VMMemory* getMemory() { return m_memory.get(); }
    bool setMemory(uint64_t sizeMB);
    
    // Device management
    bool addDevice(const VDeviceConfig& config);
    bool removeDevice(const std::string& deviceId);
    VDevice* getDevice(const std::string& deviceId);
    
    // Console/Display
    bool getConsoleOutput(std::string& output);
    bool sendConsoleInput(const std::string& input);
    
    // Snapshots
    bool createSnapshot(const std::string& name);
    bool restoreSnapshot(const std::string& name);
    bool deleteSnapshot(const std::string& name);
    std::vector<std::string> listSnapshots() const;
    
    // Migration
    bool migrateTo(const std::string& targetHost);
    bool saveState(const std::string& filepath);
    bool loadState(const std::string& filepath);
    
    // Statistics
    VMStats getStats() const;
    
    // Events
    using StateHandler = std::function<void(VMState)>;
    void onStateChanged(StateHandler handler);
    
private:
    bool initialize();
    void setState(VMState state);
    
    VMConfig m_config;
    VMState m_state;
    
    std::vector<std::shared_ptr<VCPU>> m_vcpus;
    std::unique_ptr<VMMemory> m_memory;
    std::map<std::string, std::unique_ptr<VDevice>> m_devices;
    
    std::vector<StateHandler> m_stateHandlers;
    
    mutable std::mutex m_mutex;
};

// ============================================================================
// Virtual CPU
// ============================================================================

class VCPU {
public:
    VCPU(uint32_t vcpuId, const VCPUConfig& config);
    ~VCPU();
    
    uint32_t getId() const { return m_vcpuId; }
    
    // Execution control
    bool run();
    bool halt();
    bool singleStep();
    
    // Register access
    struct Registers {
        uint64_t rax, rbx, rcx, rdx;
        uint64_t rsi, rdi, rbp, rsp;
        uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
        uint64_t rip, rflags;
        uint64_t cr0, cr2, cr3, cr4;
        uint64_t efer;
    };
    
    bool getRegisters(Registers& regs);
    bool setRegisters(const Registers& regs);
    
    // Scheduling
    uint32_t getPinnedCPU() const { return m_pinnedCPU; }
    bool pinToCPU(uint32_t cpu);
    double getCPUUsage() const { return m_cpuUsage; }
    
private:
    uint32_t m_vcpuId;
    VCPUConfig m_config;
    uint32_t m_pinnedCPU;
    double m_cpuUsage;
    bool m_running;
    
    std::mutex m_mutex;
};

// ============================================================================
// Virtual Memory
// ============================================================================

class VMMemory {
public:
    VMMemory(uint64_t sizeMB);
    ~VMMemory();
    
    uint64_t getSize() const { return m_size; }
    void* getHostPointer(uint64_t guestAddress);
    
    // Memory operations
    bool read(uint64_t guestAddr, void* buffer, size_t size);
    bool write(uint64_t guestAddr, const void* buffer, size_t size);
    
    // Page tables
    bool mapPage(uint64_t guestAddr, uint64_t hostAddr, bool writable);
    bool unmapPage(uint64_t guestAddr);
    
    // Statistics
    uint64_t getUsedMemory() const;
    uint64_t getFreeMemory() const;
    
private:
    uint64_t m_size;
    void* m_hostMemory;
    std::map<uint64_t, uint64_t> m_pageTable;
    
    mutable std::mutex m_mutex;
};

// ============================================================================
// Virtual Device (Base Class)
// ============================================================================

class VDevice {
public:
    VDevice(const std::string& deviceId, const VDeviceConfig& config);
    virtual ~VDevice();
    
    std::string getId() const { return m_deviceId; }
    std::string getType() const { return m_config.type; }
    
    // Device operations
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual bool reset() = 0;
    
    // I/O operations
    virtual uint32_t readIO(uint16_t port) { return 0; }
    virtual void writeIO(uint16_t port, uint32_t value) {}
    virtual uint64_t readMMIO(uint64_t addr) { return 0; }
    virtual void writeMMIO(uint64_t addr, uint64_t value) {}
    
    // DMA
    virtual bool setupDMA(uint64_t guestAddr, size_t size) { return false; }
    
protected:
    std::string m_deviceId;
    VDeviceConfig m_config;
    bool m_initialized;
};

// ============================================================================
// Specific Virtual Devices
// ============================================================================

class VNetwork : public VDevice {
public:
    VNetwork(const std::string& id, const VDeviceConfig& config);
    
    bool initialize() override;
    void shutdown() override;
    bool reset() override;
    
    // Network-specific
    bool setMACAddress(const std::string& mac);
    std::string getMACAddress() const;
    bool connect(const std::string& network);
    bool disconnect();
    
private:
    std::string m_macAddress;
    bool m_connected;
};

class VStorage : public VDevice {
public:
    VStorage(const std::string& id, const VDeviceConfig& config);
    
    bool initialize() override;
    void shutdown() override;
    bool reset() override;
    
    // Storage-specific
    uint64_t getSize() const { return m_size; }
    bool read(uint64_t sector, void* buffer, size_t count);
    bool write(uint64_t sector, const void* buffer, size_t count);
    bool flush();
    
private:
    uint64_t m_size;
    std::string m_backingFile;
    int m_fd;
};

class VGPU : public VDevice {
public:
    VGPU(const std::string& id, const VDeviceConfig& config);
    
    bool initialize() override;
    void shutdown() override;
    bool reset() override;
    
    // GPU-specific
    bool allocateMemory(uint64_t size);
    bool freeMemory(uint64_t addr);
    bool submitCommand(const void* cmd, size_t size);
    
private:
    uint64_t m_vramSize;
    void* m_vramBuffer;
};

} // namespace NexusOS

#endif // NEXUS_HYPERVISOR_H
