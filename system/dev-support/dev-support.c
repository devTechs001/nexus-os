/*
 * NEXUS OS - Multi-Language Development Support Implementation
 * system/dev-support/dev-support.c
 * 
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "dev-support.h"

/*===========================================================================*/
/*                         LANGUAGE CONFIGURATIONS                           */
/*===========================================================================*/

static language_config_t g_languages[LANG_COUNT];
static bool g_initialized = false;

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

/**
 * dev_support_init - Initialize developer support
 */
int dev_support_init(void)
{
    if (g_initialized) return 0;
    
    printk("[DEV] Initializing development support...\n");
    
    /* Initialize C/C++ */
    g_languages[LANG_C].id = LANG_C;
    strcpy(g_languages[LANG_C].name, "C");
    strcpy(g_languages[LANG_C].extension, ".c");
    strcpy(g_languages[LANG_C].compile_cmd, "gcc -o ${OUTPUT} ${SOURCE}");
    strcpy(g_languages[LANG_C].run_cmd, "./${OUTPUT}");
    strcpy(g_languages[LANG_C].debug_cmd, "gdb ${OUTPUT}");
    strcpy(g_languages[LANG_C].package_manager, "apt");
    strcpy(g_languages[LANG_C].compiler, "gcc");
    strcpy(g_languages[LANG_C].debugger, "gdb");
    strcpy(g_languages[LANG_C].formatter, "clang-format");
    strcpy(g_languages[LANG_C].linter, "clang-tidy");
    strcpy(g_languages[LANG_C].build_system, "make");
    strcpy(g_languages[LANG_C].build_file, "Makefile");
    
    /* Initialize C++ */
    g_languages[LANG_CPP].id = LANG_CPP;
    strcpy(g_languages[LANG_CPP].name, "C++");
    strcpy(g_languages[LANG_CPP].extension, ".cpp");
    strcpy(g_languages[LANG_CPP].compile_cmd, "g++ -o ${OUTPUT} ${SOURCE}");
    strcpy(g_languages[LANG_CPP].run_cmd, "./${OUTPUT}");
    strcpy(g_languages[LANG_CPP].debug_cmd, "gdb ${OUTPUT}");
    strcpy(g_languages[LANG_CPP].package_manager, "apt");
    strcpy(g_languages[LANG_CPP].compiler, "g++");
    strcpy(g_languages[LANG_CPP].debugger, "gdb");
    strcpy(g_languages[LANG_CPP].formatter, "clang-format");
    strcpy(g_languages[LANG_CPP].linter, "clang-tidy");
    strcpy(g_languages[LANG_CPP].build_system, "cmake");
    strcpy(g_languages[LANG_CPP].build_file, "CMakeLists.txt");
    
    /* Initialize Python */
    g_languages[LANG_PYTHON].id = LANG_PYTHON;
    strcpy(g_languages[LANG_PYTHON].name, "Python");
    strcpy(g_languages[LANG_PYTHON].extension, ".py");
    strcpy(g_languages[LANG_PYTHON].run_cmd, "python3 ${SOURCE}");
    strcpy(g_languages[LANG_PYTHON].debug_cmd, "python3 -m pdb ${SOURCE}");
    strcpy(g_languages[LANG_PYTHON].package_manager, "pip");
    strcpy(g_languages[LANG_PYTHON].interpreter, "python3");
    strcpy(g_languages[LANG_PYTHON].debugger, "pdb");
    strcpy(g_languages[LANG_PYTHON].formatter, "black");
    strcpy(g_languages[LANG_PYTHON].linter, "pylint");
    strcpy(g_languages[LANG_PYTHON].build_system, "setuptools");
    strcpy(g_languages[LANG_PYTHON].build_file, "setup.py");
    
    /* Initialize Rust */
    g_languages[LANG_RUST].id = LANG_RUST;
    strcpy(g_languages[LANG_RUST].name, "Rust");
    strcpy(g_languages[LANG_RUST].extension, ".rs");
    strcpy(g_languages[LANG_RUST].compile_cmd, "cargo build");
    strcpy(g_languages[LANG_RUST].run_cmd, "cargo run");
    strcpy(g_languages[LANG_RUST].debug_cmd, "cargo run -- --debug");
    strcpy(g_languages[LANG_RUST].package_manager, "cargo");
    strcpy(g_languages[LANG_RUST].compiler, "rustc");
    strcpy(g_languages[LANG_RUST].debugger, "gdb");
    strcpy(g_languages[LANG_RUST].formatter, "rustfmt");
    strcpy(g_languages[LANG_RUST].linter, "clippy");
    strcpy(g_languages[LANG_RUST].build_system, "cargo");
    strcpy(g_languages[LANG_RUST].build_file, "Cargo.toml");
    
    /* Initialize Go */
    g_languages[LANG_GO].id = LANG_GO;
    strcpy(g_languages[LANG_GO].name, "Go");
    strcpy(g_languages[LANG_GO].extension, ".go");
    strcpy(g_languages[LANG_GO].compile_cmd, "go build -o ${OUTPUT} ${SOURCE}");
    strcpy(g_languages[LANG_GO].run_cmd, "go run ${SOURCE}");
    strcpy(g_languages[LANG_GO].debug_cmd, "dlv debug ${SOURCE}");
    strcpy(g_languages[LANG_GO].package_manager, "go");
    strcpy(g_languages[LANG_GO].compiler, "go");
    strcpy(g_languages[LANG_GO].debugger, "dlv");
    strcpy(g_languages[LANG_GO].formatter, "gofmt");
    strcpy(g_languages[LANG_GO].linter, "golint");
    strcpy(g_languages[LANG_GO].build_system, "go");
    strcpy(g_languages[LANG_GO].build_file, "go.mod");
    
    /* Initialize Java */
    g_languages[LANG_JAVA].id = LANG_JAVA;
    strcpy(g_languages[LANG_JAVA].name, "Java");
    strcpy(g_languages[LANG_JAVA].extension, ".java");
    strcpy(g_languages[LANG_JAVA].compile_cmd, "javac ${SOURCE}");
    strcpy(g_languages[LANG_JAVA].run_cmd, "java ${CLASS}");
    strcpy(g_languages[LANG_JAVA].debug_cmd, "java -agentlib:jdwp ${CLASS}");
    strcpy(g_languages[LANG_JAVA].package_manager, "maven");
    strcpy(g_languages[LANG_JAVA].compiler, "javac");
    strcpy(g_languages[LANG_JAVA].debugger, "jdb");
    strcpy(g_languages[LANG_JAVA].formatter, "google-java-format");
    strcpy(g_languages[LANG_JAVA].linter, "checkstyle");
    strcpy(g_languages[LANG_JAVA].build_system, "maven");
    strcpy(g_languages[LANG_JAVA].build_file, "pom.xml");
    
    /* Initialize JavaScript/Node.js */
    g_languages[LANG_JAVASCRIPT].id = LANG_JAVASCRIPT;
    strcpy(g_languages[LANG_JAVASCRIPT].name, "JavaScript");
    strcpy(g_languages[LANG_JAVASCRIPT].extension, ".js");
    strcpy(g_languages[LANG_JAVASCRIPT].run_cmd, "node ${SOURCE}");
    strcpy(g_languages[LANG_JAVASCRIPT].debug_cmd, "node --inspect ${SOURCE}");
    strcpy(g_languages[LANG_JAVASCRIPT].package_manager, "npm");
    strcpy(g_languages[LANG_JAVASCRIPT].interpreter, "node");
    strcpy(g_languages[LANG_JAVASCRIPT].debugger, "node-inspect");
    strcpy(g_languages[LANG_JAVASCRIPT].formatter, "prettier");
    strcpy(g_languages[LANG_JAVASCRIPT].linter, "eslint");
    strcpy(g_languages[LANG_JAVASCRIPT].build_system, "npm");
    strcpy(g_languages[LANG_JAVASCRIPT].build_file, "package.json");
    
    /* Initialize TypeScript */
    g_languages[LANG_TYPESCRIPT].id = LANG_TYPESCRIPT;
    strcpy(g_languages[LANG_TYPESCRIPT].name, "TypeScript");
    strcpy(g_languages[LANG_TYPESCRIPT].extension, ".ts");
    strcpy(g_languages[LANG_TYPESCRIPT].compile_cmd, "tsc ${SOURCE}");
    strcpy(g_languages[LANG_TYPESCRIPT].run_cmd, "node ${OUTPUT}");
    strcpy(g_languages[LANG_TYPESCRIPT].package_manager, "npm");
    strcpy(g_languages[LANG_TYPESCRIPT].compiler, "tsc");
    strcpy(g_languages[LANG_TYPESCRIPT].formatter, "prettier");
    strcpy(g_languages[LANG_TYPESCRIPT].linter, "tslint");
    strcpy(g_languages[LANG_TYPESCRIPT].build_system, "npm");
    strcpy(g_languages[LANG_TYPESCRIPT].build_file, "package.json");
    
    /* Initialize Ruby */
    g_languages[LANG_RUBY].id = LANG_RUBY;
    strcpy(g_languages[LANG_RUBY].name, "Ruby");
    strcpy(g_languages[LANG_RUBY].extension, ".rb");
    strcpy(g_languages[LANG_RUBY].run_cmd, "ruby ${SOURCE}");
    strcpy(g_languages[LANG_RUBY].debug_cmd, "ruby -rdebug ${SOURCE}");
    strcpy(g_languages[LANG_RUBY].package_manager, "gem");
    strcpy(g_languages[LANG_RUBY].interpreter, "ruby");
    strcpy(g_languages[LANG_RUBY].formatter, "rubocop");
    strcpy(g_languages[LANG_RUBY].linter, "rubocop");
    
    /* Initialize PHP */
    g_languages[LANG_PHP].id = LANG_PHP;
    strcpy(g_languages[LANG_PHP].name, "PHP");
    strcpy(g_languages[LANG_PHP].extension, ".php");
    strcpy(g_languages[LANG_PHP].run_cmd, "php ${SOURCE}");
    strcpy(g_languages[LANG_PHP].package_manager, "composer");
    strcpy(g_languages[LANG_PHP].interpreter, "php");
    strcpy(g_languages[LANG_PHP].linter, "phpcs");
    
    /* Initialize C# */
    g_languages[LANG_CSHARP].id = LANG_CSHARP;
    strcpy(g_languages[LANG_CSHARP].name, "C#");
    strcpy(g_languages[LANG_CSHARP].extension, ".cs");
    strcpy(g_languages[LANG_CSHARP].compile_cmd, "dotnet build");
    strcpy(g_languages[LANG_CSHARP].run_cmd, "dotnet run");
    strcpy(g_languages[LANG_CSHARP].package_manager, "nuget");
    strcpy(g_languages[LANG_CSHARP].compiler, "dotnet");
    strcpy(g_languages[LANG_CSHARP].build_system, "dotnet");
    strcpy(g_languages[LANG_CSHARP].build_file, "*.csproj");
    
    /* Initialize Swift */
    g_languages[LANG_SWIFT].id = LANG_SWIFT;
    strcpy(g_languages[LANG_SWIFT].name, "Swift");
    strcpy(g_languages[LANG_SWIFT].extension, ".swift");
    strcpy(g_languages[LANG_SWIFT].compile_cmd, "swiftc ${SOURCE}");
    strcpy(g_languages[LANG_SWIFT].run_cmd, "swift ${SOURCE}");
    strcpy(g_languages[LANG_SWIFT].package_manager, "swift");
    strcpy(g_languages[LANG_SWIFT].compiler, "swiftc");
    strcpy(g_languages[LANG_SWIFT].build_system, "swift");
    strcpy(g_languages[LANG_SWIFT].build_file, "Package.swift");
    
    /* Initialize Kotlin */
    g_languages[LANG_KOTLIN].id = LANG_KOTLIN;
    strcpy(g_languages[LANG_KOTLIN].name, "Kotlin");
    strcpy(g_languages[LANG_KOTLIN].extension, ".kt");
    strcpy(g_languages[LANG_KOTLIN].compile_cmd, "kotlinc ${SOURCE}");
    strcpy(g_languages[LANG_KOTLIN].run_cmd, "kotlin ${CLASS}");
    strcpy(g_languages[LANG_KOTLIN].package_manager, "gradle");
    strcpy(g_languages[LANG_KOTLIN].compiler, "kotlinc");
    strcpy(g_languages[LANG_KOTLIN].build_system, "gradle");
    strcpy(g_languages[LANG_KOTLIN].build_file, "build.gradle");
    
    g_initialized = true;
    printk("[DEV] Development support initialized (%d languages)\n", LANG_COUNT);
    
    return 0;
}

