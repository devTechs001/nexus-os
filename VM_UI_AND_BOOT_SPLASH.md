# NEXUS OS — Real Boot Splash, Real VM Manager GUI, Real VMs

**Branch:** `arena/7cd786f6-nexus-os` (branched from `c658f48`)
**Scope:** graphical boot screen, VM manager UI, and making the hypervisor
lifecycle actually work. **No features removed — everything here is a fix
or an addition.**

---

## The problem in one paragraph

Every drawing path in the tree was a stub. `gui/virtualization/virt_manager_gui.c`
had a complete 1280x800 layout — title bar, menu bar, toolbar, sidebar, VM
cards, details panel, status bar — and every single primitive under it was a
`printk()`. There was no font renderer anywhere in the repository, so no text
could be drawn at all. The boot splash filled the screen with a colour and
then reported progress through `console_print()`, which goes to VGA text mode
and the serial port: invisible the moment GRUB sets a linear framebuffer
graphics mode. Meanwhile the hypervisor's real lifecycle functions
(`vm_create`, `vm_init`, `vm_start`, …) could not be called by anything,
because their types were `typedef`s private to the `.c` file.

So the OS could show a coloured rectangle and log lines. That is what this
branch fixes.

---

## 1. A real font renderer — `drivers/video/font8x16.c`

There was no bitmap font data anywhere in the repository. This adds a real
8x16 font covering printable ASCII `0x20`–`0x7E` (95 glyphs, 16 bytes each,
MSB = leftmost pixel) plus the renderer:

```c
s32 font8x16_draw_string(u32 *fb, u32 fb_w, u32 fb_h, u32 pitch,
                         s32 x, s32 y, const char *s, u32 color,
                         u32 scale, u8 opacity);
u32 font8x16_string_width(const char *s, u32 scale);
void font8x16_draw_string_centered(...);
```

`scale` is an integer magnification; `opacity` blends against the existing
pixels (used by the splash logo fade). `pitch` is **bytes** per scanline;
the pixel stride is `pitch / 4`.

> Gotcha for anyone adding to this: `kernel.h:321` does
> `#define current get_current()`. A local variable named `current` in a
> file that includes `kernel.h` will not compile. That bit the first draft.

## 2. A real boot splash — `drivers/video/splash.c`

Rewritten as a genuine framebuffer renderer:

- vertical gradient backdrop, hexagon logo with a radial glow and an inner
  "N" chevron that fades in,
- rounded progress bar with six stage ticks, an eased fill and a leading edge,
- animated spinner,
- centred text via the new font.

Progress is now visible, and two defects were fixed:

- **`splash_update_progress()` only ever drew a 16% bar when asked for 45%.**
  The easing step was a flat `+4` per call, but boot only reports a handful
  of milestones, so the displayed value fell far behind. It now closes a
  fraction of the remaining gap (minimum 4%).
- **`splash_done()` never released the framebuffer.** `init.c` calls it
  immediately before starting the compositor and desktop, and it left
  `initialized = true` — so the timer tick would keep repainting the boot
  screen on top of the desktop forever. It now tears the splash down.

`drivers/video/splash.h` and `drivers/video/console.h` are new: neither
`console_print` nor the splash entry points had a declaration in any header,
which is why callers were silently relying on implicit declarations.

## 3. The splash now actually animates

`kernel/arch/x86_64/cpu/timer.c` had a `timer_tick()` that did exactly
`ticks++`. Nothing advanced the splash between progress updates, so it was a
static image. The tick now calls `splash_animate()`, which self-throttles
(repaints every 16th call) and returns immediately once the splash is torn
down.

## 4. The hypervisor lifecycle now works

### The blocker: the API was uncallable

`vm_config_t`, `virtual_machine_t`, `vcpu_state_t` and friends were `typedef`s
inside `nexus_hypervisor_core.c`. The header declared
`extern virtual_machine_t *vm_create(vm_config_t *)` against types that did
not exist outside that one translation unit, so **no other file could include
it**, and nothing ever called the real hypervisor. The types are now in the
header, and the `.c` includes its own header.

That immediately surfaced a second problem: eleven functions
(`cpuid_virtualize`, `msr_read_virtualize`, `msr_write_virtualize`,
`apic_virtualize_init`, `apic_virtualize_intr`, `ioapic_virtualize_init`,
`ept_init`, `vm_memory_add`, `vm_device_create`, `virtio_device_init`,
`pci_passthrough_attach`) are declared in the header but were defined
`static` — a declaration/definition mismatch that only stayed hidden because
nothing included the header. They now match.

