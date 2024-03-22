#pragma once

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <octave/oct-map.h>
#include <octave/pt-check.h>
#include <octave/pt-all.h>

#include "coder_file.h"
#include "coder_symtab.h"

class octave_value;

class octave_user_function;

class octave_user_script;

namespace coder_compiler
{
  struct coder_file;

  using coder_file_ptr = std::shared_ptr<coder_file>;

  using dgraph = std::unordered_map<coder_file_ptr,std::unordered_set<coder_file_ptr>>;

  class build_system;

  class file_time
  {
  public:
    file_time(const std::string& filename="");

    time_t mtime () const
    {
      return m_mtime;
    }

    bool is_newer (time_t t) const
    {
      return m_mtime > t;
    }

    operator bool ()
    {
      return  ok;
    }

  private:

    bool ok;
    time_t m_mtime;

  };

  class function_finder
  {
  public:
    function_finder (const octave::symbol_scope& scope);

    function_finder (const function_finder&) = delete;

    function_finder (function_finder&& f) : oldscope (f.oldscope), newscope (f.newscope)
    {
      f.oldscope = octave::symbol_scope {""};
      f.newscope = octave::symbol_scope {""};
    }

    octave_value find_function (const std::string& name);

    ~function_finder ();

  private:
    octave::symbol_scope oldscope;

    octave::symbol_scope newscope;
  };

  class scalar_map_view
  {
    octave_map* map;

    octave_idx_type idx;

  public :

    scalar_map_view (octave_idx_type  i, octave_map& m)
    : map(&m),idx(i)
    {}

    octave_value& contents (const std::string& key) const
    {
      return map->contents (key) (idx);
    }

    void setfield (const std::string& key, const octave_value& val) const
    {
      map->contents (key) (idx) = val;
    }
  };

  class scalar_map_const_view
  {
    const octave_map* map;

    octave_idx_type idx;

  public :

    scalar_map_const_view (octave_idx_type  i,const octave_map& m)
    : map(&m),idx(i)
    {}

    octave_value contents (const std::string& key) const
    {
      return map->contents (key) (idx);
    }
  };

  std::pair<octave_fields, octave_fields>
  make_schema ();

  class semantic_analyser : public octave::tree_walker
  {
  public:

    enum file_state
    {
      Old,
      New,
      Updated,
      DependencyUpdated
    };

    semantic_analyser(void);

    semantic_analyser(const octave_map& cache_index);

    void
    visit_octave_user_script (octave_user_script&){}

    void
    visit_argument_list (octave::tree_argument_list& lst);

    void
    visit_binary_expression (octave::tree_binary_expression&);

    void
    visit_boolean_expression (octave::tree_boolean_expression&);

    void
    visit_compound_binary_expression (octave::tree_compound_binary_expression&);

    void
    visit_break_command (octave::tree_break_command&);

    void
    visit_colon_expression (octave::tree_colon_expression&);

    void
    visit_continue_command (octave::tree_continue_command&);

    void
    visit_decl_command (octave::tree_decl_command&);

    void
    visit_simple_for_command (octave::tree_simple_for_command& cmd);

    void
    visit_complex_for_command (octave::tree_complex_for_command&);
#if OCTAVE_MAJOR_VERSION >= 7
    void
    visit_spmd_command (octave::tree_spmd_command&);

    void
    visit_arguments_block (octave::tree_arguments_block&);

    void
    visit_args_block_attribute_list (octave::tree_args_block_attribute_list&);

    void
    visit_args_block_validation_list (octave::tree_args_block_validation_list&);

    void
    visit_arg_validation (octave::tree_arg_validation&);

    void
    visit_arg_size_spec (octave::tree_arg_size_spec&);

    void
    visit_arg_validation_fcns (octave::tree_arg_validation_fcns&);
#endif
#if OCTAVE_MAJOR_VERSION < 6
    void
    visit_funcall (octave::tree_funcall& /* fc */)
    { }

    void
    visit_return_list (octave::tree_return_list& lst){}
    { }
#endif
    void
    visit_multi_assignment (octave::tree_multi_assignment&);

    void
    visit_no_op_command (octave::tree_no_op_command&) {}

    void
    visit_constant (octave::tree_constant&) {}

    void
    visit_index_expression (octave::tree_index_expression&);

    void
    visit_matrix (octave::tree_matrix&);

    void
    visit_cell (octave::tree_cell&);

    void
    visit_simple_assignment (octave::tree_simple_assignment& expr);

    void
    visit_statement (octave::tree_statement&);

    void
    visit_statement_list (octave::tree_statement_list&);

    void
    visit_try_catch_command (octave::tree_try_catch_command& cmd);

    void
    visit_unwind_protect_command (octave::tree_unwind_protect_command&);

    void
    visit_while_command (octave::tree_while_command&);

    void
    visit_do_until_command (octave::tree_do_until_command&);

    void
    visit_handle (octave::tree_anon_fcn_handle& anon_fh);

    void
    visit_octave_user_function (octave_user_function& fcnn);

    void
    visit_function_def (octave::tree_function_def&);

    void
    visit_anon_fcn_handle (octave::tree_anon_fcn_handle&  afh );

    void
    visit_identifier (octave::tree_identifier& expr);

    void
    visit_if_clause (octave::tree_if_clause&);

    void
    visit_if_command (octave::tree_if_command&);

    void
    visit_if_command_list (octave::tree_if_command_list&);