/**
 * dev_support_shutdown - Shutdown developer support
 */
int dev_support_shutdown(void)
{
    if (!g_initialized) return -1;
    
    printk("[DEV] Shutting down development support...\n");
    g_initialized = false;
    
    return 0;
}

/*===========================================================================*/
/*                         LANGUAGE DETECTION                                */
/*===========================================================================*/

/**
 * dev_support_detect_languages - Detect installed languages
 */
int dev_support_detect_languages(void)
{
    if (!g_initialized) return -1;
    
    printk("[DEV] Detecting installed languages...\n");
    
    /* Check C/C++ */
    if (system("which gcc &>/dev/null && echo 1") == 0) {
        g_languages[LANG_C].installed = true;
        g_languages[LANG_CPP].installed = true;
        printk("[DEV] ✓ C/C++ detected\n");
    }
    
    /* Check Python */
    if (system("which python3 &>/dev/null && echo 1") == 0) {
        g_languages[LANG_PYTHON].installed = true;
        printk("[DEV] ✓ Python detected\n");
    }
    
    /* Check Rust */
    if (system("which rustc &>/dev/null && echo 1") == 0) {
        g_languages[LANG_RUST].installed = true;
        printk("[DEV] ✓ Rust detected\n");
    }
    
    /* Check Go */
    if (system("which go &>/dev/null && echo 1") == 0) {
        g_languages[LANG_GO].installed = true;
        printk("[DEV] ✓ Go detected\n");
    }
    
    /* Check Java */
    if (system("which javac &>/dev/null && echo 1") == 0) {
        g_languages[LANG_JAVA].installed = true;
        printk("[DEV] ✓ Java detected\n");
    }
    
    /* Check Node.js */
    if (system("which node &>/dev/null && echo 1") == 0) {
        g_languages[LANG_JAVASCRIPT].installed = true;
        g_languages[LANG_TYPESCRIPT].installed = true;
        printk("[DEV] ✓ JavaScript/TypeScript detected\n");
    }
    
    return 0;
}

