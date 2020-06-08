#include "sys/stat.h"

#include <octave/builtin-defun-decls.h>
#include <octave/dir-ops.h>
#include <octave/file-ops.h>
#include <octave/file-stat.h>
#include <octave/oct.h>
#include <octave/oct-env.h>
#include <octave/ov-usr-fcn.h>

#include "semantic_analyser.h"

namespace coder_compiler
{
  file_time::file_time(const std::string& filename)
  : ok(false), m_mtime(0)
  {
    struct stat st;
    int result = ::stat(filename.c_str (), &st);
    if (! result)
      m_mtime = st.st_mtime;

    ok = result == 0;
  }

  semantic_analyser::semantic_analyser(  )
  : m_do_lvalue_check(false)
  {
    init_builtins();
  }

  semantic_analyser::semantic_analyser( const octave_map& cache_index )
  : m_do_lvalue_check(false)
  {
    build_dependency_graph (cache_index);
    init_builtins();
  }

  void
  semantic_analyser::visit_argument_list (octave::tree_argument_list& lst)
  {
    octave::tree_argument_list::iterator p = lst.begin ();

    while (p != lst.end ())
      {
        octave::tree_expression *elt = *p++;

        if (elt)
          {
            if (m_do_lvalue_check && ! elt->lvalue_ok ())
              error ("invalid lvalue in multiple assignment");

            elt->accept (*this);
          }
      }
  }

  void
  semantic_analyser::visit_simple_for_command (octave::tree_simple_for_command& cmd)
  {
    octave::tree_expression *lhs = cmd.left_hand_side ();

    if (lhs)
      {
        if (! lhs->lvalue_ok ())
          error ("invalid lvalue in for command");

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
  semantic_analyser::visit_simple_assignment (octave::tree_simple_assignment& expr)
  {
    octave::tree_expression *lhs = expr.left_hand_side ();

    if (lhs)
      {
        if (! lhs->lvalue_ok ())
          error ("invalid lvalue in assignment");

        lhs->accept (*this);
      }

    octave::tree_expression *rhs = expr.right_hand_side ();

    if (rhs)
      rhs->accept (*this);
  }

  void
  semantic_analyser::visit_try_catch_command (octave::tree_try_catch_command& cmd)
  {
    octave::tree_statement_list *try_code = cmd.body ();

    octave::tree_identifier *expr_id = cmd.identifier ();

    if (expr_id)
      {
        if (! expr_id->lvalue_ok ())
          error ("invalid lvalue used for identifier in try-catch command");

        expr_id->accept (*this);
      }

    if (try_code)
      try_code->accept (*this);

    octave::tree_statement_list *catch_code = cmd.cleanup ();

    if (catch_code)
      catch_code->accept (*this);
  }

  void
  semantic_analyser::visit_handle (octave::tree_anon_fcn_handle& anon_fh)
  {
    handles.emplace();

    auto& table = current_file()->current_local_function();

    octave::tree_parameter_list *param_list = anon_fh.parameter_list ();

    if (param_list)
      {
        for (octave::tree_decl_elt *elt : *param_list)
          {
            octave::tree_identifier *id = elt->ident ();

            if (id)
              {
                  insert_symbol(id->name(), symbol_type::ordinary);
              }
          }

        bool takes_varargs = param_list->takes_varargs ();

        if (takes_varargs)
          insert_symbol("varargin", symbol_type::ordinary);

        for (octave::tree_decl_elt *elt : *param_list)
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
    octave::symbol_scope scope = fcnn.scope() ;

    octave::symbol_table& octave_symtab = octave::interpreter::the_interpreter ()->get_symbol_table();

    octave::symbol_scope current_octave_scope = octave_symtab.current_scope();

    octave_symtab.set_scope(scope);

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
        for (octave::tree_decl_elt *elt : *ret_list)
          {
            if(elt)
              {
                octave::tree_identifier *id = elt->ident ();

                if (id)
                  {
                    insert_symbol(id->name(), symbol_type::ordinary);
                  }
              }
          }

        bool takes_var_return = fcnn.takes_var_return ();

        if (takes_var_return)
          insert_symbol("varargout", symbol_type::ordinary);

      }

    if (param_list)
      {
        for (octave::tree_decl_elt *elt : *param_list)
          {
            if(elt)
              {
                octave::tree_identifier *id = elt->ident ();
                if (id && !id->is_black_hole())
                  {
                    insert_symbol(id->name(), symbol_type::ordinary);
                  }
              }
          }

        bool takes_varargs = param_list->takes_varargs ();

        if (takes_varargs)
          insert_symbol("varargin", symbol_type::ordinary);
      }

    handles.emplace();

    if (param_list)
      {
        for (octave::tree_decl_elt *elt : *param_list)
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

            if (!ufc.is_nested_function() && !ufc.is_private_function())
              {
                current_file()->add_new_local_function(nm);

                ufc.accept(*this);
              }
          }
      }

    octave_symtab.set_scope(current_octave_scope);

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

    insert_symbol(nm);
  }

  coder_file_ptr
  semantic_analyser::analyse(const std::string& name, const octave_value& fcn)
  {
    task_queue.clear ();

    coder_file_ptr start_node = add_fcn_to_task_queue(coder_file_ptr (), name, fcn);

    for(current_file_it = task_queue.begin(); current_file_it != task_queue.end(); ++current_file_it)
      {
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

                    if (!ufc.is_nested_function() && !ufc.is_private_function())
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
              }
            else
              {
                octave::symbol_table& octave_symtab = octave::interpreter::the_interpreter ()->get_symbol_table();

                fcn = octave_symtab.find_function (name, octave_value_list ());

                file = add_fcn_to_task_queue(current_file (), name, fcn);
              }

            sym = std::make_shared<coder_symbol>(name, fcn, file);

            table.insert_symbol(sym, symbol_type::ordinary);
          }
        else
          {
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
          }
        else
          {
            octave::symbol_table& octave_symtab = octave::interpreter::the_interpreter ()->get_symbol_table();

            fcn = octave_symtab.find_function (name, octave_value_list ());

            file = add_fcn_to_task_queue(current_file (), name, fcn);
          }

        sym = std::make_shared<coder_symbol>(name, fcn, file);
      }

    table.insert_symbol(sym, type);

    return sym;
  }

  coder_file_ptr
  semantic_analyser::add_fcn_to_task_queue (const coder_file_ptr& this_file, const std::string& sym_name, const octave_value& val)
  {
    std::string name; // filename

    std::string path; // filepath

    file_type type = file_type::unknown;

    std::tie( type, name, path) = find_file_type_name_and_path(val,sym_name);

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

    octave::symbol_scope scope = fcnn->scope() ;

    octave::symbol_table& octave_symtab = octave::interpreter::the_interpreter ()->get_symbol_table();

    octave::symbol_scope current_octave_scope = octave_symtab.current_scope();

    octave_symtab.set_scope(scope);

    auto dep_old = external_symbols.at(file);

    for (const auto& callee : dep_old)
      {
        const auto& f = callee.first;

        for (const auto& sym_name : callee.second)
          {
            octave_value new_src = octave_symtab.find_function (sym_name, octave_value_list ());

            coder_file_ptr new_file = add_fcn_to_task_queue (file, sym_name, new_src);

            if (f != new_file)
              {
                old_files.push_back({f, new_file,sym_name});
              }
          }
      }

    octave_symtab.set_scope(current_octave_scope);

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
}
