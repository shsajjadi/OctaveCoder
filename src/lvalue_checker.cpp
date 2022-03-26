#include <octave/oct.h>
#include <octave/oct-env.h>
#include <octave/ov-usr-fcn.h>
#include <octave/version.h>

#include "lvalue_checker.h"

namespace coder_compiler
{
  lvalue_checker::lvalue_checker (const coder_file_ptr& file, octave::tree_statement_list * list, const std::string& loop_var)
  : m_file(file)
  , m_loop_var (loop_var)
  {
    list->accept(*this);
  }

  void
  lvalue_checker::visit_argument_list (octave::tree_argument_list& lst)
  {
    auto p = lst.begin ();

    while (p != lst.end ())
      {
        octave::tree_expression *elt = *p++;

        if (elt)
          {
            elt->accept (*this);
          }
      }
  }

  void
  lvalue_checker::visit_binary_expression (octave::tree_binary_expression& expr)
  {
    octave::tree_expression *op1 = expr.lhs ();

    if (op1)
      op1->accept (*this);

    octave::tree_expression *op2 = expr.rhs ();

    if (op2)
      op2->accept (*this);
  }

  void
  lvalue_checker::visit_boolean_expression (octave::tree_boolean_expression& expr)
  {
    visit_binary_expression (expr);
  }

  void
  lvalue_checker::visit_compound_binary_expression (octave::tree_compound_binary_expression& expr)
  {
    visit_binary_expression (expr);
  }

  void
  lvalue_checker::visit_break_command (octave::tree_break_command&)
  {
    // Nothing to do.
  }

  void
  lvalue_checker::visit_colon_expression (octave::tree_colon_expression& expr)
  {
    octave::tree_expression *op1 = expr.base ();

    if (op1)
      op1->accept (*this);

    octave::tree_expression *op3 = expr.increment ();

    if (op3)
      op3->accept (*this);

    octave::tree_expression *op2 = expr.limit ();

    if (op2)
      op2->accept (*this);
  }

  void
  lvalue_checker::visit_continue_command (octave::tree_continue_command&)
  {

  }

  void
  lvalue_checker::visit_decl_command (octave::tree_decl_command& cmd)
  {
    octave::tree_decl_init_list *init_list = cmd.initializer_list ();

    if (init_list)
      init_list->accept (*this);
  }

  void lvalue_checker::visit_decl_init_list (octave::tree_decl_init_list& lst)
  {
    // FIXME: tree_decl_elt is not derived from tree, so should it
    // really have an accept method?

    for (octave::tree_decl_elt *elt : lst)
      {
        if (elt)
          elt->accept (*this);
      }
  }

  void
  lvalue_checker::visit_simple_for_command (octave::tree_simple_for_command& cmd)
  {
    octave::tree_expression *lhs = cmd.left_hand_side ();

    if (lhs)
      {
        if (lhs->is_index_expression ())
          {
            octave::tree_index_expression *idx = static_cast<octave::tree_index_expression *> (lhs);

            if (idx)
              {
                octave::tree_expression *e = idx->expression ();

                if (e && e->is_identifier ())
                  {
                    bool slow_loop = static_cast<octave::tree_identifier *> (e)->name () == m_loop_var;

                    if (slow_loop)
                      throw -1;
                  }
              }
          }
        else if (lhs->is_identifier ())
          {
            bool slow_loop = static_cast<octave::tree_identifier *> (lhs)->name () == m_loop_var;

            if (slow_loop)
              throw -1;
          }

        lhs->accept (*this);
      }

    octave::tree_expression *expr = cmd.control_expr ();

    if (expr)
      expr->accept (*this);

    octave::tree_expression *maxproc = cmd.maxproc_expr ();

    if (maxproc)
      maxproc->accept (*this);

    octave::tree_statement_list *list = cmd.body ();

    if (list)
      list->accept (*this);
  }

