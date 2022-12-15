# Main: main.cpp
# 	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -I$(VULKAN_SDK_PATH_INCLUDE) -o main main.cpp $(LDFLAGS)

# VULKAN_SDK_PATH = /home/igorek/Storage/dev/LibsAndPrograms/VulkanSDK/1.1.73.0/x86_64
VULKAN_SDK_PATH_INCLUDE = /usr/include/vulkan
VULKAN_SDK_PATH_LIB = /usr/lib
VULKAN_SDK_PATH_LAYER = /usr/share/vulkan/explicit_layer.d

# The name of the main file and executable
mainFileName = main
# Files that have .h and .cpp versions
classFiles = HmlResourceManager HmlModel HmlCamera HmlCommands HmlSwapchain HmlDescriptors HmlDevice HmlWindow HmlPipeline HmlRenderer HmlSnowParticleRenderer HmlTerrainRenderer HmlRenderPass HmlUiRenderer HmlDeferredRenderer HmlLightRenderer HmlBloomRenderer util HmlQueries HmlImgui HmlImguiRenderer HmlDispatcher HmlPhysics HmlMath Himmel
# Files that only have the .h version
# justHeaderFiles = renderer settings
# Compilation flags
COMPILER = g++
OPTIMIZATION_FLAG = -O0
LANGUAGE_LEVEL = -std=c++20
COMPILER_FLAGS = -Wall -Wextra -Wno-unused-parameter -Wpedantic -Wno-ignored-attributes -I$(VULKAN_SDK_PATH_INCLUDE) -mavx -mavx2
LINKER_FLAGS = -L$(VULKAN_SDK_PATH_LIB) `pkg-config --static --libs glfw3` -lvulkan -lpthread
# Can also use in conjunction with:
# :> valgrind --leak-check=full ./main
# COMPILER_FLAGS = -Wall -Wextra -Wno-unused-parameter -Wpedantic -I$(VULKAN_SDK_PATH_INCLUDE) -mavx -mavx2 -fsanitize=address -g -fno-omit-frame-pointer
# LINKER_FLAGS = -L$(VULKAN_SDK_PATH_LIB) `pkg-config --static --libs glfw3` -lvulkan -lpthread -fsanitize=address


# Auxiliary
# filesObj = $(addsuffix .o, $(mainFileName) $(classFiles))
# filesH = $(addsuffix .h, $(classFiles) $(justHeaderFiles))


../build/main.o: main.cpp Himmel.h
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o $@

../build/util.o: util.cpp util.h settings.h
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o $@

../build/Himmel.o: Himmel.cpp Himmel.h HmlWindow.h HmlDevice.h HmlDescriptors.h HmlCommands.h HmlSwapchain.h HmlResourceManager.h HmlRenderer.h HmlSnowParticleRenderer.h HmlModel.h HmlCamera.h HmlTerrainRenderer.h util.h HmlRenderPass.h HmlUiRenderer.h HmlDeferredRenderer.h HmlLightRenderer.h HmlBloomRenderer.h renderer.h HmlContext.h HmlQueries.h settings.h HmlDispatcher.h HmlPhysics.h HmlMath.h
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o $@

../build/HmlCamera.o: HmlCamera.cpp HmlCamera.h settings.h
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o $@

../build/HmlDescriptors.o: HmlDescriptors.cpp HmlDescriptors.h HmlModel.h HmlDevice.h settings.h
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o $@

../build/HmlCommands.o: HmlCommands.cpp HmlCommands.h HmlDevice.h settings.h
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o $@

../build/HmlDevice.o: HmlDevice.cpp HmlDevice.h HmlWindow.h settings.h
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o $@

../build/HmlModel.o: HmlModel.cpp HmlModel.h settings.h
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o $@

../build/HmlPipeline.o: HmlPipeline.cpp HmlPipeline.h HmlDevice.h settings.h
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o $@

../build/HmlDispatcher.o: HmlDispatcher.cpp HmlDispatcher.h HmlDevice.h HmlCommands.h HmlSwapchain.h HmlRenderPass.h renderer.h settings.h
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o $@

../build/HmlRenderer.o: HmlRenderer.cpp HmlRenderer.h HmlDevice.h HmlWindow.h HmlPipeline.h HmlRenderPass.h HmlCommands.h HmlModel.h HmlResourceManager.h HmlDescriptors.h renderer.h HmlContext.h HmlQueries.h settings.h
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o $@

