# Contributing to NEXUS-OS

## Welcome!

Thank you for your interest in contributing to NEXUS-OS! This document provides guidelines and instructions for contributing to the project.

## Table of Contents

1. [Code of Conduct](#code-of-conduct)
2. [Getting Started](#getting-started)
3. [Development Workflow](#development-workflow)
4. [Coding Standards](#coding-standards)
5. [Submitting Changes](#submitting-changes)
6. [Code Review Process](#code-review-process)
7. [Reporting Bugs](#reporting-bugs)
8. [Requesting Features](#requesting-features)
9. [Documentation](#documentation)
10. [Testing](#testing)

---

## Code of Conduct

### Our Pledge

We pledge to make participation in NEXUS-OS a harassment-free experience for everyone, regardless of age, body size, disability, ethnicity, gender identity and expression, level of experience, nationality, personal appearance, race, religion, or sexual identity and orientation.

### Our Standards

Examples of behavior that contributes to creating a positive environment:

- Using welcoming and inclusive language
- Being respectful of differing viewpoints and experiences
- Gracefully accepting constructive criticism
- Focusing on what is best for the community
- Showing empathy towards other community members

Examples of unacceptable behavior:

- The use of sexualized language or imagery and unwelcome sexual attention
- Trolling, insulting/derogatory comments, and personal or political attacks
- Public or private harassment
- Publishing others' private information without explicit permission
- Other conduct which could reasonably be considered inappropriate

### Enforcement

Instances of abusive, harassing, or otherwise unacceptable behavior may be reported by contacting the project team at nexus-os@darkhat.dev. All complaints will be reviewed and investigated and will result in a response that is deemed necessary and appropriate to the circumstances.

---

## Getting Started

### Prerequisites

Before contributing, please ensure you have:

1. Read the [Architecture Overview](../architecture/overview.md)
2. Set up your development environment (see [Building Guide](building.md))
3. Familiarized yourself with the codebase
4. Joined our communication channels (IRC, Discord, or mailing list)

### Finding Issues to Work On

Good starting points for new contributors:

- Issues labeled `good first issue`
- Issues labeled `help wanted`
- Documentation improvements
- Bug fixes
- Test improvements

Browse issues at: https://github.com/darkhat/NEXUS-OS/issues

### Setting Up Your Development Environment

```bash
# Fork the repository
git clone https://github.com/YOUR_USERNAME/NEXUS-OS.git
cd NEXUS-OS

# Add upstream remote
git remote add upstream https://github.com/darkhat/NEXUS-OS.git

# Create a branch for your work
git checkout -b feature/your-feature-name
```

---

## Development Workflow

### Branch Naming

Use descriptive branch names:

```
feature/add-new-driver          # New feature
fix/memory-leak-in-scheduler    # Bug fix
docs/update-api-docs            # Documentation
test/add-unit-tests             # Tests
refactor/cleanup-kernel-init    # Refactoring
```

### Commit Messages

Follow the conventional commit format:

```
<type>(<scope>): <subject>

<body>

<footer>
```

#### Types

- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style changes (formatting, etc.)
- `refactor`: Code refactoring
- `test`: Adding or updating tests
- `chore`: Build/config changes
- `perf`: Performance improvements

#### Examples

```
feat(drivers): add support for Intel I219-V network adapter

Add driver for Intel I219-V Gigabit Ethernet controller.
Supports basic TX/RX operations and interrupt handling.

Closes #123

---

fix(kernel): resolve race condition in slab allocator

The slab allocator had a race condition when allocating
from the same cache on multiple CPUs simultaneously.

Add per-CPU partial lists to eliminate lock contention.

Fixes #456

---

docs(api): update memory management API documentation

Add examples for kmalloc, kzalloc, and kfree functions.
Include memory alignment considerations.
```

### Making Changes

1. **Create a branch** from the latest main:
   ```bash
   git checkout main
   git pull upstream main
   git checkout -b feature/your-feature
   ```

2. **Make your changes** following coding standards

3. **Test your changes**:
   ```bash
   make test
   make qemu
   ```

4. **Commit your changes**:
   ```bash
   git add changed_file.c
   git commit -m "feat(scope): description"
   ```

5. **Push to your fork**:
   ```bash
   git push origin feature/your-feature
   ```

---

## Coding Standards

### C Coding Style

#### Indentation

- Use 4 spaces per indentation level (no tabs)
- Maximum line length: 100 characters

```c
/* Good */
void my_function(int arg1, int arg2)
{
    if (condition) {
        do_something();
    }
}

/* Bad */
void my_function(int arg1, int arg2) {  // Wrong brace style
    if (condition)  // Missing braces
        do_something();
}
```

#### Naming Conventions

```c
/* Types: lowercase with underscores */
typedef struct {
    int field1;
    int field2;
} my_type_t;

/* Functions: lowercase with underscores */
void my_function(void);
int calculate_checksum(void *data, size_t len);

/* Variables: lowercase with underscores */
int my_variable;
size_t buffer_size;

/* Constants: uppercase with underscores */
#define MAX_BUFFER_SIZE 4096
#define DEFAULT_TIMEOUT 1000

/* Struct/Union members: no prefix */
struct my_struct {
    int member1;
    void *member2;
};
```

#### Comments

```c
/*
 * Multi-line comment for functions and complex logic.
 * Explain the WHY, not just the WHAT.
 */
void complex_function(void)
{
    /* Single-line comment for simple explanations */
    int x = calculate_value();
    
    /* TODO: Optimize this for performance */
    /* FIXME: Handle edge case when x is negative */
    /* NOTE: This assumes x is always positive */
}
```

#### Function Structure

```c
/*
 * function_name - Brief description of what the function does
 * @param1: Description of parameter 1
 * @param2: Description of parameter 2
 *
 * Detailed description of the function's behavior,
 * including any side effects or special cases.
 *
 * Returns: Description of return value
 */
int function_name(int param1, const char *param2)
{
    /* Variable declarations at start of block */
    int result;
    void *buffer = NULL;
    
    /* Input validation */
    if (param2 == NULL) {
        return NEXUS_EINVAL;
    }
    
    /* Main logic */
    buffer = kmalloc(BUFFER_SIZE);
    if (!buffer) {
        return NEXUS_ENOMEM;
    }
    
    result = do_work(buffer, param1);
    
    /* Cleanup */
    kfree(buffer);
    
    return result;
}
```

#### Error Handling

```c
/* Always check return values */
void *ptr = kmalloc(size);
if (!ptr) {
    pr_err("Failed to allocate %zu bytes\n", size);
    return NEXUS_ENOMEM;
}

/* Use goto for cleanup on error */
int my_function(void)
{
    void *buf1 = NULL;
    void *buf2 = NULL;
    int ret = 0;
    
    buf1 = kmalloc(SIZE1);
    if (!buf1) {
        ret = -NEXUS_ENOMEM;
        goto err_out;
    }
    
    buf2 = kmalloc(SIZE2);
    if (!buf2) {
        ret = -NEXUS_ENOMEM;
        goto err_free_buf1;
    }
    
    /* ... do work ... */
    
err_free_buf2:
    kfree(buf2);
err_free_buf1:
    kfree(buf1);
err_out:
    return ret;
}
```

### Header Files

```c
/*
 * filename.h - Brief description
 *
 * Copyright (c) 2024 NEXUS Development Team
 * Licensed under the NEXUS License
 */

#ifndef _NEXUS_FILENAME_H
#define _NEXUS_FILENAME_H

/* Include guards are mandatory */

/* Include system headers first */
#include <stdint.h>
#include <stddef.h>

/* Then include project headers */
#include "kernel/types.h"
#include "kernel/config.h"

/* Forward declarations */
struct my_struct;

/* Function prototypes */
int my_function(int arg);

/* Inline functions should be short */
static inline int add(int a, int b)
{
    return a + b;
}

#endif /* _NEXUS_FILENAME_H */
```

### Documentation

All public APIs must be documented:

```c
/**
 * @brief Initialize the memory manager
 * 
 * @param heap_start  Start address of kernel heap
 * @param heap_size   Size of heap in bytes
 * @return int        NEXUS_OK on success, error code on failure
 * 
 * @note This function must be called before any memory allocations
 * @see kmalloc(), kfree()
 */
int mm_init(void *heap_start, size_t heap_size);
```

---

## Submitting Changes

### Pull Request Process

1. **Fork the repository** on GitHub

2. **Create a branch** for your changes:
   ```bash
   git checkout -b feature/your-feature
   ```

3. **Make your changes** and commit them

4. **Push to your fork**:
   ```bash
   git push origin feature/your-feature
   ```

5. **Create a Pull Request**:
   - Go to https://github.com/darkhat/NEXUS-OS
   - Click "New Pull Request"
   - Select your branch
   - Fill in the PR template

### Pull Request Template

```markdown
## Description
Brief description of the changes.

## Type of Change
- [ ] Bug fix (non-breaking change that fixes an issue)
- [ ] New feature (non-breaking change that adds functionality)
- [ ] Breaking change (fix or feature that would cause existing functionality to change)
- [ ] Documentation update

## Testing
Describe how you tested your changes:
- [ ] Unit tests pass
- [ ] Integration tests pass
- [ ] Manual testing performed

## Checklist
- [ ] My code follows the project's coding standards
- [ ] I have commented my code, particularly in hard-to-understand areas
- [ ] I have updated the documentation accordingly
- [ ] I have added tests that prove my fix/feature works
- [ ] All new and existing tests pass locally
```

### Required Information

Every PR must include:

1. **Clear description** of what the PR does
2. **Motivation** for the change
3. **Testing evidence** (logs, screenshots, etc.)
4. **Related issues** (use "Fixes #123" syntax)

---

## Code Review Process

### Review Timeline

- Initial review: Within 48 hours
- Full review: Within 1 week
- Merge: After approval and CI passes

### Review Criteria

Reviewers will check:

1. **Correctness**: Does the code work as intended?
2. **Style**: Does it follow coding standards?
3. **Testing**: Are there adequate tests?
4. **Documentation**: Is it properly documented?
5. **Performance**: Are there performance implications?
6. **Security**: Are there security concerns?

### Responding to Reviews

- Be respectful and professional
- Address all feedback
- Ask questions if feedback is unclear
- Push updates as new commits (don't force push during review)

### Approval Process

```
[ ] - No review yet
[x] - Approved
[~] - Changes requested
```

At least one approval from a maintainer is required before merge.

---

## Reporting Bugs

### Before Reporting

1. Search existing issues to avoid duplicates
2. Test with the latest version
3. Gather relevant information

### Bug Report Template

```markdown
## Bug Description
Clear description of what the bug is.

## Steps to Reproduce
1. Step 1
2. Step 2
3. Step 3

## Expected Behavior
What should happen.

## Actual Behavior
What actually happens.

## Environment
- NEXUS-OS Version: [e.g., 1.0.0-alpha]
- Platform: [e.g., x86_64 Desktop]
- QEMU Version: [if applicable]
- Host OS: [e.g., Ubuntu 22.04]

## Additional Information
- Logs
- Screenshots
- Configuration files
```

### Where to Report

- GitHub Issues: https://github.com/darkhat/NEXUS-OS/issues
- Security issues: nexus-os@darkhat.dev (encrypted)

---

## Requesting Features

### Before Requesting

1. Search existing issues for similar requests
2. Consider if it fits the project scope
3. Be prepared to discuss implementation

### Feature Request Template

```markdown
## Feature Description
What feature would you like to see?

## Use Case
Why is this feature needed? What problem does it solve?

## Proposed Solution
How should this feature be implemented?

## Alternatives Considered
What other solutions have you considered?

## Additional Context
Any other relevant information.
```

---

## Documentation

### Documentation Standards

All documentation should:

1. Be clear and concise
2. Include examples where appropriate
3. Be kept up-to-date with code changes
4. Follow the project style guide

### Documentation Locations

| Type | Location |
|------|----------|
| Architecture | `/docs/architecture/` |
| API Reference | `/docs/api/` |
| User Guides | `/docs/guides/` |
| Specifications | `/docs/specifications/` |
| Code Comments | In source files |

### Building Documentation

```bash
# Build HTML documentation
make docs

# View documentation locally
make docs-view
```

---

## Testing

### Running Tests

```bash
# All tests
make test

# Unit tests
make test-unit

# Integration tests
make test-integration

# Specific test
make test-scheduler
```

### Writing Tests

```c
/* test_example.c */
#include "test.h"

/* Test case */
static void test_kmalloc_basic(void)
{
    void *ptr;
    
    ptr = kmalloc(100);
    ASSERT(ptr != NULL);
    
    kfree(ptr);
}

/* Test suite */
static test_case_t test_cases[] = {
    TEST_CASE(test_kmalloc_basic),
    TEST_CASE(test_kmalloc_zero),
    TEST_CASE(test_kmalloc_large),
    TEST_CASE_NULL
};

/* Test suite definition */
TEST_SUITE(memory_allocator_tests, test_cases);
```

### Test Requirements

- New features require tests
- Bug fixes should include regression tests
- Aim for >80% code coverage
- All tests must pass before merge

---

## Communication

### Channels

- **GitHub Issues**: Bug reports and feature requests
- **GitHub Discussions**: General discussions
- **IRC**: #nexus-os on Libera.Chat
- **Email**: nexus-os@darkhat.dev

### Getting Help

- Check the documentation first
- Search existing discussions
- Ask in IRC or Discussions
- Be patient and respectful

---

## Recognition

Contributors are recognized in:

- The CONTRIBUTORS file
- Release notes
- Annual contributor report

Significant contributors may be invited to become maintainers.

---

## License

By contributing to NEXUS-OS, you agree that your contributions will be licensed under the project's license.

---

Thank you for contributing to NEXUS-OS!

*Last Updated: March 2026*
*NEXUS-OS Version 1.0.0-alpha*
