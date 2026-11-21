@echo off
rem Compiles every GLSL and Slang shader in shaders\ to SPIR-V in spv\.
rem Needs the Vulkan SDK (VULKAN_SDK set by its installer).
setlocal
cd /d "%~dp0"

if "%VULKAN_SDK%"=="" (
    echo VULKAN_SDK is not set. Install the Vulkan SDK, then open a new terminal.
    exit /b 1
)

set GLSLC="%VULKAN_SDK%\Bin\glslangValidator.exe" -V --target-env vulkan1.2
set SLANGC="%VULKAN_SDK%\Bin\slangc.exe" -target spirv -profile spirv_1_5 -capability SPV_KHR_non_semantic_info+SPV_GOOGLE_user_type+spvDerivativeControl+spvImageQuery+spvImageGatherExtended+spvSparseResidency+spvMinLod+spvFragmentFullyCoveredEXT -matrix-layout-column-major -I shaders -g

if not exist spv mkdir spv
set FAILED=0

for %%S in (post.vert post.frag scanline.vert scanline.frag denoise.comp raytrace.rgen raytrace.rmiss raytraceShadow.rmiss raytrace.rchit) do (
    echo %%S
    %GLSLC% -o spv\%%S.spv shaders\%%S > nul || (echo   failed: shaders\%%S & set FAILED=1)
)

call :Slang denoise.slang main denoise.comp
call :Slang post.slang vertexMain post.vert
call :Slang post.slang fragmentMain post.frag
call :Slang scanline.slang vertexMain scanline.vert
call :Slang scanline.slang fragmentMain scanline.frag
call :Slang raytrace.slang rayGenMain raytrace.rgen
call :Slang raytrace.slang missMain raytrace.rmiss
call :Slang raytrace.slang shadowMissMain raytraceShadow.rmiss
call :Slang raytrace.slang closestHitMain raytrace.rchit

if %FAILED%==1 (
    echo Some shaders failed to compile.
    exit /b 1
)
echo Shaders compiled to spv\
exit /b 0

:Slang
echo %1 (%2)
%SLANGC% shaders\%1 -entry %2 -o spv\%3.slang.spv || (echo   failed: shaders\%1 %2 & set FAILED=1)
exit /b 0
