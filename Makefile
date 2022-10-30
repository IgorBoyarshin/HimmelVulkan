run: $(mainFileName)
	$(MAKE) -C libs/imgui compileLibs
	$(MAKE) -C shaders compileShaders
	$(MAKE) -C src compileCode
	$(MAKE) -C src run

clean:
	$(MAKE) -C libs/imgui clean
	$(MAKE) -C shaders clean
	$(MAKE) -C src clean