### Lifecycle defects fixed

| # | Defect | Consequence |
|---|--------|-------------|
| 1 | `vm_start()` required `state == 1` (CREATED) exactly | A stopped VM (`state 4`) could **never be started again** — "Restart" silently did nothing |
| 2 | `vm_stop()` decremented `running_vms` unguarded | `vm_destroy()` calls `vm_stop()` on an already-stopped VM, underflowing the counter to `0xFFFFFFFF` |
| 3 | `vm_destroy()` decremented `vm_count` without compacting | Deleting VM #1 of 3 dropped whichever VM was **last** and left VM #1 alive |
| 4 | `vm_destroy()` printed `config.vm_id` **after** `memset` | Every log line claimed "VM 0 destroyed" |
| 5 | `ept_init()` wrote `memory_size_mb / 4` entries into a fixed 4096-byte table | A 4 GB VM needs 16 KB → **heap overflow at 2 GB+** |
| 6 | `vm_memory_add()` bounded at 256 against a 64-entry allocation | Region 65 onwards wrote past the array into the next heap block |
| 7 | `vm_create()` did `i % num_physical_cpus` with no zero guard | **Divide-by-zero**; also a 0-vCPU config allocated a zero-length array and failed with a misleading error |
| 8 | `vm_start()` set only `RIP = 0xFFF0` | Starts execution 4 GB below the firmware entry point, with no valid real-mode segment |

The EPT and memory-region bugs are heap corruption, which is worth flagging:
they are the kind of defect that looks like a random crash somewhere else.

### New lifecycle API

```c
int  vm_pause(virtual_machine_t *vm);    /* state 3, pauses at current RIP */
int  vm_resume(virtual_machine_t *vm);   /* back to state 2 */
virtual_machine_t *vm_find(u32 vm_id);   /* id -> live object */
int  vm_boot_from_reset_vector(virtual_machine_t *vm);
```

`vm_boot_from_reset_vector()` is the "make VMs boot" piece. On x86-64 a vCPU
does not begin life in long mode: reset leaves it in **real mode** with
`CS.base = 0xFFFF0000` and `EIP = 0xFFF0`, so the first fetch lands at
`0xFFFFFFF0` where the firmware ROM is mapped. The function now sets the
architectural reset state properly — `cs = 0xF000`, `rip = 0xFFF0`,
`rflags = 0x2`, `cr0 = 0x60000010` (PE and PG clear), `cr4 = 0`,
`efer = 0` (long mode off until the guest enables it) — instead of only
poking RIP.

## 5. The VM Manager GUI now draws

### New render backend — `gui/virtualization/virt_render.c`

Pixel primitives with clipping on every call: `virt_fill_rect`,
`virt_gradient_rect`, `virt_rect_outline`, `virt_rounded_rect` (corner caps
only where needed — three rectangles plus four quarter-arcs), `virt_line`
(Bresenham), `virt_circle` (filled and outline), and text via `font8x16`.

`virt_render_attach_display()` binds to the live framebuffer through
`display_get_primary_framebuffer()`, so the manager draws into whatever mode
the bootloader actually set.

### Every stub replaced

`gui_draw_rounded_rect`, `gui_draw_text`, `gui_draw_icon`, `gui_draw_progress`
and `gui_draw_graph` now call the renderer. `gui_draw_graph` had a
`(void)x1; (void)y1; (void)x2; (void)y2;` placeholder where the trace should
be; it now draws the line trace with clamping and gridlines.

Two things worth knowing:

- **The icon table is full of emoji** (`🖥️ 📦 ▶️ …`) and the font is ASCII
  only. Icons now render as a filled badge with the first printable
  character, so each one still reads as a distinct token.
- **The layout was hard-coded to 1280x800.** The window now clamps to the
  real framebuffer, and the VM list and details panel are drawn from model
  data instead of being painted as empty rectangles.

### The manager now drives the real hypervisor

`virt_manager_create_vm`, `start_vm`, `stop_vm`, `pause_vm`, `resume_vm`,
`create_snapshot`, `restore_snapshot`, `clone_vm` and `delete_vm` previously
each did `printk(...)` and `return 0`. They now resolve the id to a live
`virtual_machine_t` and call the real API. `virt_sync_card()` copies the
hypervisor's state back into the GUI model so the display reflects reality.