  void
  lvalue_checker::visit_complex_for_command (octave::tree_complex_for_command& cmd)
  {
    octave::tree_argument_list *lhs = cmd.left_hand_side ();

    if (lhs)
      {
        int len = lhs->length ();

        if (len == 0 || len > 2)
          error ("invalid number of output arguments in for command");

        auto p = lhs->begin ();

        while (p != lhs->end ())
          {
            octave::tree_expression *elt = *p++;

            if (elt)
              {
                if (elt->is_index_expression ())
                  {
                    octave::tree_index_expression *idx = static_cast<octave::tree_index_expression *> (elt);

                    if (idx)
                      {
                        octave::tree_expression *e = idx->expression ();

                        if (e && e->is_identifier ())
                          {
                            bool slow_loop = static_cast<octave::tree_identifier *> (e)->name () == m_loop_var;

                            if (slow_loop)
                              throw -1;
                          }
                      }
                  }
                else if (elt->is_identifier ())
                  {
                    bool slow_loop = static_cast<octave::tree_identifier *> (elt)->name () == m_loop_var;

                    if (slow_loop)
                      throw -1;
                  }

                elt->accept (*this);
              }
          }

        lhs->accept (*this);
      }

    octave::tree_expression *expr = cmd.control_expr ();

    if (expr)
      expr->accept (*this);

    octave::tree_statement_list *list = cmd.body ();

    if (list)
      list->accept (*this);
  }
#if OCTAVE_MAJOR_VERSION >= 7
  void
  lvalue_checker::visit_spmd_command (octave::tree_spmd_command& cmd)
  {
    octave::tree_statement_list *body = cmd.body ();

    if (body)
      body->accept (*this);
  }

  void
  lvalue_checker::visit_arguments_block (octave::tree_arguments_block&){}

  void
  lvalue_checker::visit_args_block_attribute_list (octave::tree_args_block_attribute_list&){}

  void
  lvalue_checker::visit_args_block_validation_list (octave::tree_args_block_validation_list&){}

  void
  lvalue_checker::visit_arg_validation (octave::tree_arg_validation&){}

  void
  lvalue_checker::visit_arg_size_spec (octave::tree_arg_size_spec&){}

  void
  lvalue_checker::visit_arg_validation_fcns (octave::tree_arg_validation_fcns&){}
#endif
  void
  lvalue_checker::visit_multi_assignment (octave::tree_multi_assignment& expr)
  {
    octave::tree_argument_list *lhs = expr.left_hand_side ();

    if (lhs)
      {
        auto p = lhs->begin ();

        while (p != lhs->end ())
          {
            octave::tree_expression *elt = *p++;

            if (elt)
              {
                if (elt->is_index_expression ())
                  {
                    octave::tree_index_expression *idx = static_cast<octave::tree_index_expression *> (elt);

                    if (idx)
                      {
                        octave::tree_expression *e = idx->expression ();

                        if (e && e->is_identifier ())
                          {
                            bool slow_loop = static_cast<octave::tree_identifier *> (e)->name () == m_loop_var;

                            if (slow_loop)
                              throw -1;
                          }
                      }
                  }
                else if (elt->is_identifier ())
                  {
                    bool slow_loop = static_cast<octave::tree_identifier *> (elt)->name () == m_loop_var;

                    if (slow_loop)
                      throw -1;
                  }

                elt->accept (*this);
              }
          }

        lhs->accept (*this);
      }

    octave::tree_expression *rhs = expr.right_hand_side ();

    if (rhs)
      rhs->accept (*this);
  }

  void
  lvalue_checker::visit_index_expression (octave::tree_index_expression& expr)
  {
    octave::tree_expression *e = expr.expression ();

    std::string type_tags = expr.type_tags ();

    if (e)
      e->accept (*this);

    std::list<octave::tree_argument_list *> lst = expr.arg_lists ();

    std::list<string_vector> arg_names = expr.arg_names ();

    std::list<octave::tree_expression *> dyn_field = expr.dyn_fields ();

    std::list<octave::tree_argument_list *>::iterator p = lst.begin ();

    std::list<string_vector>::iterator p_arg_names = arg_names.begin ();

    std::list<octave::tree_expression *>::iterator p_dyn_field = dyn_field.begin ();

    int n = type_tags.length ();

    for (int i = 0; i < n; i++)
      {
        octave::tree_argument_list *elt = *p++;

        if (type_tags[i] == '.')
          {
            string_vector nm = *p_arg_names;

            if (nm.numel () == 1)
              {
                std::string fn = nm(0);

                if (fn.empty ())
                  {
                    octave::tree_expression *df = *p_dyn_field;

                    if (df)
                      {
                        df->accept (*this);
                      }
                  }
              }
          }
        else if (elt)
          elt->accept (*this);

        p_arg_names++;

        p_dyn_field++;
      }
  }