/**
 * dev_support_get_language - Get language configuration
 */
language_config_t *dev_support_get_language(int lang_id)
{
    if (!g_initialized || lang_id < 0 || lang_id >= LANG_COUNT) {
        return NULL;
    }
    return &g_languages[lang_id];
}

/**
 * dev_support_get_language_by_name - Get language by name
 */
language_config_t *dev_support_get_language_by_name(const char *name)
{
    if (!g_initialized || !name) return NULL;
    
    for (int i = 0; i < LANG_COUNT; i++) {
        if (strcmp(g_languages[i].name, name) == 0) {
            return &g_languages[i];
        }
    }
    
    return NULL;
}

/**
 * dev_support_list_languages - List all languages
 */
int dev_support_list_languages(void)
{
    if (!g_initialized) return -1;
    
    printk("\n=== Supported Languages ===\n\n");
    
    for (int i = 0; i < LANG_COUNT; i++) {
        printk("  %2d. %-12s [%s]\n", 
               i + 1,
               g_languages[i].name,
               g_languages[i].installed ? "Installed" : "Not installed");
    }
    
    printk("\n");
    return 0;
}

/*===========================================================================*/
/*                         LANGUAGE INSTALLATION                             */
/*===========================================================================*/

/**
 * dev_support_install_language - Install a language
 */
