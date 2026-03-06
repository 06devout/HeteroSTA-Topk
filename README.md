# HeteroSTA-Topk

A small C++ prototype built on top of [HeteroSTA](../HeteroSTA) for:

- reporting top-k critical timing paths
- generating a simple SPEF from ISPD-style `.net` / `.cap` inputs with a non-timing-driven 3-bend router
- running an end-to-end flow:
  - net/cap -> simple route -> SPEF -> STA -> top-k paths

This repository is currently tailored to the local workspace layout used during development, especially the `ariane` benchmark under `../HeLEM-GR/data/ISPD2025`.

## Features

### 1. Top-k STA API

The reusable API is declared in [include/heterosta_topk/topk_api.hpp](include/heterosta_topk/topk_api.hpp).

Main entry:

- `heterosta_topk::report_topk_critical_paths(...)`

It supports:

- single-lib or multi-lib setup through `early_lib` / `late_lib` and `early_libs` / `late_libs`
- reading Verilog netlist, SPEF and SDC
- reporting `WNS` / `TNS`
- dumping top-k paths to a report file and parsing them back into C++ data structures

### 2. Simple 3-bend router

The lightweight router API is declared in [include/heterosta_topk/simple_3bend_router.hpp](include/heterosta_topk/simple_3bend_router.hpp).

Main entry:

- `heterosta_topk::route_3bend_and_write_spef(...)`

It:

- reads HeLEM-GR / ISPD-style `.net` and `.cap`
- routes each sink from a root pin using a fixed Manhattan 3-bend pattern
- writes a simple SPEF for downstream STA

This router is intentionally simple and **not** timing-driven.

## Repository layout

- [CMakeLists.txt](CMakeLists.txt): build configuration
- [include/heterosta_topk/topk_api.hpp](include/heterosta_topk/topk_api.hpp): top-k STA API
- [include/heterosta_topk/simple_3bend_router.hpp](include/heterosta_topk/simple_3bend_router.hpp): router API
- [src/topk_api.cpp](src/topk_api.cpp): top-k STA implementation
- [src/simple_3bend_router.cpp](src/simple_3bend_router.cpp): simple router implementation
- [src/main.cpp](src/main.cpp): end-to-end demo for `ariane`
- [src/simple_router_main.cpp](src/simple_router_main.cpp): router-only demo
- [data/](data): small local demo inputs used during development
- [output/](output): generated SPEF and timing reports

## Dependencies

This project depends on:

- CMake >= 3.18
- a C++17 compiler
- [HeteroSTA](../HeteroSTA)
- optionally CUDA when building with `-DUSE_GPU=ON`

Expected local workspace layout:

- `../HeteroSTA`
- `../HeLEM-GR`

The current demo in [src/main.cpp](src/main.cpp) uses benchmark files from:

- `../HeLEM-GR/data/ISPD2025/ariane`

## License setup

Before running, export your HeteroSTA license token:

```bash
export HeteroSTA_Lic="<your_license_token>"
```

The program initializes the HeteroSTA logger and license inside [src/topk_api.cpp](src/topk_api.cpp).

## Build

### CPU build

```bash
cmake -S . -B build
cmake --build build -j
```

### GPU build

```bash
cmake -S . -B build -DUSE_GPU=ON
cmake --build build -j
```

## Run

### End-to-end demo

```bash
./build/heterosta_topk
```

Current flow in [src/main.cpp](src/main.cpp):

1. read `ariane.net` and `ariane.cap`
2. generate `output/ariane_3bend.spef`
3. sanitize the original SDC
4. run HeteroSTA
5. write top-k timing report to `output/ariane_topk_max.rpt`

### Router-only demo

```bash
./build/simple_3bend_router_demo
```

## Current demo configuration

The default `ariane` demo in [src/main.cpp](src/main.cpp) currently:

- routes all nets with the simple router
- loads NanGate45 standard-cell library plus multiple `fakeram45_*.lib` files
- runs max/setup timing analysis
- dumps top-k paths into a text report

Important note:

- the file paths in [src/main.cpp](src/main.cpp) are currently **hardcoded** for the local workspace
- if you publish this repository, you may want to replace them with command-line arguments or config files

## Why `early_libs` / `late_libs` exist

Some designs require more than one Liberty file.

For example, `ariane` uses:

- one standard-cell library
- multiple SRAM macro libraries

So this project supports:

- `early_libs`: all Liberty files used for early corner
- `late_libs`: all Liberty files used for late corner

When only a single timing corner is available, both lists can temporarily point to the same files.

## Output files

Common generated files:

- `output/ariane_3bend.spef`
- `output/ariane_sanitized.sdc`
- `output/ariane_topk_max.rpt`

The timing report is produced by HeteroSTA and then parsed by the project code.

## Known limitations

1. **The router is only a prototype**
   - it is not congestion-aware
   - it is not timing-driven
   - RC values are simplified

2. **Warnings during timing are expected**
   You may see warnings such as:

   - `net delay calc fallback for arc ...`

   This usually means HeteroSTA fell back to a simplified net-delay calculation for some arcs.
   The flow can still complete, but the result should be treated as a prototype result rather than a signoff-quality timing result.

3. **SPEF compatibility is tool-sensitive**
   This prototype writes SPEF in a format that is compatible with the tested HeteroSTA version.

4. **Current demo is benchmark-specific**
   The provided `main.cpp` is focused on getting the `ariane` benchmark running in the local environment.

## Suggested next steps for open-sourcing

If you plan to publish this project on GitHub, the following improvements are recommended:

- replace hardcoded paths in [src/main.cpp](src/main.cpp) with CLI arguments
- add a small sample benchmark that is safe to redistribute
- document the expected HeteroSTA version
- add a dedicated `config/` directory or JSON/YAML config loader
- add a shorter summary mode for printing critical paths

## Example API usage

### Top-k STA

```cpp
heterosta_topk::TopkConfig cfg;
cfg.early_libs = {"/path/to/lib1.lib", "/path/to/lib2.lib"};
cfg.late_libs = cfg.early_libs;
cfg.netlist = "/path/to/design.v";
cfg.spef = "/path/to/design.spef";
cfg.sdc = "/path/to/design.sdc";
cfg.top_module = "top";
cfg.dump_file = "topk_max.rpt";
cfg.num_paths = 10;
cfg.nworst = 1;
cfg.report_max = true;

heterosta_topk::TopkReport report;
std::string error_message;
bool ok = heterosta_topk::report_topk_critical_paths(cfg, &report, &error_message);
```

### Simple router

```cpp
heterosta_topk::Simple3BendRouterConfig cfg;
cfg.net_file = "/path/to/design.net";
cfg.cap_file = "/path/to/design.cap";
cfg.output_spef = "/path/to/out.spef";
cfg.design_name = "design";

heterosta_topk::Simple3BendRouterStats stats;
std::string error_message;
bool ok = heterosta_topk::route_3bend_and_write_spef(cfg, &stats, &error_message);
```

## Status

This project is a research / prototype integration layer rather than a polished product.
Its current goal is to make HeteroSTA-based top-k path reporting easy to experiment with on ISPD-style routed or partially routed benchmarks.
