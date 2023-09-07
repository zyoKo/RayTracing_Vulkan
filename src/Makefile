

LIBDIR := /home/gherron/projects/frameworks/libs
VPATH = $(LIBDIR)/imgui-master $(LIBDIR)/imgui-master/backends shaders

SDK = /home/gherron/vulkanSDK/1.3.243.0/x86_64/include

pkgName=rtrtFramework
pkgDir = /home/gherron/packages

CXX = g++

OPTIMIZE = -g

CXXFLAGS = -DPROJECT_NAME=\"rtrt\" -DPROJECT_RELDIRECTORY=\".\"  $(OPTIMIZE) -std=c++17 -I. -I$(SDK) -I$(LIBDIR)/glm -I$(LIBDIR)/imgui-master -I$(LIBDIR)/imgui-master/backends -I$(LIBDIR)  -I$(LIBDIR)/glfw/include

LIBS =  -lassimp  -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi `pkg-config --static --libs glfw3`

target = rtrt.exe

headers = app.h vkapp.h camera.h buffer_wrap.h descriptor_wrap.h image_wrap.h extensions_vk.hpp acceleration_wrap.h

src = app.cpp vkapp.cpp camera.cpp vkapp_fns.cpp extensions_vk.cpp descriptor_wrap.cpp vkapp_loadModel.cpp vkapp_scanline.cpp vkapp_raytracing.cpp acceleration_wrap.cpp vkapp_denoise.cpp

shader_spvs = spv/post.frag.spv  spv/post.vert.spv spv/scanline.vert.spv spv/scanline.frag.spv spv/post.frag.spv spv/post.vert.spv spv/raytrace.rgen.spv spv/raytrace.rmiss.spv spv/raytrace.rchit.spv spv/raytraceShadow.rmiss.spv spv/denoise.comp.spv

shader_src =  shaders/shared_structs.h   shaders/post.frag shaders/post.vert   shaders/scanline.vert shaders/scanline.frag shaders/raytrace.rgen shaders/raytrace.rmiss shaders/raytrace.rchit shaders/denoise.comp shaders/raytraceShadow.rmiss

imgui_src = $(LIBDIR)/imgui-master/backends/imgui_impl_glfw.cpp $(LIBDIR)/imgui-master/backends/imgui_impl_vulkan.cpp $(LIBDIR)/imgui-master/imgui.cpp $(LIBDIR)/imgui-master/imgui_demo.cpp $(LIBDIR)/imgui-master/imgui_draw.cpp $(LIBDIR)/imgui-master/imgui_widgets.cpp

extraFiles = Makefile rtrt.vcxproj rtrt.sln rtrt.vcxproj.filters

objects = $(patsubst %.cpp,%.o,$(src)) $(patsubst %.cpp,%.o,$(imgui_src)) 

$(target): $(objects) $(shader_spvs)
	g++  $(CXXFLAGS) -o $@  $(objects) $(LIBS)

spv/denoise.comp.spv: shaders/denoise.comp shaders/shared_structs.h
	mkdir -p spv
	glslangValidator -g --target-env vulkan1.2 -o $@  $<
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

zip:
	rm -rf $(pkgDir)/$(pkgName) $(pkgDir)/$(pkgName).zip
	mkdir $(pkgDir)/$(pkgName)
	mkdir $(pkgDir)/$(pkgName)/src
	mkdir $(pkgDir)/$(pkgName)/src/shaders
	mkdir $(pkgDir)/$(pkgName)/src/spv
	mkdir $(pkgDir)/$(pkgName)/libs
	cp $(src) $(headers) $(pkgDir)/$(pkgName)/src
	cp $(shader_src) $(pkgDir)/$(pkgName)/src/shaders
	cp -r models $(pkgDir)/$(pkgName)/src
	cp -r $(LIBDIR)/* $(pkgDir)/$(pkgName)/libs
	rm -rf $(LIBDIR)/bvh* 
	rm -rf $(LIBDIR)/*glbinding*
	rm -rf  $(LIBDIR)/glfw-old
	cp -r $(extraFiles) $(pkgDir)/$(pkgName)/src
	cd $(pkgDir)/$(pkgName)/src;  unifdef -DSTUDENT -m  $(headers) || /bin/true
	cd $(pkgDir)/$(pkgName)/src;  unifdef -DSTUDENT -m  $(src) || /bin/true
	cd $(pkgDir)/$(pkgName)/src;  unifdef -DSTUDENT -m  $(shader_src) || /bin/true

	cd $(pkgDir);  zip -r $(pkgName).zip $(pkgName); 
	cd /home/gherron/projects/dev-rtrt; rm -rf $(pkgName); unzip $(pkgDir)/$(pkgName).zip

dependencies: 
	g++ -MM $(CXXFLAGS)  $(src) > dependencies

include dependencies
