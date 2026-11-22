# RayTracing_Vulkan

A real-time path tracer written in C++ on Vulkan's hardware ray tracing pipeline. It renders a scene with Monte Carlo path tracing, reuses samples from earlier frames, and cleans up the remaining noise with an edge-avoiding À-Trous denoiser on a compute shader. The shaders are written in Slang, which also generates a GLSL version of each one; you can switch between the two while the renderer runs.

![Path tracing with the denoiser](images/RayTracing_With_Denoiser.png)

## Features

### Rendering
- **Hardware ray tracing.** Uses [`VK_KHR_ray_tracing_pipeline`](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_ray_tracing_pipeline.html) and [`VK_KHR_acceleration_structure`](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_acceleration_structure.html), with a top-level and bottom-level acceleration structure built from the loaded model.
- **Monte Carlo path tracing.** One path per pixel per frame, up to a set maximum length. Russian roulette ends paths early without biasing the result.
- **GGX BRDF.** Uses the GGX microfacet distribution, from these two papers:
  - Walter et al., [Microfacet Models for Refraction through Rough Surfaces](https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf) (EGSR 2007)
  - Heitz, [Sampling the GGX Distribution of Visible Normals](https://jcgt.org/published/0007/04/01/) (JCGT 2018)
- **Temporal history.** Each pixel is reprojected into the previous frame using the previous camera matrix, and its color is blended with the four surrounding history pixels by bilinear weight. A history pixel is only used if its normal and depth match the current pixel, so history doesn't leak across edges or through disocclusions. Each pixel keeps a running average with its sample count, and an optional cap on that count keeps the image responsive to changes.
- **À-Trous denoiser.** Based on Dammertz et al., [Edge-Avoiding À-Trous Wavelet Transform for fast Global Illumination Filtering](https://jo.dreggn.org/home/2010_atrous.pdf) (HPG 2010). Runs as a compute shader over several passes, with edge stops on depth, normal, albedo and colour. Lighting is divided by albedo before filtering and multiplied back afterwards, so textures stay sharp. Light sources are left out of the filter, and outlier pixels are clamped to their neighbours' range before the first pass.
- **Rasterizer.** A standard scanline renderer of the same scene, for comparison.
- **Post-processing.** Exposure and gamma, plus debug views for albedo, normals, depth and per-pixel sample count.
- **Window resizing.** The swapchain and every window-sized image, framebuffer and pipeline are rebuilt when the window changes size, and a minimized window pauses until it is restored.

### Slang and generated GLSL
- Every shader is written once, in [Slang](https://shader-slang.org/): path tracer, rasterizer, post-processing and denoiser.
- Each entry point is compiled straight to SPIR-V, in `spv/*.slang.spv`. This is what the renderer uses by default.
- A GLSL version is generated only when you ask for it:
  - Pick **GLSL (generated)** in the **Supported shader types** section of the settings panel, then click **Build GLSL shaders**. The build runs in the background while the renderer keeps going, and the renderer switches to GLSL once it finishes.
  - Slang writes the GLSL to `shaders/glsl/`, where you can read it, and `glslangValidator` compiles it to `spv/*.spv`.
  - While GLSL is in use, **Rebuild GLSL shaders** regenerates it after a Slang change and reloads it.
  - Starting with `-glsl` builds the GLSL shaders first if they're missing.
- Switching between Slang and GLSL rebuilds every pipeline and the shader binding table.
- `shaders/shared_structs.h` is shared by C++ and Slang, so buffer and push-constant layouts are declared once.
  - Slang is compiled column-major, so glm matrices load unchanged.
  - Slang writes buffer pointers as `std430` in its GLSL output, while the C++ data is tightly packed. `compile_shaders.bat` switches them to `scalar`, the layout Slang's own SPIR-V uses.

### Tools
- **Editor interface.** A docked Dear ImGui layout that can be rearranged and is kept in `imgui.ini`:
  - **Viewport**: the path-traced image at 1:1 pixels (the render size follows the panel), with a toolbar for renderer, view mode, denoiser, reset and screenshot, and an fps / spp / resolution overlay.
  - **Outliner**: the camera, models, lights, materials and textures, with a filter.
  - **Inspector**: the selected camera (position, angles, field of view, speed), model (geometry counts) or material (base color, specular, roughness, emission, edited live).
  - **Render Settings**: integrator, accumulation, denoiser, display and shader source.
  - **Stats**: frame, UI build, record, fence and vsync timings, per-pass GPU timings from timestamp queries, and a frame/GPU time graph.
  - **Console**: timestamped engine messages.
  - A menu bar (File, View, Render, Help) and a status bar with renderer, resolution, samples and timings.
- **Screenshots.** F12 or the viewport's Screenshot button saves the viewport as a PNG in `src/screenshots/`.

![Settings panel and performance window](images/Settings_And_Performance.png)

## Screenshots

| Rasterizer | Path tracing, no denoiser | Path tracing with denoiser |
|---|---|---|
| ![Scanline](images/Scanline.png) | ![Path tracing with noise](images/RayTracing_With_Noise.png) | ![Path tracing with denoiser](images/RayTracing_With_Denoiser.png) |

## Requirements

- Windows 10 or 11, 64-bit
- A GPU and driver that support Vulkan ray tracing ([`VK_KHR_ray_tracing_pipeline`](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_ray_tracing_pipeline.html) and [`VK_KHR_acceleration_structure`](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_acceleration_structure.html))
- [Vulkan SDK](https://vulkan.lunarg.com/) 1.4 or newer, with the `VULKAN_SDK` environment variable set. The SDK provides `glslangValidator` and `slangc`, which the build uses to compile shaders.
- Visual Studio with the C++ desktop workload. The project targets platform toolset v145 (Visual Studio 2026). For an older Visual Studio, change the toolset in the project properties.

GLFW, GLM, Dear ImGui, Assimp and stb_image are bundled in `libs/`.

### Development system

The renderer is developed and tested on this machine:

| Component | Spec |
|---|---|
| OS | Windows 11 Pro (build 26200) |
| CPU | Intel Core i7-13700K (16 cores, 24 threads) |
| RAM | 64 GB DDR5 |
| GPU | NVIDIA GeForce RTX 4060 Ti, 8 GB (driver 617.42) |
| IDE | Visual Studio Community 2026 (18.8.2) |
| Vulkan SDK | 1.4.335.0 (slangc 2025.23.2) |

At the default 1280×768 window, the path tracer with the denoiser runs at about 120 FPS on this system.

## Building and running

1. Open `vs/rtrt.sln` in Visual Studio.
2. Choose the `x64` platform and either the `Debug` or `Release` configuration, then build. The shaders are compiled to `src/spv/` as part of the build.
3. The build writes `rtrt.exe` to `vs/x64/<configuration>/`. Visual Studio starts it with `src/` as the working directory, so it can find `models/` and `spv/`. When starting it some other way, run it from `src/`.

The compiled shaders in `src/spv/` and the generated GLSL in `src/shaders/glsl/` aren't kept in the repository. If any Slang shaders are missing when `rtrt.exe` starts, it runs `src/compile_shaders.bat` to build them with the Vulkan SDK's `slangc`, then carries on. You can also run `compile_shaders.bat` yourself after editing a shader, `compile_shaders.bat raytrace.slang` to rebuild one file, or `compile_shaders.bat glsl` to also generate the GLSL versions (which uses `glslangValidator` too). It needs the `VULKAN_SDK` environment variable, which the SDK installer sets.

Command-line options:

| Option | Effect |
|---|---|
| `-glsl` | Start with the shaders built from Slang's generated GLSL, building them first if needed |
| `-d` | Enable the Vulkan API dump layer |
| `-shot <file>` | Save the viewport as a PNG once 240 frames have accumulated |

## Controls

| Input | Action |
|---|---|
| Left-mouse drag in the viewport | Look around |
| W / A / S / D | Move forward / left / back / right (viewport hovered or focused) |
| E / C | Move up / down |
| Mouse wheel over the viewport | Change move speed |
| F1 | Hide or show the interface |
| F5 | Reset accumulation |
| F12 | Save a screenshot |
| Esc | Quit |

## Coding guidelines

The C++ code follows the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html), with these project exceptions:

- Code is indented with 4 spaces instead of 2, and `public:`/`private:` labels line up with the `class` keyword.
- Every opening brace goes on its own line (Allman style), including functions, classes and control statements.
- `if`, `else`, `for` and `while` always use braces, even around a single statement, and no block or function body is kept on one line.
- A statement that fits on one line stays on one line. If its trailing comment would push it past the limit, the comment goes on its own line above it.
- Lines can be up to 90 columns instead of 80.
- All names are PascalCase, including local variables, parameters, class members and struct fields. Members don't use an `m_` prefix or a trailing `_`.

Formatting is set in `.clang-format`. Format the code with the clang-format that comes with Visual Studio (`VC/Tools/Llvm/x64/bin/clang-format.exe`), or with clang-format 14 or newer. Third-party code in `libs/` and the generated `extensions_vk.*` files are excluded.

## Changelog

### 2.1 GLSL shaders built on request ([c2d7368](https://github.com/zyoKo/RayTracing_Vulkan/commit/c2d7368ab42e122535607695c693fe9105e78e22))

The builds and the startup check now compile only the Slang shaders. The GLSL versions are generated when asked for: from the **Build GLSL shaders** button in the new **Supported shader types** section of the settings panel (which switches to GLSL once the build finishes), by starting with `-glsl`, or with `compile_shaders.bat glsl`.

### 2.0 Slang as the only shader source ([a2a6b92](https://github.com/zyoKo/RayTracing_Vulkan/commit/a2a6b92d0b869668fe6e4843fc82d3ce954052a1))

The hand-written GLSL shaders are gone, so a fix only has to be made once, in Slang. Slang is now the default, and `compile_shaders.bat` generates a GLSL version of every shader in `src/shaders/glsl/` and compiles it for the `-glsl` mode.

### 1.1 Shaders compiled on first run ([8a1f3e4](https://github.com/zyoKo/RayTracing_Vulkan/commit/8a1f3e491f193cf058f76e618ba52ee504f5b3b7))

The compiled SPIR-V files are no longer stored in the repository. `rtrt.exe` checks `src/spv/` when it starts and runs `src/compile_shaders.bat` to build any that are missing.

### 1.0 Denoiser fix around emissive windows ([4f1e4f4](https://github.com/zyoKo/RayTracing_Vulkan/commit/4f1e4f4f785beb294b390b7b4065ccba910e563c))

The emissive window panes used to leak into the walls around them, and the wide filter passes spread the brightest samples into square blotches. The denoiser now leaves light sources out of the filter, weights neighbours by colour as well as depth, normal and albedo, and clamps outlier pixels before the first pass.

| Before | After |
|---|---|
| ![Denoiser before the fix](images/Denoiser_Before.png) | ![Denoiser after the fix](images/RayTracing_With_Denoiser.png) |
| ![Window area before the fix](images/Denoiser_Before_Window.png) | ![Window area after the fix](images/Denoiser_After_Window.png) |
