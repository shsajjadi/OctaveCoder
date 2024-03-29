#include <functional>
#include <memory>
#include "sys/stat.h"

#include <octave/builtin-defun-decls.h>
#include <octave/dir-ops.h>
#include <octave/file-ops.h>
#include <octave/file-stat.h>
#include <octave/oct.h>
#include <octave/oct-env.h>
#include <octave/ov-usr-fcn.h>
#include <octave/version.h>

#include "semantic_analyser.h"

namespace coder_compiler
{
  struct unwind
  {
    unwind (std::function<void ()> fcn) : m_fcn (std::move (fcn))
    {}

    ~unwind ()
    {
      m_fcn ();
    }

    std::function<void ()> m_fcn;
  };

  file_time::file_time(const std::string& filename)
  : ok(false), m_mtime(0)
  {
    struct stat st;
    int result = ::stat(filename.c_str (), &st);
    if (! result)
      m_mtime = st.st_mtime;

    ok = result == 0;
  }

  function_finder::function_finder (const octave::symbol_scope& scope)
    :
      oldscope (octave::interpreter::the_interpreter ()->get_symbol_table().current_scope()),
      newscope (scope)
  {
#if OCTAVE_MAJOR_VERSION < 6
    octave::symbol_table& octave_symtab = octave::interpreter::the_interpreter ()->get_symbol_table();

    octave_symtab.set_scope(scope);
#endif
  }

  octave_value function_finder::find_function (const std::string& name)
  {
    octave::symbol_table& octave_symtab = octave::interpreter::the_interpreter ()->get_symbol_table();

#if OCTAVE_MAJOR_VERSION >= 6
    return octave_symtab.find_function (name, octave_value_list (), newscope);
#else
    return octave_symtab.find_function (name, octave_value_list ());
#endif
  }

  function_finder::~function_finder ()
  {
#if OCTAVE_MAJOR_VERSION < 6
    if (! oldscope)
      return;

    octave::symbol_table& octave_symtab = octave::interpreter::the_interpreter ()->get_symbol_table();

    octave_symtab.set_scope(oldscope);
#endif
  }

  semantic_analyser::semantic_analyser(  )
  {
    find_resolvabale_names_in_octave_path ();
    init_builtins();
  }

  semantic_analyser::semantic_analyser( const octave_map& cache_index )
  {
    find_resolvabale_names_in_octave_path ();
    build_dependency_graph (cache_index);
    init_builtins();
  }

  void
  semantic_analyser::visit_argument_list (octave::tree_argument_list& lst)
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
  semantic_analyser::visit_binary_expression (octave::tree_binary_expression& expr)
  {
    octave::tree_expression *op1 = expr.lhs ();

    if (op1)
      op1->accept (*this);

    octave::tree_expression *op2 = expr.rhs ();

    if (op2)
      op2->accept (*this);
  }

  void
  semantic_analyser::visit_boolean_expression (octave::tree_boolean_expression& expr)
  {
    visit_binary_expression (expr);
  }

  void
  semantic_analyser::visit_compound_binary_expression (octave::tree_compound_binary_expression& expr)
  {
    visit_binary_expression (expr);
  }

  void
  semantic_analyser::visit_break_command (octave::tree_break_command&)
  {
    // Nothing to do.
  }

  void
  semantic_analyser::visit_colon_expression (octave::tree_colon_expression& expr)
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
  semantic_analyser::visit_continue_command (octave::tree_continue_command&)
  {

  }

  void
  semantic_analyser::visit_decl_command (octave::tree_decl_command& cmd)
  {
    octave::tree_decl_init_list *init_list = cmd.initializer_list ();

    if (init_list)
      init_list->accept (*this);
  }