`virt_manager_handle_button_click()` previously only logged. It now invokes
the actions (create/start/pause/stop/restart/snapshot/clone/delete) and
repaints. Snapshot graph samples are derived from hypervisor counters, not
made-up values.

`restore_snapshot` deliberately stops the VM and brings the vCPUs back to the
reset vector, then holds it **paused**: restoring memory underneath running
vCPUs would let them execute from a stale RIP.

### `virt_manager_gui.h` was unusable standalone

It declared functions taking `network_config_t`, `shared_folder_t`,
`resource_optimization_t` and `vmware_settings_t` — all four defined only
inside the `.c`. Moving them into the header makes it self-contained; it now
compiles on its own.

## 6. The manager is reachable

`virt_gui_init()` had **no callers at all**. A complete applications manager
that nothing could start is the same as not having one. Added:

- `gui/apps_nexus.c` / `.h` — a small application registry
  (`nexus_app_launch`, `nexus_app_close`, `nexus_app_redraw`).
- a `vmmanager` kernel boot parameter which launches the manager over the
  desktop once the GUI framework is up. It is **off by default**, so the
  standard boot path is unchanged.

Boot it with `vmmanager` on the kernel command line to land directly in the
Virtualization Manager.

---

## Verification

There is no VM toolchain in this environment (`qemu-system-x86_64`, `nasm`,
`grub-mkrescue` and `xorriso` are all absent, and the checked-in `build/*.o`
are stale objects from another machine), so this was verified by compiling
each translation unit against the real flags and by running host harnesses.

**Compilation** — clean with `-Wall -Wextra` against the Makefile's own
`CFLAGS`, for `font8x16.c`, `splash.c`, `console.c`, `display.c`, `timer.c`,
`nexus_hypervisor_core.c`, `virt_render.c`, `virt_manager_gui.c`,
`apps_nexus.c`, `cmdline.c` and `init.c`. Every other `.c` that includes one
of the changed headers also still compiles.

**Linking** — the VM manager's object resolves all ten hypervisor symbols
(`vm_create`, `vm_init`, `vm_start`, `vm_stop`, `vm_pause`, `vm_resume`,
`vm_destroy`, `vm_find`, `vm_snapshot`, `vm_boot_from_reset_vector`) against
the hypervisor object, with no undefined `vm_*` symbols.

**Lifecycle, 30 assertions under AddressSanitizer, all passing** — create →
init → start (RIP `0xFFF0`, CS `0xF000`), pause → resume, stop → **restart**
(the old regression), double-stop not underflowing, id lookup including a
missing id, deleting the middle of three VMs leaving the other two intact, a
0-vCPU config, and a 64 GB VM (which previously overflowed the EPT table).
The old code asserts out with an FPE on the same test.

**Rendering** — the real `virt_render.c` + `font8x16.c` were driven against a
1024x768 and 900x620 buffer and the output converted to PNG. Text is legible
at scale 1, rounded corners and circles are clean, and the graph trace draws.
The splash was rendered the same way at 60% with the spinner advancing; the
bar tracks the reported percentage and the stage label advances with it.

## Files

**New:** `drivers/video/font8x16.{c,h}`, `drivers/video/splash.h`,
`drivers/video/console.h`, `gui/virtualization/virt_render.{c,h}`,
`gui/apps_nexus.{c,h}`

**Modified:** `drivers/video/splash.c`, `drivers/display/display.c`,
`kernel/arch/x86_64/cpu/timer.c`, `kernel/core/init.c`,
`kernel/core/cmdline.c`, `kernel/include/kernel.h`,
`virt/hypervisor/nexus_hypervisor_core.{c,h}`,
`gui/virtualization/virt_manager_gui.{c,h}`, `Makefile`

## Not done

- `gui/boot-splash/boot_splash.c` (397 lines) is dead code — it is not in the
  Makefile and calls seven helpers that do not exist anywhere. It is left
  alone rather than deleted; the live boot screen is `drivers/video/splash.c`.
- `virt/vm/vm_manager.c` is still disabled (it is commented out of the
  build). Enabling it needs `struct vm_config` / `struct vm_state` completed
  first.
- The compositor and desktop still paint over the framebuffer independently.
  The VM Manager is drawn directly to the framebuffer, so a full compositor
  integration is still outstanding.
- No ISO could be produced here, so none of this has been booted on real
  hardware or in QEMU from this environment.
