# game-2d

Independent 2D game consumer of the installed `engine` CMake Config package.

This repository creates a real Platform window, uses the public `GraphicsContext` boundary for ordinary Vulkan bootstrap plus presentation/rendering lifecycle orchestration, and draws multiple simple colored 2D squares. Game-owned shaders and shader compilation, color-only graphics state/workload, game-loop behavior, and bounded policy stay in this repository. It does not introduce a speculative Renderer2D abstraction.

## Engine requirement

This consumer is pinned to:

- engine repository checkpoint: `ac17a6d9016e2f87e36c92683df5748e9cb82fc7`
- validated implementation tree: `b09e8a387296053439c54b33398fada2776c7cbf` (same tree as the merged checkpoint above)
- engine CMake package: `0.4.0` exact
- package component: `graphics`

There is no annotated `v0.4.0` tag for this package boundary. The CMake request is:

```cmake
find_package(engine 0.4.0 EXACT CONFIG REQUIRED COMPONENTS graphics)
```

The ordinary game target boundary is `engine::platform` plus `engine::vulkan_graphics`.

Package version `0.4.0 EXACT` is not source provenance. The consumed install must be produced from exact engine checkpoint `ac17a6d9016e2f87e36c92683df5748e9cb82fc7`, which contains the same implementation tree developer-locally validated at `b09e8a387296053439c54b33398fada2776c7cbf`. This project does not follow `main`, `master`, or another moving engine branch.

Upgrades are explicit consumer changes to a later engine release identity/checkpoint. They are never automatic.

## Boundary ownership

`game-2d` owns the Platform `WindowSystem` / `Window`, square shaders and local `glslc` rules, color-only `GraphicsState` creation intent, square draw/pass data, event/game loop, bounded frame policy, and application reporting.

`GraphicsContext` owns the lower windowed Vulkan Runtime / Surface / device / LogicalDevice / ResourceAllocator / Execution / Swapchain / RenderingContext / `SwapchainLifecycle` composition. The consumer uses the facade for sync/recreation observation, acquire, render/present, retired-generation reclamation, and submitted-work completion. Context-backed graphics state is destroyed before the context.

## Prerequisite

Install exact engine checkpoint `ac17a6d9016e2f87e36c92683df5748e9cb82fc7` to a prefix you control. This repository does not vendor engine source, add the engine as a subdirectory or submodule, or fetch engine source through CMake.

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

`--bounded` creates the normal rendering path, counts four `FramePresentDisposition::accepted` presentations, and exits successfully without interaction. Omit it for interactive execution and resize/recreation validation.

The consumer target does not set `CXX_STANDARD` or request `cxx_std_23`. Effective C++23 compilation comes from the imported engine usage requirements.
