#pragma once

#include <deque>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include <octave/ov.h>
#include <octave/ovl.h>

#include "coder_symtab.h"

namespace coder_compiler
{
  using symscope_ptr = std::shared_ptr<symscope>;

  enum class file_type
  {
    m,
    oct,
    mex,
    global,
    builtin,
    cmdline,
    package ,
    classdef,
    unknown
  };

  void change_directory (octave::interpreter&, const octave_value_list& = octave_value_list (), int = 0);

  struct coder_file
  {
    coder_file(
      const std::string& name = "",
      int id = 1,
      const std::string& path = "",
      time_t timestamp = 0,
      file_type type = file_type::unknown,
      const octave_value& fcn = octave_value()
    );

    coder_file(const coder_file&)=delete;

    symtab&
    current_local_function();

    void
    add_new_local_function(const std::string& name = "");

    std::deque<std::vector<std::deque<symscope_ptr>>>
    traverse();

    std::string name;

    int id;

    std::string path;

    time_t timestamp;

    file_type type;

    octave_value fcn;

    std::deque<symtab> local_functions;
  };

  std::string
  find_meta_path(const octave_value& meta, const std::string& nm, const std::string& lookup_path);

  std::tuple<file_type ,std::string, std::string>
  find_file_type_name_and_path(const octave_value& val, const std::string& symbol_name, const std::string& lookup_path);
}