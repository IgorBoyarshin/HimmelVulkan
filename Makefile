# Main: main.cpp
# 	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -I$(VULKAN_SDK_PATH_INCLUDE) -o main main.cpp $(LDFLAGS)

# VULKAN_SDK_PATH = /home/igorek/Storage/dev/LibsAndPrograms/VulkanSDK/1.1.73.0/x86_64
VULKAN_SDK_PATH_INCLUDE = /usr/include/vulkan
VULKAN_SDK_PATH_LIB = /usr/lib
VULKAN_SDK_PATH_LAYER = /usr/share/vulkan/explicit_layer.d

# The name of the main file and executable
mainFileName = main
# Files that have .h and .cpp versions
classFiles = Himmel HmlResourceManager HmlModel HmlCamera HmlCommands HmlSwapchain HmlDescriptors HmlDevice HmlWindow HmlPipeline HmlRenderer HmlSnowParticleRenderer
# Files that only have the .h version
# justHeaderFiles =
# Compilation flags
OPTIMIZATION_FLAG = -O0
LANGUAGE_LEVEL = -std=c++20
COMPILER_FLAGS = -Wall -Wextra -Wno-unused-parameter -Wpedantic -I$(VULKAN_SDK_PATH_INCLUDE)
LINKER_FLAGS = -L$(VULKAN_SDK_PATH_LIB) `pkg-config --static --libs glfw3` -lvulkan


# Auxiliary
filesObj = $(addsuffix .o, $(mainFileName) $(classFiles))
# filesH = $(addsuffix .h, $(classFiles) $(justHeaderFiles))



Himmel.o: Himmel.cpp Himmel.h HmlWindow.h HmlDevice.h HmlDescriptors.h HmlCommands.h HmlSwapchain.h HmlResourceManager.h HmlRenderer.h HmlSnowParticleRenderer.h HmlModel.h HmlCamera.h
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

HmlRenderer.o: HmlRenderer.cpp HmlRenderer.h HmlDevice.h HmlWindow.h HmlPipeline.h HmlSwapchain.h HmlCommands.h HmlModel.h HmlResourceManager.h HmlDescriptors.h
	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $<

HmlResourceManager.o: HmlResourceManager.cpp HmlResourceManager.h HmlDevice.h HmlCommands.h
	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $<

HmlSnowParticleRenderer.o: HmlSnowParticleRenderer.cpp HmlSnowParticleRenderer.h HmlDevice.h HmlWindow.h HmlPipeline.h HmlSwapchain.h HmlCommands.h HmlModel.h HmlResourceManager.h HmlDescriptors.h
	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $<

HmlSwapchain.o: HmlSwapchain.cpp HmlSwapchain.h HmlWindow.h HmlDevice.h HmlResourceManager.h
	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $<

HmlWindow.o: HmlWindow.cpp HmlWindow.h
	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $<


all: cleanExe $(mainFileName)


run: all compileShaders
	LD_LIBRARY_PATH=$(VULKAN_SDK_PATH_LIB) VK_LAYER_PATH=$(VULKAN_SDK_PATH_LAYER) ./main


# Compiler
# %.o: %.cpp $(filesH)
# 	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $<

# Linker
$(mainFileName): $(filesObj)
	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) $^ -o $@ $(LINKER_FLAGS)


# Utils
clean:
	rm -f a.out *.o *.gch .*.gch $(mainFileName) shaders/out/*.spv

cleanExe:
	rm -f $(mainFileName)

compileShaders: shaders/simple.vert shaders/simple.frag shaders/snow.vert shaders/snow.frag
	glslc shaders/simple.vert -o shaders/out/simple.vert.spv
	glslc shaders/simple.frag -o shaders/out/simple.frag.spv
	glslc shaders/snow.vert -o shaders/out/snow.vert.spv
	glslc shaders/snow.frag -o shaders/out/snow.frag.spv