int dev_support_install_language(int lang_id)
{
    if (!g_initialized || lang_id < 0 || lang_id >= LANG_COUNT) {
        return -1;
    }
    
    language_config_t *lang = &g_languages[lang_id];
    
    printk("[DEV] Installing %s...\n", lang->name);
    
    switch (lang_id) {
    case LANG_C:
    case LANG_CPP:
        system("sudo apt-get update && sudo apt-get install -y build-essential gcc g++ gdb clang-format clang-tidy");
        break;
    
    case LANG_PYTHON:
        system("sudo apt-get install -y python3 python3-pip python3-venv black pylint");
        break;
    
    case LANG_RUST:
        system("curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y");
        break;
    
    case LANG_GO:
        system("sudo apt-get install -y golang-go");
        break;
    
    case LANG_JAVA:
        system("sudo apt-get install -y openjdk-17-jdk maven gradle");
        break;
    
    case LANG_JAVASCRIPT:
    case LANG_TYPESCRIPT:
        system("sudo apt-get install -y nodejs npm");
        system("sudo npm install -g typescript tslint prettier eslint");
        break;
    
    case LANG_RUBY:
        system("sudo apt-get install -y ruby ruby-dev bundler rubocop");
        break;
    
    case LANG_PHP:
        system("sudo apt-get install -y php php-cli composer phpcs");
        break;
    
    case LANG_CSHARP:
        system("sudo apt-get install -y dotnet-sdk-7.0");
        break;
    
    case LANG_SWIFT:
        system("sudo apt-get install -y swift");
        break;
    
    case LANG_KOTLIN:
        system("sudo apt-get install -y kotlin");
        break;
    
    default:
        printk("[DEV] Unknown language: %d\n", lang_id);
        return -1;
    }
    
    lang->installed = true;
    printk("[DEV] %s installed successfully\n", lang->name);
    
    return 0;
}