  void
  lvalue_checker::visit_matrix (octave::tree_matrix& lst)
  {
    auto p = lst.begin ();

    while (p != lst.end ())
      {
        octave::tree_argument_list *elt = *p++;

        if (elt)
          elt->accept (*this);
      }
  }

  void
  lvalue_checker::visit_cell (octave::tree_cell& lst)
  {
    auto p = lst.begin ();

    while (p != lst.end ())
      {
        octave::tree_argument_list *elt = *p++;

        if (elt)
          elt->accept (*this);
      }
  }

  void
  lvalue_checker::visit_simple_assignment (octave::tree_simple_assignment& expr)
  {
    octave::tree_expression *lhs = expr.left_hand_side ();

    if (lhs)
      {
        if (lhs->is_index_expression ())
          {
            octave::tree_index_expression *idx = static_cast<octave::tree_index_expression *> (lhs);

            if (idx)
              {
                octave::tree_expression *e = idx->expression ();

                if (e && e->is_identifier ())
                  {
                    bool slow_loop = static_cast<octave::tree_identifier *> (e)->name () == m_loop_var;

                    if (slow_loop)
                      throw -1;
                  }
              }
          }
        else if (lhs->is_identifier ())
          {
            bool slow_loop = static_cast<octave::tree_identifier *> (lhs)->name () == m_loop_var;

            if (slow_loop)
              throw -1;
          }

        lhs->accept (*this);
      }

    octave::tree_expression *rhs = expr.right_hand_side ();

    if (rhs)
      rhs->accept (*this);
  }

  void
  lvalue_checker::visit_statement (octave::tree_statement& stmt)
  {
    octave::tree_command *cmd = stmt.command ();

    if (cmd)
      cmd->accept (*this);
    else
      {
        octave::tree_expression *expr = stmt.expression ();

        if (expr)
          expr->accept (*this);
      }
  }

  void
  lvalue_checker::visit_statement_list (octave::tree_statement_list& lst)
  {
    for (octave::tree_statement *elt : lst)
      {
        if (elt)
          elt->accept (*this);
      }
  }

  void
  lvalue_checker::visit_try_catch_command (octave::tree_try_catch_command& cmd)
  {
    octave::tree_statement_list *try_code = cmd.body ();

    octave::tree_identifier *expr_id = cmd.identifier ();

    if (expr_id)
      {
        if (expr_id->name () == m_loop_var)
          throw -1;

        expr_id->accept (*this);
      }

    if (try_code)
      try_code->accept (*this);

    octave::tree_statement_list *catch_code = cmd.cleanup ();

    if (catch_code)
      catch_code->accept (*this);
  }

  void lvalue_checker::visit_unwind_protect_command (octave::tree_unwind_protect_command& cmd)
  {
    octave::tree_statement_list *unwind_protect_code = cmd.body ();

    if (unwind_protect_code)
      unwind_protect_code->accept (*this);

    octave::tree_statement_list *cleanup_code = cmd.cleanup ();

    if (cleanup_code)
      cleanup_code->accept (*this);
  }

  void lvalue_checker::visit_while_command (octave::tree_while_command& cmd)
  {
    octave::tree_expression *expr = cmd.condition ();

    if (expr)
      expr->accept (*this);

    octave::tree_statement_list *list = cmd.body ();

    if (list)
      list->accept (*this);
  }

  void lvalue_checker::visit_do_until_command (octave::tree_do_until_command& cmd)
  {
    octave::tree_statement_list *list = cmd.body ();

    if (list)
      list->accept (*this);

    octave::tree_expression *expr = cmd.condition ();

    if (expr)
      expr->accept (*this);
  }

  void
  lvalue_checker::visit_anon_fcn_handle (octave::tree_anon_fcn_handle&  afh )
  {
    octave::tree_parameter_list *param_list = afh.parameter_list ();

    if (param_list)
      {
        param_list->accept (*this);
      }

    octave::tree_expression *expr = afh.expression ();

    if (expr)
      {
        expr->accept (*this);
      }
  }

  void
  lvalue_checker::visit_if_clause (octave::tree_if_clause& cmd)
  {
    octave::tree_expression *expr = cmd.condition ();

    if (expr)
      expr->accept (*this);

    octave::tree_statement_list *list = cmd.commands ();

    if (list)
      list->accept (*this);
  }

