# Area 51 (2005) Source Code

Welcome to the unofficial release of the Area 51 (2005) video game source code! This project aims to resurrect and preserve a piece of early 2000s video game history for enthusiasts, historians, and developers alike. Below is a brief overview of the source code details, its origin, and guidance on how the community can help bring this game into the modern era.

123

## How to Contribute 

The main goal is to get the source code into a buildable state on modern systems. Whether you're interested in game development, historical preservation, or simply a fan of Area 51, here's how you can help:

1. **Building the Project:** Assistance is needed to compile and run the game on contemporary hardware. If you have experience with game development, legacy systems, or cross-platform development, your expertise is invaluable.
2. **Documentation and Research:** Insights into the original development environment, including compilers, libraries, and tools, would greatly assist the restoration effort.
3. **Debugging and Porting:** Once buildable, the project will require debugging and potentially porting to newer platforms to reach a wider audience.
4. **Forking and contributing:** Make changes in your forks, and submit a pull request with your updates.

## Join our Discord

[![Join our Discord](https://github.com/gabengaGamer/area51-pc/assets/54669564/bac6c8a8-2d95-4513-8943-c5c26bd09173)](https://discord.gg/7gGhFSjxsq)

## Building Code

The following prerequisites are required to build the source tree:

1. **CMake**
2. **GCC** or **MSVC**

**Visual Studio solutions is deprecated**

## Running Build

1. Download last [asset pack](https://github.com/Source2Spy/area51-pc/releases/tag/ASSETS-14.08.2026) from releases
2. Download last [executable ](https://github.com/Source2Spy/area51-pc/actions) from github actions
3. Unzip the archive and place your binary file into the unzipped folder
4. Run game

## CMake Target Selection

[![CMake portability](https://github.com/Source2Spy/area51-pc/actions/workflows/cmake.yml/badge.svg?branch=master)](https://github.com/Source2Spy/area51-pc/actions/workflows/cmake.yml)

The game consumes a prepared shader kit. ShaderTool and DXC are built separately
on the host; they are not dependencies of `A51`.

To prepare shaders and build the game in one command:

```sh
cmake -DA51_PRESET=linux-gcc-x64 -DA51_CONFIG=Debug -P cmake/BuildGame.cmake
```

The same command accepts Windows game presets. `A51_JOBS` controls build
parallelism and defaults to four. Host shader tools use the native CMake generator and require Python.
The `a51_prepare_shaders` target uses four shader workers; set `A51_SHADER_JOBS`
when configuring the host tools to select another count from one to twelve.

To run the stages separately:

```sh
cmake --preset shader-tools
cmake --build --preset prepare-shaders --parallel 4
cmake --preset linux-gcc-x64 -DA51_SHADER_KIT=/absolute/path/to/build/shaders/GameApp/release
cmake --build --preset linux-gcc-x64-debug
```

`A51_SHADER_KIT` points to one published configuration directory containing
`shaderkit.json`, `generated/`, `catalog.json`, and ECS packages. It defaults to
`build/shaders/GameApp/release`. CMake verifies the manifest's file hashes before
using the kit. A missing or damaged kit stops configuration with an error.
The regular game presets build `A51` only and do not inspect HLSL sources.

After C++ changes, rebuild `A51`. After shader or contract changes, prepare the
kit again, then rebuild `A51`. ShaderTool validates the compiled layouts and
publishes headers and packages together; failed preparation preserves the
previous kit. `ShaderTool -generate` writes inspection output under
`.preview/<config>/generated` and does not publish a usable kit.

Release shaders can be used with a Debug game build. To prepare debug shaders,
run ShaderTool with `-config debug` and configure the game with the resulting
`build/shaders/GameApp/debug` directory. Install the `.ecs` files from the same
kit into the game's `SHADERS` filesystem. The game checks their catalog identity
against its compiled headers before creating native shaders.

To build an individual application or tool, pass its target name:

```text
cmake --build build/linux-gcc-x64 --config Debug --target xbmptool
cmake --build build/linux-gcc-x64 --config Debug --target ResourceTool
```

The aggregate targets are available when needed:

```text
cmake --build build/linux-gcc-x64 --config Debug --target a51_selected_apps
cmake --build build/linux-gcc-x64 --config Debug --target a51_tools
```

Running a build without `--target` uses the generator's default `all` target and can build every non-excluded target.

### ResourceTool

`ResourceTool` uses one descriptor-driven workflow for geometry in both directions:

```text
ResourceTool compile   asset.rawgeom
ResourceTool decompile asset.rawgeom
```

The descriptor is the round-trip manifest. Decompilation writes the render
mesh as glTF, rigid collision as a separate `.collision.gltf`, and embeds
runtime material parameters, UV keys, virtual meshes/textures, bone metadata,
bone masks, custom properties, and collision-cluster metadata in the generated
`.rawgeom`. Texture files are referenced rather than copied; rigid-body blobs
and external animation clips remain source references until their dedicated
round-trip formats are implemented.

## Additional info

<details>
<summary>Valid platforms</summary>
  
Platform | Status
---------|--------
Windows  | Working
Linux    | Working
macOS    | Not planed
Android  | Working
iOS      | Not planed

</details>

<details>
<summary>Compiled apps</summary>

## Attention: During the migration from VS 2004 to VS 2022, many tools are not functioning. Please use LEGACY branch.

Name           | Status
---------------|---------------
AnimCompiler   | Working
Art2Code       | Working
ArtistViewer   | **UNDER CONSTRUCTION**
AudioEditor    | **UNDER CONSTRUCTION**/Lib for: [Editor](https://github.com/gabengaGamer/area51-pc/releases/tag/Editor-1.0)
BinaryString   | **UNDER CONSTRUCTION**/Lib for: [Editor](https://github.com/gabengaGamer/area51-pc/releases/tag/Editor-1.0)
BitmapEditor   | **UNDER CONSTRUCTION**/Lib for: [Editor](https://github.com/gabengaGamer/area51-pc/releases/tag/Editor-1.0)
DecalCompiler  | Working
DecalEditor    | **UNDER CONSTRUCTION**/Lib for: [Editor](https://github.com/gabengaGamer/area51-pc/releases/tag/Editor-1.0)
DFSTool        | Working
EDRscDesc      | Working
Editor         | **DEAD**
EffectsEditor  | **UNDER CONSTRUCTION**
ELFTool        | **DELETED**
EventEditor    | **UNDER CONSTRUCTION**/Lib for: [Editor](https://github.com/gabengaGamer/area51-pc/releases/tag/Editor-1.0)
FontEditor     | **UNDER CONSTRUCTION**/Lib for: [Editor](https://github.com/gabengaGamer/area51-pc/releases/tag/Editor-1.0)
FxEditor       | **UNDER CONSTRUCTION**/Lib for: [Editor](https://github.com/gabengaGamer/area51-pc/releases/tag/Editor-1.0)
FXCompiler     | **DEAD**
GameApp        | Working
ResourceTool   | **UNDER CONSTRUCTION**
LocoEditor     | **UNDER CONSTRUCTION**/Lib for: [Editor](https://github.com/gabengaGamer/area51-pc/releases/tag/Editor-1.0)
MeshViewer     | **UNDER CONSTRUCTION**/Lib for: [Editor](https://github.com/gabengaGamer/area51-pc/releases/tag/Editor-1.0)
PropertyEditor | **UNDER CONSTRUCTION**/Lib for: [Editor](https://github.com/gabengaGamer/area51-pc/releases/tag/Editor-1.0)
SoundPackager  | Working
Viewer         | **DELETED**
WinControls    | **UNDER CONSTRUCTION**/Lib for: [Editor](https://github.com/gabengaGamer/area51-pc/releases/tag/Editor-1.0)
WorldEditor    | **UNDER CONSTRUCTION**/Lib for: [Editor](https://github.com/gabengaGamer/area51-pc/releases/tag/Editor-1.0)
XBMPTool       | Working
XBMPViewer     | Working
xCL            | **DELETED**
XSCC           | **DELETED**
xTool          | **DELETED**

</details>

<details>
<summary>Custom tools</summary>

Name                                                                                         | Description                                                                             
---------------------------------------------------------------------------------------------| ----------------------------------------------------------------------------------------
[Engine-51](https://github.com/bigianb/engine-51)                                            | Area-51 Asset Viewer.                                                                                         
[Json-Playsurface-Processer](https://github.com/gabengaGamer/json-playsurface-processer)     | Script for importing .playsurface maps into blender.                    
[DAT-Tool](https://github.com/gabengaGamer/DAT-Tool)                                         | Command-line interface (CLI) utility for working with Tribes: Aerial Assault DAT archive format.

</details>

## Building XBOX Code

Select the **Xbox** branch and follow the instructions.

## Historical Overview of Area 51

- **Initial Release:** Area 51 was originally released on April 25, 2005, for PC, PS2, and Xbox. It has become a memorable cult classic.
- **Air Force Sponsorship:** In a unique turn of events, the game was sponsored by the United States Air Force and released as freeware for PC.
- **Abandonware Status:** Despite the game's initial success and the novel sponsorship, it eventually fell into obscurity, becoming abandonware. The game's support and distribution were discontinued, leaving it in a state where it was difficult for fans to access or play on modern systems.

## Source Code Snapshot Details

- **Snapshot Date:** The source code is a snapshot from 2005-03-31 10:40:19, just before the game's official release.
- **Discovery:** It was found at a garage sale of a former THQ developer.
- **Contents:** This release includes the source code for the Entropy engine, game logic, and targets for PC, PS2, Xbox, and an early version for GameCube. Additionally, debug symbols for various platforms are available in the release. Assets are not included, but those can be recovered from the retail game files.