/*===========================================================================*/
/*                         PROJECT MANAGEMENT                                */
/*===========================================================================*/

/**
 * dev_support_create_project - Create a new project
 */
int dev_support_create_project(const char *name, const char *language, const char *template)
{
    if (!name || !language) return -1;
    
    printk("[DEV] Creating %s project: %s\n", language, name);
    
    /* Create project directory */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", name);
    system(cmd);
    
    /* Create based on language */
    language_config_t *lang = dev_support_get_language_by_name(language);
    if (!lang) {
        printk("[DEV] Unknown language: %s\n", language);
        return -1;
    }
    
    /* Create template files */
    switch (lang->id) {
    case LANG_C:
        snprintf(cmd, sizeof(cmd), 
            "cd %s && "
            "echo '#include <stdio.h>' > main.c && "
            "echo 'int main() { printf(\"Hello from %s!\\n\"); return 0; }' >> main.c && "
            "echo 'CC=gcc' > Makefile && "
            "echo 'CFLAGS=-Wall -Wextra' >> Makefile && "
            "echo 'all: main' >> Makefile && "
            "echo 'main: main.c' >> Makefile && "
            "echo '\t$(CC) $(CFLAGS) -o main main.c' >> Makefile",
            name, name);
        system(cmd);
        break;
    
    case LANG_CPP:
        snprintf(cmd, sizeof(cmd),
            "cd %s && "
            "echo '#include <iostream>' > main.cpp && "
            "echo 'int main() { std::cout << \"Hello from %s!\" << std::endl; return 0; }' >> main.cpp && "
            "echo 'cmake_minimum_required(VERSION 3.10)' > CMakeLists.txt && "
            "echo 'project(%s)' >> CMakeLists.txt && "
            "echo 'set(CMAKE_CXX_STANDARD 17)' >> CMakeLists.txt && "
            "echo 'add_executable(main main.cpp)' >> CMakeLists.txt",
            name, name, name);
        system(cmd);
        break;
    
    case LANG_PYTHON:
        snprintf(cmd, sizeof(cmd),
            "cd %s && "
            "echo 'print(\"Hello from %s!\")' > main.py && "
            "echo '# NEXUS OS Python Project' > README.md && "
            "echo 'requirements.txt' >> .gitignore",
            name, name);
        system(cmd);
        break;
    
    case LANG_RUST:
        snprintf(cmd, sizeof(cmd), "cd %s && cargo init --name %s", name, name);
        system(cmd);
        break;
    
    case LANG_GO:
        snprintf(cmd, sizeof(cmd), "cd %s && go mod init %s", name, name);
        system(cmd);
        break;
    
    case LANG_JAVA:
        snprintf(cmd, sizeof(cmd), "cd %s && mvn archetype:generate -DgroupId=com.example -DartifactId=%s -DarchetypeArtifactId=maven-archetype-quickstart -DinteractiveMode=false", name, name);
        system(cmd);
        break;
    
    case LANG_JAVASCRIPT:
        snprintf(cmd, sizeof(cmd), "cd %s && npm init -y", name);
        system(cmd);
        break;
    }
    
    printk("[DEV] Project created: %s\n", name);
    return 0;
}

