

LIBDIR := ../libs
VPATH = $(LIBDIR)/imgui-master $(LIBDIR)/imgui-master/backends shaders

SDK = $(VULKAN_SDK)/include

CXX = g++

OPTIMIZE = -g

CXXFLAGS = -DGLM_FORCE_RADIANS -DPROJECT_NAME=\"rtrt\" -DPROJECT_RELDIRECTORY=\".\"  $(OPTIMIZE) -std=c++17 -I. -I$(SDK) -I$(LIBDIR)/glm -I$(LIBDIR)/imgui-master -I$(LIBDIR)/imgui-master/backends -I$(LIBDIR)  -I$(LIBDIR)/glfw/include

LIBS =  -lassimp  -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi `pkg-config --static --libs glfw3`

target = rtrt.exe

headers = core/window.h renderer.h scene/camera.h scene/camera_controller.h extensions_vk.hpp vk_check.h vk/context.h vk/device.h vk/physical_device.h vk/swapchain.h core/application.h vk/buffer.h vk/image.h vk/sampler.h vk/command_pool.h vk/command_buffer.h vk/fence.h vk/semaphore.h vk/shader.h vk/descriptor_set_layout.h vk/descriptor_pool.h vk/descriptor_set.h vk/descriptor_builder.h vk/pipeline.h vk/pipeline_layout.h vk/render_pass.h vk/framebuffer.h scene/scene.h scene/mesh.h scene/texture.h rt/acceleration_structure.h rt/shader_binding_table.h renderer/graphics_pipeline.h renderer/post_pass.h renderer/pass_context.h renderer/scanline_pass.h renderer/ray_trace_pass.h renderer/render_image.h renderer/denoise_pass.h

src = core/window.cpp renderer.cpp scene/camera.cpp scene/camera_controller.cpp renderer_setup.cpp extensions_vk.cpp renderer_scanline.cpp renderer_raytracing.cpp vk/context.cpp vk/device.cpp vk/physical_device.cpp vk/swapchain.cpp core/application.cpp main.cpp vk/buffer.cpp vk/image.cpp vk/sampler.cpp vk/command_pool.cpp vk/command_buffer.cpp vk/fence.cpp vk/semaphore.cpp vk/shader.cpp vk/descriptor_set_layout.cpp vk/descriptor_pool.cpp vk/descriptor_set.cpp vk/descriptor_builder.cpp vk/pipeline.cpp vk/pipeline_layout.cpp vk/render_pass.cpp vk/framebuffer.cpp scene/scene.cpp scene/texture.cpp rt/acceleration_structure.cpp rt/shader_binding_table.cpp renderer/graphics_pipeline.cpp renderer/post_pass.cpp renderer/scanline_pass.cpp renderer/ray_trace_pass.cpp renderer/render_image.cpp renderer/denoise_pass.cpp

shader_spvs = spv/post.frag.spv  spv/post.vert.spv spv/scanline.vert.spv spv/scanline.frag.spv spv/post.frag.spv spv/post.vert.spv spv/raytrace.rgen.spv spv/raytrace.rmiss.spv spv/raytrace.rchit.spv spv/raytraceShadow.rmiss.spv spv/denoise.comp.spv $(slang_spvs)

slang_spvs = spv/denoise.comp.slang.spv spv/post.vert.slang.spv spv/post.frag.slang.spv spv/scanline.vert.slang.spv spv/scanline.frag.slang.spv spv/raytrace.rgen.slang.spv spv/raytrace.rmiss.slang.spv spv/raytraceShadow.rmiss.slang.spv spv/raytrace.rchit.slang.spv

SLANGC = slangc -target spirv -profile spirv_1_5 -capability SPV_KHR_non_semantic_info+SPV_GOOGLE_user_type+spvDerivativeControl+spvImageQuery+spvImageGatherExtended+spvSparseResidency+spvMinLod+spvFragmentFullyCoveredEXT -matrix-layout-column-major -I shaders -g

shader_src =  shaders/shared_structs.h   shaders/post.frag shaders/post.vert   shaders/scanline.vert shaders/scanline.frag shaders/raytrace.rgen shaders/raytrace.rmiss shaders/raytrace.rchit shaders/denoise.comp shaders/raytraceShadow.rmiss

