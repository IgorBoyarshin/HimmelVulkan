# Main: main.cpp
# 	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -I$(VULKAN_SDK_PATH_INCLUDE) -o main main.cpp $(LDFLAGS)

# VULKAN_SDK_PATH = /home/igorek/Storage/dev/LibsAndPrograms/VulkanSDK/1.1.73.0/x86_64
VULKAN_SDK_PATH_INCLUDE = /usr/include/vulkan
VULKAN_SDK_PATH_LIB = /usr/lib
VULKAN_SDK_PATH_LAYER = /usr/share/vulkan/explicit_layer.d

# The name of the main file and executable
mainFileName = main
# Files that have .h and .cpp versions
classFiles = Himmel HmlResourceManager HmlModel HmlCamera HmlCommands HmlSwapchain HmlDescriptors HmlDevice HmlWindow HmlPipeline HmlRenderer HmlSnowParticleRenderer HmlTerrainRenderer HmlRenderPass HmlUiRenderer HmlDeferredRenderer
# Files that only have the .h version
justHeaderFiles = util
# Compilation flags
OPTIMIZATION_FLAG = -O0
LANGUAGE_LEVEL = -std=c++20
# COMPILER_FLAGS = -Wall -Wextra -Wno-unused-parameter -Wpedantic -I$(VULKAN_SDK_PATH_INCLUDE) -fsanitize=address -g -fno-omit-frame-pointer
# LINKER_FLAGS = -L$(VULKAN_SDK_PATH_LIB) `pkg-config --static --libs glfw3` -lvulkan -fsanitize=address
COMPILER_FLAGS = -Wall -Wextra -Wno-unused-parameter -Wpedantic -I$(VULKAN_SDK_PATH_INCLUDE)
LINKER_FLAGS = -L$(VULKAN_SDK_PATH_LIB) `pkg-config --static --libs glfw3` -lvulkan


# Auxiliary
filesObj = $(addsuffix .o, $(mainFileName) $(classFiles))
# filesH = $(addsuffix .h, $(classFiles) $(justHeaderFiles))


main.o: main.cpp Himmel.h
	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $<

util.o: util.cpp util.h
	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $<

Himmel.o: Himmel.cpp Himmel.h HmlWindow.h HmlDevice.h HmlDescriptors.h HmlCommands.h HmlSwapchain.h HmlResourceManager.h HmlRenderer.h HmlSnowParticleRenderer.h HmlModel.h HmlCamera.h HmlTerrainRenderer.h util.h HmlRenderPass.h HmlUiRenderer.h HmlDeferredRenderer.h
	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $<

HmlCamera.o: HmlCamera.cpp HmlCamera.h
	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $<

HmlDescriptors.o: HmlDescriptors.cpp HmlDescriptors.h HmlModel.h HmlDevice.h
	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $<

HmlCommands.o: HmlCommands.cpp HmlCommands.h HmlDevice.h
	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $<

HmlDevice.o: HmlDevice.cpp HmlDevice.h HmlWindow.h
	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $<

HmlModel.o: HmlModel.cpp HmlModel.h
	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $<

HmlPipeline.o: HmlPipeline.cpp HmlPipeline.h HmlDevice.h
	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $<

HmlRenderer.o: HmlRenderer.cpp HmlRenderer.h HmlDevice.h HmlWindow.h HmlPipeline.h HmlRenderPass.h HmlCommands.h HmlModel.h HmlResourceManager.h HmlDescriptors.h
	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $<

HmlDeferredRenderer.o: HmlDeferredRenderer.cpp HmlDeferredRenderer.h HmlDevice.h HmlWindow.h HmlPipeline.h HmlRenderPass.h HmlCommands.h HmlModel.h HmlResourceManager.h HmlDescriptors.h
	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $<

HmlUiRenderer.o: HmlUiRenderer.cpp HmlUiRenderer.h HmlDevice.h HmlWindow.h HmlPipeline.h HmlRenderPass.h HmlCommands.h HmlModel.h HmlResourceManager.h HmlDescriptors.h
	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $<

HmlResourceManager.o: HmlResourceManager.cpp HmlResourceManager.h HmlDevice.h HmlCommands.h
	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $<

HmlSnowParticleRenderer.o: HmlSnowParticleRenderer.cpp HmlSnowParticleRenderer.h HmlDevice.h HmlWindow.h HmlPipeline.h HmlRenderPass.h HmlCommands.h HmlModel.h HmlResourceManager.h HmlDescriptors.h util.h
	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $<

# HmlLightRenderer.o: HmlLightRenderer.cpp HmlLightRenderer.h HmlDevice.h HmlWindow.h HmlPipeline.h HmlRenderPass.h HmlCommands.h HmlModel.h HmlResourceManager.h HmlDescriptors.h
# 	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $<

HmlTerrainRenderer.o: HmlTerrainRenderer.cpp HmlTerrainRenderer.h HmlDevice.h HmlWindow.h HmlPipeline.h HmlRenderPass.h HmlCommands.h HmlModel.h HmlResourceManager.h HmlDescriptors.h
	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $<

HmlSwapchain.o: HmlSwapchain.cpp HmlSwapchain.h HmlWindow.h HmlDevice.h HmlResourceManager.h
	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $<

HmlRenderPass.o: HmlRenderPass.cpp HmlRenderPass.h HmlDevice.h HmlCommands.h HmlResourceManager.h
	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $<

HmlWindow.o: HmlWindow.cpp HmlWindow.h
	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $<


# Compiler
# %.o: %.cpp $(filesH)
# 	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $<

# Linker
$(mainFileName): $(filesObj)
	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) $^ -o $@ $(LINKER_FLAGS)


compileCode: $(mainFileName)


run:
	LD_LIBRARY_PATH=$(VULKAN_SDK_PATH_LIB) VK_LAYER_PATH=$(VULKAN_SDK_PATH_LAYER) ./$(mainFileName)


clean:
	rm -f a.out *.o *.gch .*.gch $(mainFileName)

cleanExe:
	rm -f $(mainFileName)
