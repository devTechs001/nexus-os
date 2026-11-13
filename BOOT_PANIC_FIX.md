# NEXUS OS — Late-Stage Kernel Panic: Root Cause & Fixes

**Branch:** `arena/5a7f960b-nexus-os`
**Scope:** Make the kernel boot in the VM by default. No features removed.

---

## Symptom

The kernel got through GRUB, the 32→64-bit transition and early serial
markers (`BmCLP6AGsRSI`), then died late in `kernel_main_init()` — right
around the final init phase where interrupts are enabled and the scheduler
starts.

## Root causes

Five independent defects, each of which is fatal on its own. All are in the
"last stage" of boot, which is why earlier boot stages always looked healthy.

### 1. Interrupt stub corrupted the stack and registers (`interrupts.asm`)

The common ISR stub pushed **15** registers but popped only **14**.

```
push rax rcx rdx rbx rbp rsi rdi r8 r9 r10 r11 r12 r13 r14 r15   (15)
pop  r15 r14 r13 r12 r11 r10 r9 r8      rsi rbp rbx rdx rcx rax   (14)
```

`rdi` was pushed and never popped, then `add rsp, 16` discarded the error
code *and* `rdi`. The resulting `iretq` popped `rip/cs/rflags/rsp/ss` from
the wrong slots — jumping to `rdi`'s stale value in kernel context with a
corrupted stack. Every single interrupt returned to garbage.

**Fix:** pop `rdi` in the correct reverse position (`pop rdi` after
`pop r8`, before `pop rsi`). Verified: push and pop lists are now exact
mirrors, and `struct registers` field offsets (`isr_num` @120,
`error_code` @128, `rip` @136) match the real stack frame.

### 2. PIC was never remapped → IRQ0 became a double-fault (`irq.c`)

`pic_init()` / `apic_init()` were only called from `interrupt_early_init()`,
which **had no callers anywhere in the tree**.

Consequence: the PIC stayed in its BIOS configuration, where IRQ0 (the PIT)
is delivered on **vector 0x08** — which the IDT defines as a double fault.
The first timer tick after `arch_enable_interrupts()` in
`kernel_init_subsystems()` therefore raised a CPU exception, and
`interrupt_handler()` called `kernel_panic("CPU Exception: 8")`.

**Fix:** `interrupt_init()` now calls `interrupt_early_init()` (idempotent)
before installing the IDT, so the PIC is remapped to vectors 32–47.

### 3. No end-of-interrupt was ever sent (`idt.c`)

`interrupt_handler()` had the comment `/* IRQ handling would go here */` and
did nothing else. With no EOI the PIC keeps the line asserted and re-delivers
the same IRQ immediately — an interrupt storm that starves the kernel and
looks like a hang or panic.

**Fix:** dispatch hardware IRQs (32–47), call `pic_send_eoi()`, and service
IRQ0 by calling `timer_tick()` + `scheduler_tick()`. No per-IRQ logging is
done, because `printk()` busy-waits on the UART and would stall the system.

### 4. `cpu_count` was never initialized → NULL idle task (`kernel.c`)

`kernel_state` lives in `.bss`, which the boot stub zeroes, and **nothing
ever assigned `cpu_count`**. `get_num_cpus()` returned `0`, so:

* `scheduler_init()` looped `for (cpu = 0; cpu < 0; cpu++)` — creating **no**
  run queues and **no** idle tasks;
* `scheduler_start()` then did `rq->curr = idle_task(0)` i.e. `rq->curr = NULL`
  and dereferenced it.

**Fix:** initialize `kernel_state` (version, codename, `cpu_count = 1`,
state flags) at the top of `kernel_main()`. Defense in depth in the
scheduler: `scheduler_init()` falls back to 1 CPU, and `scheduler_start()`
clamps the CPU index, falls back to the boot CPU's idle task, and creates
one on demand instead of writing through NULL.

### 5. `delay_ms()` halted the CPU forever (`kernel.c`)

`delay_ms()` called `arch_halt_cpu()`, which is `cli; hlt; ret`. Halting with
interrupts **disabled** means nothing can ever wake the CPU. The first
`delay_ms()` call hung the machine permanently.

The same misuse existed in the GUI event-loop frame limiter in `init.c`.

**Fix:** both are now `pause`-based busy-waits (the clock comes from the
TSC, so no interrupts are needed). `arch_halt_cpu()` is left for the
genuine permanent-halt paths (panic, dead idle loop), where `cli; hlt` is
correct.

## Supporting hardening

| File | Change |
|---|---|
| `hal/interrupts/apic.c` | Bounded the `apic_write()` MMIO wait (was an unbounded `while` that hangs forever if the LAPIC is absent) and the timer-calibration loop. APIC failure already degrades gracefully to the PIC. |
| `kernel/sched/scheduler.c` | `get_cpu_rq()` indexes `run_queues[]` with the raw **APIC ID**; it now routes through `get_rq()`, which clamps. `rebalance_domains()` divided by `num_cpus` and is called on the *first* tick (`tick_count % HZ == 0`) — now returns early when `num_cpus < 2`. |
| `kernel/arch/x86_64/cpu/timer.h` | **New.** Proper prototypes for `arch_init_timer()` / `timer_tick()`; `idt.c` includes it instead of relying on implicit declarations. |

## Boot-in-the-VM-by-default

The default GRUB entry hardcoded `virt=vmware`, which is wrong when the ISO
is booted under QEMU (the documented `make run` path) or VirtualBox.

* Default entry is now `splash virt=auto`.
* `parse_boot_params()` understands `virt=auto` and leaves hypervisor
  selection to the existing CPUID-based `detect_virtualization()`, which
  already recognizes QEMU, VMware, VirtualBox, Hyper-V and bare metal.
* `Makefile` and the checked-in `build/iso/boot/grub/grub.cfg` agree.
* All existing menu entries (Text, Safe, Debug, Native, Interactive) are
  kept unchanged.

## Verification performed

* `interrupts.asm` push/pop sets verified as exact reverse mirrors; the
  corrected sequence assembled and disassembled cleanly, with the stack
  frame landing on the expected offsets.
* All 10 changed translation units compile with the project's real flags
  (`-ffreestanding -m64 -mno-red-zone -O2 …`) with **zero errors**.
* Cross-module symbols (`timer_tick`, `scheduler_tick`, `pic_send_eoi`)
  confirmed to have exactly one definition each and to resolve from
  `idt.o`.

QEMU/NASM are not present in this sandbox, so the ISO could not be booted
here; the analysis above is static plus object-level verification.
