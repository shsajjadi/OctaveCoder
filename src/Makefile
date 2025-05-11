all: octave2oct.oct

MKOCTFILE ?= mkoctfile

OBJECTS := dgraph.o coder_file.o coder_symtab.o lvalue_checker.o semantic_analyser.o code_generator.o coder_runtime.o build_system.o

%.o: %.cpp %.h
	$(MKOCTFILE) -c $<

octave2oct.oct: octave2oct.cpp $(OBJECTS)
	$(MKOCTFILE) $^ -o $@

clean:
	rm -f *.o *.oct
