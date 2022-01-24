shaderSrcs = $(wildcard *.frag) $(wildcard *.vert) $(wildcard *.geom) $(wildcard *.tesc) $(wildcard *.tese)

shaderSpvs = $(addsuffix .spv, $(shaderSrcs))

%.spv: %
	glslc $^ -o out/$@

compileShaders: $(shaderSpvs)

clean:
	rm -f out/*.spv
