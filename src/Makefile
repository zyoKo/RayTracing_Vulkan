

LIBDIR := ../libs
VPATH = $(LIBDIR)/imgui-master $(LIBDIR)/imgui-master/backends shaders

SDK = $(VULKAN_SDK)/include

CXX = g++

OPTIMIZE = -g

CXXFLAGS = -DGLM_FORCE_RADIANS -DPROJECT_NAME=\"rtrt\" -DPROJECT_RELDIRECTORY=\".\"  $(OPTIMIZE) -std=c++17 -I. -I$(SDK) -I$(LIBDIR)/glm -I$(LIBDIR)/imgui-master -I$(LIBDIR)/imgui-master/backends -I$(LIBDIR)  -I$(LIBDIR)/glfw/include

LIBS =  -lassimp  -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi `pkg-config --static --libs glfw3`

target = rtrt.exe

headers = core/window.h scene/camera.h scene/camera_controller.h extensions_vk.hpp vk_check.h vk/context.h vk/device.h vk/physical_device.h vk/swapchain.h core/application.h core/shader_check.h vk/buffer.h vk/image.h vk/sampler.h vk/command_pool.h vk/command_buffer.h vk/fence.h vk/semaphore.h vk/shader.h vk/descriptor_set_layout.h vk/descriptor_pool.h vk/descriptor_set.h vk/descriptor_builder.h vk/pipeline.h vk/pipeline_layout.h vk/render_pass.h vk/framebuffer.h scene/scene.h scene/mesh.h scene/texture.h rt/acceleration_structure.h rt/shader_binding_table.h renderer/graphics_pipeline.h renderer/post_pass.h renderer/pass_context.h renderer/scanline_pass.h renderer/ray_trace_pass.h renderer/render_image.h renderer/denoise_pass.h ui/gui.h renderer/renderer.h

src = core/window.cpp scene/camera.cpp scene/camera_controller.cpp extensions_vk.cpp vk/context.cpp vk/device.cpp vk/physical_device.cpp vk/swapchain.cpp core/application.cpp core/shader_check.cpp main.cpp vk/buffer.cpp vk/image.cpp vk/sampler.cpp vk/command_pool.cpp vk/command_buffer.cpp vk/fence.cpp vk/semaphore.cpp vk/shader.cpp vk/descriptor_set_layout.cpp vk/descriptor_pool.cpp vk/descriptor_set.cpp vk/descriptor_builder.cpp vk/pipeline.cpp vk/pipeline_layout.cpp vk/render_pass.cpp vk/framebuffer.cpp scene/scene.cpp scene/texture.cpp rt/acceleration_structure.cpp rt/shader_binding_table.cpp renderer/graphics_pipeline.cpp renderer/post_pass.cpp renderer/scanline_pass.cpp renderer/ray_trace_pass.cpp renderer/render_image.cpp renderer/denoise_pass.cpp ui/gui.cpp renderer/renderer.cpp

names = denoise.comp post.vert post.frag scanline.vert scanline.frag raytrace.rgen raytrace.rmiss raytraceShadow.rmiss raytrace.rchit

shader_spvs = $(patsubst %,spv/%.slang.spv,$(names))
glsl_spvs = $(patsubst %,spv/%.spv,$(names))

SLANG_FLAGS = -matrix-layout-column-major -I shaders
SLANGC_SPIRV = slangc $(SLANG_FLAGS) -target spirv -profile spirv_1_5 -capability SPV_KHR_non_semantic_info+SPV_GOOGLE_user_type+spvDerivativeControl+spvImageQuery+spvImageGatherExtended+spvSparseResidency+spvMinLod+spvFragmentFullyCoveredEXT -g
SLANGC_GLSL = slangc $(SLANG_FLAGS) -target glsl -profile glsl_460 -warnings-disable 41012

imgui_src = $(LIBDIR)/imgui-master/backends/imgui_impl_glfw.cpp $(LIBDIR)/imgui-master/backends/imgui_impl_vulkan.cpp $(LIBDIR)/imgui-master/imgui.cpp $(LIBDIR)/imgui-master/imgui_demo.cpp $(LIBDIR)/imgui-master/imgui_draw.cpp $(LIBDIR)/imgui-master/imgui_widgets.cpp $(LIBDIR)/imgui-master/imgui_tables.cpp


objects = $(patsubst %.cpp,%.o,$(src)) $(patsubst %.cpp,%.o,$(imgui_src)) 

$(target): $(objects) $(shader_spvs)
	g++  $(CXXFLAGS) -o $@  $(objects) $(LIBS)

# Slang is the shader source. Each entry point is compiled to spv/<name>.slang.spv. On
# request (make glsl) it is also translated to GLSL in shaders/glsl/ and compiled from
# there to spv/<name>.spv.
# Slang writes buffer pointers as std430 in GLSL; the C++ data is scalar, so sed fixes it.
# $(1): output name, $(2): Slang file, $(3): entry point
define shader_rules
spv/$(1).slang.spv: shaders/$(2) shaders/shared_structs.h shaders/rng.slang
	mkdir -p spv
	$$(SLANGC_SPIRV) $$< -entry $(3) -o $$@
shaders/glsl/$(1): shaders/$(2) shaders/shared_structs.h shaders/rng.slang
	mkdir -p shaders/glsl
	$$(SLANGC_GLSL) $$< -entry $(3) -o $$@
	sed -i -e 's/buffer_reference, std430/buffer_reference, scalar/' -e 's/^#version 460$$$$/&\n#extension GL_EXT_scalar_block_layout : require/' $$@
spv/$(1).spv: shaders/glsl/$(1)
	mkdir -p spv
	glslangValidator -V --target-env vulkan1.2 -o $$@ $$<
endef

$(eval $(call shader_rules,denoise.comp,denoise.slang,main))
$(eval $(call shader_rules,post.vert,post.slang,vertexMain))
$(eval $(call shader_rules,post.frag,post.slang,fragmentMain))
$(eval $(call shader_rules,scanline.vert,scanline.slang,vertexMain))
$(eval $(call shader_rules,scanline.frag,scanline.slang,fragmentMain))
$(eval $(call shader_rules,raytrace.rgen,raytrace.slang,rayGenMain))
$(eval $(call shader_rules,raytrace.rmiss,raytrace.slang,missMain))
$(eval $(call shader_rules,raytraceShadow.rmiss,raytrace.slang,shadowMissMain))
$(eval $(call shader_rules,raytrace.rchit,raytrace.slang,closestHitMain))

# The generated-GLSL shaders, built only on request: make glsl
.PHONY: glsl
glsl: $(glsl_spvs)

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
