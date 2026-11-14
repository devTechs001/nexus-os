# Userspace as a Separate Image

The build linked 175 C objects into `nexus-kernel.bin`, six of which were
userspace code. The kernel image could not link, and no user program could ever
have run, because the code was in the wrong binary.

`make` now produces two images:

| Artifact | Contents | Packaged into the ISO |
|---|---|---|
| `build/nexus-kernel.bin` | 169 kernel objects | `/boot/nexus-kernel.bin` |
| `build/userspace/init.elf` | PID 1: libc, libsys, init | `/boot/init.elf` (multiboot2 module) |
| `build/userspace/service_manager.elf` | the first service init execs | not yet — needs a root filesystem |

## What was wrong

`C_OBJECTS` ended with six userspace objects:

```
syscall_wrapper.o  libc_string.o  libc_stdlib.o  libc_stdio.o
user_init.o        service_manager.o
```

Every one of them redefined a symbol the kernel already defines. `ld` reported
33 `multiple definition` errors and stopped:

- `libc_string.o` — 21 `mem*`/`str*` functions vs. `kernel/core/string.c`
- `libc_stdlib.o` — `atoi`, `strtoul`, `abs`
- `libc_stdio.o` — `vsnprintf`, `snprintf`, `sprintf`
- `syscall_wrapper.o` — `mmap`, `munmap`, `mprotect`, `reboot`, `exit`
- `main` — defined by both `user_init.o` and `service_manager.o`

These were not symbol clashes to be renamed away. They were two programs: a
kernel and a user process, with different entry points, different address
spaces, and different lifecycles. They need two binaries.

## What the build does now

- The six objects are gone from `C_OBJECTS`. A guard in `check-build` fails the
  build if any of them is ever added back:

  ```
    [ERR]   userspace objects found in C_OBJECTS:
              build/libc_string.o
    [ERR]   these are linked into their own image by build/userspace/init.elf
  ```

- Userspace is built into `build/userspace/` so it can never be picked up by a
  kernel pattern rule or counted by the object guard.
- `userspace/linker.ld` links the image as a flat `ET_EXEC` at `0x400000`, above
  the kernel's 1 MB load address, with no `PT_INTERP`.
- `userspace/libsys/crt0.S` supplies `_start`: it aligns the stack, synthesises
  `argc`/`argv`/`envp`, calls `main`, and exits through `SYS_EXIT`.

## Links that were missing, not just misplaced

Linking userspace for the first time exposed three real defects that nothing had
ever caught, because nothing had ever linked the tree:

1. **No `syscall_*` implementations.** libc and init call `syscall_write()`,
   `syscall_open()`, `syscall_exit()` and 23 others, declared `extern` at each
   call site. `syscall_wrapper.c` defines the POSIX names (`write`, `open`,
   `exit`) and none of the `syscall_*` ones. Every buffered stdio write, every
   heap expansion and every process exit in the userspace tree pointed at a
   symbol that did not exist. `userspace/libsys/syscall_glue.c` now implements
   all 26, issuing the same numbers as the POSIX wrappers.

2. **No `_start`.** Nothing defined an entry point. `crt0.S` is it.

3. **`pow()` declared, never defined.** `libc/stdlib.c`'s `atof()` calls it.
   `userspace/libc/math.c` implements it.

Two duplicate definitions were also resolved rather than papered over:

- `exit()` existed in both `libc/stdlib.c` and `libsys/syscall_wrapper.c`. The
  libc one runs `atexit` handlers and flushes `stdout`/`stderr` before exiting;
  the wrapper's called `SYS_EXIT` immediately and would have discarded every
  buffered write. The libc one is kept, the wrapper's removed.
- `SYS_MKNOD` and `SYS_GET_CMDLINE` had no numbers. They are now `95` and `96`.

## How PID 1 actually starts

The runtime path is wired end to end:

1. **`boot.asm`** — the multiboot2 header requests modules: an information
   request tag (type 1, requesting type 3) and a module alignment tag (type 6).
   Header length is now 72 bytes; checksum `0x17ADAEE2`.
2. **`grub.cfg`** — every menu entry carries `module2 /boot/init.elf init`.
3. **`kernel_main()`** — parses module tag type 3 into
   `g_userspace_module_start/end`, `g_userspace_module_name`.
4. **`init_start_userspace()`** (`kernel/sched/process.c`) —
   `elf_load_module()` → image entry → `arch_exec_user()` → `enter_user_mode()`
   → ring 3. It runs from `init_thread()`, so if the module is missing or
   malformed the kernel prints the old banner and keeps running instead of
   dying.

The VFS path was not usable for this. `vfs_mount()` has no caller and `nexfs` is
registered but never mounted, so at boot there is no root filesystem for the
path-based `elf_load_image()` to read from. `elf_load_module()` takes the bytes
the bootloader supplied instead.

## Verifying it

```sh
make userspace-bin          # both ELF images
make build/nexus-kernel.bin # 169 objects, one ld invocation
```

Checked here: 169/169 kernel objects compile, `ld` exits 0 with zero duplicate
definitions and zero undefined references (4.4 MB image); `init.elf` and
`service_manager.elf` are `ET_EXEC` at `0x400000` with zero undefined symbols.
The multiboot2 header is validated byte for byte — length, checksum, and tag
chain.

`grub-mkrescue` and `xorriso` are not present in this environment, so the ISO
itself and a QEMU boot were not produced. The kernel-side assembly
(`boot.asm`, `interrupts.asm`) needs `nasm`, which is also absent; the link
above was verified with a stand-in for those two objects, which affects only the
boot stub and ISR stubs, not any code in this change.

## Still open

- **No root filesystem.** `service_manager.elf` is built but cannot be exec'd:
  init resolves `/sbin/service_manager` through the VFS, and nothing is mounted.
- **No address-space isolation.** `elf_load_buffer()` places the image on the
  kernel heap and `enter_user_mode()` switches privilege but not page tables, so
  ring 3 can still read kernel memory.
- **`task_sleep` has no wakeup counterpart**, which is why the fallback path in
  `init_thread()` parks the task rather than waiting on an event.
