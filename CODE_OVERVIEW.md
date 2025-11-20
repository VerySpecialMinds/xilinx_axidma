# Code Overview

This repository contains a Linux kernel driver and userspace library that expose high-bandwidth DMA access to Xilinx AXI DMA and VDMA IP blocks. The sections below summarize the main components and how they interact.

## Kernel driver (driver/)
- **Module entry and platform binding (`driver/axi_dma.c`)**: Registers a platform driver for device-tree nodes compatible with `xlnx,axidma-chrdev`, allocates a device structure, initializes DMA handling, and sets up the character device interface. Probe and remove hooks ensure DMA and character device resources are initialized and cleaned up with the platform device lifecycle.
- **Module parameters**: The `cached_buffers` boolean enables cached, non-coherent CMA allocations for `mmap` requests to accelerate CPU-side copies (for example, moving DMA data to SSDs). When enabled, the driver performs cache maintenance before and after transfers so applications can use the buffers without manual flushing.
- **Driver-wide definitions (`driver/axidma.h`)**: Defines the `axidma_device` structure that tracks device metadata (character device info, channel counts, callback data, and DMA buffer lists) and declares helper functions for character device setup, DMA operations, and device-tree parsing.
- **Character device and buffer management (`driver/axidma_chrdev.c`)**: Implements the `/dev/axidma` interface, including tracking DMA buffer allocations, converting user virtual addresses to DMA bus addresses, and importing shared DMA buffers from other kernel drivers. Allocation metadata is kept in linked lists so transfer preparation code can validate requests.
- **DMA operation helpers (`driver/axidma_dma.c`)**: Prepares scatter-gather tables from registered buffers, converts AXI DMA directions to DMA engine enums, and sets up completion callbacks. Transfers carry completion or signal-delivery metadata so synchronous callers can wait and asynchronous callers can receive POSIX real-time signals when DMA finishes.

## Userspace library (library/libaxidma.c)
- Maintains a single `axidma_dev` instance with channel metadata, file descriptor, and callback registrations. On initialization, the library probes the driver for available channels via IOCTLs, categorizes them by direction and type, and caches their IDs for future transfers.
- Provides the callback trampoline that delivers completion signals back to the caller, mapping kernel-provided channel indices into per-channel callbacks supplied by the application.

## Example applications (examples/)
- Sample programs demonstrate synchronous and asynchronous transfer flows and utility routines for buffer handling. They compile against the exported headers in `include/` and the shared library built into `outputs/`.
