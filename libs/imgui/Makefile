COMPILER = g++
OPTIMIZATION_FLAG = -O0
LANGUAGE_LEVEL = -std=c++20
# COMPILER_FLAGS = -Wall -Wextra -Wno-unused-parameter -Wpedantic -I$(VULKAN_SDK_PATH_INCLUDE) -fsanitize=address -g -fno-omit-frame-pointer
COMPILER_FLAGS = -Wall -Wextra -Wno-unused-parameter -Wpedantic -I$(VULKAN_SDK_PATH_INCLUDE)

classFiles = imgui imgui_impl_glfw imgui_draw imgui_widgets imgui_tables

filesObj = $(addprefix ../../build/, $(addsuffix .o, $(classFiles)))


../../build/imgui.o: imgui.cpp imgui.h
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o $@

../../build/imgui_impl_glfw.o: imgui_impl_glfw.cpp imgui_impl_glfw.h
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o $@

../../build/imgui_draw.o: imgui_draw.cpp
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o $@

../../build/imgui_widgets.o: imgui_widgets.cpp
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o $@

../../build/imgui_tables.o: imgui_tables.cpp
	$(COMPILER) $(COMPILER_FLAGS) $(OPTIMIZATION_FLAG) $(LANGUAGE_LEVEL) -c $< -o $@


compileLibs: $(filesObj)

clean:
	rm -f a.out *.gch .*.gch