../build/HmlDeferredRenderer.o: HmlDeferredRenderer.cpp HmlDeferredRenderer.h HmlDevice.h HmlWindow.h HmlPipeline.h HmlRenderPass.h HmlCommands.h HmlModel.h HmlResourceManager.h HmlDescriptors.h renderer.h HmlContext.h HmlQueries.h settings.h
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o $@

# HmlBlurRenderer.o: HmlBlurRenderer.cpp HmlBlurRenderer.h HmlDevice.h HmlWindow.h HmlPipeline.h HmlRenderPass.h HmlCommands.h HmlModel.h HmlResourceManager.h HmlDescriptors.h renderer.h HmlContext.h settings.h
# 	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o ../build/$@

../build/HmlBloomRenderer.o: HmlBloomRenderer.cpp HmlBloomRenderer.h HmlDevice.h HmlWindow.h HmlPipeline.h HmlRenderPass.h HmlCommands.h HmlModel.h HmlResourceManager.h HmlDescriptors.h renderer.h HmlContext.h HmlQueries.h settings.h
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o $@

../build/HmlUiRenderer.o: HmlUiRenderer.cpp HmlUiRenderer.h HmlDevice.h HmlWindow.h HmlPipeline.h HmlRenderPass.h HmlCommands.h HmlModel.h HmlResourceManager.h HmlDescriptors.h renderer.h HmlContext.h HmlQueries.h settings.h
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o $@

../build/HmlResourceManager.o: HmlResourceManager.cpp HmlResourceManager.h HmlDevice.h HmlCommands.h HmlContext.h settings.h
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o $@

../build/HmlSnowParticleRenderer.o: HmlSnowParticleRenderer.cpp HmlSnowParticleRenderer.h HmlDevice.h HmlWindow.h HmlPipeline.h HmlRenderPass.h HmlCommands.h HmlModel.h HmlResourceManager.h HmlDescriptors.h util.h renderer.h HmlContext.h HmlQueries.h settings.h
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o $@

../build/HmlLightRenderer.o: HmlLightRenderer.cpp HmlLightRenderer.h HmlDevice.h HmlWindow.h HmlPipeline.h HmlRenderPass.h HmlCommands.h HmlModel.h HmlResourceManager.h HmlDescriptors.h renderer.h HmlContext.h HmlQueries.h settings.h
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o $@

../build/HmlTerrainRenderer.o: HmlTerrainRenderer.cpp HmlTerrainRenderer.h HmlDevice.h HmlWindow.h HmlPipeline.h HmlRenderPass.h HmlCommands.h HmlModel.h HmlResourceManager.h HmlDescriptors.h renderer.h HmlContext.h HmlQueries.h settings.h
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o $@

../build/HmlSwapchain.o: HmlSwapchain.cpp HmlSwapchain.h HmlWindow.h HmlDevice.h HmlResourceManager.h settings.h
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o $@

../build/HmlRenderPass.o: HmlRenderPass.cpp HmlRenderPass.h HmlDevice.h HmlCommands.h HmlResourceManager.h settings.h
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o $@

../build/HmlWindow.o: HmlWindow.cpp HmlWindow.h settings.h
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o $@

../build/HmlQueries.o: HmlQueries.cpp HmlQueries.h settings.h
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o $@

../build/HmlImguiRenderer.o: HmlImguiRenderer.cpp HmlImguiRenderer.h HmlQueries.h renderer.h settings.h
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o $@

../build/HmlImgui.o: HmlImgui.cpp HmlImgui.h HmlWindow.h HmlResourceManager.h settings.h
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o $@

../build/HmlMath.o: HmlMath.cpp HmlMath.h settings.h
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o $@

../build/HmlPhysics.o: HmlPhysics.cpp HmlPhysics.h settings.h HmlMath.h
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o $@


# Compiler
# %.o: %.cpp $(filesH)
# 	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $<

# Linker
filesObjPath = $(addprefix ../build/, $(addsuffix .o, $(mainFileName) $(classFiles)))
allObjFiles = $(wildcard ../build/*.o)

$(mainFileName): $(filesObjPath)
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) $(allObjFiles) -o $@ $(LINKER_FLAGS)


compileCode: $(mainFileName)


run:
	LD_LIBRARY_PATH=$(VULKAN_SDK_PATH_LIB) VK_LAYER_PATH=$(VULKAN_SDK_PATH_LAYER) ./$(mainFileName)


clean:
	rm -f a.out *.o ../build/*.o *.gch .*.gch $(mainFileName)

cleanExe:
	rm -f $(mainFileName)
