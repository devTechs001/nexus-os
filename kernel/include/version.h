/*
 * NEXUS OS - Kernel Version
 * kernel/include/version.h
 */

#ifndef _NEXUS_VERSION_H
#define _NEXUS_VERSION_H

#define NEXUS_VERSION_MAJOR     1
#define NEXUS_VERSION_MINOR     0
#define NEXUS_VERSION_PATCH     0
#define NEXUS_VERSION_STRING    "1.0.0-alpha"
#define NEXUS_CODENAME          "Genesis"

#define NEXUS_VERSION_CODE      ((NEXUS_VERSION_MAJOR << 16) | \
                                 (NEXUS_VERSION_MINOR << 8) |  \
                                 NEXUS_VERSION_PATCH)

#endif /* _NEXUS_VERSION_H */
