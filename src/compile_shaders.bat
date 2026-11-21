@echo off
rem Compiles the Slang shaders in shaders\ to SPIR-V in spv\ (<name>.slang.spv). Each
rem entry point is also translated to GLSL in shaders\glsl\ and compiled from there to
rem spv\<name>.spv, for viewing and testing the GLSL version.
rem Pass a .slang file name to build only that file. Needs the Vulkan SDK (VULKAN_SDK).
setlocal
cd /d "%~dp0"

if "%VULKAN_SDK%"=="" (
    echo VULKAN_SDK is not set. Install the Vulkan SDK, then open a new terminal.
    exit /b 1
)

set SLANGC="%VULKAN_SDK%\Bin\slangc.exe" -matrix-layout-column-major -I shaders
set TO_SPIRV=-target spirv -profile spirv_1_5 -capability SPV_KHR_non_semantic_info+SPV_GOOGLE_user_type+spvDerivativeControl+spvImageQuery+spvImageGatherExtended+spvSparseResidency+spvMinLod+spvFragmentFullyCoveredEXT -g
set TO_GLSL=-target glsl -profile glsl_460 -warnings-disable 41012
set GLSLANG="%VULKAN_SDK%\Bin\glslangValidator.exe" -V --target-env vulkan1.2
set ONLY=%~1

if not exist spv mkdir spv
if not exist shaders\glsl mkdir shaders\glsl
set FAILED=0

call :Shader denoise.slang main denoise.comp
call :Shader post.slang vertexMain post.vert
call :Shader post.slang fragmentMain post.frag
call :Shader scanline.slang vertexMain scanline.vert
call :Shader scanline.slang fragmentMain scanline.frag
call :Shader raytrace.slang rayGenMain raytrace.rgen
call :Shader raytrace.slang missMain raytrace.rmiss
call :Shader raytrace.slang shadowMissMain raytraceShadow.rmiss
call :Shader raytrace.slang closestHitMain raytrace.rchit

if %FAILED%==1 (
    echo Some shaders failed to compile.
    exit /b 1
)
echo Shaders compiled to spv\, generated GLSL in shaders\glsl\
exit /b 0

:Shader
if not "%ONLY%"=="" if /i not "%ONLY%"=="%1" exit /b 0
echo %1 (%2)
%SLANGC% shaders\%1 -entry %2 %TO_SPIRV% -o spv\%3.slang.spv || (
    echo   failed: Slang to SPIR-V
    set FAILED=1
    exit /b 0
)
%SLANGC% shaders\%1 -entry %2 %TO_GLSL% -o shaders\glsl\%3 || (
    echo   failed: Slang to GLSL
    set FAILED=1
    exit /b 0
)
rem Slang writes buffer pointers as std430 in GLSL, but the C++ data is tightly packed
rem (scalar), which is how Slang's own SPIR-V reads it.
findstr /c:"buffer_reference, std430" shaders\glsl\%3 > nul && powershell -NoProfile -ExecutionPolicy Bypass -Command "$f='shaders\glsl\%3'; $t=[IO.File]::ReadAllText($f).Replace('buffer_reference, std430', 'buffer_reference, scalar').Replace('#version 460', '#version 460' + [Environment]::NewLine + '#extension GL_EXT_scalar_block_layout : require'); [IO.File]::WriteAllText($f, $t)"
%GLSLANG% -o spv\%3.spv shaders\glsl\%3 > nul || (
    echo   failed: GLSL to SPIR-V
    set FAILED=1
)
exit /b 0