    void
    visit_switch_case (octave::tree_switch_case&);

    void
    visit_switch_case_list (octave::tree_switch_case_list&);

    void
    visit_switch_command (octave::tree_switch_command&);

    void
    visit_decl_elt (octave::tree_decl_elt& elt);

    void
    visit_decl_init_list (octave::tree_decl_init_list&);

    void
    visit_fcn_handle (octave::tree_fcn_handle& expr);

    void
    visit_parameter_list (octave::tree_parameter_list&);

    void
    visit_postfix_expression (octave::tree_postfix_expression&);

    void
    visit_prefix_expression (octave::tree_prefix_expression&);

    void
    visit_return_command (octave::tree_return_command&) {}

    void
    visit_superclass_ref (octave::tree_superclass_ref&) {}

    void
    visit_metaclass_query (octave::tree_metaclass_query&) {}

    void
    visit_classdef_attribute (octave::tree_classdef_attribute&) {}

    void
    visit_classdef_attribute_list (octave::tree_classdef_attribute_list&) {}

    void
    visit_classdef_superclass (octave::tree_classdef_superclass&) {}

    void
    visit_classdef_superclass_list (octave::tree_classdef_superclass_list&) {}

    void
    visit_classdef_property (octave::tree_classdef_property&) {}

    void
    visit_classdef_property_list (octave::tree_classdef_property_list&) {}

    void
    visit_classdef_properties_block (octave::tree_classdef_properties_block&) {}

    void
    visit_classdef_methods_list (octave::tree_classdef_methods_list&) {}

    void
    visit_classdef_methods_block (octave::tree_classdef_methods_block&) {}

    void
    visit_classdef_event (octave::tree_classdef_event&) {}

    void
    visit_classdef_events_list (octave::tree_classdef_events_list&) {}

    void
    visit_classdef_events_block (octave::tree_classdef_events_block&) {}

    void
    visit_classdef_enum (octave::tree_classdef_enum&) {}

    void
    visit_classdef_enum_list (octave::tree_classdef_enum_list&) {}

    void
    visit_classdef_enum_block (octave::tree_classdef_enum_block&) {}

    void
    visit_classdef_body (octave::tree_classdef_body&) {}

    void
    visit_classdef (octave::tree_classdef&) {}

    coder_file_ptr
    init_builtins();

    void
    resolve_cache_dependency(const coder_file_ptr& file);

    void
    update_globals( const std::string& sym_name);

    coder_file_ptr
    analyse(const std::string& name, const octave_value& fcn );

    coder_file_ptr
    add_fcn_to_task_queue (const coder_file_ptr& this_file, const std::string& sym_name, const octave_value& val);

    coder_symbol_ptr
    insert_symbol(const std::string& name);

    coder_symbol_ptr
    insert_symbol(const coder_symbol_ptr& sym, symbol_type type);

    coder_symbol_ptr
    insert_symbol(const std::string& name, const octave_value& fcn);

    coder_symbol_ptr
    insert_symbol(const std::string& name, symbol_type type);

    coder_symbol_ptr
    insert_formal_symbol(const std::string& name);

    coder_file_ptr
    current_file();

    const dgraph&
    dependency()  { return dependency_graph; }

    bool
    should_generate(const coder_file_ptr& file) const;

    void
    mark_as_generated(const coder_file_ptr& file);

    void
    write_dep(const coder_file_ptr& file , const scalar_map_view& file_attr) const;

    coder_file_ptr
    find_in_cache(const coder_file_ptr& file) const  ;

    coder_file_ptr
    read_dep(const scalar_map_const_view& file_attr);

    octave_map
    write_oct_list (const std::map<std::string, coder_file_ptr>& oct_map) const;

    std::map<std::string, coder_file_ptr>
    read_oct_list (const octave_map& dyn_oct_list) const;

    void
    build_dependency_graph (const octave_map& cache_index);

    octave_value find_function (const std::string& name);

    void find_resolvabale_names_in_octave_path ();

     void find_resolvabale_names (std::shared_ptr<std::set<std::string>> resolvable_names, const std::string& d, bool is_private = false);

    struct compare_file_ptr
    {
      bool operator() (const coder_file_ptr& lhs, const coder_file_ptr& rhs) const
      {
        return lhs->name < rhs->name;
      }
    };

  private:

    std::multiset<coder_file_ptr, compare_file_ptr> dependent_files;

    dgraph dependency_graph;

    std::list<coder_file_ptr> task_queue;

    std::list<coder_file_ptr>::iterator current_file_it;

    std::map<coder_file_ptr, bool> visited;

    std::map<coder_file_ptr, file_state> state;

    std::map<std::string, octave_value> local_functions;

    std::stack<std::vector<std::reference_wrapper<octave::tree_anon_fcn_handle>>> handles;

    std::map<coder_file_ptr, std::map<coder_file_ptr, std::set<std::string>>> external_symbols;

    std::map <std::string , coder_file_ptr> oct_list;

    std::stack<function_finder> scope_stack;

    std::map<std::string, std::set<coder_symbol_ptr, typename symscope::compare_symbol>> directory_table;

    std::set<coder_symbol_ptr, typename symscope::compare_symbol> *file_directory_it;

    std::map<std::string, std::shared_ptr<std::set<std::string>>> path_map;

    std::shared_ptr<std::set<std::string>> resolvable_path_names;

    std::shared_ptr<std::set<std::string>> current_path_map;
  };
}
