#include <ctime>
#include <iostream>

#include <octave/builtin-defun-decls.h>
#include <octave/dir-ops.h>
#include <octave/file-ops.h>
#include <octave/file-stat.h>
#include <octave/interpreter.h>
#include <octave/load-path.h>
#include <octave/oct-env.h>
#include <octave/ov-classdef.h>
#include <octave/ov-fcn.h>
#include <octave/ov-usr-fcn.h>

#include "coder_file.h"


namespace coder_compiler
{
  coder_file::coder_file(
    const std::string& name,
    int id ,
    const std::string& path ,
    time_t timestamp ,
    file_type type ,
    const octave_value& fcn 
  )
  :name(name), id(id), path(path), timestamp(timestamp),
  type(type), fcn(fcn),  local_functions()
  {
    if(type == file_type::m || type == file_type::cmdline)
      local_functions.emplace_back(name);
  }
    
	symtab& 
  coder_file::current_local_function() 
  {
		return local_functions.back();
	}
  
	void 
  coder_file::add_new_local_function(const std::string& name )
	{
		local_functions.emplace_back(name);
	}
  
  std::deque<std::vector<std::deque<symscope_ptr>>>
  coder_file::traverse()
  {
    std::deque<std::vector<std::deque<symscope_ptr>>> traverserd_scopes;

    for (const auto& table: local_functions)
      {
        traverserd_scopes.push_back(table.traverse());
      }
      
    return traverserd_scopes;
  }
  
  std::string 
  find_meta_path(const octave_value& meta, const std::string& nm)
  {
    std::string result;
      
    {
      octave::load_path& lpath = octave::interpreter::the_interpreter ()->get_load_path ();
      
      for (auto d: lpath.dir_list())
        {
          octave::sys::dir_entry dir (d);
          
          if (dir)
            {
              string_vector flist = dir.read ();
              
              octave_idx_type len = flist.numel ();
              
              for (octave_idx_type i = 0; i < len; i++)
                {
                  std::string fname = flist[i];

                  std::string full_name = octave::sys::file_ops::concat (d, fname);

                  octave::sys::file_stat fs (full_name);
                  
                  if (fs)
                    {
                      if (meta.is_package())
                        {
                          if (fs.is_dir ())
                            {
                              if (fname[0] == '+' && fname.substr (1) == nm)
                              {
                                cdef_manager& cdm = octave::interpreter::the_interpreter ()->get_cdef_manager ();
                                
                                auto cur_dir = ovl(octave::sys::env::get_current_directory ());
                                
                                Fcd(ovl(octave_value(d)));
                                
                                cdef_package pack = cdm.find_package (nm, false, true);	
                                
                                Fcd(cur_dir);
                                
                                if (pack.ok ())
                                  return d;
                              }
                            }
                        }
                      else // if it is a class
                        {
                          if (
                          (fs.is_dir () && (fname[0] == '@' && fname.substr (1) == nm)) 
                          ||
                          (!fs.is_dir () && fname == nm + ".m")
                          )
                            {
                              cdef_manager& cdm = octave::interpreter::the_interpreter ()->get_cdef_manager ();
                              
                              auto cur_dir = ovl(octave::sys::env::get_current_directory ());
                              
                              Fcd(ovl(octave_value(d)));
                              
                              cdef_class klass = cdm.find_class (nm, false, true);	
                              
                              Fcd(cur_dir);

                              if (klass.ok () )
                                return d;
                            }                   
                        }
                    }
                }
            }
        }        
    }
    
    return result;
  }	
  
  std::tuple<file_type ,std::string, std::string>
  find_file_type_name_and_path(const octave_value& val, const std::string& symbol_name)
  {
    std::string file_name,dir_name;
    
    file_type type = file_type::unknown;
    
    octave_function *fcn = nullptr;
    
    if(val.is_function())
      fcn = val.function_value ();
    
    if (val.is_user_function ())
      {
        octave_user_function * user_fcn =val.user_function_value();
        
        if (! user_fcn->is_subfunction() && ! user_fcn->is_nested_function())
          {
            if (fcn && ! (fcn->is_system_fcn_file() && (symbol_name == "narginchk" || symbol_name == "nargoutchk")))
              {
                std::string file_full_name
                  = octave::sys::env::make_absolute(octave::sys::file_ops::tilde_expand (fcn->fcn_file_name ()));
                
                if (file_full_name.empty())
                  {
                    type = file_type::cmdline;
                    
                    file_name = symbol_name;
                  }
                else
                  {
                    type = file_type::m;
                    
                    size_t pos
                      = file_full_name.find_last_of (octave::sys::file_ops::dir_sep_str ());

                    dir_name 
                      = file_full_name.substr (0, pos);
                      
                    file_name 
                      = file_full_name.substr (pos+1, file_full_name.length() - pos - 3);
                  }
              }
          }
      }
    else if ( val.is_builtin_function() )
      {
        // nargin, nargout and isargout are special functions
        // that are generated as macro
        if (symbol_name != "nargin" && symbol_name != "nargout" && symbol_name != "isargout")
          {
            file_name = "builtins";
            
            type = file_type::builtin;
          }
      }
    else if ( val.is_dld_function() )
      {
        if (fcn)
          {
            std::string file_full_name
              = octave::sys::env::make_absolute(octave::sys::file_ops::tilde_expand (fcn->fcn_file_name ()));

            size_t pos
              = file_full_name.find_last_of (octave::sys::file_ops::dir_sep_str ());

            dir_name 
              = file_full_name.substr (0, pos);
              
            file_name 
              = file_full_name.substr (pos+1, file_full_name.length() - pos - 5);
          }
      
        type = file_type::oct;
      }
    else if ( val.is_mex_function())
      {
        if (fcn)
          {
            std::string file_full_name
              = octave::sys::env::make_absolute(octave::sys::file_ops::tilde_expand (fcn->fcn_file_name ()));

            size_t pos
              = file_full_name.find_last_of (octave::sys::file_ops::dir_sep_str ());

            dir_name 
              = file_full_name.substr (0, pos);
              
            file_name 
              = file_full_name.substr (pos+1, file_full_name.length() - pos - 5);
          }
      
        type = file_type::mex;
      }
    else if ( val.is_classdef_meta())
      {
        if (val.is_package())
          {
            type = file_type::package;
          }
        else
          {
            type = file_type::classdef;
          }
          
        dir_name = octave::sys::env::make_absolute(octave::sys::file_ops::tilde_expand(find_meta_path(val,symbol_name)));
        
        file_name = symbol_name;
      }

    return {type, file_name, dir_name};
  }
}
