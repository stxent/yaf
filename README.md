# YAF — FAT32 Driver for Embedded Devices

![C23](https://img.shields.io/badge/C-23-blue)
![CMake](https://img.shields.io/badge/CMake-3.21+-blue)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

YAF is a lightweight and portable library implementing a FAT32 file system
driver tailored for embedded devices with limited resources. The library
is designed to be platform‑agnostic — it does not rely on any specific
hardware or operating system, and supports the same platforms
as the **Xcore** library.

It is written using the C23 standard and built with CMake.

## Requirements

To build and use YAF, you need the following packages:

* **GCC 13** or newer — required for C23 language standard support.
* **CMake 3.21 or newer** — used for configuring and generating build
  systems across platforms.
* **libcheck** — required for building and running unit tests
  (optional if tests are disabled).
* **Xcore** — can be built separately or included as CMake submodule
  in the project.

## Features

YAF supports the following functionality:

* Mounting FAT32 file systems
* Reading directories and files
* Creating directories and files
* Modifying entry attributes
* Static allocation of internal buffers
* Single‑threaded and multi‑threaded configurations
* UTF‑8 support for paths
* Formatting partitions as FAT32
* Calculating free space

## Usage Examples

1. Including in a CMake Project

To integrate YAF into your existing CMake project, use `add_subdirectory`
to pull it in as a subproject:

```cmake
add_subdirectory("/path/to/yaf" yaf)
```

After this, the YAF library target (yaf) will be available for linking:

```cmake
target_link_libraries(your_target PRIVATE yaf)
```

2. Building for x86 with Unit Tests

If you want to verify the library’s correctness on a development machine,
enable unit tests:

```sh
cmake .. -DCMAKE_PREFIX_PATH=/path/to/output/dir/ -DBUILD_TESTING=ON
make
make test
```

This builds the test suite and runs all unit tests using libcheck.
Useful during development and CI pipelines.

3. Building for x86 Outside the Project Tree

If you want to build the library as a standalone package, follow these steps:

```sh
cmake .. -DCMAKE_PREFIX_PATH=/path/to/output/dir/ \
  -DCMAKE_INSTALL_PREFIX=/path/to/output/dir/
make
make install
```

3. Building for LPC175x Outside the Project Tree

For building the library for the LPC175x platform, use the following command:

```sh
cmake .. -DCMAKE_PREFIX_PATH=/path/to/output/dir/ \
  -DCMAKE_INSTALL_PREFIX=/path/to/output/dir/ \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/xcore/toolchains/cortex-m3.cmake
make
```

## Build Options

YAF uses CMake options to customize the build. Below is a list
of available options:

* **BUILD_TESTING** — Enables building of unit tests. Set to ON if you want to
  validate YAF on your host or target.
* **YAF_THREADS** — Enables OS support and thread‑safety mechanisms.
  Required if your app uses multiple threads accessing the file system.
* **YAF_UNICODE** — Enables UTF‑8 support for file and directory names.
  When disabled, only ASCII paths are supported.
* **YAF_WRITE** — Enables all write operations: creating, renaming,
  deleting files and directories, modifying attributes. If disabled, the library
  runs in read‑only mode.
* **YAF_DEBUG** — Sets the debug message level: 0 — disabled (no output),
  1 — errors only, 2 — warnings and errors, 3 — verbose (all debug messages).
* **YAF_SECTOR_SIZE** — Defines the memory sector size (in bytes) used by the
  underlying block device. Supported values: 512, 1024, 2048, 4096.
  Must match your hardware (e.g., SD cards typically use 512 B sectors).
