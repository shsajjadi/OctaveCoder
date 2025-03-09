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

  options = { '-c', '-O2', '-g0' , ['-I' sourcedir]};

  version_num = str2num(strsplit(version, '.'){1});

  if version_num < 10
    stdflag = '-std=gnu++11';
  else
    ccnames = ...
      {
        'pt-walk'
      };

    parserdir = format_path(fullfile(sourcedir, 'parse-tree'));
    cc  = cellfun (@format_path, fullfile (parserdir , strcat (ccnames, '.cc')), 'Un', 0);
    objcc = cellfun (@format_path, fullfile (sourcedir , strcat (ccnames, '.o')), 'Un', 0);
    cpp = [cpp cc];
    obj = [obj objcc];
    options = [options, {['-I' parserdir]}];
    stdflag = '-std=gnu++17';
  end
  
  options = [{stdflag}, options];
  
  for k = 1:numel (cpp)
    mkoctfile ( options{:}, '-o', obj{k}, cpp{k});
  endfor

  mkoctfile ('-o', oct, obj {:});

endfunction

function retval = format_path (str)
  str(str == '\') = '/';
  retval = str;
endfunction
