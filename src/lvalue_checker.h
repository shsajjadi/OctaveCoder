#pragma once

#include <string>

#include <octave/pt-all.h>

#include "coder_file.h"

class octave_value;

class octave_user_function;

class octave_user_script;

namespace coder_compiler
{
  struct coder_file;

  using coder_file_ptr = std::shared_ptr<coder_file>;

  class lvalue_checker : public octave::tree_walker
  {
  public:

    lvalue_checker(const coder_file_ptr& file, octave::tree_statement_list * list, const std::string& loop_var);

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
    visit_octave_user_function (octave_user_function&) {}

    void 
    visit_function_def (octave::tree_function_def&) {}
    
    void
    visit_anon_fcn_handle (octave::tree_anon_fcn_handle&  afh );

    void
    visit_identifier (octave::tree_identifier&) {}
    
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
    visit_fcn_handle (octave::tree_fcn_handle&) {}
    
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

  private:
    coder_file_ptr m_file;
    
    std::string m_loop_var;
  };
}
