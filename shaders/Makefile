shaderSrcs = $(wildcard *.frag) $(wildcard *.vert) $(wildcard *.geom) $(wildcard *.tesc) $(wildcard *.tese)

shaderSpvs = $(addprefix out/, $(addsuffix .spv, $(shaderSrcs)))

out/%.spv: %
	glslc $^ -o $@

compileShaders: $(shaderSpvs)

clean:
	rm -f out/*.spv
