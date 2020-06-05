#include <string>
#include <vector>

#include <octave/Cell.h>
#include <octave/defun-dld.h>
#include <octave/dir-ops.h>
#include <octave/file-ops.h>
#include <octave/file-stat.h>
#include <octave/oct-env.h>
#include <octave/ov.h>

#include "build_system.h"

DEFUN_DLD (octave2oct, args, nargout,
           R"helpstr( -*- texinfo -*-
@deftypefn  {} {} octave2oct (@var{name})
@deftypefnx  {} {} octave2oct (@var{name}, OptionName, OptionValue)

Compile octave function @var{name} to .oct file.

Any function name that can be called from the command-line including the name
of a function file or a name of a command-line function is accepted.
Octave instructions of the function and all of its dependencies are
translated to C++ and the C++ source is compiled to .oct file.
@var{name} can be a character string or a cell array of character strings.
If a cell array of strings is provided, for each name a .oct file is generated.
If an empty string is provided no .oct file is created. It can be
used in combination with 'upgrade' option.

The following pairs of options and values are accepted:

@table @code
@item 'outname'    :   same as @var{name} (default)

The name of the generated .oct file. By default it has the value of @var{name}.
It is recommended to use other names to avoid shadowing of other functions.
It can be a character string or a cell array of character strings. If @var{name}
is a cell array, 'outname' should also be a cell array.

@item 'outdir'     :   current folder (default)

The directory that the generated .oct file will be saved to. By default it is
the current folder (current working directory). It can be a character string or
a cell array of character strings.

@item 'mode'       :   'single' (default)| 'static' | 'dynamic'

The build system can operate in three modes:

@table @code
@item  single
In the 'single' mode the generated c++ code of a function
and all of its dependencies is placed in a single file.
The file then compiled to a .oct file. It is the default mode.

@item  static
In the 'static' mode each .m file is translated to a separate .cpp file.
The .cpp files are compiled to object modules. The compiled object files are
then combined and linked into a .oct file.

@item  dynamic
In the 'dynamic' mode each .m file is translated to a separate .cpp file.
The .cpp files are compiled to object modules. Each of object files is linked
as a separate shared library (.dll/.so/.dylib) and the final .oct file is
linked against those shared libraries.
@end table

@item cache

When 'static' or 'dynamic' are used as the build mode, a 'cache' directory name
should be provided. It can be used in the subsequent compilation tasks to speed up
the compilation time. Five sub-directories are created in the cache :
include, src, lib, bin and tmp. If the compilation is done in 'dynamic' mode the
'bin' sub-directory should be added to the system's 'PATH' ( Windows) or
'LD_LIBRARY_PATH' ( Linux )  or 'DYLD_LIBRARY_PATH' ( Mac OS ) environment variable
before the startup of Octave, otherwise a restart is required. The bin sub-directory
contains the required shared libraries that are used by the generated .oct file.

@item 'upgrade'    :   false (default) | true;

Used in combination with 'dynamic' mode to udgrade the cache because of any possible change
in the dependency of the previously created .oct files. The default value is false.

@item 'debug'      :   false (default) | true

By default debug symbols aren't preserved. Also the linked libraries are stripped.
It results in the smaller file size and usually faster compilation time.
The debug symbols can be preserved by setting the option to true.

@item 'verbose' :   false (default) | true

Prints process of compilation.

@item 'KeepSource' :   false (default) | true

When a .oct file is generated a .cc and a .o file are also created in the same directory
as the .oct file. By default they are deleted after the creation of .oct file.
If 'KeepSource' is set to true they aren't deleted.

@item CompilerOptions

The build system internally calls "mkoctfile". Additional options as a character
string can be set by 'CompilerOptions' to be provided to the compiler through mkoctfile.

@end table
Example usage:

@example
octave2oct ('my_function')
octave2oct ('my_function', 'KeepSource', true)
octave2oct ('my_function', 'outname', 'my_other_function')
octave2oct ('my_function', 'outname', 'my_other_function',...
            'outdir', 'D:\oct', 'mode', 'dynamic',...
            'cache', 'D:\cache', 'upgrade', true, 'verbose', true)
octave2oct ('', 'mode', 'dynamic', 'cache', 'D:\cache', 'upgrade', true)
@end example

Some usage tips:

* Handle to nested function is supported.

* Classdef constructors or functions in package folders that are resolved as the
dependencies of a function aren't compiled but they are called through the interpreter.

* The only supported classdef method call is dot call : "X.setColor('RED')" and
method dispatching "setColor(X,'RED')" isn't supported.

* Compilation of script files isn't supported.

* The name and symbol resolution is done at translation time so the workspace and scope
of a compiled function cannot be changed/queried dynamically. Because of that, if
the generated .oct file calls functions such as "eval", "evalin", "assignin", "who" ,
"whos", "exist" and "clear" that dynamically change / query the workspace, they are
evaluated in the workspace that the .oct file is called from. Moreover Adding a
path to Octave's path, loading packages and autoload functions and changing the
current folder via "cd" should be done before the compilation to help the compiler
to find and resolve the symbols correctly.

* The global and persistent variables are shared between .oct files that are generated
in the 'dynamic' mode while in the 'single' and 'static' mode each .oct file contains
all of its dependencies including the global and persistent variables and cannot access
the global and persistent variables of other .oct files.
@seealso{mkoctfile}
@end deftypefn)helpstr")
{
  int n = args.length ();

  std::string cache_dir;

  coder_compiler::build_mode mode = coder_compiler::bm_single;

  if (n < 1)
    error ("coder: input should be a name");

  octave_value names = args(0);

  octave_idx_type input_sz = 0;

  std::vector<std::string> sym_names;

  std::vector<std::string> out_names;

  std::vector<std::string> out_dirs;

  bool upgrade_cache = false;

  bool debug = false;

  bool keepcc = false;

  bool verbose = false;

  std::string compiler_options;

  if (names.isempty ())
    {

    }
  else if (names.iscell ())
    {
      Cell sym_list = names.cell_value ();

      input_sz = sym_list.numel ();

      for (octave_idx_type i = 0; i < input_sz; i++)
        {
          octave_value sym = sym_list (i);

          if (! sym.is_string () )
            error ("coder: cell entry {%lld} should contain a name", i+1);
        }

      sym_names.reserve (input_sz);

      for (octave_idx_type i = 0; i < input_sz; i++)
        {
          std::string sym = sym_list (i).string_value ();

          sym_names.push_back (sym);
        }
    }
  else if (names.is_string ())
    {
      input_sz = 1;
      sym_names.push_back (names.string_value ());
    }
  else
    {
      error ("coder: invalid input type");
    }

  if (n > 1)
    {
      if (n % 2 == 0)
        error ("coder: options should be pairs of names and values");

      for (int i = 1; i < n - 1; i+=2)
        {
          std::string option_name = args(i).xstring_value ("coder: required string as value of \"name\"");

          octave_value val = args(i+1);

          if (option_name == "mode")
            {
              std::string option_val = val.xstring_value ("coder: required string as value of \"mode\"");

              if (option_val == "single")
                {
                  mode = coder_compiler::bm_single;
                }
              else if (option_val == "static")
                {
                  mode = coder_compiler::bm_static;
                }
              else if (option_val == "dynamic")
                {
                  mode = coder_compiler::bm_dynamic;
                }
              else
                error ("coder: invalid build mode");
            }
          else if (option_name == "cache")
            {
              cache_dir = val.xstring_value ("coder: required string as value of \"cache\"");
            }
          else if (option_name == "outname" && ! names.isempty ())
            {
              if (val.iscell ())
                {
                  if (! names.iscell ())
                    {
                      error ("coder: The option \"outname\" provided as cell while input name is not a cell");
                    }

                  Cell outname = val.cell_value ();

                  if (outname.numel () != input_sz)
                    {
                      error ("coder: The option \"outname\" provided as cell while its size isn't eqaul to the size of input");
                    }

                  out_names.reserve (input_sz);

                  for (octave_idx_type j = 0 ; j < input_sz; j++)
                    {
                      out_names.push_back (outname(j).xstring_value ("coder: outname isn't a string cell"));
                    }
                }
              else if (val.is_string ())
                {
                  if (names.iscell ())
                    {
                      error ("coder: The option \"outname\" provided as string while input name is a cell");
                    }

                  out_names.push_back ( val.xstring_value ("coder: required string as option value of \"outname\""));
                }
            }
          else if (option_name == "outdir" && ! names.isempty ())
            {
              if (val.iscell ())
                {
                  if (! names.iscell ())
                    {
                      error ("coder: The option \"outdir\" provided as cell while input name is not a cell");
                    }

                  Cell outdir = val.cell_value ();

                  if (outdir.numel () != input_sz)
                    {
                      error ("coder: The option \"outdir\" provided as cell while its size isn't eqaul to the size of input");
                    }

                  out_dirs.reserve (input_sz);

                  for (octave_idx_type j = 0 ; j < input_sz; j++)
                    out_dirs.push_back (outdir(j).xstring_value ("coder: outdir isn't a string cell"));
                }
              else if (val.is_string ())
                {
                    {
                      out_dirs = std::vector<std::string> (input_sz, val.string_value ());
                    }
                }
            }
          else if (option_name == "upgrade")
            {
              upgrade_cache = val.xbool_value ("coder: required bool as value of \"upgrade\"");
            }
          else if (option_name == "debug")
            {
              debug = val.xbool_value ("coder: required bool as value of \"debug\"");
            }
          else if (option_name == "verbose")
            {
              verbose = val.xbool_value ("coder: required bool as value of \"verbose\"");
            }
          else if (option_name == "KeepSource")
            {
              keepcc = val.xbool_value ("coder: required bool as value of \"KeepSource\"");
            }
          else if (option_name == "CompilerOptions")
            {
              compiler_options = val.xstring_value ("coder: required string as value of \"CompilerOptions\"");
            }
          else
            error ("coder: invalid option name %s", option_name.c_str ());
        }
    }

  if (out_names.empty () && !sym_names.empty ())
    {
      out_names = sym_names;
    }

  if (out_dirs.empty () && !sym_names.empty ())
    {
      out_dirs = std::vector<std::string>(
        sym_names.size (),
        octave::sys::env::make_absolute (octave::sys::env::get_current_directory ())
      );
    }

  if (mode == coder_compiler::bm_static || mode == coder_compiler::bm_dynamic)
    {
      if (cache_dir.empty ())
        error ("coder: for static or dynamic build modes cache directory should be specified");

      cache_dir = octave::sys::file_ops::tilde_expand (cache_dir);

      octave::sys::file_stat fs (cache_dir);

      if (!(fs && fs.exists () && fs.is_dir ()))
        {
          std::string msg;

          int status = octave::sys::mkdir (cache_dir, 0777, msg);

          if (status < 0)
            error("coder: failed to create the cache - %s", msg.c_str());
        }
    }

  coder_compiler::build_system  bsys (mode, cache_dir);

  bsys.run (
    sym_names,
    out_names,
    out_dirs,
    upgrade_cache,
    debug,
    keepcc,
    verbose,
    compiler_options);

  return ovl ();
}