imgui_src = $(LIBDIR)/imgui-master/backends/imgui_impl_glfw.cpp $(LIBDIR)/imgui-master/backends/imgui_impl_vulkan.cpp $(LIBDIR)/imgui-master/imgui.cpp $(LIBDIR)/imgui-master/imgui_demo.cpp $(LIBDIR)/imgui-master/imgui_draw.cpp $(LIBDIR)/imgui-master/imgui_widgets.cpp


objects = $(patsubst %.cpp,%.o,$(src)) $(patsubst %.cpp,%.o,$(imgui_src)) 

$(target): $(objects) $(shader_spvs)
	g++  $(CXXFLAGS) -o $@  $(objects) $(LIBS)

spv/denoise.comp.spv: shaders/denoise.comp shaders/shared_structs.h
	mkdir -p spv
	glslangValidator -g --target-env vulkan1.2 -o $@  $<
spv/denoise.comp.slang.spv: shaders/denoise.slang shaders/shared_structs.h
	mkdir -p spv
	$(SLANGC) $< -entry main -o $@
spv/post.vert.slang.spv: shaders/post.slang shaders/shared_structs.h
	mkdir -p spv
	$(SLANGC) $< -entry vertexMain -o $@
spv/post.frag.slang.spv: shaders/post.slang shaders/shared_structs.h
	mkdir -p spv
	$(SLANGC) $< -entry fragmentMain -o $@
spv/scanline.vert.slang.spv: shaders/scanline.slang shaders/shared_structs.h
	mkdir -p spv
	$(SLANGC) $< -entry vertexMain -o $@
spv/scanline.frag.slang.spv: shaders/scanline.slang shaders/shared_structs.h
	mkdir -p spv
	$(SLANGC) $< -entry fragmentMain -o $@
spv/raytrace.rgen.slang.spv: shaders/raytrace.slang shaders/shared_structs.h shaders/rng.slang
	mkdir -p spv
	$(SLANGC) $< -entry rayGenMain -o $@
spv/raytrace.rmiss.slang.spv: shaders/raytrace.slang shaders/shared_structs.h shaders/rng.slang
	mkdir -p spv
	$(SLANGC) $< -entry missMain -o $@
spv/raytraceShadow.rmiss.slang.spv: shaders/raytrace.slang shaders/shared_structs.h shaders/rng.slang
	mkdir -p spv
	$(SLANGC) $< -entry shadowMissMain -o $@
spv/raytrace.rchit.slang.spv: shaders/raytrace.slang shaders/shared_structs.h shaders/rng.slang
	mkdir -p spv
	$(SLANGC) $< -entry closestHitMain -o $@
spv/post.frag.spv: shaders/post.frag shaders/shared_structs.h
	mkdir -p spv
	glslangValidator -g --target-env vulkan1.2 -o $@  $<
spv/post.vert.spv: shaders/post.vert shaders/shared_structs.h
	mkdir -p spv
	glslangValidator -g --target-env vulkan1.2 -o $@  $<
spv/raytrace.rchit.spv: shaders/raytrace.rchit shaders/shared_structs.h
	mkdir -p spv
	glslangValidator -g --target-env vulkan1.2 -o $@  $<
spv/raytrace.rgen.spv: shaders/raytrace.rgen shaders/shared_structs.h
	mkdir -p spv
	glslangValidator -g --target-env vulkan1.2 -o $@  $<
spv/raytrace.rmiss.spv: shaders/raytrace.rmiss shaders/shared_structs.h
	mkdir -p spv
	glslangValidator -g --target-env vulkan1.2 -o $@  $<
spv/raytraceShadow.rmiss.spv: shaders/raytraceShadow.rmiss shaders/shared_structs.h
	mkdir -p spv
	glslangValidator -g --target-env vulkan1.2 -o $@  $<
spv/scanline.frag.spv: shaders/scanline.frag shaders/shared_structs.h
	mkdir -p spv
	glslangValidator -g --target-env vulkan1.2 -o $@  $<
spv/scanline.vert.spv: shaders/scanline.vert shaders/shared_structs.h
	mkdir -p spv
	glslangValidator -g --target-env vulkan1.2 -o $@  $<

test:
	ls -1 spv

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(target)  $(objects)
	./rtrt.exe

rund: $(target)  $(objects)
	./rtrt.exe -d

clean:
	rm -rf *.suo *.sdf *.orig Release Debug ipch *.o *~ raytrace dependencies *13*scn  *13*ppm

dependencies: 
	g++ -MM $(CXXFLAGS)  $(src) > dependencies

include dependencies