  void
  lvalue_checker::visit_if_command (octave::tree_if_command& cmd)
  {
    octave::tree_if_command_list *list = cmd.cmd_list ();

    if (list)
      list->accept (*this);
  }

  void
  lvalue_checker::visit_if_command_list (octave::tree_if_command_list& lst)
  {
    auto p = lst.begin ();

    while (p != lst.end ())
      {
        octave::tree_if_clause *elt = *p++;

        if (elt)
          elt->accept (*this);
      }
  }

  void
  lvalue_checker::visit_switch_case (octave::tree_switch_case& cs)
  {
    octave::tree_expression *label = cs.case_label ();

    if (label)
      label->accept (*this);

    octave::tree_statement_list *list = cs.commands ();

    if (list)
      list->accept (*this);
  }

  void
  lvalue_checker::visit_switch_case_list (octave::tree_switch_case_list& lst)
  {
    auto p = lst.begin ();

    while (p != lst.end ())
      {
        octave::tree_switch_case *elt = *p++;

        if (elt)
          elt->accept (*this);
      }
  }

  void
  lvalue_checker::visit_switch_command (octave::tree_switch_command& cmd)
  {
    octave::tree_expression *expr = cmd.switch_value ();

    if (expr)
      expr->accept (*this);

    octave::tree_switch_case_list *list = cmd.case_list ();

    if (list)
      list->accept (*this);
  }

  void
  lvalue_checker::visit_decl_elt (octave::tree_decl_elt& elt)
  {
    octave::tree_identifier *id = elt.ident ();

    if (id)
      {
        std::string nm = id->name();

        if (elt.is_global () || elt.is_persistent ())
          {

            bool slow_loop = nm == m_loop_var;

            if (slow_loop)
              {
                warning ("coder: %s\\%s.m : %d. cannot generate code. global or persistent variable shadowed the loop variable"
                , m_file->path.c_str ()
                , m_file->name.c_str ()
                , id->line ());

                throw -1;
              }
          }

        octave::tree_expression *expr = elt.expression ();

        if (expr)
          expr->accept(*this);
      }
  }

  void
  lvalue_checker::visit_parameter_list (octave::tree_parameter_list& lst)
  {
    auto p = lst.begin ();

    while (p != lst.end ())
      {
        octave::tree_decl_elt *elt = *p++;

        if (elt)
          elt->accept (*this);
      }
  }

  void
  lvalue_checker::visit_postfix_expression (octave::tree_postfix_expression& expr)
  {
    octave::tree_expression *lhs = expr.operand ();

    if (lhs)
      {
        octave_value::unary_op etype = expr.op_type ();

        if (etype == octave_value::op_incr || etype == octave_value::op_incr)
          {
            if (lhs->is_index_expression ())
              {
                octave::tree_index_expression *idx = static_cast<octave::tree_index_expression *> (lhs);

                if (idx)
                  {
                    octave::tree_expression *e = idx->expression ();

                    if (e && e->is_identifier ())
                      {
                        bool slow_loop = static_cast<octave::tree_identifier *> (e)->name () == m_loop_var;

                        if (slow_loop)
                          throw -1;
                      }
                  }
              }
            else if (lhs->is_identifier ())
              {
                bool slow_loop = static_cast<octave::tree_identifier *> (lhs)->name () == m_loop_var;

                if (slow_loop)
                  throw -1;
              }
          }

        lhs->accept (*this);
      }
  }

  void
  lvalue_checker::visit_prefix_expression (octave::tree_prefix_expression& expr)
  {
    octave::tree_expression *rhs = expr.operand ();

    if (rhs)
      {
        octave_value::unary_op etype = expr.op_type ();

        if (etype == octave_value::op_incr || etype == octave_value::op_incr)
          {
            if (rhs->is_index_expression ())
              {
                octave::tree_index_expression *idx = static_cast<octave::tree_index_expression *> (rhs);

                if (idx)
                  {
                    octave::tree_expression *e = idx->expression ();

                    if (e && e->is_identifier ())
                      {
                        bool slow_loop = static_cast<octave::tree_identifier *> (e)->name () == m_loop_var;

                        if (slow_loop)
                          throw -1;
                      }
                  }
              }
            else if (rhs->is_identifier ())
              {
                bool slow_loop = static_cast<octave::tree_identifier *> (rhs)->name () == m_loop_var;

                if (slow_loop)
                  throw -1;
              }
          }

        rhs->accept (*this);
      }
  }
}
