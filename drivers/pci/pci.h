/*
 * NEXUS OS - PCI Driver Registration
 * drivers/pci/pci.h
 *
 * drivers/bluetooth/bluetooth.c and drivers/network/wifi.c both call
 * pci_register_driver() at init time, but no header declared it - the
 * compiler fell back to an implicit int-returning prototype.
 */

#ifndef _NEXUS_PCI_H
#define _NEXUS_PCI_H

#include "../../kernel/include/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * pci_register_driver - Register a PCI driver probe callback
 * @vendor:  PCI vendor ID
 * @device:  PCI device ID
 * @probe:   Called with (vendor, device, bar0) when a matching device appears
 * @name:    Human readable driver name
 *
 * Returns: 0 on success, negative error code on failure
 */
int pci_register_driver(u16 vendor, u16 device,
                        int (*probe)(u16, u16, u32),
                        const char *name);

#ifdef __cplusplus
}
#endif

#endif /* _NEXUS_PCI_H */
