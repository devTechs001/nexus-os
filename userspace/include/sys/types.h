/*
 * NEXUS OS - userspace sys/types.h
 * userspace/include/sys/types.h
 *
 * POSIX types the userspace services use. There was no sys/ header tree, so
 * files needing mode_t, time_t, timeval or timespec got "unknown type name"
 * and "storage size isn't known" - several of which are errors only because
 * the type is used by value.
 */

#ifndef _NEXUS_USERSPACE_SYS_TYPES_H
#define _NEXUS_USERSPACE_SYS_TYPES_H

#include <stdint.h>
#include <stddef.h>

typedef int32_t  pid_t;
typedef uint32_t uid_t;
typedef uint32_t gid_t;
typedef uint32_t mode_t;
typedef int64_t  off_t;
typedef int64_t  ssize_t;
typedef uint64_t dev_t;
typedef int64_t  time_t;
typedef int64_t  clock_t;

#define CLOCKS_PER_SEC 1000000

/* Nanosecond precision time, used by nanosleep(). */
struct timespec {
    time_t tv_sec;
    long   tv_nsec;
};

/* Microsecond precision time, used by gettimeofday(). */
struct timeval {
    time_t tv_sec;
    long   tv_usec;
};

#endif /* _NEXUS_USERSPACE_SYS_TYPES_H */