  void semantic_analyser::visit_decl_init_list (octave::tree_decl_init_list& lst)
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
  semantic_analyser::visit_simple_for_command (octave::tree_simple_for_command& cmd)
  {
    octave::tree_expression *lhs = cmd.left_hand_side ();

    if (lhs)
      {
        lhs->accept(*this);
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
  semantic_analyser::visit_complex_for_command (octave::tree_complex_for_command& cmd)
  {
    octave::tree_argument_list *lhs = cmd.left_hand_side ();

    if (lhs)
      {
        int len = lhs->length ();

        if (len == 0 || len > 2)
          error ("invalid number of output arguments in for command");

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
  semantic_analyser::visit_spmd_command (octave::tree_spmd_command& cmd)
  {
    octave::tree_statement_list *body = cmd.body ();

    if (body)
      body->accept (*this);
  }

  void
  semantic_analyser::visit_arguments_block (octave::tree_arguments_block&){}

  void
  semantic_analyser::visit_args_block_attribute_list (octave::tree_args_block_attribute_list&){}

  void
  semantic_analyser::visit_args_block_validation_list (octave::tree_args_block_validation_list&){}

  void
  semantic_analyser::visit_arg_validation (octave::tree_arg_validation&){}

  void
  semantic_analyser::visit_arg_size_spec (octave::tree_arg_size_spec&){}

  void
  semantic_analyser::visit_arg_validation_fcns (octave::tree_arg_validation_fcns&){}
#endif
  void
  semantic_analyser::visit_multi_assignment (octave::tree_multi_assignment& expr)
  {
    octave::tree_argument_list *lhs = expr.left_hand_side ();

    if (lhs)
      {
        lhs->accept (*this);
      }

    octave::tree_expression *rhs = expr.right_hand_side ();

    if (rhs)
      rhs->accept (*this);
  }

  void
  semantic_analyser::visit_index_expression (octave::tree_index_expression& expr)
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
  semantic_analyser::visit_matrix (octave::tree_matrix& lst)
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
  semantic_analyser::visit_cell (octave::tree_cell& lst)
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
  semantic_analyser::visit_simple_assignment (octave::tree_simple_assignment& expr)
  {
    octave::tree_expression *lhs = expr.left_hand_side ();

    if (lhs)
      {
        lhs->accept (*this);
      }

    octave::tree_expression *rhs = expr.right_hand_side ();

    if (rhs)
      rhs->accept (*this);
  }

  void
  semantic_analyser::visit_statement (octave::tree_statement& stmt)
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
  semantic_analyser::visit_statement_list (octave::tree_statement_list& lst)
  {
    for (octave::tree_statement *elt : lst)
      {
        if (elt)
          elt->accept (*this);
      }
  }

  void
  semantic_analyser::visit_try_catch_command (octave::tree_try_catch_command& cmd)
  {
    octave::tree_statement_list *try_code = cmd.body ();

    octave::tree_identifier *expr_id = cmd.identifier ();

    if (expr_id)
      {
        expr_id->accept (*this);
      }

    if (try_code)
      try_code->accept (*this);

    octave::tree_statement_list *catch_code = cmd.cleanup ();

    if (catch_code)
      catch_code->accept (*this);
  }

  void semantic_analyser::visit_unwind_protect_command (octave::tree_unwind_protect_command& cmd)
  {
    octave::tree_statement_list *unwind_protect_code = cmd.body ();

    if (unwind_protect_code)
      unwind_protect_code->accept (*this);

    octave::tree_statement_list *cleanup_code = cmd.cleanup ();

    if (cleanup_code)
      cleanup_code->accept (*this);
  }

  void semantic_analyser::visit_while_command (octave::tree_while_command& cmd)
  {
    octave::tree_expression *expr = cmd.condition ();

    if (expr)
      expr->accept (*this);

    octave::tree_statement_list *list = cmd.body ();

    if (list)
      list->accept (*this);
  }

  void semantic_analyser::visit_do_until_command (octave::tree_do_until_command& cmd)
  {
    octave::tree_statement_list *list = cmd.body ();

    if (list)
      list->accept (*this);

    octave::tree_expression *expr = cmd.condition ();

    if (expr)
      expr->accept (*this);
  }

  void
  semantic_analyser::visit_handle (octave::tree_anon_fcn_handle& anon_fh)
  {
    handles.emplace();

    auto& table = current_file()->current_local_function();

    octave::tree_parameter_list *param_list = anon_fh.parameter_list ();

    if (param_list)
      {
        for (octave::tree_decl_elt *elt : * param_list)
          {
            octave::tree_identifier *id = elt->ident ();

            if (id && !id->is_black_hole())
              {
                  insert_formal_symbol(id->name());
              }
          }

        bool takes_varargs = param_list->takes_varargs ();

        if (takes_varargs)
          insert_formal_symbol("varargin");

        for (octave::tree_decl_elt *elt : * param_list)
          {
            octave::tree_expression *expr = elt->expression ();

            if(expr)
              expr->accept(*this);
          }
      }

    octave::tree_expression *expr = anon_fh.expression ();

    if (expr)
      expr->accept (*this);

    for (auto& fh: handles.top())
      {
        table.open_new_anon_scope();

        visit_handle(fh);

        table.close_current_scope();
      }

    handles.pop();
  }

  void
  semantic_analyser::visit_octave_user_function (octave_user_function& fcnn)
  {
    scope_stack.push (fcnn.scope());

    unwind unw ([&](){scope_stack.pop ();});

    octave::tree_parameter_list *ret_list = fcnn.return_list ();

    octave::tree_parameter_list *param_list = fcnn.parameter_list ();

    octave::tree_statement_list *cmd_list = fcnn.body ();

    auto& table = current_file()->current_local_function();

    for (const auto& sub: fcnn.subfunctions())
      {
        const auto& nm = sub.first;

        const auto& f = sub.second;

        octave_function *fcn = f.function_value (true);

        if (fcn && fcn->is_user_function())
          {
            octave_user_function& ufc = *(fcn->user_function_value());

            if (ufc.is_nested_function())
              {
                auto sym = insert_symbol(nm, f);

                insert_symbol(sym, symbol_type::nested_fcn);
              }
          }
      }

    if (ret_list)
      {
        for (octave::tree_decl_elt *elt : * ret_list)
          {
            if(elt)
              {
                octave::tree_identifier *id = elt->ident ();

                if (id && !id->is_black_hole())
                  {
                    insert_formal_symbol(id->name());
                  }
              }
          }

        bool takes_var_return = fcnn.takes_var_return ();

        if (takes_var_return)
          insert_formal_symbol("varargout");
      }

    if (param_list)
      {
        for (octave::tree_decl_elt *elt : * param_list)
          {
            if(elt)
              {
                octave::tree_identifier *id = elt->ident ();
                if (id && !id->is_black_hole())
                  {
                    insert_formal_symbol(id->name());
                  }
              }
          }

        bool takes_varargs = param_list->takes_varargs ();

        if (takes_varargs)
          insert_formal_symbol("varargin");
      }

    handles.emplace();

    if (param_list)
      {
        for (octave::tree_decl_elt *elt : * param_list)
          {
            if (elt)
            {
              octave::tree_expression *expr = elt->expression ();

              if(expr)
                expr->accept(*this);
            }
          }
      }

    if (cmd_list)
      cmd_list->accept (*this);

    for (auto& fh: handles.top())
      {
        table.open_new_anon_scope();
              visit_handle(fh);

        table.close_current_scope();
      }

    handles.pop();

    for (const auto& sub: fcnn.subfunctions())
      {
        const auto& f = sub.second;

        octave_function *fcn = f.function_value (true);

        if (fcn && fcn->is_user_function())
          {
            octave_user_function& ufc = *(fcn->user_function_value());

            if (ufc.is_nested_function())
              {
                table.open_new_nested_scope();

                  ufc.accept(*this);

                table.close_current_scope();
              }
          }
      }

    for (const auto& sub: fcnn.subfunctions())
      {
        const auto& nm = sub.first;

        const auto& f = sub.second;

        octave_function *fcn = f.function_value (true);

        if (fcn && fcn->is_user_function())
          {
            octave_user_function& ufc = *(fcn->user_function_value());

            if (!ufc.is_nested_function())
              {
                current_file()->add_new_local_function(nm);

                ufc.accept(*this);
              }
          }
      }
  }

  void semantic_analyser::visit_function_def (octave::tree_function_def& fdef)
  {
    octave_value fcn = fdef.function ();

    octave_function *f = fcn.function_value ();

    if (f)
      f->accept (*this);
  }

  void
  semantic_analyser::visit_anon_fcn_handle (octave::tree_anon_fcn_handle&  afh )
  {
    handles.top().push_back(afh);
  }

  void
  semantic_analyser::visit_identifier (octave::tree_identifier& expr)
  {
    std::string nm = expr.name ();

    if(nm == "~" || nm == "end" )
      return;

    insert_symbol(nm);
  }

  void
  semantic_analyser::visit_if_clause (octave::tree_if_clause& cmd)
  {
    octave::tree_expression *expr = cmd.condition ();

    if (expr)
      expr->accept (*this);

    octave::tree_statement_list *list = cmd.commands ();

    if (list)
      list->accept (*this);
  }

  void
  semantic_analyser::visit_if_command (octave::tree_if_command& cmd)
  {
    octave::tree_if_command_list *list = cmd.cmd_list ();

    if (list)
      list->accept (*this);
  }

  void
  semantic_analyser::visit_if_command_list (octave::tree_if_command_list& lst)
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
  semantic_analyser::visit_switch_case (octave::tree_switch_case& cs)
  {
    octave::tree_expression *label = cs.case_label ();

    if (label)
      label->accept (*this);

    octave::tree_statement_list *list = cs.commands ();

    if (list)
      list->accept (*this);
  }

  void
  semantic_analyser::visit_switch_case_list (octave::tree_switch_case_list& lst)
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
  semantic_analyser::visit_switch_command (octave::tree_switch_command& cmd)
  {
    octave::tree_expression *expr = cmd.switch_value ();

    if (expr)
      expr->accept (*this);

    octave::tree_switch_case_list *list = cmd.case_list ();

    if (list)
      list->accept (*this);
  }

  void
  semantic_analyser::visit_decl_elt (octave::tree_decl_elt& elt)
  {
    octave::tree_identifier *id = elt.ident ();

    if (id)
      {
        if (id->is_black_hole())
          return;

        std::string nm = id->name();

        auto sym = insert_symbol(nm);

        if (elt.is_global ())
          {
            update_globals(nm);
          }
        else if (elt.is_persistent ())
          {
            insert_symbol(sym, symbol_type::persistent);
          }
        else
          {
            //error ("declaration list element not global or persistent");
          }

        octave::tree_expression *expr = elt.expression ();

        if (expr)
          expr->accept(*this);
      }
  }

  void
  semantic_analyser::visit_fcn_handle (octave::tree_fcn_handle& expr)
  {
    std::string nm = expr.name ();

    size_t pos = nm.find ('.', 0);

    if (pos != std::string::npos)
      {
        insert_symbol(nm.substr (0, pos));
      }
    else
      insert_symbol(nm);
  }

  void
  semantic_analyser::visit_parameter_list (octave::tree_parameter_list& lst)
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
  semantic_analyser::visit_postfix_expression (octave::tree_postfix_expression& expr)
  {
    octave::tree_expression *e = expr.operand ();

    if (e)
      e->accept (*this);
  }

  void
  semantic_analyser::visit_prefix_expression (octave::tree_prefix_expression& expr)
  {
    octave::tree_expression *e = expr.operand ();

    if (e)
      e->accept (*this);
  }

  coder_file_ptr
  semantic_analyser::analyse(const std::string& name, const octave_value& fcn)
  {
    task_queue.clear ();

    coder_file_ptr start_node = add_fcn_to_task_queue(coder_file_ptr (), name, fcn);

    for(current_file_it = task_queue.begin(); current_file_it != task_queue.end(); ++current_file_it)
      {
        file_directory_it = &directory_table[current_file()->path];

        auto& p = path_map[current_file()->path];

        if (! p)
          {
            p = std::shared_ptr<std::set<std::string>> {new std::set<std::string> {}};
          }

        current_path_map = p;

        local_functions.clear();

        if (current_file()->fcn.is_user_function())
          {
            for (const auto& sub: current_file()->fcn.user_function_value()->subfunctions())
              {
                const auto& f = sub.second;

                octave_function *fcn = f.function_value (true);

                if (fcn && fcn->is_user_function())
                  {
                    octave_user_function& ufc = *(fcn->user_function_value());

                    if (!ufc.is_nested_function())
                      {
                        local_functions.insert(sub);
                      }
                  }
              }

            current_file()->fcn.user_function_value()->accept(*this);
          }
      }

    return start_node;
  }

  coder_symbol_ptr
  semantic_analyser::insert_symbol(const std::string& name)
  {
    auto& table = current_file()->current_local_function();

    auto sym = table.contains(name);

    if(! sym)
      {
        sym = table.lookup_in_parent_scopes(name);

        if(! sym)
          {
            octave_value fcn;

            coder_file_ptr file;

            auto f_local = local_functions.find(name);

            if (f_local != local_functions.end())
              {
                fcn = f_local->second;

                file = current_file();

                sym = std::make_shared<coder_symbol>(name, fcn, file);
              }
            else
              {
                auto symbol_it = file_directory_it->find (coder_symbol_ptr (new coder_symbol (name)));

                if (symbol_it != file_directory_it->end ())
                  {
                    sym = *symbol_it;

                    add_fcn_to_task_queue(current_file (), sym->name, sym->fcn);

                    table.insert_symbol(sym, symbol_type::ordinary);

                    return sym;
                  }

                if (resolvable_path_names->find (name) != resolvable_path_names->end ()
                  || current_path_map->find (name) != current_path_map->end ())
                  {
                    fcn = find_function (name);
                  }

                file = add_fcn_to_task_queue(current_file (), name, fcn);

                sym = std::make_shared<coder_symbol>(name, fcn, file);

                file_directory_it->insert(sym);
              }

            table.insert_symbol(sym, symbol_type::ordinary);
          }
        else
          {
            if (! table.current_scope()->contains(sym, symbol_type::formal))
                table.insert_symbol(sym, symbol_type::inherited);
          }
      }

    return sym;
  }

  coder_symbol_ptr
  semantic_analyser::insert_symbol(const coder_symbol_ptr& sym, symbol_type type)
  {
    auto& table = current_file()->current_local_function();

    table.insert_symbol(sym, type);

    return sym;
  }

  coder_symbol_ptr
  semantic_analyser::insert_symbol(const std::string& name, const octave_value& fcn)
  {
    auto& table = current_file()->current_local_function();

    auto sym = std::make_shared<coder_symbol>(name, fcn, coder_file_ptr());

    table.insert_symbol(sym, symbol_type::ordinary);

    return sym;
  }

  coder_symbol_ptr
  semantic_analyser::insert_symbol(const std::string& name, symbol_type type)
  {
    auto& table = current_file()->current_local_function();

    auto sym = table.contains(name);

    if(! sym)
      {
        octave_value fcn;

        coder_file_ptr file;

        auto f_local = local_functions.find(name);

        if (f_local != local_functions.end())
          {
            fcn = f_local->second;

            file = current_file();

            sym = std::make_shared<coder_symbol>(name, fcn, file);
          }
        else
          {
            auto symbol_it = file_directory_it->find (coder_symbol_ptr (new coder_symbol (name)));

            if (symbol_it != file_directory_it->end () && type == symbol_type::ordinary)
              {
                sym = *symbol_it;

                table.insert_symbol(sym, type);

                add_fcn_to_task_queue(current_file (), sym->name, sym->fcn);

                return sym;
              }
                if (resolvable_path_names->find (name) != resolvable_path_names->end ()
                  || current_path_map->find (name) != current_path_map->end ())
                  {
                    fcn = find_function (name);
                  }

            file = add_fcn_to_task_queue(current_file (), name, fcn);

            sym = std::make_shared<coder_symbol>(name, fcn, file);

            file_directory_it->insert (sym);
          }
      }

    table.insert_symbol(sym, type);

    return sym;
  }

  coder_symbol_ptr
  semantic_analyser::insert_formal_symbol(const std::string& name)
  {
    auto& table = current_file()->current_local_function();

    auto sym = std::make_shared<coder_symbol>(name, octave_value (), coder_file_ptr ());

    table.insert_symbol(sym, symbol_type::formal);

    table.insert_symbol(sym, symbol_type::ordinary);

    return sym;
  }

  coder_file_ptr
  semantic_analyser::add_fcn_to_task_queue (const coder_file_ptr& this_file, const std::string& sym_name, const octave_value& val)
  {
    std::string name; // filename

    std::string path; // filepath

    file_type type = file_type::unknown;

    const std::string& this_file_path = this_file ? this_file->path : std::string ("");

    std::tie( type, name, path) = find_file_type_name_and_path(val, sym_name, this_file_path);

    coder_file_ptr retval;

    if (type != file_type::unknown)
      {
        auto range = dependent_files.equal_range(std::make_shared<coder_file>(name));

        std::string ext ;

        if (type == file_type::m)
          ext = ".m";
        else if (type == file_type::oct)
          ext = ".oct";
        else if (type == file_type::mex)
          ext = ".mex";

        bool is_file_on_disk =  (type == file_type::m || type == file_type::oct || type == file_type::mex);

        std::string full_file_name ;

        if (is_file_on_disk)
          full_file_name = octave::sys::file_ops::concat(path, name + ext);

        file_time src (full_file_name);

        time_t timestamp = 0;

        if (type == file_type::m)
          {
            timestamp = src.mtime();
          }

        if (range.first ==dependent_files.end() || range.first == range.second)
          {
            if (type == file_type::cmdline)
              {
                octave_user_function * cmd_fcn = val.user_function_value();

                time_t cmd_fcn_timestamp = cmd_fcn->time_parsed ().unix_time ();

                if (cmd_fcn_timestamp == 0 )
                  {
                    octave::sys::time now;

                    cmd_fcn->stash_fcn_file_time (now);

                    timestamp = now.unix_time ();
                  }
              }

            coder_file_ptr new_file(new coder_file{name, 2, path, timestamp, type, val});

            dependent_files.insert(new_file);

            dependency_graph[new_file]={};

            if(this_file)
              dependency_graph[this_file].insert(new_file);

            task_queue.emplace_back(new_file);

            visited[new_file] = true;

            state[new_file] = file_state::New;

            retval = new_file;

            if (type == file_type::oct && name != sym_name)
              {
                new_file->add_new_local_function(sym_name);
              }
          }
        else
          {
            int n_overloads = (*std::max_element(range.first, range.second,
              [](const coder_file_ptr &a, const coder_file_ptr &b)
              {
                return a->id < b->id;
              }))->id;


            auto cache_file = std::find_if(range.first, range.second,
            [&val,&path,&type](const coder_file_ptr& file)
              {
                if (type == file_type::cmdline && file->type == type)
                  {
                    octave_user_function * cmd_fcn = val.user_function_value();

                    time_t cmd_fcn_timestamp = cmd_fcn->time_parsed ().unix_time ();

                    return cmd_fcn_timestamp != 0 && file->timestamp == cmd_fcn_timestamp;
                  }
                return
                   file->type == type
                    && file->path == path;
              }
            );

            if (cache_file != range.second )
              {
                coder_file_ptr file = *cache_file;

                retval = file;

                if (type == file_type::m  )
                  {
                    if (! visited[file])
                      {
                        visited[file] = true;

                        if ( src.is_newer (file->timestamp))
                          {
                            file->fcn = val;

                            file->timestamp = timestamp;

                            dependency_graph[file] = {};

                            if (this_file && file != this_file)
                              dependency_graph[this_file].insert(file);

                            task_queue.emplace_back(file);

                            state[file] = file_state::Updated;
                          }
                        else
                          {
                            if (this_file && file != this_file)
                              dependency_graph[this_file].insert(file);

                            file->fcn = val;

                            resolve_cache_dependency(file);
                          }
                      }
                    else
                      {
                        if (this_file && file != this_file)
                          dependency_graph[this_file].insert(file);
                      }
                  }
                else if (type == file_type::cmdline)
                  {
                    if (! visited[file])
                      {
                        visited[file] = true;

                        octave_user_function * cmd_fcn = val.user_function_value();

                        time_t cmd_fcn_timestamp = cmd_fcn->time_parsed ().unix_time ();

                        if (cmd_fcn_timestamp == 0 || file->timestamp != cmd_fcn_timestamp)
                          {
                            octave::sys::time now;

                            // The m_file_info member of octave_user_code is private
                            // and the timestamp of a commandline function cannot be accessed.
                            // As a workaround we can set the parsing time of the command line function
                            // and use it as the timestamp. A commandline function is uniquely identified
                            // by its name and timestamp.

                            cmd_fcn->stash_fcn_file_time (now);

                            timestamp = now.unix_time ();

                            coder_file_ptr new_file(new coder_file{name , n_overloads+1, path, timestamp, type, val});

                            dependent_files.insert(new_file);

                            dependency_graph[new_file];

                            state[new_file] = file_state::New;

                            if(this_file)
                              dependency_graph[this_file].insert (new_file);

                            task_queue.emplace_back(new_file);

                            retval = new_file;
                          }
                        else
                          {
                            if (this_file && file != this_file)
                              dependency_graph[this_file].insert(file);

                            file->fcn = val;

                            resolve_cache_dependency (file);
                          }
                      }
                  }
                else  if (type == file_type::oct )
                  {
                    visited[file] = true;

                    if (this_file && file != this_file)
                      dependency_graph[this_file].insert(file);

                    if (file->name != sym_name)
                      {
                        for (const auto& member: file->local_functions)
                          {
                            if (member.name()  == sym_name)
                              {
                                return file;
                              }
                          }

                        file->add_new_local_function(sym_name);

                        state[file] = file_state::Updated;
                      }
                  }
                else
                  {
                    if (this_file && file != this_file)
                      dependency_graph[this_file].insert(file);

                    visited[file] = true;
                  }
              }
            else
              {
                if (type == file_type::cmdline)
                  {
                    octave_user_function * cmd_fcn = val.user_function_value();

                    octave::sys::time now;

                    cmd_fcn->stash_fcn_file_time (now);

                    timestamp = now.unix_time ();
                  }

                coder_file_ptr new_file(new coder_file{name , n_overloads+1, path, timestamp, type, val});

                dependent_files.insert(new_file);

                if(this_file)
                  dependency_graph[this_file].insert(new_file);

                task_queue.emplace_back(new_file);

                dependency_graph[new_file];

                state[new_file] = file_state::New;

                visited[new_file] = true;

                retval = new_file;

                if (type == file_type::oct && name != sym_name)
                  {
                    new_file->add_new_local_function(sym_name);
                  }
              }
          }
      }

    if (this_file)
      {
        if (retval)
          {
            if (retval != this_file)
              external_symbols[this_file][retval].insert(sym_name);
          }
        else
          external_symbols[this_file][retval].insert(sym_name);
      }

    return retval;
  }

  void
  semantic_analyser::resolve_cache_dependency(const coder_file_ptr& file)
  {
    std::vector<std::tuple<coder_file_ptr,coder_file_ptr,std::string>> old_files;

    octave_user_function* fcnn = nullptr;

    if (file->fcn.is_user_function())
      fcnn = file->fcn.user_function_value ();

    if (! fcnn)
      return;

    scope_stack.push (fcnn->scope ());

    unwind unw ([&](){scope_stack.pop ();});

    auto dep_old = external_symbols.at(file);

    auto& directory_idx  = directory_table [file->path];

    auto path_idx = path_map[file->path];

    if (! path_idx)
      {
        path_map[file->path] = std::shared_ptr<std::set<std::string>> {new std::set<std::string> {}};

        path_idx = path_map[file->path];
      }

    for (const auto& callee : dep_old)
      {
        const auto& f = callee.first;

        for (const auto& sym_name : callee.second)
          {
            coder_file_ptr new_file;

            auto symbol_it = directory_idx.find (coder_symbol_ptr (new coder_symbol (sym_name)));

            if (symbol_it != directory_idx.end ())
              {
                new_file = (* symbol_it)->file;

                add_fcn_to_task_queue (file, sym_name, (* symbol_it)->fcn);
              }
            else
              {
                 octave_value new_src;

                if (resolvable_path_names->find (sym_name) != resolvable_path_names->end ()
                  || path_idx->find (sym_name) != path_idx->end ())
                  {
                    new_src = find_function (sym_name);
                  }

                new_file = add_fcn_to_task_queue (file, sym_name, new_src);

                auto sym = std::make_shared<coder_symbol>(sym_name, new_src, new_file);

                directory_idx.insert (sym);
              }

            if (f != new_file)
              {
                old_files.push_back({f, new_file, sym_name});
              }
          }
      }

    auto& dep = dependency_graph.at(file);

    auto& dep_sym = external_symbols.at(file);

    for (const auto& of : old_files)
      {
        dep_sym.at(std::get<0>(of)).erase(std::get<2>(of));

        dep_sym[std::get<1>(of)].insert(std::get<2>(of));

        if (std::get<1>(of)) //prevent unresolved symbols to be inserted into dependency_graph
          dependency_graph[file].insert (std::get<1>(of));
      }

    auto it = dep_sym.begin ();

    while (it != dep_sym.end ())
      {
        if (it->second.empty ())
          {
            if (it->first)
              {
                dep.erase (it->first);
              }

            it = dep_sym.erase (it);
          }
        else
          it++;
      }

    if (! old_files.empty ())
      {
        state[file] = file_state::DependencyUpdated;

        task_queue.emplace_back(file);
      }
  }

  void
  semantic_analyser::update_globals( const std::string& sym_name)
  {
    auto global_file = std::make_shared<coder_file>("globals");

    auto range = dependent_files.equal_range(global_file);

    bool create_new_file = false;

    if(range.first != dependent_files.end() && range.first != range.second)
      {
        auto cache_file = std::find_if(range.first, range.second,
          [](const coder_file_ptr& file)
          {
            return
               file->type == file_type::global ;
          }
        );

        if (cache_file != range.second )
          {
            global_file = *cache_file;

            visited[global_file] = true;

            dependency_graph[current_file()].insert(global_file);

            for (const auto& var: global_file->local_functions)
              if (var.name() == sym_name)
                return;

            state[global_file] = file_state::Updated;

            global_file->add_new_local_function(sym_name);
          }
        else
          create_new_file = true;
      }
    else
      create_new_file = true;

    if (create_new_file)
      {
        global_file->id = 0;

        global_file->type = file_type::global;

        visited[global_file] = true;

        state[global_file] = file_state::New;

        global_file->add_new_local_function(sym_name);

        dependent_files.insert(global_file);

        dependency_graph[global_file];

        dependency_graph[current_file()].insert(global_file);
      }
  }

  coder_file_ptr
  semantic_analyser::init_builtins()
  {
    coder_file_ptr retval ;

    auto builtin_file = std::make_shared<coder_file>("builtins");

    auto range = dependent_files.equal_range(builtin_file);

    bool create_new_file = false;

    if (range.first != dependent_files.end() && range.first != range.second)
      {
        auto cache_file = std::find_if(range.first, range.second, [](const coder_file_ptr& file)
          {
            return
               file->type == file_type::builtin ;
          }
        );

        if (cache_file == range.second )
          {
            create_new_file = true;
          }
        else
          {
            visited[*cache_file] = true;

            retval = *cache_file;
          }
      }
    else
      create_new_file = true;

    if (create_new_file)
      {
        builtin_file->id = 0;

        builtin_file->type = file_type::builtin;

        visited[builtin_file] = true;

        state[builtin_file] = file_state::New;

        dependent_files.insert(builtin_file);

        dependency_graph[builtin_file];

        retval = builtin_file;
      }

    return retval;
  }
  coder_file_ptr
  semantic_analyser::current_file()
  {
    return *current_file_it;
  }

  bool
  semantic_analyser::should_generate(const coder_file_ptr& file) const
  {
    return state.at(file) != file_state::Old;
  }

  void
  semantic_analyser::mark_as_generated(const coder_file_ptr& file)
  {
    state.at(file) = file_state::Old;
  }

  std::pair<octave_fields, octave_fields>
  make_schema ()
  {
    static octave_fields file (string_vector(std::vector<std::string>
    {
      "name",
      "path",
      "id",
      "timestamp",
      "type",
      "local_functions",
      "dependencies",
      "unresolved_symbols"
    }));

    static octave_fields dependency(string_vector(std::vector<std::string>
    {
      "name",
      "id",
      "import"
    }));

    return {file, dependency};
  }

  void
  semantic_analyser::write_dep(const coder_file_ptr& file , const scalar_map_view& file_attr) const
  {
    static const std::vector <std::string> filetypes
    ({
      "m",
      "oct",
      "mex",
      "global",
      "builtin",
      "cmdline",
      "package" ,
      "classdef",
      "unknown"
    });

    std::pair<octave_fields, octave_fields> schema = make_schema ();

    octave_map dependencies_attr (schema.second);

    Cell local_functions;

    Cell unresolved_symbols;

    if (file->type == file_type::global)
      {
        local_functions = Cell (1, file->local_functions.size (), octave_value ());

        octave_idx_type i = 0;

        for (const auto& global_var : file->local_functions)
          local_functions(i++) = global_var.name();
      }

    if (file->type == file_type::oct)
      {
        local_functions = Cell (1, file->local_functions.size (), octave_value ());

        octave_idx_type i = 0;

        for (const auto& autoload : file->local_functions)
          local_functions(i++) = autoload.name();
      }

    if (file->type == file_type::m || file->type == file_type::cmdline)
      {
        size_t depsz = dependency_graph.at(file).size ();

        Cell dep_names (1, depsz, octave_value () );

        Cell dep_ids (1, depsz, octave_value () );

        Cell dep_imports (1, depsz, Cell () );

        octave_idx_type i = 0;

        for (const auto& dep : dependency_graph.at(file))
          {
            dep_names(i) = dep->name;

            dep_ids(i) = dep->id;

            if (dep->type != file_type::global)
              {
                size_t impsz  = external_symbols.at(file).at(dep).size ();

                Cell imports (1, impsz, octave_value ());

                octave_idx_type j = 0;

                for (const std::string& sym_name : external_symbols.at(file).at(dep))
                  imports(j++) = sym_name;

                dep_imports(i) = imports;
              }
            i++;
          }

        auto file_dependency = external_symbols.find(file);

        if (file_dependency!= external_symbols.end())
          {
            auto it = file_dependency->second.find(coder_file_ptr ());

            if (it != file_dependency->second.end ())
              {
                unresolved_symbols = Cell (1, it->second.size (), octave_value ());

                octave_idx_type i = 0;

                for (const auto& sym_name : it->second)
                  unresolved_symbols(i++) = sym_name;
              }
          }

        if (dep_names.numel () > 0)
          {
            dependencies_attr.resize (dep_names.dims ());

            dependencies_attr.setfield ("name", dep_names);

            dependencies_attr.setfield ("id", dep_ids);

            dependencies_attr.setfield ("import", dep_imports);
          }
      }

    file_attr.setfield ("name", file->name);

    file_attr.setfield ("path", file->path);

    file_attr.setfield ("id", file->id);

    file_attr.setfield ("timestamp", file->timestamp);

    file_attr.setfield ("type", filetypes[(int)file->type]);

    file_attr.setfield ("local_functions", local_functions);

    file_attr.setfield ("dependencies", dependencies_attr);

    file_attr.setfield ("unresolved_symbols", unresolved_symbols);
  }

  coder_file_ptr
  semantic_analyser::find_in_cache(const coder_file_ptr& file) const
  {
    auto range = dependent_files.equal_range(file);

    coder_file_ptr result;

    if (range.first != dependent_files.end() && range.first != range.second)
      {
        auto cache_file = std::find_if(range.first, range.second, [&file](const coder_file_ptr& f)
          {
            return
               f->id == file->id;
          }
        );

        if (cache_file != range.second )
          {
            result = *cache_file;
          }
      }

    return result;
  }


  coder_file_ptr
  semantic_analyser::read_dep(const scalar_map_const_view& file_attr)
  {
    static const std::map <std::string , file_type> filetypes     ({
      {"m", file_type::m },
      {"oct", file_type::oct },
      {"mex", file_type::mex },
      {"global", file_type::global },
      {"builtin", file_type::builtin },
      {"cmdline", file_type::cmdline },
      {"package" , file_type::package },
      {"classdef", file_type::classdef },
      {"unknown", file_type::unknown }
    });

    std::string name = file_attr.contents ("name").string_value ();

    int id = file_attr.contents ("id").int_value ();

    std::string path = file_attr.contents ("path").string_value ();

    time_t timestamp = file_attr.contents ("timestamp").idx_type_value ();

    file_type type = filetypes.at(file_attr.contents ("type").string_value ());

    std::deque<symtab> local_functions;

    if (type == file_type::global || type == file_type::oct)
      {
        Cell local_fcn = file_attr.contents ("local_functions").cell_value ();

        octave_idx_type n = local_fcn.numel ();

        for (octave_idx_type i = 0; i < n ; i++)
          {
            local_functions.emplace_back (local_fcn (i).string_value ());
          }
      }

    auto file = std::make_shared<coder_file>(name, id);

    auto res = find_in_cache(file);

    if (! res)
      {
        file->path = path;

        file->timestamp = timestamp;

        file->type = type;

        file->local_functions = std::move(local_functions);

        dependent_files.insert(file);

        dependency_graph[file] = {};
      }
    else
      {
        res-> name = name;

        res-> path = path;

        res-> id = id;

        res-> timestamp = timestamp;

        res-> type = type;

        res-> local_functions = std::move(local_functions);
      }

    if (type == file_type::m || type == file_type::cmdline)
      {
        if (! res)
          file->local_functions.emplace_back(name);
        else
          res->local_functions.emplace_back(name);

        external_symbols[res? res : file];

        std::unordered_set<coder_file_ptr> dependency;

        octave_map dep_map = file_attr.contents ("dependencies").map_value ();

        const Cell& dep_names = dep_map.contents (dep_map.seek ("name"));
        const Cell& dep_ids = dep_map.contents (dep_map.seek ("id"));
        const Cell& dep_imports = dep_map.contents (dep_map.seek ("import"));

        octave_idx_type n = dep_names.numel ();

        for (octave_idx_type i = 0; i < n ; i++)
          {
            std::string dep_name = dep_names (i).string_value ();

            int dep_id = dep_ids (i).int_value ();

            Cell sym_names = dep_imports (i).cell_value ();

            auto dep_file = std::make_shared<coder_file>(dep_name, dep_id);

            auto res_dep = find_in_cache(dep_file);

            if (! res_dep)
              {
                dependent_files.insert(dep_file);

                dependency_graph[dep_file] = {};
              }

            if (! (dep_name == "globals" && dep_id == 0))
              external_symbols[res? res : file][res_dep? res_dep : dep_file] = {};

            dependency.insert (res_dep? res_dep : dep_file);

            if (dep_name == "globals" && dep_id == 0)
              continue;

            auto& ext = external_symbols[res? res : file][res_dep? res_dep : dep_file];

            octave_idx_type n_sym = sym_names.numel ();

            for (octave_idx_type j = 0; j < n_sym; j++)
              ext.insert (sym_names (j).string_value ());
          }

        Cell unresolved_symbols = file_attr.contents ("unresolved_symbols").cell_value ();

        octave_idx_type n_unresolved = unresolved_symbols.numel ();

        if (n_unresolved > 0)
          {
            coder_file_ptr unknown;

            auto& ext = external_symbols[res? res : file][unknown];

            for (octave_idx_type k = 0 ; k < n_unresolved; k++)
              {
                ext.insert(unresolved_symbols (k).string_value ());
              }
          }

        dependency_graph[res? res : file] = std::move(dependency);
      }

    return res?res:file;
  }

  std::map<std::string, coder_file_ptr>
  semantic_analyser::read_oct_list (const octave_map& dyn_oct_list) const
  {
    std::map<std::string, coder_file_ptr> oct_map;

    const Cell& names = dyn_oct_list.contents ("name");
    const Cell& ids = dyn_oct_list.contents ("id");
    const Cell& imports = dyn_oct_list.contents ("import");

    octave_idx_type n = dyn_oct_list.numel ();

    for (octave_idx_type i = 0 ; i < n; i++)
      {
        std::string name = names(i).string_value ();

        int id = ids(i).int_value ();

        std::string sym_name = imports(i).string_value ();

        auto dep_file = std::make_shared<coder_file>(name, id);

        auto res_dep = find_in_cache(dep_file);

        if (res_dep)
          oct_map[sym_name] = res_dep;
      }

    return oct_map;
  }

  octave_map
  semantic_analyser::write_oct_list (const std::map<std::string, coder_file_ptr>& oct_map) const
  {
    auto schema = make_schema ();

    octave_idx_type n = oct_map.size ();

    octave_map dyn_oct_list(dim_vector(1, n), schema.second);

    Cell& names = dyn_oct_list.contents ("name");
    Cell& ids = dyn_oct_list.contents ("id");
    Cell& imports = dyn_oct_list.contents ("import");

    octave_idx_type i = 0;

    for (const auto& oct : oct_map)
      {
        names(i) = oct.second->name;

        ids(i) = oct.second->id;

        imports(i) = oct.first;

        i++;
      }

    return dyn_oct_list;
  }

  void
  semantic_analyser::build_dependency_graph (const octave_map& cache_index)
  {
    octave_idx_type n = cache_index.numel ();

    for (octave_idx_type i = 0; i < n; i++)
      {
        auto file = read_dep (scalar_map_const_view(i, cache_index));

        state[file] = file_state::Old;

        visited[file] = false;
      }
  }

  octave_value
  semantic_analyser::find_function (const std::string& name)
  {
    return scope_stack.top ().find_function (name);
  }

  static std::string
  extract_valid_name (const std::string& n)
  {
    size_t s = n.size ();

    if(s>2 && n[s-1]=='m'&& n[s-2] == '.')
      {
        return n.substr(0, s-2);
      }

    if ((s > 1 && ( n[0] == '@' || n[0] == '+')))
      {
        return n.substr(1, s-1);
      }

    if(s>4 && n[s-1]=='t'&& n[s-2] == 'c' && n[s-3] == 'o' && n[s-4] == '.')
      {
        return n.substr(0, s-4);
      }

    if(s>4 && n[s-1]=='x'&& n[s-2] == 'e' && n[s-3] == 'm' && n[s-4] == '.')
      {
        return n.substr(0, s-4);
      }

    return "" ;
  }

  void semantic_analyser::find_resolvabale_names_in_octave_path ()
  {
    path_map["<>"] = std::shared_ptr<std::set<std::string>> {new std::set<std::string> {}};

    resolvable_path_names = path_map["<>"];

    octave::symbol_table& octave_symtab = octave::interpreter::the_interpreter ()->get_symbol_table();

    octave::load_path& lp = octave::interpreter::the_interpreter ()->get_load_path ();

    octave::tree_evaluator& ev = octave::interpreter::the_interpreter ()->get_evaluator ();

    string_vector bif = octave_symtab.built_in_function_names ();

    string_vector cmd = octave_symtab.cmdline_function_names ();

    std::list<std::string> autoloads = ev.autoloaded_functions ();

    string_vector dirs = lp.dirs ();

    octave_idx_type len = dirs.numel ();

    for (octave_idx_type i = 0; i < len; i++)
      {
        std::string d = dirs[i];

        auto& syms = path_map[d];

        syms =  std::shared_ptr<std::set<std::string>> {new std::set<std::string> {}};

        find_resolvabale_names (resolvable_path_names, d);
      }

    len = bif.numel ();

    for (octave_idx_type i = 0; i < len; i++)
      {
        resolvable_path_names->insert (bif(i));
      }

    len = cmd.numel ();

    for (octave_idx_type i = 0; i < len; i++)
      {
        resolvable_path_names->insert (cmd(i));
      }

    for (auto& fcn : autoloads)
      {
        resolvable_path_names->insert (fcn);
      }
  }
  void semantic_analyser::find_resolvabale_names (std::shared_ptr<std::set<std::string>> resolvable_names, const std::string& d, bool is_private)
  {
    octave::sys::dir_entry dir (d);

    if (dir)
      {
        string_vector flist = dir.read ();

        octave_idx_type len = flist.numel ();

        bool has_private_subdir = false;

        for (octave_idx_type i = 0; i < len; i++)
          {
            std::string n = flist[i];

            if (! is_private && n == "private")
              {
                has_private_subdir = true;
                continue;
              }

            std::string sym = extract_valid_name (n);

            if (sym.empty ())
              continue;

            resolvable_names->insert (sym);
          }

        if (has_private_subdir)
          {
            std::string pv_dir = octave::sys::file_ops::concat (d, "private");

            octave::sys::file_stat fs (pv_dir);

            if (fs && fs.is_dir ())
              {
                auto& pv_set = path_map[d];

                pv_set = std::shared_ptr<std::set<std::string>> {new std::set<std::string> {}};

                path_map[pv_dir] = pv_set;

                find_resolvabale_names (pv_set, pv_dir, true);
              }
          }
      }
  }
}
