# game-2d

Independent 2D game consumer of the installed `engine` CMake Config package.

This repository creates a real window, presents through the public engine WSI/Rendering contracts, uses the public `SwapchainLifecycle` coordinator for ordinary Presentation↔Rendering lifecycle orchestration, and draws multiple simple colored 2D squares. Game-owned shaders and shader compilation stay in this repository. It does not introduce a speculative Renderer2D abstraction.

## Engine requirement

This consumer is pinned to:

- engine Git tag: `v0.3.1`
- engine release checkpoint: `2f2fe2b628f2eb4198fb94dba67dded5f34e7ef0`
- engine CMake package: `0.3.1` exact

The CMake request is:

```cmake
find_package(
    engine 0.3.1 EXACT CONFIG REQUIRED
    COMPONENTS vulkan wsi
)
```

Package version `0.3.1 EXACT` is not source provenance. The consumed install must be produced from the engine commit identified by annotated tag `v0.3.1`, which peels to `2f2fe2b628f2eb4198fb94dba67dded5f34e7ef0`. This project does not follow `main`, `master`, or any other moving engine branch.

Upgrades are explicit consumer changes to a later engine release identity. They are never automatic.

## Prerequisite

Install the `v0.3.1` engine tree to a prefix you control. This repository does not vendor engine source, add the engine as a subdirectory or submodule, or fetch engine source through CMake.

Supply that prefix through normal CMake package search input such as `CMAKE_PREFIX_PATH` or `engine_DIR`. Do not commit a machine-specific absolute prefix.

The `vulkan` and `wsi` components require:

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

`--bounded` creates the normal rendering path, presents a fixed number of frames, and exits successfully without interaction. Omit it for interactive execution.

The consumer target does not set `CXX_STANDARD` or request `cxx_std_23`. Effective C++23 compilation comes from the imported engine usage requirements.
