#include <cstring>
#include <algorithm>
#include <string>

#include <octave/load-save.h>
#include <octave/ls-mat4.h>
#include <octave/ls-mat5.h>
#include <octave/mach-info.h>
#include <octave/oct-time.h>
#include <octave/ov.h>
#include <octave/version.h>

#include "octave_refactored.h"

namespace coder_compiler
{
  //for some unknown reasons it has been made private in 5.1
  static void write_header (std::ostream& os, const octave::load_save_format& fmt)
  { 
    switch (fmt.type ())
      {
      case octave::load_save_system::MAT5_BINARY:
      case octave::load_save_system::MAT7_BINARY:
        {
          char const *versionmagic;
          char headertext[128];
          octave::sys::gmtime now;

          // ISO 8601 format date
          const char *matlab_format = "MATLAB 5.0 MAT-file, written by Octave "
            OCTAVE_VERSION ", %Y-%m-%d %T UTC";
          std::string comment_string = now.strftime (matlab_format);

          size_t len = std::min (comment_string.length (), static_cast<size_t> (124));
          memset (headertext, ' ', 124);
          memcpy (headertext, comment_string.data (), len);

          // The first pair of bytes give the version of the MAT file
          // format.  The second pair of bytes form a magic number which
          // signals a MAT file.  MAT file data are always written in
          // native byte order.  The order of the bytes in the second
          // pair indicates whether the file was written by a big- or
          // little-endian machine.  However, the version number is
          // written in the *opposite* byte order from everything else!
          if (octave::mach_info::words_big_endian ())
            versionmagic = "\x01\x00\x4d\x49"; // this machine is big endian
          else
            versionmagic = "\x00\x01\x49\x4d"; // this machine is little endian

          memcpy (headertext+124, versionmagic, 4);
          os.write (headertext, 128);
        }

        break;
        
      default:
        break;
      }  
  }
  
  bool write_mat (std::ostream& os, const octave_value& val)
  {
    octave::load_save_format format = octave::load_save_system::MAT5_BINARY;
    
    write_header (os, format);
    
    return save_mat5_binary_element (os, val, "index", false, false, false, false);
  }
}