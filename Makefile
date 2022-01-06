# Main: main.cpp
# 	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -I$(VULKAN_SDK_PATH_INCLUDE) -o main main.cpp $(LDFLAGS)

# VULKAN_SDK_PATH = /home/igorek/Storage/dev/LibsAndPrograms/VulkanSDK/1.1.73.0/x86_64
VULKAN_SDK_PATH_INCLUDE = /usr/include/vulkan
VULKAN_SDK_PATH_LIB = /usr/lib
VULKAN_SDK_PATH_LAYER = /usr/share/vulkan/explicit_layer.d

# The name of the main file and executable
mainFileName = main
# Files that have .h and .cpp versions
classFiles =
# Files that only have the .h version
justHeaderFiles = HmlResourceManager HmlRenderer HmlModel HmlCommands HmlPipeline HmlWindow HmlDevice HmlSwapchain Himmel
# Compilation flags
OPTIMIZATION_FLAG = -O0
LANGUAGE_LEVEL = -std=c++20
COMPILER_FLAGS = -Wall -Wextra -Wno-unused-parameter -Wpedantic -I$(VULKAN_SDK_PATH_INCLUDE)
LINKER_FLAGS = -L$(VULKAN_SDK_PATH_LIB) `pkg-config --static --libs glfw3` -lvulkan


# Auxiliary
filesObj = $(addsuffix .o, $(mainFileName) $(classFiles))
filesH = $(addsuffix .h, $(classFiles) $(justHeaderFiles))


all: cleanExe $(mainFileName)


run: all compileShaders
	LD_LIBRARY_PATH=$(VULKAN_SDK_PATH_LIB) VK_LAYER_PATH=$(VULKAN_SDK_PATH_LAYER) ./main


# Compiler
%.o: %.cpp $(filesH)
	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $<

# Linker
$(mainFileName): $(filesObj)
	g++ $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) $^ -o $@ $(LINKER_FLAGS)


# Utils
clean:
	rm -f a.out *.o *.gch .*.gch $(mainFileName) *.spv

cleanExe:
	rm -f $(mainFileName)

compileShaders: shader.vert shader.frag
	glslc shader.vert -o vertex.spv
	glslc shader.frag -o fragment.spv
