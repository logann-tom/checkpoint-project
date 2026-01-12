# checkpoint-project

A single-threaded process checkpointing system for Linux x86-64. Captures the complete state of a running process and restores it from a checkpoint file.

## Overview

This project implements transparent checkpointing using a shared library injected via `LD_PRELOAD`. When the target process receives `SIGUSR2` (signal 12), the library captures:

- All memory regions from `/proc/self/maps` (stack, heap, libraries, etc.)
- CPU register state via `getcontext()`
- The FS segment register (for thread-local storage)

The checkpoint is saved to `myckpt` and can be restored later with the `restart` program.

## Files

| File | Description |
|------|-------------|
| `checkpoint.c` | Example program to be checkpointed |
| `libckpt.c` | Shared library that handles checkpointing (compiles to `libckpt.so`) |
| `restart.c` | Restores process state from `myckpt` |
| `readckpt.c` | Debug tool that prints human-readable checkpoint contents |

## Building

```bash
make build
```

## Usage

**Option 1: Using make**

```bash
make check
```

**Option 2: Manual**

```bash
# Start the target program (runs with libckpt.so injected)
./checkpoint &

# Trigger checkpoint (replace PID with actual process ID)
kill -12 <PID>

# Restore from checkpoint
./restart
```

## Technical Details

### FS Register Handling

A subtle issue with `getcontext()` is that it can corrupt the FS segment register, which points to thread-local storage (TLS) on x86-64. Corrupting FS causes crashes when the program accesses `errno` or other thread-local variables.

The solution is to manually save and restore FS around context operations:

```c
syscall(SYS_arch_prctl, ARCH_GET_FS, &saved_fs);
getcontext(&ctx);
syscall(SYS_arch_prctl, ARCH_SET_FS, saved_fs);
```

### Restart Memory Layout

The `restart` binary is compiled with fixed memory segment addresses:

```bash
gcc -static -Wl,-Ttext-segment=5000000 -Wl,-Tdata=5100000 -Wl,-Tbss=5200000 ...
```

This places restart's code, data, and BSS segments at low addresses (0x5000000 range) that won't collide with the checkpointed process's memory regions. Without this, restoring the original process's memory would overwrite the restart program while it's still running.

### Checkpoint File Format

The `myckpt` file contains a series of memory region headers followed by their data, plus the saved CPU context. Use `readckpt` to inspect the contents:

```bash
./readckpt
```

## Future Work

- [ ] Multithreaded process support
- [ ] Incremental checkpointing (only save modified pages)
- [ ] Periodic automatic checkpointing

## Requirements

- Linux x86-64
- GCC
- Make
