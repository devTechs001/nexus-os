/*
 * NEXUS OS - Package Manager
 * system/package-manager/package-manager.h
 * 
 * Copyright (c) 2026 NEXUS Development Team
 * 
 * Unified package management system supporting:
 * - Native NEXUS packages (.nexus-pkg)
 * - APT/DEB packages
 * - RPM packages
 * - Flatpak
 * - AppImage
 * - Source compilation (C, C++, etc.)
 */

#ifndef _NEXUS_PACKAGE_MANAGER_H
#define _NEXUS_PACKAGE_MANAGER_H

#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         PACKAGE TYPES                                     */
/*===========================================================================*/

#define PKG_TYPE_NEXUS          0
#define PKG_TYPE_DEB            1
#define PKG_TYPE_RPM            2
#define PKG_TYPE_FLATPAK        3
#define PKG_TYPE_APPIMAGE       4
#define PKG_TYPE_SOURCE         5
#define PKG_TYPE_SNAP           6

/*===========================================================================*/
/*                         PACKAGE STRUCTURES                                */
/*===========================================================================*/

/**
 * package_t - Package information
 */
typedef struct package {
    char id[128];
    char name[256];
    char version[64];
    char description[1024];
    char **authors;
    int author_count;
    char license[64];
    char homepage[256];
    
    /* Package info */
    int type;
    u64 size;
    u64 installed_size;
    char **dependencies;
    int dependency_count;
    char **conflicts;
    int conflict_count;
    
    /* Installation status */
    bool installed;
    bool upgradable;
    bool auto_installed;
    char install_date[32];
    char install_source[256];
    
    /* Repository info */
    char repository[128];
    char repository_url[256];
    char checksum[128];
    char signature[512];
    
    /* Build info (for source packages) */
    char **build_dependencies;
    int build_dependency_count;
    char configure_args[512];
    char make_args[128];
    
    /* Ratings */
    float rating;
    int rating_count;
    int download_count;
    
    /* Screenshots */
    char **screenshots;
    int screenshot_count;
    
    /* Categories */
    char **categories;
    int category_count;
    
    /* Links */
    struct package *next;
    struct package *prev;
} package_t;

/**
 * repository_t - Package repository
 */
typedef struct {
    char name[128];
    char url[256];
    char distribution[64];
    char components[256];
    bool enabled;
    bool signed;
    char gpg_key[512];
    int priority;
    package_t *packages;
    int package_count;
    u64 last_update;
} repository_t;

/**
 * package_manager_config_t - Package manager configuration
 */
typedef struct {
    /* General */
    char cache_dir[256];
    char install_dir[256];
    char temp_dir[256];
    
    /* Repositories */
    repository_t *repositories;
    int repository_count;
    
    /* Behavior */
    bool auto_resolve_deps;
    bool recommend_suggests;
    bool install_recommends;
    bool install_suggests;
    
    /* Security */
    bool verify_signatures;
    bool verify_checksums;
    bool sandbox_builds;
    
    /* Performance */
    int download_threads;
    bool use_cache;
    bool use_delta;
    
    /* UI */
    bool show_progress;
    bool assume_yes;
    int verbosity;
} package_manager_config_t;

/*===========================================================================*/
/*                         PACKAGE MANAGER API                               */
/*===========================================================================*/

/* Initialization */
int package_manager_init(package_manager_config_t *config);
int package_manager_shutdown(void);

/* Repository management */
int repository_add(const char *name, const char *url, const char *distribution);
int repository_remove(const char *name);
int repository_enable(const char *name);
int repository_disable(const char *name);
int repository_update(const char *name);
int repository_update_all(void);
int repository_list(void);

/* Package search */
package_t *package_search(const char *query);
package_t *package_search_name(const char *name);
package_t *package_search_file(const char *file);
package_t *package_get_installed(const char *name);
int package_list_installed(void);
int package_list_available(void);
int package_list_upgradable(void);

/* Package operations */
int package_install(const char *name);
int package_install_file(const char *path);
int package_remove(const char *name);
int package_update(const char *name);
int package_update_all(void);
int package_upgrade(const char *name);
int package_reinstall(const char *name);
int package_downgrade(const char *name, const char *version);

/* Dependency resolution */
int package_resolve_dependencies(package_t *pkg);
package_t **package_get_dependencies(package_t *pkg, int *count);
package_t **package_get_reverse_dependencies(package_t *pkg, int *count);

/* Source packages */
int package_build_source(const char *path);
int package_build_source_with_deps(const char *path, bool install_deps);
int package_install_build_deps(const char *name);

/* Cache operations */
int package_clean_cache(void);
int package_clean_cache_all(void);
int package_get_cache_size(void);

/* Verification */
int package_verify(package_t *pkg);
int package_verify_signature(package_t *pkg);
int package_verify_checksum(package_t *pkg);

/* Statistics */
int package_get_stats(void *stats);
int package_count_installed(void);
int package_count_available(void);

/* GUI integration */
int package_manager_gui_init(void);
int package_manager_gui_show(void);
int package_manager_gui_install(const char *name);
int package_manager_gui_remove(const char *name);

/* CLI interface */
int package_manager_cli(int argc, char **argv);

#endif /* _NEXUS_PACKAGE_MANAGER_H */
