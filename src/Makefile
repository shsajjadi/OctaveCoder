all: octave2oct.oct

MKOCTFILE ?= mkoctfile

OBJECTS := dgraph.o coder_file.o coder_symtab.o lvalue_checker.o semantic_analyser.o code_generator.o coder_runtime.o build_system.o octave2oct.o

%.o: %.cpp
	$(MKOCTFILE) -g0 -c $^

octave2oct.oct: $(OBJECTS)
	$(MKOCTFILE) $^ -o $@

clean:
	rm -f *.o *.oct