/**
 * dev_support_build_project - Build a project
 */
int dev_support_build_project(project_config_t *config)
{
    if (!config) return -1;
    
    printk("[DEV] Building project: %s\n", config->name);
    
    /* Execute build command */
    if (config->build_cmd[0]) {
        printk("[DEV] Running: %s\n", config->build_cmd);
        system(config->build_cmd);
    }
    
    return 0;
}

/**
 * dev_support_run_project - Run a project
 */
int dev_support_run_project(project_config_t *config)
{
    if (!config) return -1;
    
    printk("[DEV] Running project: %s\n", config->name);
    
    /* Execute run command */
    if (config->run_cmd[0]) {
        printk("[DEV] Running: %s\n", config->run_cmd);
        system(config->run_cmd);
    }
    
    return 0;
}

/*===========================================================================*/
/*                         QUICK COMMANDS                                    */
/*===========================================================================*/

/**
 * dev_support_quick_setup - Quick setup for a language
 */
int dev_support_quick_setup(const char *language)
{
    if (!language) return -1;
    
    printk("[DEV] Quick setup for %s...\n", language);
    
    language_config_t *lang = dev_support_get_language_by_name(language);
    if (!lang) {
        printk("[DEV] Unknown language: %s\n", language);
        return -1;
    }
    
    if (!lang->installed) {
        printk("[DEV] Installing %s...\n", language);
        dev_support_install_language(lang->id);
    }
    
    printk("[DEV] %s setup complete!\n", language);
    return 0;
}

/**
 * dev_support_quick_start - Quick start a new project
 */
int dev_support_quick_start(const char *language, const char *project_name)
{
    if (!language || !project_name) return -1;
    
    printk("[DEV] Quick start: %s project '%s'\n", language, project_name);
    
    /* Setup language if needed */
    dev_support_quick_setup(language);
    
    /* Create project */
    dev_support_create_project(project_name, language, "default");
    
    printk("[DEV] Ready to code! cd %s\n", project_name);
    return 0;
}

/**
 * dev_support_quick_build - Quick build a project
 */
