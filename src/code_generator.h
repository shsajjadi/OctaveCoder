#pragma once

#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <ostream>
#include <queue>
#include <sstream>
#include <streambuf>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <octave/pt-all.h>
#include <octave/pt-walk.h>
#include <octave/version.h>

class octave_user_function;

class octave_value;

namespace coder_compiler
{
  enum  loop_or_unwind_state
  {
    normal_context,
    looping_context,
    unwinding_context
  };

  class IndentingOStreambuf : public std::streambuf
  {
  //https://stackoverflow.com/a/9600752/6579744
  protected:

    virtual int overflow( int ch );

  public:

    explicit IndentingOStreambuf(std::ostream& dest, int indentsz = 2 )
    : myDest( dest.rdbuf () )
    , myIsAtStartOfLine( true )
    , indentSz( indentsz )
    , myIndent( )
    , myOwner( &dest )
    {
      myOwner->rdbuf( this );
    }

    void release ();

    void reset ();

    void inc ();

    void dec ();

    virtual ~IndentingOStreambuf();

    std::streambuf*  myDest;

    bool             myIsAtStartOfLine;

    int              indentSz;

    std::string      myIndent;

    std::ostream*    myOwner;
  };

  struct coder_file;

  using coder_file_ptr = std::shared_ptr<coder_file>;

  using dgraph = std::unordered_map<coder_file_ptr,std::unordered_set<coder_file_ptr>>;

  struct symscope;

  using symscope_ptr = std::shared_ptr<symscope>;

  std::string
  capitalize (const std::string& str);

  std::string
  lowercase (const std::string& str);

  void
  reformat_scalar (std::ostream& os, const std::string& source, bool is_complex = false);

  void
  print_value(std::ostream& os, const octave_value& m_value);

  enum generation_mode
  {
    gm_compact,
    gm_full
  };

  class code_generator: public octave::tree_walker
  {
  public:

    explicit code_generator( const coder_file_ptr& file, const dgraph& dependency_graph, std::iostream& header, std::iostream& source, std::iostream& partial_source, generation_mode mode);

    void
    increment_indent_level (std::ostream& st);

    void
    decrement_indent_level (std::ostream& st);

    void
    release (std::ostream& st);

    void
    visit_anon_fcn_handle (octave::tree_anon_fcn_handle& anon_fh);

    void
    visit_argument_list (octave::tree_argument_list& lst);

    void
    visit_binary_expression (octave::tree_binary_expression& expr);

    void
    visit_boolean_expression (octave::tree_boolean_expression& expr);

    void
    visit_compound_binary_expression (octave::tree_compound_binary_expression& expr);

    void
    visit_break_command (octave::tree_break_command&);

    void
    visit_colon_expression (octave::tree_colon_expression& expr);

    void
    visit_continue_command (octave::tree_continue_command&);

    void
    visit_decl_command (octave::tree_decl_command& cmd);

    void
    visit_decl_init_list (octave::tree_decl_init_list& lst);

    void
    visit_decl_elt (octave::tree_decl_elt& elt);

    void
    visit_simple_for_command (octave::tree_simple_for_command& cmd);

    void
    visit_complex_for_command (octave::tree_complex_for_command& cmd);
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
    void
    visit_octave_user_script (octave_user_script&);

    void
    visit_octave_user_function (octave_user_function& fcn);

    void
    visit_octave_user_function_header (octave_user_function& fcn);

    void
    visit_octave_user_function_trailer (octave_user_function& fcn);

    void
    visit_function_def (octave::tree_function_def&){}

    void
    visit_identifier (octave::tree_identifier&  id );

    void
    visit_if_clause (octave::tree_if_clause& cmd);

    void
    visit_if_command (octave::tree_if_command& cmd);

    void
    visit_if_command_list (octave::tree_if_command_list& lst);

    bool
    includes_magic_end (octave::tree_argument_list& arg_list) const;

    void
    visit_index_expression (octave::tree_index_expression& expr);

    void
    visit_matrix (octave::tree_matrix& lst);

    void
    visit_cell (octave::tree_cell& lst);

    void
    visit_multi_assignment (octave::tree_multi_assignment& expr);

    void
    visit_no_op_command (octave::tree_no_op_command&){}

    void
    visit_constant (octave::tree_constant& val);

    bool
    visit_dot_separated_fcn_handle (octave::tree_fcn_handle& expr);

