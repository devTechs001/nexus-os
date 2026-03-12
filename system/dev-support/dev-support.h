/*
 * NEXUS OS - Multi-Language Development Support
 * system/dev-support/dev-support.h
 * 
 * Copyright (c) 2026 NEXUS Development Team
 * 
 * Supports multiple programming languages:
 * - C/C++
 * - Python
 * - Rust
 * - Go
 * - Java/Kotlin
 * - JavaScript/TypeScript (Node.js)
 * - Ruby
 * - PHP
 * - C# (.NET)
 * - Swift
 * - Kotlin
 */

#ifndef _NEXUS_DEV_SUPPORT_H
#define _NEXUS_DEV_SUPPORT_H

#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         LANGUAGE SUPPORT                                  */
/*===========================================================================*/

#define LANG_C              0
#define LANG_CPP            1
#define LANG_PYTHON         2
#define LANG_RUST           3
#define LANG_GO             4
#define LANG_JAVA           5
#define LANG_JAVASCRIPT     6
#define LANG_TYPESCRIPT     7
#define LANG_RUBY           8
#define LANG_PHP            9
#define LANG_CSHARP         10
#define LANG_SWIFT          11
#define LANG_KOTLIN         12
#define LANG_COUNT          13

/*===========================================================================*/
/*                         LANGUAGE CONFIGURATION                            */
/*===========================================================================*/

/**
 * language_config_t - Language configuration
 */
typedef struct {
    int id;
    char name[64];
    char version[32];
    char extension[16];
    char compile_cmd[256];
    char run_cmd[256];
    char debug_cmd[256];
    char package_manager[64];
    char ide_support[64];
    
    /* Toolchain */
    char compiler[128];
    char interpreter[128];
    char debugger[128];
    char formatter[128];
    char linter[128];
    
    /* Build system */
    char build_system[64];
    char build_file[64];
    char output_dir[128];
    
    /* Dependencies */
    char dep_file[64];
    char dep_dir[128];
    
    /* Runtime */
    char runtime_libs[256];
    char env_vars[512];
    
    /* Status */
    bool installed;
    bool configured;
    char install_path[256];
} language_config_t;

/**
 * project_config_t - Project configuration
 */
typedef struct {
    char name[128];
    char language[32];
    char version[32];
    char description[512];
    
    /* Source */
    char source_dir[256];
    char **source_files;
    int source_count;
    
    /* Build */
    char build_cmd[256];
    char run_cmd[256];
    char test_cmd[256];
    char output_name[128];
    
    /* Dependencies */
    char **dependencies;
    int dependency_count;
    
    /* IDE */
    char ide[64];
    char **ide_files;
    int ide_file_count;
    
    /* Settings */
    bool debug_mode;
    bool optimize;
    bool warnings_as_errors;
    int optimization_level;
} project_config_t;

/*===========================================================================*/
/*                         DEVELOPER SUPPORT API                             */
/*===========================================================================*/

/* Language management */
int dev_support_init(void);
int dev_support_shutdown(void);
int dev_support_detect_languages(void);
language_config_t *dev_support_get_language(int lang_id);
language_config_t *dev_support_get_language_by_name(const char *name);
int dev_support_list_languages(void);
int dev_support_install_language(int lang_id);
int dev_support_configure_language(int lang_id, language_config_t *config);

/* Project management */
int dev_support_create_project(const char *name, const char *language, const char *template);
int dev_support_load_project(const char *path, project_config_t *config);
int dev_support_save_project(project_config_t *config);
int dev_support_build_project(project_config_t *config);
int dev_support_run_project(project_config_t *config);
int dev_support_test_project(project_config_t *config);
int dev_support_clean_project(project_config_t *config);

/* Build systems */
int dev_support_build_cmake(const char *path);
int dev_support_build_make(const char *path);
int dev_support_build_meson(const char *path);
int dev_support_build_cargo(const char *path);  /* Rust */
int dev_support_build_go(const char *path);
int dev_support_build_gradle(const char *path);  /* Java/Kotlin */
int dev_support_build_npm(const char *path);  /* Node.js */
int dev_support_build_maven(const char *path);  /* Java */

/* Package managers */
int dev_support_install_package(const char *language, const char *package);
int dev_support_uninstall_package(const char *language, const char *package);
int dev_support_list_packages(const char *language);
int dev_support_update_packages(const char *language);

/* IDE Integration */
int dev_support_setup_vscode(const char *project_path);
int dev_support_setup_clion(const char *project_path);
int dev_support_setup_intellij(const char *project_path);
int dev_support_setup_eclipse(const char *project_path);
int dev_support_setup_vim(const char *project_path);
int dev_support_setup_emacs(const char *project_path);

/* Debugging */
int dev_support_start_debugger(const char *language, const char *program);
int dev_support_stop_debugger(void);
int dev_support_set_breakpoint(const char *file, int line);
int dev_support_step_over(void);
int dev_support_step_into(void);
int dev_support_step_out(void);
int dev_support_continue(void);

/* Code analysis */
int dev_support_run_linter(const char *language, const char *file);
int dev_support_run_formatter(const char *language, const char *file);
int dev_support_run_static_analysis(const char *language, const char *file);

/* Templates */
int dev_support_list_templates(const char *language);
int dev_support_get_template(const char *language, const char *name);
int dev_support_create_from_template(const char *language, const char *template, const char *output);

/* Documentation */
int dev_support_generate_docs(const char *language, const char *source_dir, const char *output_dir);
int dev_support_setup_docs(const char *language);

/* Testing */
int dev_support_run_tests(const char *language, const char *test_dir);
int dev_support_setup_testing(const char *language);

/* Container support */
int dev_support_create_container(const char *language, const char *project_path);
int dev_support_run_in_container(const char *language, const char *command);

/* Version management */
int dev_support_install_version(const char *language, const char *version);
int dev_support_switch_version(const char *language, const char *version);
int dev_support_list_versions(const char *language);

/* Quick commands */
int dev_support_quick_setup(const char *language);
int dev_support_quick_start(const char *language, const char *project_name);
int dev_support_quick_build(const char *project_path);
int dev_support_quick_run(const char *project_path);

#endif /* _NEXUS_DEV_SUPPORT_H */
