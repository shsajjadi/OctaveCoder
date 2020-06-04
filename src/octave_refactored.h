#include <ostream>

class octave_value;

namespace coder_compiler
{
  bool write_mat (std::ostream& os, const octave_value& val);
}