    void
    visit_fcn_handle (octave::tree_fcn_handle&  fh );
#if OCTAVE_MAJOR_VERSION < 6
    void
    visit_funcall (octave::tree_funcall& /* fc */)
    { }
#endif
    void
    visit_parameter_list (octave::tree_parameter_list& lst);

    void
    return_list_from_param_list(octave::tree_parameter_list& lst);

    void
    visit_postfix_expression (octave::tree_postfix_expression& expr);

    void
    visit_prefix_expression (octave::tree_prefix_expression& expr);

    void
    visit_return_command (octave::tree_return_command&);
#if OCTAVE_MAJOR_VERSION < 6
    void
    visit_return_list (octave::tree_return_list& lst){}
#endif
    void
    visit_simple_assignment (octave::tree_simple_assignment& expr);

    std::string
    ambiguity_check(octave::tree_expression &expr);

    void
    visit_statement (octave::tree_statement& stmt);

    void
    visit_statement_list (octave::tree_statement_list& lst);

    void
    visit_switch_case (octave::tree_switch_case& cs);

    void
    visit_switch_case_list (octave::tree_switch_case_list& lst);

    void
    visit_switch_command (octave::tree_switch_command& cmd);

    void
    visit_try_catch_command (octave::tree_try_catch_command& cmd);

    void
    visit_unwind_protect_command (octave::tree_unwind_protect_command& cmd);

    void
    visit_while_command (octave::tree_while_command& cmd);

    void
    visit_do_until_command (octave::tree_do_until_command& cmd);
#if OCTAVE_MAJOR_VERSION >= 8
    void
    visit_superclass_ref(octave::tree_superclass_ref&){}

    void
    visit_metaclass_query(octave::tree_metaclass_query&){}

    void
    visit_classdef_attribute(octave::tree_classdef_attribute&){}

    void
    visit_classdef_attribute_list(octave::tree_classdef_attribute_list&){}

    void
    visit_classdef_superclass(octave::tree_classdef_superclass&){}

    void
    visit_classdef_superclass_list(octave::tree_classdef_superclass_list&){}

    void
    visit_classdef_property(octave::tree_classdef_property&){}

    void
    visit_classdef_property_list(octave::tree_classdef_property_list&){}

    void
    visit_classdef_properties_block(octave::tree_classdef_properties_block&){}

    void
    visit_classdef_methods_list(octave::tree_classdef_methods_list&){}

    void
    visit_classdef_methods_block(octave::tree_classdef_methods_block&){}

    void
    visit_classdef_event(octave::tree_classdef_event&){}

    void
    visit_classdef_events_list(octave::tree_classdef_events_list&){}

    void
    visit_classdef_events_block(octave::tree_classdef_events_block&){}

    void
    visit_classdef_enum(octave::tree_classdef_enum&){}

    void
    visit_classdef_enum_list(octave::tree_classdef_enum_list&){}

    void
    visit_classdef_enum_block(octave::tree_classdef_enum_block&){}

    void
    visit_classdef_body(octave::tree_classdef_body&){}

    void
    visit_classdef(octave::tree_classdef&){}
#endif
    std::string
    mangle (const std::string& str);

    void
    declare_and_define_variables();

    void
    declare_and_define_nested_variables();

    void
    declare_and_define_handle_variables();

    void
    define_nested_functions(octave_user_function& fcnn);

    std::string
    init_persistent_variables() ;

    void
    declare_persistent_variables();

    void
    reset();

    void
    generate_header( );

    void
    generate_partial_source();

    void
    generate_full_source( );

    bool
    has_nested_function(octave_user_function& ufcn);

    void
    generate_builtin_header();

    void
    generate_builtin_source();

    void
    generate(bool iscyclic=false);

  private:

    coder_file_ptr m_file;

    const dgraph& dependency_graph;

    std::deque<       // local functions
      std::vector<     // vector of two elements 1-anon_fcn_handles 2-nested functions
        std::deque<symscope_ptr>>> traversed_scopes;

    std::vector<loop_or_unwind_state> loop_or_unwind;

    int nconst;

    std::map<std::string , int> constant_map;

    std::iostream& os_hdr_ext;

    std::iostream& os_src_ext;

    std::iostream& os_prt_ext;

    std::stringstream os_hdr;

    std::stringstream os_src;

    std::stringstream os_prt;

    generation_mode mode;

    std::map <std::ostream*, IndentingOStreambuf> streams;

    int nesting_context;

    std::vector<std::string> nested_fcn_names;

    std::vector<symscope_ptr > fcn_scopes;
  };
}
