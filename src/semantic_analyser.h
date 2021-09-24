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

  class semantic_analyser : public octave::tree_checker
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
    visit_octave_user_script (octave_user_script& fcn){}

    void
    visit_argument_list (octave::tree_argument_list& lst);

    void
    visit_simple_for_command (octave::tree_simple_for_command& cmd);

    void
    visit_index_expression (octave::tree_index_expression&);

    void
    visit_simple_assignment (octave::tree_simple_assignment& expr);

    void
    visit_try_catch_command (octave::tree_try_catch_command& cmd);

    void
    visit_handle (octave::tree_anon_fcn_handle& anon_fh);

    void
    visit_octave_user_function (octave_user_function& fcnn);

    void
    visit_anon_fcn_handle (octave::tree_anon_fcn_handle&  afh );

    void
    visit_identifier (octave::tree_identifier& expr);

    void
    visit_decl_elt (octave::tree_decl_elt& elt);

    void
    visit_fcn_handle (octave::tree_fcn_handle& expr);

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

    struct compare_file_ptr
    {
      bool operator() (const coder_file_ptr& lhs, const coder_file_ptr& rhs) const
      {
        return lhs->name < rhs->name;
      }
    };

  private:

    bool m_do_lvalue_check;

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
  };
}
