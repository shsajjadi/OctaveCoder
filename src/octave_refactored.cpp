#include <octave/load-save.h>
#include <octave/ls-mat5.h>
#include <octave/ov.h>

#include "octave_refactored.h"

namespace coder_compiler
{
  bool write_mat (std::ostream& os, const octave_value& val)
  {
    load_save_format format = LS_MAT5_BINARY;

    write_header (os, format);

    return save_mat5_binary_element (os, val, "index", false, false, false, false);
  }
}