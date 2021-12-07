# Octave Coder

Octave Coder is a code generator and build system that, given a function name translates the function and all of its dependencies to C++ and builds a .oct shared module.
  
### Simple usage:

    octave2oct('myfunction');

All versions of GNU Octave starting from 4.4.0 are supported. Coder supports compilation of .m function files and command-line functions. Script files aren't supported. Classdef classes and the functions contained in the package folders are supported through the interpreter so the generated .oct files are just wrappers. Classdef method dispatching "X = setColor(X,'red');" isn't supported and the only supported classdef method call is calling via dot notation "X = X.setColor('red')". Handle to nested function is also supported. 

The name and symbol resolution is done at translation time so the workspace and scope of a compiled function cannot be changed/queried dynamically. Because of that, if a compiled .oct file calls functions such as "eval", "evalin", "assignin", "who" , "whos", "exist" and "clear" that dynamically change / query the workspace, they are evaluated in the workspace that the generated .oct file is called from. Moreover Adding a path to Octave's path, loading packages and autoload functions and changing the current folder via "cd" should be done before the start of the compilation. Doing so helps compiler to correctly find and resolve symbols.

### How does it work?
Octave instructions, are translated to the intermediate Coder C++ API. The intermediate API as its backend uses the high level oct API and links against Octave core libraries. Names and symbols are resolved at translation time to get rid of symbol table lookup at the runtime and there is no AST traversal so the generated .oct files are supposed to run faster than the original .m files. Speed-up is usually 3X - 4X relative to the interpreter.

### Build system
Coder's build system supports three modes of building: single, static and dynamic. In the "single" mode the generated c++ code of a function and all of its dependencies are combined in a single file. The file then compiled to a .oct file. In the "static" and "dynamic" modes each .m file is translated to a separate .cpp file. The .cpp files are compiled to separate object modules. In the "static" mode the compiled object files are combined and linked into a .oct file but in the "dynamic" mode each object file is linked as a separate shared library (.dll/.so/.dylib) and the final .oct file is linked against those shared libraries.

The default build mode is the "single" mode. However sometimes because of the large number of dependencies the generated source in "single" mode may contain thousands lines of code that increases the compilation time. In the cases that the compiler is frequently used it is preferable to use "dynamic" and or "static" modes because all of the generated intermediate files are placed in a cache to be used in the later compilation tasks. Moreover there are cases that a user compiles a file and after that they want to modify the original .m file or they want to shadow a function with a new function that has the same name. The build system can handle such changes. In each compilation task all dependencies of the currently compiled .oct file are upgraded and cached but the dependency of the already compiled .oct files in the "static" mode aren't upgraded so they should be recompiled. But in the "dynamic" mode there is an option to upgrade the dependency of the already compiled .oct files.

There is also a difference in handling of `global` and `persistent` variables. `global` and `persistent` variables are shared between .oct files that are generated in the "dynamic" mode while in the "single" and "static" mode each .oct file contains all of its dependencies including the global and persistent variables and cannot access the global and persistent variables related to other .oct files. 

### Development style
Previously new branches were created per each Octave release but currently there is a main development line that works on all Octave official releases starting from Octave 4.4.0 . When a new Octave version say 6.1 is released it brings two types of features, refactoring or fixes. The first type is mostly related to evaluation of expressions. Those fixes when are also fixed in Coder results in the improvement of Coder for previous Octave versions. So the (correct) result of an expression in the new Coder release that is installed on Octave 5.2 may be different than the erroneous result of the same expression when evaluated on the interpreter of Octave 5.2. The second types of features and fixes cannot be applied to previous versions so #if macro is used to conditionally include those features. They will be specific to the new Octave version and possibly the later Octave releases.

### Installing

Similar to other Octave packages download the latest *.tar.gz released package and in Octave use `pkg` command to install and load the package:

    > pkg install coder-4.4.0.tar.gz
    > pkg load coder

### License

Coder is released under GNU GPL v3.0. The generated sources are treated as data files but since they are linked against octave libraries they will get GNU GPL license.

### Usage

    octave2oct (NAME)
    octave2oct (____, OptionName, OptionValue)

Compile NAME to .oct file. 

NAME

Any function name that can be called from the command-line including the name of a function file or a name of a command-line function.
Octave instructions of the function and all of its dependencies are translated to C++ and the C++ source is compiled to .oct file.  NAME can be a character string or a cell array of character strings. If a cell array of strings is provided, for each name a .oct file is generated. If an empty string is provided no .oct file is created. It can be used in combination with "upgrade" option.

The following pairs of options and values are accepted:

- 'outname'    :   same as NAME (default)

The name of the generated .oct file. By default it gets the value of NAME. It is recommended to use other names to avoid shadowing of functions. It can be a character string or a cell array of character strings. If NAME is a cell array, 'outname' should also be a cell array.

- 'outdir'     :   current folder (default)

The directory that the generated .oct file will be saved to. By default it is the current folder (current working directory). It can be a character string or a cell array of character strings.

- 'mode'       :   'single' (default)| 'static' | 'dynamic'

The build system can operate in three modes:

1. 'single'
In the "single" mode the generated c++ code of a function and all of its dependencies are combined in a single file. The file then compiled to a .oct file. It is the default mode.

2. 'static'
In the "static" each .m file is translated to a separate .cpp file. The .cpp files are compiled to separate object modules. The compiled object files are then combined and linked into a .oct file.

3. 'dynamic'
In the "dynamic" mode each .m file is translated to a separate .cpp file. The .cpp files are compiled to separate object modules. Each object file is linked as a separate shared library (.dll/.so/.dylib) and the final .oct file is linked against those shared libraries.


- 'cache'   

When 'static' or 'dynamic' are used as the build mode, a 'cache' directory name should be provided. It can be used in the subsequent compilation tasks to speed up the compilation time. Five sub-directories are created in the cache : include, src, lib, bin and tmp. If the compilation is done in 'dynamic' mode the 'bin' sub-directory should be added to the system's 'PATH' ( Windows) or 'LD_LIBRARY_PATH'  ( Linux )  or 'DYLD_LIBRARY_PATH' ( Mac OS ) environment variable before the startup of Octave, otherwise a restart is required. The bin sub-directory contains the required shared libraries that are used by the generated .oct file.

- 'upgrade'    :   false (default) | true;

Used in combination with 'dynamic' mode to upgrade the cache for any possible change in the dependency of the previously created .oct files. The default value is false.

- 'KeepSource' :   false (default) | true

When a .oct file is generated a .cc and a .o file are also created in the same directory as the .oct file. By default they are deleted after the creation of .oct file. If 'KeepSource' is set to true they aren't deleted.

- 'debug'      :   false (default) | true

By default the debug symbols aren't preserved. Also the linked libraries are stripped (gcc -g and -s options are used). It results in the smaller file size and usually faster compilation time. Debug symbols can be preserved by setting the option to true.

- 'verbose' :   false (default) | true

Prints process of compilation.

- 'CompilerOptions' 

The build system internally calls the "mkoctfile" command. Additional options as a character string can be set by 'CompilerOptions' to be provided to the compiler through mkoctfile.

### Known issues

- .m files that contain call to functions like 'eval' and 'clear' are not suppoed to work when compiled to .oct file
- Always call the generated oct files form a .m function file. It prevent runtime errors related to functions that contain mlock and munlock.
 
### TODO

- Shared memory parallelism using parfor
- Using other backends like plain c or Eigen library instead of .oct API
