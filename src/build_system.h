#pragma once

#include <map>
#include <string>
#include <unordered_set>
#include <vector>

#include <octave/oct-map.h>

#include "coder_file.h"
#include "semantic_analyser.h"

class octave_value;

namespace coder_compiler
{
  enum build_mode
  {
    bm_single,
    bm_static,
    bm_dynamic
  };
  
  class build_system
  {
  public:

    build_system(     
      build_mode mode = bm_single,
      const std::string& cache_dir = ""
    );
        
    void
    init_builtins();
    
    coder_file_ptr
    add_fcn_to_task_queue ( const std::string& sym_name, const octave_value& val);

    void 
    resolve_catch_dependency(const coder_file_ptr& file);
    
    void 
    upgrade_cache ();
    
    void
    run (
      const std::vector<std::string>& sym_names,
      const std::vector<std::string>& out_names,
      const std::vector<std::string>& out_dirs,
      bool upgrade_cache,
      bool debug,
      bool keepcc,
      bool verbose,
      const std::string& compiler_options
    );
    
    
    void
    build( 
      const coder_file_ptr& start_node,
      const std::string& sym_name,
      bool mkoct,
      bool mkoct_bridge,
      const std::string& out_name,
      const std::string& out_dir,
      bool debug,
      bool keepcc,
      bool verbose,
      const std::string& compiler_options
    );

    void 
    update_cache_index ();  
    
    void 
    update_cache_index (const std::map<std::string, coder_file_ptr>& oct_map);
    
    bool write_cache_index () const;
    
    void read_cache_index ();
    
  private:
  
    semantic_analyser analyser;
    
    bool cache_updated;
    
    std::unordered_set<coder_file_ptr> new_or_updated_files;
    
    octave_map cache_index;
    
    octave_map dyn_oct_list;

    std::string cache_directory;

    build_mode mode;
  };
}
