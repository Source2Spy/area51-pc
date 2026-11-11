```text
██████  ██████  ███████  █████  ███    ███ ██       █████  ███    ██ ██████        ███████  ██ 
██   ██ ██   ██ ██      ██   ██ ████  ████ ██      ██   ██ ████   ██ ██   ██       ██      ███ 
██   ██ ██████  █████   ███████ ██ ████ ██ ██      ███████ ██ ██  ██ ██   ██ █████ ███████  ██ 
██   ██ ██   ██ ██      ██   ██ ██  ██  ██ ██      ██   ██ ██  ██ ██ ██   ██            ██  ██ 
██████  ██   ██ ███████ ██   ██ ██      ██ ███████ ██   ██ ██   ████ ██████        ███████  ██ 
```

[![CMake portability](https://github.com/Source2Spy/area51-pc/actions/workflows/cmake.yml/badge.svg?branch=forward%2B)](https://github.com/Source2Spy/area51-pc/actions/workflows/cmake.yml)
[![Branch](https://img.shields.io/badge/branch-master-555555)](https://github.com/Source2Spy/area51-pc/tree/master)
[![GitHub issues](https://img.shields.io/github/issues/Source2Spy/area51-pc)](https://github.com/Source2Spy/area51-pc/issues)

Links: [Game assets](https://github.com/Source2Spy/area51-pc/releases/) | [Development builds](https://github.com/Source2Spy/area51-pc/actions) | [Discord](https://discord.gg/7gGhFSjxsq)

<!-- Add a gameplay screenshot or a project banner here. -->

# Table of Contents

This file contains the following sections:

1. [About the Port](#about-the-port)
2. [Renderer Features Explained](#renderer-features-explained)
3. [TODO](#todo)
4. [General Notes](#general-notes)
5. [Getting the Source Code](#getting-the-source-code)
6. [Compiling on Windows](#compiling-on-windows)
7. [Compiling on Linux](#compiling-on-linux)
8. [Compiling on Android](#compiling-on-android)
9. [Original Xbox (Legacy)](#original-xbox-legacy)
10. [Shader Pipeline](#shader-pipeline)
11. [Installation](#installation)
12. [Development and Modding Tools](#development-and-modding-tools)
13. [Bug Reports](#bug-reports)
14. [License](#license)
15. [FAQ](#faq)

# About the Port

Dreamland-51 is a source port of *Area-51* (2005), built on the original Entropy engine. The aim is to run the PC game on current hardware without replacing its gameplay, assets or interface with a remake.

The original PC code was written around Direct3D, 32-bit Windows and the development tools available in 2005. Dreamland-51 replaces much of that platform code, reworks the renderer, restores effects from earlier and console versions of the game, and brings back parts of the original toolchain. The `master` branch contains the modern renderer and the Windows, Linux and Android builds.

## Gaming / Graphics Related

* Direct3D 12 and Vulkan renderer support. SDL3 handles window creation and the platform layer; the rendering backends use the corresponding graphics APIs.
* Reworked framegraph renderer with separate rendering passes for effects such as XeGTAO and Screen Space Reflections. New passes no longer have to be inserted into the old primitive rendering path.
* Reimplemented `d3deng_draw` primitive renderer, including proper shading and lighting for primitives drawn by the engine.
* Exponential Variance Shadow Maps (EVSM) and hard shadows for light sources.
* Blinn-Phong lighting support for the game's existing materials.
* XeGTAO ambient occlusion.
* Screen Space Reflections (SSR).
* SMAA, CMAA2 and MSAA anti-aliasing.
* Tessellated cloth rendering.
* Improved cloth rendering with restored console-style cloth destruction.
* Per-pixel flashlight lighting.
* Restored the LensFilter, a vertex-color modulator.
* Fixed lighting on `.skingeom` meshes, including skinned characters and other animated geometry.
* Fixed lighting on `.rigidgeom` meshes used for debris and effects.
* Restored beta-era film grain and screen-warp effects.
* Restored PlayStation 2 radial blur and fog effects.
* Correct brightness and contrast adjustments in the game's video settings.
* High-resolution UI textures, without AI upscaling. The original PC menu artwork and layout are retained.
* Support for 500+ FPS, with corrected gameplay logic, AI and movement at high frame rates instead of tying these systems to the original frame timing.

## Gameplay / Controls / Interface

* Reworked mouse and keyboard controls. The original hard-coded linear gamepad acceleration is no longer applied to mouse and keyboard input.
* SDL3 gamepad support, with controls ready for Steam Input remapping.
* Fixed camera handling, including unintended 360-degree rotation and the ability to see through walls.
* Fixed the aiming bug that prevented the player from aiming correctly while looking upward.
* Reimplemented menu controls and navigation while preserving the appearance of the original Console-style interface.
* Reimplemented the multiplayer dialogs and menu tabs.
* Improved navigation and presentation in the Secrets and Lore menus.
* Runtime font loading, replacing parts of the original fixed font setup.

## Audio / Video / Localization

* SDL3 platform support for window management, video integration, audio and input.
* Native WebM video decoding through WebM libraries instead of the original RAD Game Tools Bink (`.bik`) playback path.
* Native MP3 decoding using `minimp3` instead of the Miles Audio System.
* Language selection at runtime for text, voice audio and video.
* Russian, Czech and Japanese language support.
* New, editable ASCII-based `.strings` and `.fonts` formats.

## Multiplayer

* LAN multiplayer without the original GameSpy service.
* GameSpy-free networking, including direct connections to a host by IP address.
* Internet multiplayer by manual connect. Port forwarding may be required when the host is behind a router.
* Cross-platform multiplayer between Windows, Linux and Android.
* Reworked multiplayer dialogs and tabs in the original Console-style interface.
* Working voice chat, like on console versions.

## Programming and Code Quality

* Native 64-bit support.
* Native Windows, Linux and Android platform support.
* SDL3-based OS layer in place of the old Windows-specific code for windows, audio and input.
* CMake build system for desktop development with GCC and MSVC support. The original Visual Studio solutions are deprecated.
* All libraries and supporting technologies are built from source, without relying on the original game's binary-only middleware.
* Reworked serialization for models, decals and lightmaps. The formats no longer depend on the binary layout produced by a particular compiler or on the target's pointer size.
* Separate host-side shader tools (`ShaderTool` and DXC) and a prepared shader kit for the game build.
* Shader-kit manifests with hash verification and matching runtime shader packages.

## Modding Support

* Runtime emulation of `.DFS` archives for loading and modifying game resources.
* Editable ASCII `.strings` and `.fonts` resource formats.
* Experimental Lua scripting.
* Updated resource serialization for models, decals and lightmaps across compiler and architecture boundaries.
* `ResourceTool` geometry compile/decompile workflow using `.rawgeom` descriptors, glTF meshes and separate collision output.
* Restored and rewritten command-line tools for working with original game resources. See [Development and Modding Tools](#development-and-modding-tools).

# Renderer Features Explained

## Direct3D 12 and Vulkan

The renderer has Direct3D 12 and Vulkan backends. Both use the original game's geometry and material data, but shaders and GPU resources are handled through the modern renderer rather than the old PC graphics code.

SDL3 provides the operating-system layer: windows, events, input and audio. It is not the implementation of Direct3D 12 or Vulkan. Rendering work takes place in the engine's graphics backends.

## Lighting and Shadows

Dreamland-51 supports EVSM shadow maps and hard shadows. EVSM allows filtered shadow-map lookups; hard shadows preserve a sharp edge. Blinn-Phong lighting remains available for the original materials, and the flashlight has its own per-pixel lighting path.

Lighting on `.skingeom` and `.rigidgeom` has also been corrected. Those formats are used by different parts of the game, so character geometry and rigid debris/effects both need to be handled by the renderer.

## XeGTAO and Screen Space Reflections

XeGTAO provides screen-space ambient occlusion. It uses scene depth to estimate occlusion around corners, objects and nearby surfaces.

Screen Space Reflections use information from the rendered frame to produce reflections. As with other SSR implementations, geometry that is not represented in the current screen image cannot contribute to those reflections.

Both effects are integrated into the reworked framegraph instead of being bolted onto the original rendering loop.

## Anti-aliasing and Post-processing

SMAA and CMAA2 are post-process anti-aliasing methods, while MSAA operates on coverage samples during rendering. The renderer supports all three.

The post-processing work also includes film grain and screen warp from beta versions of *Area-51*, along with radial blur and fog from the PlayStation 2 release. Brightness and contrast settings have been corrected as part of the same renderer work.

## Cloth and Primitive Rendering

Cloth uses tessellation in the new renderer. The rewritten `d3deng_draw` path handles engine-drawn primitives with proper lighting and shading, including geometry that did not behave correctly through the old rendering code.

# TODO

## Planned Features

* Add path tracing support.
* Add NVIDIA DLSS support.
* Introduce DSP-based audio processing.
* Expand the existing Lua scripting support.
* Add a mod loader for custom game content.
* Jolt Physics.
* Add support for `.DFS` V4 archives.

## Known Issues

* Investigate movement in water on Rift. The player cannot jump while submerged; it is unclear whether jumping or surfacing should take precedence.
* Certain settings can produce a black prewarm screen.

## Deferred Work

* Improve physics.
* Reduce delays when switching between zones.
* Fix cases where the multiplayer death camera breaks zone visibility.
* Optimize runtime font generation.
* Revisit font scaling.
* Fix button prompts for keyboard controls.
* Restore the older `bin_level` files in Dreamland (low priority).
* Investigate cubemaps appearing as star-shaped artifacts (BC compression) (low priority).

<details>
<summary>Items crossed off in the original notes</summary>

* ~~Fix the Invader elevator.~~
* ~~Prevent the player controller from catching on walls.~~
* ~~Fix shadow artifacts near the bases of omnidirectional lights.~~
* ~~Correct the glow effect.~~
* ~~Fix localization issues.~~
* ~~Include the player's hands in the depth prepass so they contribute to XeGTAO.~~
* ~~Remove the Xbox fog implementation.~~
* ~~Set up Android builds through GitHub Actions.~~
* ~~Update the credits.~~
* ~~Remove Carl Johnson.~~
* ~~Investigate shader caching.~~
* ~~Reduce the size of the button UI element by half.~~
* ~~Fix the camera exploit that allows looking past cover by tilting upward and backing into a wall.~~

</details>

# General Notes

## Supported Platforms

| Platform | Status |
| --- | --- |
| Windows | Working |
| Linux | Working |
| Android | Working |
| macOS | Not planned |
| iOS | Not planned |
| Original Xbox | Legacy branch |

The original Xbox target uses the older console toolchain and is separate from the CMake/SDL3 port.

## Source Tree

| Directory | Contents |
| --- | --- |
| `Apps/` | Game application, editors and command-line tools |
| `xCore/` | Entropy engine core and libraries |
| `Support/` | Shared game and engine support code |
| `MiscUtils/` | Additional utilities |
| `cmake/` | Build scripts, toolchains, shader preparation and packaging |

# Getting the Source Code

Clone the `master` branch:

```sh
git clone --branch "master" --single-branch https://github.com/Source2Spy/area51-pc.git
cd area51-pc
```

Existing checkouts can switch to the same branch with:

```sh
git fetch origin
git switch "master"
git pull --ff-only
```

Desktop builds use CMake. The original Visual Studio project and solution files belong to the old toolchain and are no longer used for current Windows builds.

# Compiling on Windows

You will need Visual Studio 2022 with the C++ desktop tools, CMake, and Python for the host shader utilities. Run the following commands from a Visual Studio developer terminal in the repository root.

For Release:

```powershell
cmake -DA51_PRESET=windows-msvc-x64 -DA51_CONFIG=Release -P cmake/BuildGame.cmake
```

For Debug:

```powershell
cmake -DA51_PRESET=windows-msvc-x64 -DA51_CONFIG=Debug -P cmake/BuildGame.cmake
```

`BuildGame.cmake` prepares the shader kit, configures the selected game preset and builds `A51`. The Release executable is written to:

```text
build/windows-msvc-x64/Apps/GameApp/Release/A51.exe
```

A Debug build can use the Release shader kit. If you modify the shader sources, prepare the kit again before rebuilding the executable.

# Compiling on Linux

Install GCC, CMake, Ninja, Python and the development packages needed by SDL3. The package list used by the project's Linux CI can be found in [`.github/workflows/cmake.yml`](https://github.com/Source2Spy/area51-pc/blob/forward%2B/.github/workflows/cmake.yml).

Build from the repository root:

```sh
cmake -DA51_PRESET=linux-gcc-x64 -DA51_CONFIG=Release -P cmake/BuildGame.cmake
```

For Debug:

```sh
cmake -DA51_PRESET=linux-gcc-x64 -DA51_CONFIG=Debug -P cmake/BuildGame.cmake
```

The Release executable is written to:

```text
build/linux-gcc-x64/Apps/GameApp/Release/A51
```

Other architecture presets are defined in `CMakePresets.json`. When using a different preset, supply its name through `A51_PRESET`.

# Compiling on Android

The Android build uses Gradle and the Android NDK. The repository's Android CI configuration uses Java 17, Gradle 8.9, Android API 36, NDK `29.0.14206865` and Android SDK CMake 3.31.6.

Prepare the shader tools and kit on the host first:

```sh
cmake --preset shader-tools
cmake --build --preset prepare-shaders --parallel 4
```

Then build the Android application:

```sh
cd Apps/GameApp/android
gradle :app:assembleRelease
```

The Release APK is written to:

```text
Apps/GameApp/android/app/build/outputs/apk/release/app-release.apk
```

Signing, packaging and installing the game data are separate steps. See the [Android CI workflow](https://github.com/Source2Spy/area51-pc/blob/forward%2B/.github/workflows/cmake.yml) for the build environment and packaging procedure.

# Original Xbox (Legacy)

For the original Xbox version, switch to the repository's `Xbox` branch and use its original console build instructions. The Xbox code is not built by the Windows, Linux or Android commands above.

# Shader Pipeline

The game uses a prepared shader kit. `ShaderTool` and DXC run on the host, before the game is compiled, and are not direct dependencies of the `A51` target.

The one-command Windows and Linux builds in the previous sections handle shader preparation automatically. To do the steps separately, use:

```sh
cmake --preset shader-tools
cmake --build --preset prepare-shaders --parallel 4

cmake --preset linux-gcc-x64 -DA51_SHADER_KIT=/absolute/path/to/build/shaders/GameApp/release
cmake --build --preset linux-gcc-x64-release
```

`A51_SHADER_KIT` points to a published shader configuration directory, normally `build/shaders/GameApp/release`. This contains `shaderkit.json`, `catalog.json`, generated headers and compiled `.ecs` packages. CMake checks the files against the manifest before configuring the game. At runtime, the game checks that the `.ecs` packages match the shader catalog compiled into the executable.

ShaderTool publishes the generated headers and compiled packages together. If shader preparation fails, the existing published kit is left intact. `ShaderTool -generate` produces inspection files in `.preview/<config>/generated`; those files are not a usable runtime kit.

* C++ changes: rebuild `A51`.
* Shader or shader-contract changes: prepare the kit again, then rebuild `A51`.
* `A51_JOBS` controls game-build parallelism. The default is four jobs.
* `A51_SHADER_JOBS` sets the number of shader workers for host-tool configuration, from 1 to 12.
* Release shaders can be used with a Debug executable. Debug shaders can be generated with `ShaderTool -config debug` and placed in `build/shaders/GameApp/debug`.

## Packaging a Runtime Build

`cmake/PackageRuntime.cmake` copies the executable and `.ecs` packages into a runtime directory. For Linux Release:

```sh
cmake \
  -DA51_BINARY=build/linux-gcc-x64/Apps/GameApp/Release/A51 \
  -DA51_SHADER_KIT=build/shaders/GameApp/release \
  -DA51_PACKAGE_DIR=build/packages/linux-x64 \
  -P cmake/PackageRuntime.cmake
```

The package contains the game executable and `SHADERS/` directory. Game data must be installed separately.

# Installation

## Windows, Linux and Android

1. Download the [Asset Pack](https://github.com/Source2Spy/area51-pc/releases).
2. Download the executable for your platform from [GitHub Actions](https://github.com/Source2Spy/area51-pc/actions), or compile it yourself using the instructions above.
3. Extract the game assets to a new directory.
4. Copy the executable and its `SHADERS/` folder into that directory, keeping the existing resource paths.
5. Install the required textures according to the internal guide.
6. Start `A51.exe` on Windows or `A51` on Linux or install `.apk` file on Android.

Keep the compiled shader packages from the same build as the executable. The runtime checks their identity against the compiled shader catalog.

## Multiplayer

You can join a host directly from the `select server` dialog.

The host may need to forward the game's network port through their router. LAN connections do not require port forwarding.

Also all platforms are cross-platform. And servers with disabled `allow cheats` Requires for a build of the game that was created on THIS REPO by Github Actions.

# Development and Modding Tools

The port includes modernized tools alongside the game executable. Many of the original editor applications were designed for Visual Studio 2003/2004 XTremeToolkit and are still being migrated to modern compilers. For tools that have not been restored, use the legacy branch where applicable.

## CMake Targets

To compile a specific tool from an already configured Linux Debug build tree:

```sh
cmake --build build/linux-gcc-x64 --config Debug --target xbmptool
cmake --build build/linux-gcc-x64 --config Debug --target ResourceTool
```

To build a group of applications or tools:

```sh
cmake --build build/linux-gcc-x64 --config Debug --target a51_selected_apps
cmake --build build/linux-gcc-x64 --config Debug --target a51_tools
```

A build without `--target` uses the generator's default target and may build additional applications.

## ResourceTool

`ResourceTool` uses a descriptor-driven workflow for geometry compilation and decompilation:

```text
ResourceTool compile   asset.rawgeom
ResourceTool decompile asset.rawgeom
```

The `.rawgeom` descriptor records the data needed for a round trip. Decompilation writes render geometry to glTF and rigid collision geometry to a separate `.collision.gltf`. Runtime material parameters, UV keys, virtual meshes and textures, bone metadata, bone masks, custom properties and collision-cluster data are recorded in the descriptor.

Texture files are referenced rather than copied. Rigid-body blobs and external animation clips remain source references until their own round-trip formats are implemented.

## Original Applications

<details>
<summary>Show the original tools</summary>

The statuses below come from the project's original tool list. Several programs still require work after the migration from the Visual Studio 2003/2004 toolchain.

| Application | Status |
| --- | --- |
| AnimCompiler | Liquidated |
| Art2Code | Working |
| ArtistViewer | Liquidated |
| AudioEditor | Liquidated |
| BinaryString | Liquidated |
| BitmapEditor | Liquidated |
| DecalCompiler | Working |
| DecalEditor | Liquidated |
| DFSTool | Working |
| EDRscDesc | Working |
| Editor | Under construction |
| EffectsEditor | Under construction |
| ELFTool | Liquidated |
| EventEditor | Liquidated |
| FontEditor | Liquidated |
| FxEditor | Liquidated |
| FXCompiler | Under construction |
| GameApp | Working |
| ResourceTool | Under construction |
| LocoEditor | Liquidated |
| MeshViewer | Liquidated |
| PropertyEditor | Under construction; library for Editor |
| SoundPackager | Working |
| Viewer | Liquidated |
| WinControls | Under construction; library for Editor |
| WorldEditor | Under construction; library for Editor |
| XBMPTool | Working |
| XBMPViewer | Working |
| xCL | Liquidated |
| XSCC | Liquidated |
| xTool | Liquidated |

</details>

## Community Tools

<details>
<summary>Show community tools</summary>

| Project | Description |
| --- | --- |
| [Engine-51](https://github.com/bigianb/engine-51) | Area-51 asset viewer |
| [Json-Playsurface-Processer](https://github.com/gabengaGamer/json-playsurface-processer) | Imports `.playsurface` maps into Blender |
| [DAT-Tool](https://github.com/gabengaGamer/DAT-Tool) | Command-line tools for Tribes: Aerial Assault DAT archives |

</details>

# Bug Reports

Report bugs through the [GitHub issue tracker](https://github.com/Source2Spy/area51-pc/issues). Include the build or commit, operating system, graphics API, and instructions for reproducing the problem. Logs and screenshots help with startup and rendering bugs.

Project discussion is also available on [Discord](https://discord.gg/7gGhFSjxsq).

## A Short History of Area-51

*Area-51* was released on April 25, 2005, for PC, PlayStation 2 and the original Xbox. It was developed by Inevitable Entertainment and published by Midway Games. The PC version was later distributed as freeware with United States Air Force sponsorship. Official support eventually ended.

The source used by this project comes from a development snapshot dated March 31, 2005, at 10:40:19, shortly before the game's commercial release. According to the history accompanying the original source release, the files were discovered at a garage sale involving a former THQ developer.

That snapshot contains the Entropy engine, game code and targets for PC, PlayStation 2 and Xbox, as well as early GameCube work and debug symbols for several platforms. The source snapshot does not contain the complete retail game assets.

# License

This repository contains source code originating from an unofficial release of *Area-51* (2005), developed by Inevitable Entertainment and published by Midway Games. The original game code, assets, name and trademarks remain the property of their respective rights holders, including Warner Bros. Discovery.

Dreamland-51 is an independent, non-commercial community project. The repository is maintained in good faith for preservation and development work and is not an official release from the original developers or publishers. Making the source available here does not grant a license to the original game or its assets.

Warner Bros. Discovery, Skydance Corporation, Midway Games and Inevitable Entertainment are not affiliated with, do not endorse and are not involved in the project.

Third-party components have their own licenses, including Unlicense, BSD, MIT and LGPLv3 terms where applicable. See `legalnotices.txt`, `thirdpartylegalnotices.txt`, `fontlegalnotices.txt` and the license files distributed with individual libraries.

# FAQ

### Is this a remake of Area-51?

No. Dreamland-51 uses the original game code and assets, with changes to the engine and platform support. It retains the PC game's interface and gameplay.

### Do I need the original game data?

Yes. The executable does not contain the original game assets. Download the asset pack linked in [Installation](#installation) and keep its resource layout intact.

### Can I still use the old Visual Studio solutions?

They belong to the original development environment. Use CMake for the `master` Windows and Linux builds; Android uses Gradle and the NDK.

### What are the shader kit files for?

The modern graphics backends use compiled `.ecs` shader blob packages and generated headers. The shader kit makes sure the executable and installed shaders use the same layouts and catalog.

### Does multiplayer require GameSpy?

No. LAN and direct-IP multiplayer work without GameSpy. For internet hosting, the router may need port forwarding.

### Can I use `ResourceTool` for a complete asset round trip?

Almost. Its descriptor-driven geometry workflow is evolving, and some data types are still handled as external references.

### Can I use the original Xbox build with this branch?

The Xbox version remains on its legacy branch and uses the original console tools. It is not part of the CMake-based port.
