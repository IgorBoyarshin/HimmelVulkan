run: $(mainFileName)
	$(MAKE) -C shaders compileShaders
	$(MAKE) -C src compileCode
	$(MAKE) -C src run

clean:
	$(MAKE) -C shaders clean
	$(MAKE) -C src clean
