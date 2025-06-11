function pre_install (in)
  if strcmp(version, '10.1.0')
    error ('Coder cannot be installed on Octave 10.1.0. Please use a newer or, if you prefer, an older Octave version.');
  endif

  disp ('Installing package coder. It may take a while. Please wait ...')
endfunction
