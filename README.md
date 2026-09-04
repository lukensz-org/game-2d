# game-2d

Independent 2D game consumer of the installed `engine` CMake Config package.

This repository creates a real Platform window, uses the public `GraphicsContext` boundary for ordinary Vulkan bootstrap plus the `0.5.0` `beginFrame(...)` / `presentFrame(...)` frame protocol, and draws multiple simple colored 2D squares. Game-owned shaders and shader compilation, color-only graphics state/workload, game-loop behavior, and bounded policy stay in this repository. It does not introduce a speculative Renderer2D abstraction.

## Engine requirement

This consumer is pinned to:

- engine repository checkpoint: `deeb4c551af89bc0fb683b2a20b76a9cc9e9e43c`
- engine CMake package: `0.5.0` exact
- package component: `graphics`

The CMake request is:

```cmake
find_package(engine 0.5.0 EXACT CONFIG REQUIRED COMPONENTS graphics)
```

The ordinary game target boundary is `engine::platform` plus `engine::vulkan_graphics`.

Package version `0.5.0 EXACT` is not source provenance. The consumed install must be produced from exact engine checkpoint `deeb4c551af89bc0fb683b2a20b76a9cc9e9e43c`, the developer-locally validated frame-protocol implementation checkpoint. This consumer does not follow `main`, `master`, or another moving engine branch, and it does not require an annotated `v0.5.0` tag as its provenance proof.

Upgrades are explicit consumer changes to a later engine release identity/checkpoint. They are never automatic.

## Boundary ownership

`game-2d` owns the Platform `WindowSystem` / `Window`, square shaders and local `glslc` rules, color-only `GraphicsState` creation intent, square draw/pass data, event/game loop, bounded frame policy, sleep/backoff behavior, and application reporting.

`GraphicsContext` owns the lower windowed Vulkan Runtime / Surface / device / LogicalDevice / ResourceAllocator / Execution / Swapchain / RenderingContext / `SwapchainLifecycle` composition and the ordinary frame protocol. The consumer passes the observed framebuffer extent to `beginFrame(...)`, handles its compact ready/try-again/deferred/surface-lost result, observes reclaimed generations and generation/extent/color metadata, prepares its game-owned draw/pass data, and pairs each ready frame with one `presentFrame(...)`.

The consumer does not reconstruct the lower `sync(...)` / reclaim / acquire / render-present lifecycle protocol. Its caller-owned graphics state remains compatible with the active ready-frame color format; if a color-format change requires replacement, submitted GPU work is completed before the old state is destroyed. Shutdown also waits for submitted work before context-backed state leaves scope. There is no unconditional per-frame idle wait.

## Prerequisite

Install exact engine checkpoint `deeb4c551af89bc0fb683b2a20b76a9cc9e9e43c` to a prefix you control. This repository does not vendor engine source, add the engine as a subdirectory or submodule, or fetch engine source through CMake.

Supply that prefix through normal CMake package search input such as `CMAKE_PREFIX_PATH` or `engine_DIR`. Do not commit a machine-specific absolute prefix.

The `graphics` component requires:

- a CMake-resolvable Vulkan 1.3+ development package;
- a CMake-resolvable GLFW package (`glfw3` >= 3.5.1);
- a local `glslc` on `PATH` for consumer-owned shader compilation.

Shader tooling is a `game-2d` build requirement. It is not exported by the installed engine package.

For Debug Khronos validation evidence, consume an engine package built in Debug so the installed Runtime retains validation-enabled behavior.

## Configure, build, and run

From a developer shell that can already resolve Vulkan, GLFW, `glslc`, and the installed engine prefix:

```bash
cmake --preset default -DCMAKE_PREFIX_PATH=/path/to/engine-prefix
cmake --build --preset default
./build/game_2d --bounded
./build/game_2d
```

`--bounded` uses the ordinary `beginFrame(...)` / `presentFrame(...)` path, counts four `FrameEndDisposition::accepted` presentations, and exits successfully without interaction. Omit it for interactive resize/minimize/recovery and repeated recreation validation.

The consumer target does not set `CXX_STANDARD` or request `cxx_std_23`. Effective C++23 compilation comes from the imported engine usage requirements.