int dev_support_quick_build(const char *project_path)
{
    if (!project_path) return -1;
    
    printk("[DEV] Building: %s\n", project_path);
    
    char cmd[512];
    
    /* Detect build system */
    snprintf(cmd, sizeof(cmd), "test -f %s/CMakeLists.txt && echo cmake", project_path);
    if (system(cmd) == 0) {
        snprintf(cmd, sizeof(cmd), "cd %s && mkdir -p build && cd build && cmake .. && make", project_path);
        system(cmd);
        return 0;
    }
    
    snprintf(cmd, sizeof(cmd), "test -f %s/Makefile && echo make", project_path);
    if (system(cmd) == 0) {
        snprintf(cmd, sizeof(cmd), "cd %s && make", project_path);
        system(cmd);
        return 0;
    }
    
    snprintf(cmd, sizeof(cmd), "test -f %s/Cargo.toml && echo cargo", project_path);
    if (system(cmd) == 0) {
        snprintf(cmd, sizeof(cmd), "cd %s && cargo build", project_path);
        system(cmd);
        return 0;
    }
    
    snprintf(cmd, sizeof(cmd), "test -f %s/package.json && echo npm", project_path);
    if (system(cmd) == 0) {
        snprintf(cmd, sizeof(cmd), "cd %s && npm install && npm run build", project_path);
        system(cmd);
        return 0;
    }
    
    printk("[DEV] Unknown build system in: %s\n", project_path);
    return -1;
}

/**
 * dev_support_quick_run - Quick run a project
 */
int dev_support_quick_run(const char *project_path)
{
    if (!project_path) return -1;
    
    printk("[DEV] Running: %s\n", project_path);
    
    char cmd[512];
    
    /* Try common run commands */
    snprintf(cmd, sizeof(cmd), "cd %s && ./main 2>/dev/null || ./a.out 2>/dev/null || echo 'No executable found'", project_path);
    system(cmd);
    
    return 0;
}

/*===========================================================================*/
/*                         IDE INTEGRATION                                   */
/*===========================================================================*/

/**
 * dev_support_setup_vscode - Setup VS Code for project
 */
int dev_support_setup_vscode(const char *project_path)
{
    if (!project_path) return -1;
    
    printk("[DEV] Setting up VS Code for: %s\n", project_path);
    
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s/.vscode", project_path);
    system(cmd);
    
    /* Create settings.json */
    snprintf(cmd, sizeof(cmd),
        "echo '{' > %s/.vscode/settings.json && "
        "echo '  \"editor.formatOnSave\": true,' >> %s/.vscode/settings.json && "
        "echo '  \"editor.defaultFormatter\": \"ms-vscode.cpptools\"' >> %s/.vscode/settings.json && "
        "echo '}' >> %s/.vscode/settings.json",
        project_path, project_path, project_path, project_path);
    system(cmd);
    
    /* Create tasks.json */
    snprintf(cmd, sizeof(cmd),
        "echo '{' > %s/.vscode/tasks.json && "
        "echo '  \"version\": \"2.0.0\",' >> %s/.vscode/tasks.json && "
        "echo '  \"tasks\": [' >> %s/.vscode/tasks.json && "
        "echo '    {' >> %s/.vscode/tasks.json && "
        "echo '      \"label\": \"build\",' >> %s/.vscode/tasks.json && "
        "echo '      \"type\": \"shell\",' >> %s/.vscode/tasks.json && "
        "echo '      \"command\": \"make\",' >> %s/.vscode/tasks.json && "
        "echo '      \"group\": \"build\"' >> %s/.vscode/tasks.json && "
        "echo '    }' >> %s/.vscode/tasks.json && "
        "echo '  ]' >> %s/.vscode/tasks.json && "
        "echo '}' >> %s/.vscode/tasks.json",
        project_path, project_path, project_path, project_path, project_path, project_path, project_path, project_path, project_path, project_path, project_path);
    system(cmd);
    
    printk("[DEV] VS Code setup complete\n");
    return 0;
}
