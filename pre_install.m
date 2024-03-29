function pre_install (in)

  disp ('Installing package coder. It may take a while. Please wait ...')
  basedir = fileparts(mfilename ('fullpath'));
  sourcedir = format_path(fullfile(basedir, 'src'));

  cppnames = ...
    {
      'dgraph'
      'coder_file'
      'coder_symtab'
      'lvalue_checker'
      'semantic_analyser'
      'code_generator'
      'coder_runtime'
      'build_system'
      'octave2oct'
    };

  cpp = cellfun (@format_path, fullfile (sourcedir , strcat (cppnames, '.cpp')), 'Un', 0);
  obj = cellfun (@format_path, fullfile (sourcedir , strcat (cppnames, '.o')), 'Un', 0);
  oct = format_path (fullfile (sourcedir , 'octave2oct.oct'));

  for k = 1:numel (cpp)
    mkoctfile ( '-c', '-O2', '-std=gnu++11', '-g0' , ['-I' sourcedir], '-o', obj{k}, cpp{k});
  endfor

  mkoctfile ('-o', oct, obj {:});

endfunction

function retval = format_path (str)
  str(str == '\') = '/';
  retval = str;
endfunction
