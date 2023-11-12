#include  <bitset>
#include  <memory>

#include <octave/defaults.h>
#include <octave/error.h>
#include <octave/file-ops.h>
#include <octave/input.h>
#include <octave/oct.h>
#include <octave/ov-fcn-handle.h>
#include <octave/ov-usr-fcn.h>
#include <octave/ov-range.h>
#include <octave/pt-all.h>
#include <octave/utils.h>
#include <octave/version.h>

#include "code_generator.h"
#include "coder_file.h"
#include "lvalue_checker.h"

#if OCTAVE_MAJOR_VERSION >= 7
  #define OCTAVE_DEPR_NS octave::
  #define OCTAVE_RANGE octave::range<double>
#else
  #define OCTAVE_DEPR_NS ::
  #define OCTAVE_RANGE ::Range
#endif

namespace coder_compiler
{
  namespace
  {
    struct delimiter
    {
      delimiter(const std::string& del = ""): next_del(del) {}

      friend std::ostream& operator <<(std::ostream& os, delimiter& del)
      {
        auto& cur_del = del.strms[&os];

        if(! del.next_del.empty())
          os << cur_del;

        cur_del = del.next_del;

        return os;
      }

      delimiter& operator()(const std::string& del = "")
      {
        next_del = del;
        return *this;
      }

    private:

      std::unordered_map<std::ostream*, std::string> strms;
      std::string next_del;

    };
  }

  int IndentingOStreambuf::overflow( int ch )
  {
    if ( myIsAtStartOfLine && ch != '\n' )
      {
        myDest->sputn( myIndent.data(), myIndent.size() );
      }

    myIsAtStartOfLine = ch == '\n';

    return myDest->sputc( ch );
  }

  void IndentingOStreambuf::release ()
  {
    if ( myOwner != NULL )
      {
        myOwner->rdbuf( myDest );
      }
  }

  void IndentingOStreambuf::reset ()
  {
    if ( myOwner != NULL )
      {
        myOwner->rdbuf( myDest );

        myOwner->rdbuf( this );
      }
  }

  void IndentingOStreambuf::inc ()
  {
    reset ();

    myIndent = std::string( myIndent.size () + indentSz, ' ' );
  }

  void IndentingOStreambuf::dec ()
  {
    reset ();

    myIndent = std::string( myIndent.size () - indentSz, ' ' );
  }

  IndentingOStreambuf::~IndentingOStreambuf()
  {
    if ( myOwner != NULL )
      {
        myOwner->rdbuf( myDest );
      }
  }

  std::string
  capitalize (const std::string& str)
  {
    size_t num_cap2 = 0;

    for (char c : str)
      if (c == '[' || c == ']')
        num_cap2++;

    if (num_cap2 == 0)
      return str;

    std::string result (str.size () - num_cap2, ' ');

    size_t i = 0;

    bool begin_capitalize = false;

    for (char c : str)
      {
        if (c == '[')
          {
            begin_capitalize = true;

            continue;
          }
        else if (c == ']')
          {
            begin_capitalize = false;

            continue;
          }

        if (begin_capitalize)
          {
            result[i++] = std::toupper (c);
          }
        else
          result[i++] = c;
      }

    return result;
  }

  std::string
  lowercase (const std::string& str)
  {
    size_t num_cap = 0;

    bool up = false;

    for (char c : str)
      {
        bool tmp = std::isupper (c);

        if (! up && tmp)
          num_cap++;

        up = tmp;
      }

    if (num_cap == 0)
      return str;

    std::string result (str.size () + num_cap * 2,' ');

    size_t i = 0;

    bool begin_lowercase = false;

    for (char c : str)
      {
        if (isupper (c))
          {
            if (! begin_lowercase)
              {
                begin_lowercase = true;

                result[i++] = '[';
              }

            result[i++] = std::tolower (c);
          }
        else
          {
            if (begin_lowercase)
              {
                begin_lowercase = false;

                result[i++] = ']';
              }

            result[i++] = c;
          }
      }

    if (begin_lowercase)
      result[i++] = ']';

    return result;
  }

  std::string
  undo_string_escapes1 (char c)
  {
    switch (c)
      {
      case '\0':
        return R"(\0)";

      case '\a':
        return R"(\a)";

      case '\b': // backspace
        return R"(\b)";

      case '\f': // formfeed
        return R"(\f)";

      case '\n': // newline
        return R"(\n)";

      case '\r': // carriage return
        return R"(\r)";

      case '\t': // horizontal tab
        return R"(\t)";

      case '\v': // vertical tab
        return R"(\v)";

      case '\\': // backslash
        return R"(\\)";

      case '"': // double quote
        return R"(\")";

      default:
        return std::string (1, c);
      }
  }

  std::string
  undo_string_escapes1 (const std::string& s)
  {
    std::string retval;

    for (size_t i = 0; i < s.length (); i++)
      retval.append (undo_string_escapes1 (s[i]));

    return retval;
  }

  code_generator::code_generator( const coder_file_ptr& file,
    const dgraph& dependency_graph, std::iostream& header, std::iostream& source,
    std::iostream& partial_source, generation_mode mode )
  :
    m_file(file),
    dependency_graph(dependency_graph),
    traversed_scopes (m_file->traverse()),
    loop_or_unwind (),
    nconst(0),
    constant_map(),
    os_hdr_ext(header),
    os_src_ext(source),
    os_prt_ext(partial_source),
    mode(mode),
    nesting_context(0),
    nested_fcn_names()
  {
    loop_or_unwind.push_back (normal_context);

    streams.insert ({&os_hdr, IndentingOStreambuf(os_hdr, 2)});

    streams.insert ({&os_src, IndentingOStreambuf(os_src, 2)});

    streams.insert ({&os_prt, IndentingOStreambuf(os_prt, 2)});
  }

  static std::string quote(const std::string& str)
  {
    return "\"" + str + "\"";
  }

  void code_generator::increment_indent_level (std::ostream& st)
  {
    auto it = streams.find(&st);

    if (it == streams.end ())
      streams.insert ({&st, IndentingOStreambuf(st, 2)}).first->second.inc ();
    else
      it->second.inc ();
  }

  void code_generator::decrement_indent_level (std::ostream& st)
  {
    auto it = streams.find(&st);

    if (it == streams.end ())
      streams.insert ({&st, IndentingOStreambuf(st, 2)}).first->second.dec ();
    else
      it->second.dec ();
  }

  void code_generator::release (std::ostream& st)
  {
    auto it = streams.find(&st);

    if (it != streams.end ())
      it->second.release ();
  }

  void
  code_generator::visit_anon_fcn_handle (octave::tree_anon_fcn_handle& anon_fh)
  {
    os_src
      << "Anonymous (fcn2ov(["
      << "="
      << "](coder_value_list& output, const octave_value_list& args, int nargout) mutable\n{\n";

    increment_indent_level (os_src);

    fcn_scopes.push_back (traversed_scopes.front()[1].front());

    declare_and_define_handle_variables();

    octave::tree_parameter_list *param_list = anon_fh.parameter_list ();

    if (param_list)
      {
        bool takes_varargs = param_list->takes_varargs ();

        int len = param_list->length ();

        if (len > 0 || takes_varargs)
          {
            os_src << "define_parameter_list_from_arg_vector(";
          }

        if (len > 0)
          param_list->accept (*this);

        if (takes_varargs)
          {
            if (len > 0)
              os_src << ", ";

            os_src << mangle("varargin");
          }

        if (len > 0 || takes_varargs)
          {
            os_src << ", args);\n";
          }
      }

    octave::tree_expression *expr = anon_fh.expression ();

    os_src << "return make_return_val(output";

    if (expr)
      {
        os_src << ", ";

        expr->accept (*this);

        os_src << ", nargout";
      }
    os_src << ");\n";

    decrement_indent_level (os_src);

    os_src << "}))";

    fcn_scopes.pop_back ();
  }

  void
  code_generator::visit_argument_list (octave::tree_argument_list& lst)
  {
    octave::tree_argument_list::iterator p = lst.begin ();

    while (p != lst.end ())
      {
        octave::tree_expression *elt = *p++;

        if (elt)
          {
            elt->accept(*this);
          }

        if (p != lst.end ())
          os_src << ", ";
      }
  }

  void
  code_generator::visit_binary_expression (octave::tree_binary_expression& expr)
  {
    octave::tree_expression *op_lhs = expr.lhs ();

    octave::tree_expression *op_rhs = expr.rhs ();

    octave_value::binary_op etype = expr.op_type ();

    auto do_binary = [&](const std::string& op)
    {
      os_src << op <<" (" ;
        op_lhs->accept (*this);

      os_src << "," ;
        op_rhs->accept (*this) ;

      os_src << ")";
    };

    if (op_lhs && op_rhs)
      switch (etype)
      {
        case octave_value::op_add:            // plus
          do_binary("Plus");
          break;

        case octave_value::op_sub:            // minus
          do_binary("Minus");
          break;

        case octave_value::op_mul:            // mtimes
          do_binary("Mtimes");
          break;

        case octave_value::op_div:            // mrdivide
          do_binary("Mrdivide");
          break;

        case octave_value::op_pow:            // mpower
          do_binary("Mpower");
          break;

        case octave_value::op_ldiv:           // mldivide
          do_binary("Mldivide");
          break;

        case octave_value::op_lt:             // lt
          do_binary("Lt");
          break;

        case octave_value::op_le:             // le
          do_binary("Le");
          break;

        case octave_value::op_eq:             // eq
          do_binary("Eq");
          break;

        case octave_value::op_ge:             // ge
          do_binary("Ge");
          break;

        case octave_value::op_gt:             // gt
          do_binary("Gt");
          break;

        case octave_value::op_ne:             // ne
          do_binary("Ne");
          break;

        case octave_value::op_el_mul:         // times
          do_binary("Times");
          break;

        case octave_value::op_el_div:         // rdivide
          do_binary("Rdivide");
          break;

        case octave_value::op_el_pow:         // power
          do_binary("Power");
          break;

        case octave_value::op_el_ldiv:        // ldivide
          do_binary("Ldivide");
          break;

        case octave_value::op_el_and:         // and
          do_binary("And");
          break;

        case octave_value::op_el_or:          // or
          do_binary("Or");
          break;

        case octave_value::op_struct_ref:
          break;

        default:
          break;
      }
  }
  void
  code_generator::visit_boolean_expression (octave::tree_boolean_expression& expr)
  {
    octave_value val;

    octave::tree_expression *op_lhs = expr.lhs ();

    octave::tree_expression *op_rhs = expr.rhs ();

    if (op_lhs && op_rhs)
      {
        octave::tree_boolean_expression::type etype = expr.op_type ();

        if (etype == octave::tree_boolean_expression::bool_or)
          {
            os_src << "OrOr" <<" (" ;

              op_lhs->accept (*this);

            os_src << "," ;

              op_rhs->accept (*this) ;

            os_src << ")";
          }
        else if (etype == octave::tree_boolean_expression::bool_and)
          {
            os_src << "AndAnd" <<" (" ;

              op_lhs->accept (*this);

            os_src << "," ;

              op_rhs->accept (*this) ;

            os_src << ")";
          }
      }
  }

  void
  code_generator::visit_compound_binary_expression (octave::tree_compound_binary_expression& expr)
  {
    octave::tree_expression *op_lhs = expr.clhs ();

    octave::tree_expression *op_rhs = expr.crhs ();

    octave_value::compound_binary_op etype = expr.cop_type ();

    auto do_binary = [&](const std::string& op)
    {
      os_src << op <<" (" ;

        op_lhs->accept (*this);

      os_src << "," ;

        op_rhs->accept (*this) ;

      os_src << ")";
    };

    if (op_lhs && op_rhs)
      switch (etype)
      {
        case octave_value::op_trans_mul:
          do_binary("TransMul");
          break;
        case octave_value::op_mul_trans:
          do_binary("MulTrans");
          break;
        case octave_value::op_herm_mul:
          do_binary("HermMul");
          break;
        case octave_value::op_mul_herm:
          do_binary("MulHerm");
          break;
        case octave_value::op_trans_ldiv:
          do_binary("TransLdiv");
          break;
        case octave_value::op_herm_ldiv:
          do_binary("HermLdiv");
          break;
        case octave_value::op_el_not_and:
          do_binary("NotAnd");
          break;
        case octave_value::op_el_not_or:
          do_binary("NotOr");
          break;
        case octave_value::op_el_and_not:
          do_binary("AndNot");
          break;
        case octave_value::op_el_or_not:
          do_binary("OrNot");
          break;
        default:
          break;
      }
  }

  void
  code_generator::visit_break_command (octave::tree_break_command&)
  {
    if (loop_or_unwind.back () == unwinding_context)
      {
        os_src << "throw break_unwind {};\n";
      }
    else
      os_src << "break;\n";
  }

  void
  code_generator::visit_colon_expression (octave::tree_colon_expression& expr)
  {
    octave::tree_expression *op_base = expr.base ();

    octave::tree_expression *op_limit = expr.limit ();

    octave::tree_expression *op_increment = expr.increment ();

    if (! op_base || ! op_limit)
      {
        os_src << "Symbol()" ;
        return;
      }

    if ( op_base &&  op_limit)
      {
        if (op_increment)
          {
            os_src << "Colon (" ;

            op_base->accept(*this);

            os_src << "," ;

            op_increment->accept(*this) ;

            os_src << "," ;

            op_limit->accept(*this) ;

            os_src << ")";
          }
        else
          {
            os_src  << "Colon (";

            op_base->accept(*this);

            os_src  << ",";

            op_limit->accept(*this);

            os_src  << ")";
          }
      }
  }

  void
  code_generator::visit_continue_command (octave::tree_continue_command&)
  {
    if (loop_or_unwind.back () == unwinding_context)
      {
        os_src << "throw continue_unwind {};\n";
      }
    else
      os_src << "continue;\n";
  }

  void
  code_generator::visit_decl_command (octave::tree_decl_command& cmd)
  {
    octave::tree_decl_init_list *init_list = cmd.initializer_list ();

    if (init_list)
      init_list->accept (*this);
  }

  void
  code_generator::visit_decl_init_list (octave::tree_decl_init_list& lst)
  {
    octave::tree_decl_init_list::iterator p = lst.begin ();

    while (p != lst.end ())
      {
        octave::tree_decl_elt *elt = *p++;

        if (elt)
          elt->accept (*this);
      }
  }

  void
  code_generator::visit_decl_elt (octave::tree_decl_elt& elt)
  {
    octave::tree_identifier *id = elt.ident ();

    octave::tree_expression *expr = elt.expression ();

    if (id)
      {
        if (expr)
          {
            if (elt.is_global ())
              {
                os_src
                  << "GLOBALASSIGN("
                  << mangle(id->name())
                  << " , ";
                  expr->accept(*this);

                os_src
                  << ")\n";
              }
            else if (elt.is_persistent ())
              {
                os_src
                  << "PERSISTENTASSIGN("
                  << mangle(id->name())
                  << " , ";
                  expr->accept(*this);

                os_src
                  << ")\n";
              }
            else
              {
                os_src << "{" << mangle(id->name())  << "," ;

                expr->accept(*this);

                os_src << "}";
              }
          }
        else
          {
            if (elt.is_global ())
              {
                os_src
                  << "GLOBAL("
                  << mangle(id->name())
                  << ")\n";
              }
            else if (elt.is_persistent ())
              {
                os_src
                  << "PERSISTENT("
                  << mangle(id->name())
                  << ")\n";
              }
            else
              {
                os_src << "{";
                if(id->is_black_hole())
                  os_src << "Tilde()";
                else
                  os_src<< mangle(id->name());
                os_src << "}" ;
              }
          }
      }
  }

  static std::string extract_loop_var ( octave::tree_expression *expr)
  {
    if (expr)
      {
        if (expr->is_index_expression ())
          {
            octave::tree_index_expression *idx = static_cast<octave::tree_index_expression *> (expr);

            if (idx)
              {
                octave::tree_expression *e = idx->expression ();

                if (e && e->is_identifier ())
                  {
                    return static_cast<octave::tree_identifier *> (e)->name ();
                  }
              }
          }
        else if (expr->is_identifier ())
          {
            return static_cast<octave::tree_identifier *> (expr)->name ();
          }
      }

    return {};
  }

  void
  code_generator::visit_simple_for_command (octave::tree_simple_for_command& cmd)
  {
    octave::tree_expression *lhs = cmd.left_hand_side ();

    octave::tree_expression *expr = cmd.control_expr ();

    octave::tree_statement_list *list = cmd.body ();

    int fast_loop = 1;

    if (list)
      {
        try
          {
            lvalue_checker {m_file , list, extract_loop_var (lhs)};
          }
        catch (int v)
          {
            fast_loop = int(v == 0);
          }
      }

    os_src
      << "for (auto i : for_loop (" ;

    lhs->accept(*this) ;

    os_src << " , " ;

    expr->accept(*this) ;

    loop_or_unwind.push_back(looping_context);

    os_src
      << ", "
      << fast_loop
      << "))\n" ;

    increment_indent_level (os_src);

    os_src << "{\n";

    increment_indent_level (os_src);

    if(list) list->accept(*this);

    decrement_indent_level (os_src);

    os_src
      << "}\n";

    decrement_indent_level (os_src);

    loop_or_unwind.pop_back();
  }

  void
  code_generator::visit_complex_for_command (octave::tree_complex_for_command& cmd)
  {
    octave::tree_argument_list *lhs = cmd.left_hand_side ();

    octave::tree_argument_list::iterator p = lhs->begin ();

    octave::tree_expression *v = *p++;

    octave::tree_expression *k = *p;

    octave::tree_expression *expr = cmd.control_expr ();

    octave::tree_statement_list *list = cmd.body ();

    os_src
      << "for (auto i : struct_loop (" ;

    if(v) v->accept(*this) ;

    os_src
      << " , " ;

    if(k) k->accept(*this) ;

    os_src
      << " , " ;

    if(expr) expr->accept(*this) ;

    loop_or_unwind.push_back(looping_context);

    os_src
      << "))\n" ;

    increment_indent_level (os_src);

      os_src << "{\n";

    increment_indent_level (os_src);

    if(list) list->accept(*this);

    decrement_indent_level (os_src);

    os_src
      << "}\n";

    decrement_indent_level (os_src);

    loop_or_unwind.pop_back ();
  }

#if OCTAVE_MAJOR_VERSION >= 7
  void
  code_generator::visit_spmd_command (octave::tree_spmd_command& cmd)
  {
    octave::tree_statement_list *body = cmd.body ();

    if (body)
      body->accept (*this);
  }

  void
  code_generator::visit_arguments_block (octave::tree_arguments_block&){}

  void
  code_generator::visit_args_block_attribute_list (octave::tree_args_block_attribute_list&){}

  void
  code_generator::visit_args_block_validation_list (octave::tree_args_block_validation_list&){}

  void
  code_generator::visit_arg_validation (octave::tree_arg_validation&){}

  void
  code_generator::visit_arg_size_spec (octave::tree_arg_size_spec&){}

  void
  code_generator::visit_arg_validation_fcns (octave::tree_arg_validation_fcns&){}
#endif

  void
  code_generator::visit_octave_user_script (octave_user_script&)
  {
    warning("user script is not supported!");
  }

  void
  code_generator::visit_octave_user_function (octave_user_function& fcn)
  {
    fcn_scopes.push_back (traversed_scopes.front()[0].front());

    visit_octave_user_function_header (fcn);

    octave::tree_statement_list *cmd_list = fcn.body ();

    if (cmd_list)
      {
        cmd_list->accept (*this);
      }

    visit_octave_user_function_trailer (fcn);

    fcn_scopes.pop_back ();
  }

  void
  code_generator::visit_octave_user_function_header (octave_user_function& fcn)
  {
    octave::comment_list *leading_comment = fcn.leading_comment ();

    if (leading_comment)
      {
        //print_comment_list (leading_comment);
      }

    bool nested  = fcn.is_nested_function() ;

    if(nested )
      {
				increment_indent_level (os_src);

				declare_persistent_variables ();

        declare_and_define_nested_variables();

				std::string pers =  init_persistent_variables();

				define_nested_functions(fcn);

				os_src << pers;
      }
    else
      {
        os_src
          << "Symbol (fcn2ov(["
          << "](coder_value_list& output, const octave_value_list& args, int nargout)\n{\n";

        increment_indent_level (os_src);

				declare_persistent_variables();

				declare_and_define_variables();

				std::string pers =  init_persistent_variables();

				define_nested_functions(fcn);

				os_src << pers;
      }

    octave::tree_parameter_list *param_list = fcn.parameter_list ();

    if (param_list)
      {
        bool takes_varargs = fcn.takes_varargs ();

        int len = param_list->length ();

        if (len > 0 || takes_varargs)
          {
            os_src << "define_parameter_list_from_arg_vector(";
          }

        if (len > 0)
          param_list->accept (*this);

        if (takes_varargs)
          {
            if (len > 0)
              os_src << ", ";

            os_src << mangle("varargin");
          }

        if (len > 0 || takes_varargs)
          {
            os_src << ", args);\n";
          }
      }
  }

  static bool
  is_system_fcn_file (octave_function* fcn)
  {
#if (OCTAVE_MAJOR_VERSION < 6 || (OCTAVE_MAJOR_VERSION == 6 && OCTAVE_MINOR_VERSION > 2) || OCTAVE_MAJOR_VERSION > 6)
    return fcn->is_system_fcn_file ();
#else
    bool retval = false;

    octave_user_function *ufn = nullptr;

    std::string file_name;

    if (fcn->is_user_function ())
      ufn = fcn->user_function_value ();

    if (ufn)
      {
        file_name = ufn->fcn_file_name ();
      }

    if (! file_name.empty ())
      {
        std::string ff_name = octave::fcn_file_in_path (file_name);

        static const std::string fcn_file_dir = octave::sys::canonicalize_file_name (octave::config::fcn_file_dir ());

        if (fcn_file_dir == ff_name.substr (0, fcn_file_dir.length ()))
          retval = true;
      }

    return retval;
#endif
  }

  void
  code_generator::declare_and_define_nested_variables()
  {
    static const std::map<std::string,std::string> special_functions ({
      {"nargin", "NARGIN"},
      {"nargout", "NARGOUT"},
      {"isargout", "ISARGOUT"},
      {"narginchk", "NARGINCHK"},
      {"nargoutchk", "NARGOUTCHK"}
    });

    delimiter sep;

    const auto& scope = traversed_scopes.front()[0].front()->symbols();

    auto scope_searcher = traversed_scopes.front()[0].front();

    os_src
      << "ConstCast (Symbol (fcn2ov (ConstCast ([=";

    os_src
      << "](coder_value_list& output, const octave_value_list& args, int nargout) mutable\n{\n";

		increment_indent_level (os_src);

		declare_persistent_variables ();

    for(const auto& symbol : scope[(int)symbol_type::ordinary])
      {
        os_src
          << "Symbol "
          << mangle(symbol->name) ;

        auto f = special_functions.find(symbol->name);

        if (f != special_functions.end())
          {
            if (symbol->fcn.is_function())
              {
                octave_function* fcn = symbol->fcn.function_value(true);

                if
                (
                  fcn
                  &&
                  (symbol->fcn.is_builtin_function() || is_system_fcn_file(fcn))
                )
                  {
                  }
                else if (symbol->file )
                  {
                    os_src
                      << " = Copy (";

                    os_src
                      << mangle(symbol->file->name)
                      << symbol->file->id;

                    os_src
                      << "::"
                      << mangle(symbol->name)
                      << "make())";
                  }
              }
          }
        else if (symbol->file )
          {
            os_src
              << " = Copy (";

            os_src
              << mangle(symbol->file->name)
              << symbol->file->id;

            os_src
              << "::"
              << mangle(symbol->name)
              << "make())";
          }

        os_src << ";\n";
      }

    static std::vector<std::string> special_names  =
    {
      "nargin",
      "nargout",
      "isargout",
      "narginchk",
      "nargoutchk"
    };

    for (auto& name : special_names)
			{
			  if (scope_searcher->contains(name))
				os_src
					<< ([name]  () mutable {for (auto & c: name) c = toupper(c); return name;})()
					<< "_MAKER;\n";
			}
  }
  void
  code_generator::visit_octave_user_function_trailer (octave_user_function& fcn)
  {
    bool nested  = fcn.is_nested_function() ;

    os_src << "Return:\n";

    increment_indent_level (os_src);

    octave::tree_parameter_list *ret_list = fcn.return_list ();

    if (ret_list)
      {
        bool takes_var_return = fcn.takes_var_return ();

        int len = ret_list->length ();

        os_src << "return make_return_list(output";

        if (len > 0 )
          {
            os_src << ", ";

            return_list_from_param_list(*ret_list);

            os_src
              << ", "
              << "nargout";
          }

        if (takes_var_return)
          {
            os_src << ", ";

            os_src << mangle("varargout");
          }

        os_src << ");\n";
      }
    else
      os_src << "return make_return_list(output);\n";

    decrement_indent_level (os_src);

    if (! nested)
      {
        decrement_indent_level (os_src);

        os_src << "}))";
      }
    else
      {
        decrement_indent_level (os_src);

        os_src << "}))));";
      }
  }

  void
  code_generator::visit_identifier (octave::tree_identifier&  id )
  {
    static const std::map<std::string,std::string> special_functions ({
      {"nargin", "NARGIN"},
      {"nargout", "NARGOUT"},
      {"isargout", "ISARGOUT"},
      {"narginchk", "NARGINCHK"},
      {"nargoutchk", "NARGOUTCHK"}
    });

    const std::string & name = id.name ();

    auto f = special_functions.find(name);

    if (f != special_functions.end())
      {
        os_src << f->second;
      }
    else if (name == "end")
      os_src << "End()";
    else if (name == "~")
      os_src << "Tilde()";
    else
      os_src << mangle(id.name ());
  }

  void
  code_generator::visit_if_clause (octave::tree_if_clause& cmd)
  {
    octave::tree_expression *expr = cmd.condition ();

    if (expr)
      {
        os_src << "(";

        expr->accept (*this);

        os_src << ")";
      }

    octave::tree_statement_list *list = cmd.commands ();

    increment_indent_level (os_src);

    os_src << "\n{\n";

    increment_indent_level (os_src);

    if (list)
      {
        list->accept (*this);
      }

    decrement_indent_level (os_src);

    os_src << "}\n";

    decrement_indent_level (os_src);
  }

  void
  code_generator::visit_if_command (octave::tree_if_command& cmd)
  {
    os_src << "if ";

    octave::tree_if_command_list *list = cmd.cmd_list ();

    if (list)
      list->accept (*this);
  }

  void
  code_generator::visit_if_command_list (octave::tree_if_command_list& lst)
  {
    octave::tree_if_command_list::iterator p = lst.begin ();

    bool first_elt = true;

    while (p != lst.end ())
      {
        octave::tree_if_clause *elt = *p++;

        if (elt)
          {
            if (! first_elt)
              {
                if (elt->is_else_clause ())
                  os_src << "else";
                else
                  os_src << "else if ";
              }

            elt->accept (*this);
          }

        first_elt = false;
      }
  }

  void
  code_generator::visit_index_expression (octave::tree_index_expression& expr)
  {
    octave::tree_expression *e = expr.expression ();

    std::string type_tags = expr.type_tags ();

    os_src  << "Index (";

    if (e)
      e->accept (*this);

    if (e && e->is_identifier())
      {
        os_src << ", \"" << e->name () << "\"";
      }

    os_src << ", \"" << type_tags << "\", ";

    std::list<octave::tree_argument_list *> lst = expr.arg_lists ();

    std::list<string_vector> arg_names = expr.arg_names ();

    std::list<octave::tree_expression *> dyn_field = expr.dyn_fields ();

    std::list<octave::tree_argument_list *>::iterator p = lst.begin ();

    std::list<string_vector>::iterator p_arg_names = arg_names.begin ();

    std::list<octave::tree_expression *>::iterator p_dyn_field = dyn_field.begin ();

    int n = type_tags.length ();

    os_src  << "{";

    for (int i = 0; i < n; i++)
      {
        octave::tree_argument_list *elt = *p++;

        os_src  << "{";

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
                    else
                      panic_impossible ();
                  }
                else
                  {
                    std::string text_rep = quote(fn) + "__";

                    auto f = constant_map.find(text_rep);

                    if(f == constant_map.end())
                      {
                        os_src << "Const(" << nconst << ")";

                        constant_map[text_rep] = nconst++;
                      }
                    else
                      os_src << "Const(" << f->second << ")";
                  }
              }
          }
        else if (elt)
          elt->accept (*this);

        os_src
          << "}";

        if (i+1 < n)
          os_src  << ", ";

        p_arg_names++;

        p_dyn_field++;
      }

    os_src  << "}";

    os_src  << ")" ;
  }

  void
  code_generator::visit_matrix (octave::tree_matrix& lst)
  {
    os_src << "Matrixc({";

    octave::tree_matrix::iterator p = lst.begin ();

    while (p != lst.end ())
      {
        octave::tree_argument_list *elt = *p++;

        if (elt)
          {
            os_src << '{';

            elt->accept (*this);

            os_src << '}';

            if (p != lst.end ())
              os_src << ", ";
          }
      }

    os_src << "})";
  }

  void
  code_generator::visit_cell (octave::tree_cell& lst)
  {
    os_src << "Cellc({";

    octave::tree_matrix::iterator p = lst.begin ();

    if (lst.empty())
      os_src << "{}";

    while (p != lst.end ())
      {
        octave::tree_argument_list *elt = *p++;

        if (elt)
          {
            os_src << '{';

            elt->accept (*this);

            os_src << '}';

            if (p != lst.end ())
              os_src << ", ";
          }
      }

    os_src << "})";

  }

  void
  code_generator::visit_multi_assignment (octave::tree_multi_assignment& expr)
  {
    octave::tree_argument_list *lhs = expr.left_hand_side ();

    if (lhs)
      {
        os_src << "MultiAssign({";

        lhs->accept (*this);

        os_src << '}';
      }

    os_src << ", " ;

    octave::tree_expression *rhs = expr.right_hand_side ();

    if (rhs)
      rhs->accept (*this);

    os_src << ")" ;
  }

  void print_value(std::ostream& os, const octave_value& m_value)
  {
    std::streamsize prec = os.precision();

    os.precision(18);

    if ( m_value.isnull() )
      {
        if ( m_value.is_sq_string() )
          {
            os << "NullSqStr()";
          }
        else
        if ( m_value.is_dq_string() )
          {
            os << "NullStr()";
          }
        else
          {
            os << "Null()";
          }
      }
    else if(m_value.is_char_matrix())
      {
        bool sq = m_value.is_sq_string();

        charMatrix chm = m_value.char_matrix_value();

        octave_idx_type nstr = chm.rows ();

        if (nstr > 1)
          os << "Matrixc({ ";

        if (nstr != 0)
          {
            for (octave_idx_type i = 0; i < nstr; i++)
              {
                std::string row = chm.row_as_string (i);

                if (nstr > 1)
                  os << "{" ;

                row = undo_string_escapes1 (row);

                os
                  << "\""
                  << row
                  << "\""
                  << (sq ? "__" : "_dq") ;

                if (nstr > 1)
                  os << "}" ;

                if (i < nstr - 1)
                  os << ", ";
              }
          }

        if ( nstr > 1)
          os << " })";
      }
    else if( m_value.is_range() )
      {
        OCTAVE_RANGE r {m_value.range_value ()};
#if OCTAVE_MAJOR_VERSION >= 7
        os
          << "Colon ("
          << r.base()
          << "__, "
          << r.increment()
          << "__, "
          << r.limit()
          << "__)";
#else
         os
          << "Colon ("
          << r.base()
          << "__, "
          << r.inc()
          << "__, "
          << r.limit()
          << "__)";
#endif
      }
    else if ( m_value.is_real_scalar() )
      {
        switch (m_value.builtin_type ())
          {
          case btyp_double:
            {
              os << m_value.double_value() << "__";

              break;
            }
          case btyp_int8:
            {
              os << m_value.int8_scalar_value() << "_i8";

              break;
            }
          case btyp_int16:
            {
              os << m_value.int16_scalar_value() << "_i16";

              break;
            }
          case btyp_int32:
            {
              os << m_value.int32_scalar_value() << "_i32";

              break;
            }
          case btyp_int64:
            {
              os << m_value.int64_scalar_value() << "_i64";

              break;
            }
          case btyp_uint8:
            {
              os << m_value.uint8_scalar_value() << "_ui8";

              break;
            }
          case btyp_uint16:
            {
              os << m_value.uint16_scalar_value() << "_ui16";

              break;
            }
          case btyp_uint32:
            {
              os << m_value.uint32_scalar_value() << "_ui32";

              break;
            }
          case btyp_uint64:
            {
              os << m_value.uint64_scalar_value() << "_ui64";

              break;
            }
          default:
            {}
          }
      }
    else if ( m_value.is_complex_scalar() )
      {
        os << m_value.complex_value().imag ();
        os << "_i";
      }
    else if(m_value.is_real_matrix() )
      {
        ::Matrix m = m_value.matrix_value();

        octave_idx_type nr = m.rows ();

        octave_idx_type nc = m.columns ();

        if (nc > 1 || nr > 1)
          os << "Matrixc({";

        for (octave_idx_type i = 0; i < nr; i++)
          {
            os << "{";

            for (octave_idx_type j = 0; j < nc; j++)
              {
                os << m.elem(i,j) << "__";

                if (j < nc - 1)
                  os << ", ";
              }
            os << "}";

            if (i < nr - 1)
              os << ", ";
          }

        os << "})";
      }
    else if(m_value.is_complex_matrix())
      {
        ComplexMatrix m = m_value.complex_matrix_value();

        octave_idx_type nr = m.rows ();

        octave_idx_type nc = m.columns ();

        if (nc > 1 || nr > 1)
          os << "Matrixc({";

        for (octave_idx_type i = 0; i < nr; i++)
          {
            os << "{";

            for (octave_idx_type j = 0; j < nc; j++)
              {
                os << m.elem(i,j).imag () << "_i";

                if (j < nc - 1)
                  os << ", ";
              }

            os << "}";

            if (i < nr - 1)
              os << ", ";
          }

        os << "})";
      }
    else if ( m_value.is_magic_colon() )
      {
        os << "MagicColon()";
      }
    else if( m_value.iscell() )
      {
        ::Cell m = m_value.cell_value();

        octave_idx_type nr = m.rows ();

        octave_idx_type nc = m.columns ();

        if (nc == 0 && nr == 0)
          {
            os << "Cellc({{}})";

            return;
          }

        os << "Cellc({";

        for (octave_idx_type i = 0; i < nr; i++)
          {
            os << "{";

            for (octave_idx_type j = 0; j < nc; j++)
              {
                print_value( os, m.elem(i,j));

                if (j < nc - 1)
                  os << ", ";
              }
            os << "}";

            if (i < nr - 1)
              os << ", ";
          }

        os << "})";
      }

    os.precision(prec);
  }

  void
  code_generator::visit_constant (octave::tree_constant& val)
  {
    octave_value m_value = val.value();

    std::ostringstream os;

    print_value(os, m_value);

    std::string text_rep = os.str();

    auto f = constant_map.find(text_rep);

    if(f == constant_map.end())
      {
        os_src << "Const(" << nconst << ")";

        constant_map[text_rep] = nconst++;
      }
    else
      os_src << "Const(" << f->second << ")";
  }

  bool
  code_generator::visit_dot_separated_fcn_handle (octave::tree_fcn_handle& expr)
  {
    std::string nm = expr.name ();

    size_t pos = nm.find ('.',  0);

    bool result = false;

    if (pos != std::string::npos)
      {
        std::string id_name = nm.substr (0, pos);

        std::string type_tags;

        std::vector<std::string> fields;

        for (size_t i = pos+1 ;i < nm.size ();)
          {
            pos = std::min(nm.find ('.',  i), nm.size ());

            std::string field = nm.substr (i, pos-i);

            type_tags += ".";

            fields.emplace_back (std::move(field));

            i = pos + 1;
          }
        os_src  << "Index (";

        os_src << mangle(id_name);

        os_src << ", \"" << type_tags << "\"";

        for (auto& str : fields)
          {
            std::string text_rep = quote(str) + "__";

            auto f = constant_map.find(text_rep);

            os_src << ", {{ ";

            if(f == constant_map.end())
              {
                os_src << "Const(" << nconst << ")";

                constant_map[text_rep] = nconst++;
              }
            else
              os_src << "Const(" << f->second << ")";

            os_src << "}}";
          }

        os_src  << ")";
      }

    return result;
  }

  void
  code_generator::visit_fcn_handle (octave::tree_fcn_handle&  fh )
  {
    const auto& scope = fcn_scopes.back ();

    static const std::map<std::string,std::string> special_functions ({
      {"nargin", "NARGIN"},
      {"nargout", "NARGOUT"},
      {"isargout", "ISARGOUT"},
      {"narginchk", "NARGINCHK"},
      {"nargoutchk", "NARGOUTCHK"}
    });

    bool is_nested =
      scope->contains(fh.name(), symbol_type::nested_fcn)
      ||
      scope->lookup_in_parent_scopes(fh.name(), symbol_type::nested_fcn);

    bool is_special_function =
      special_functions.find(fh.name()) != special_functions.end ();

    if(is_nested)
      os_src
        << "NestedHandle(";
    else
      os_src
        <<"Handle(";

    auto idx = visit_dot_separated_fcn_handle (fh);

    if (! idx)
      {
        if (! is_special_function && ! is_nested)
          {
            auto symbol = scope->contains(fh.name(), symbol_type::ordinary);

            if (! (symbol && symbol->file))
              symbol = scope->lookup_in_parent_scopes(fh.name(), symbol_type::ordinary);

            if (symbol && symbol->file)
              {
                os_src
                  << "&"
                  << mangle(symbol->file->name)
                  << symbol->file->id
                  << "::";
              }
          }

        os_src
          << mangle(fh.name());

        if (! is_special_function && ! is_nested)
          os_src
            << "make";
      }

    os_src
      << ", \""
      << fh.name()
      << "\" )";
  }

  void
  code_generator::visit_parameter_list (octave::tree_parameter_list& lst)
  {
    octave::tree_parameter_list::iterator p = lst.begin ();

    os_src << "{";

    while (p != lst.end ())
      {
        octave::tree_decl_elt *elt = *p++;

        if (elt)
          {
            elt->accept (*this);

            if (p != lst.end ())
              os_src << ", ";
          }
      }
    os_src << "}";
  }
  void
  code_generator::return_list_from_param_list(octave::tree_parameter_list& lst)
  {
    octave::tree_parameter_list::iterator p = lst.begin ();

    os_src << "{";

    while (p != lst.end ())
      {
        octave::tree_decl_elt *elt = *p++;

        if (elt)
          {
            octave::tree_identifier *id = elt->ident ();
            if (id)
            {
              os_src << mangle(id->name());
            }

            if (p != lst.end ())
              os_src << ", ";
          }
      }
    os_src << "}";
  }
  void
  code_generator::visit_postfix_expression (octave::tree_postfix_expression& expr)
  {
    octave::tree_expression *op = expr.operand ();

    auto do_post = [&](const std::string& fcn)
    {
      os_src << fcn <<" (" ;
        op->accept (*this);
      os_src << ")";
    };

    if (op)
      {
        octave_value::unary_op etype = expr.op_type ();

        switch (etype)
        {
        case octave_value::op_transpose:
          do_post("Transpose");
          break;
        case octave_value::op_hermitian:
          do_post("Ctranspose");
          break;
        case octave_value::op_incr:
          do_post("Postinc");
          break;
        case octave_value::op_decr:
          do_post("Postdec");
          break;
        default:
          break;
        }
      }
  }

  void
  code_generator::visit_prefix_expression (octave::tree_prefix_expression& expr)
  {
    octave::tree_expression *op = expr.operand ();

    auto do_pre = [&](const std::string& fcn) {
      os_src << fcn <<" (" ;
        op->accept (*this);
      os_src << ")";
    };

    if (op)
      {
        octave_value::unary_op etype = expr.op_type ();
        switch (etype)
          {
          case octave_value::op_not:
            do_pre("Not");
            break;
          case octave_value::op_uplus:
            do_pre("Uplus");
            break;
          case octave_value::op_uminus:
            do_pre("Uminus");
            break;
          case octave_value::op_incr:
            do_pre("Preinc");
            break;
          case octave_value::op_decr:
            do_pre("Predec");
            break;
          default:
            break;
          }
      }
  }

  void
  code_generator::visit_return_command (octave::tree_return_command&)
  {
    for (auto lu : loop_or_unwind)
      if (lu == unwinding_context)
        {
          os_src << "throw return_unwind {};\n";

          return;
        }

    os_src << "goto Return;\n";
  }

  void
  code_generator::visit_simple_assignment (octave::tree_simple_assignment& expr)
  {
    octave::tree_expression *lhs = expr.left_hand_side ();

    octave::tree_expression *rhs = expr.right_hand_side ();

    octave_value::assign_op etype = expr.op_type ();

    auto do_assign = [&](std::string fcn)
    {
      os_src << fcn <<" (" ;
        lhs->accept (*this);
      os_src << "," ;
        rhs->accept (*this) ;
      os_src << ")";
    };

    if (lhs && rhs)
      {
        switch (etype)
          {
          case octave_value::op_asn_eq:
            do_assign("Assign");
            break;
          case octave_value::op_add_eq:
            do_assign("AssignAdd");
            break;
          case octave_value::op_sub_eq:
            do_assign("AssignSub");
            break;
          case octave_value::op_mul_eq:
            do_assign("AssignMul");
            break;
          case octave_value::op_div_eq:
            do_assign("AssignDiv");
            break;
          case octave_value::op_ldiv_eq:
            do_assign("AssignLdiv");
            break;
          case octave_value::op_pow_eq:
            do_assign("AssignPow");
            break;
          case octave_value::op_el_mul_eq:
            do_assign("AssignElMul");
            break;
          case octave_value::op_el_div_eq:
            do_assign("AssignElDiv");
            break;
          case octave_value::op_el_ldiv_eq:
            do_assign("AssignElLdiv");
            break;
          case octave_value::op_el_pow_eq:
            do_assign("AssignElPow");
            break;
          case octave_value::op_el_and_eq:
            do_assign("AssignElAnd");
            break;
          case octave_value::op_el_or_eq:
            do_assign("AssignElOr");
            break;
          default:
            break;
          }
      }
  }

  std::string
  code_generator::ambiguity_check(octave::tree_expression &expr)
  {
    std::string retval;

    if (expr.is_index_expression())
      {
        octave::tree_index_expression* iexpr = dynamic_cast<octave::tree_index_expression*>(&expr);

        if (iexpr)
          {
            octave::tree_expression *e = iexpr->expression ();

            if (e && e->is_identifier())
              {
                std::string type_tags = iexpr->type_tags ();

                if(type_tags == "(")
                  {
                    std::list<octave::tree_argument_list *> lsts = iexpr->arg_lists ();

                    if(lsts.size() == 1 && lsts.back())
                      {
                        octave::tree_argument_list& lst = *lsts.back();

                        if(!lst.empty())
                          {
                            int i = 0;

                            for(octave::tree_expression *elt : lst)
                              {
                                if(elt && elt->is_constant())
                                  {
                                    octave::tree_constant* cnst  = dynamic_cast<octave::tree_constant*>(elt);

                                    if (cnst && !cnst->value().isnull() && cnst->value().is_char_matrix() )
                                      {
                                        charMatrix chm = cnst->value().char_matrix_value();

                                        octave_idx_type nstr = chm.rows ();

                                        if(chm.numel() >= 1 && nstr == 1)
                                          {
                                            if(i == 0)
                                              {
                                                bool ismatch = false;

                                                char first = chm(0);

                                                for(char c: R"(.+-*/\^&|<>=:)")
                                                  {
                                                    if(first == c)
                                                      {
                                                        ismatch = true;

                                                        break;
                                                      }
                                                  }
                                                if(! ismatch)
                                                  return "";
                                              }

                                            retval += cnst->value().string_value() + " ";
                                          }
                                        else
                                          return "";
                                      }
                                    else
                                      return "";
                                  }
                                else
                                  return "";
                                i++;
                              }
                          }
                        else
                          return "";
                      }
                    else
                      return "";
                  }
                else
                  return "";

                octave::tree_identifier* id = dynamic_cast<octave::tree_identifier*>(e);

                if(id)
                  retval = id->name() + " " + retval + ";";
              }
            else
              return "";
          }
        else
          return "";
      }
    else
      return "";

    return retval;
  }

  void
  code_generator::visit_statement (octave::tree_statement& stmt)
  {
    octave::tree_command *cmd = stmt.command ();

    if (cmd)
      cmd->accept (*this);
    else
      {
        octave::tree_expression *expr = stmt.expression ();

        if (expr)
          {
            std::string eval_str = ambiguity_check (*expr);

            if(! eval_str.empty ())
              {
                warning("ambiguity:%s\\%s.m : %d. command-style function call \"%s\" .It is possibly a binary expression ",
                  m_file->path.c_str (),
                  m_file->name.c_str (),
                  expr->line (),
                  eval_str.c_str ());
              }

            expr->accept (*this);

            os_src << ".evaluate();\n";
          }
      }
  }

  void
  code_generator::visit_statement_list (octave::tree_statement_list& lst)
  {
    for (octave::tree_statement *elt : lst)
      {
        if (elt)
          elt->accept (*this);
      }
  }

  void
  code_generator::visit_switch_case (octave::tree_switch_case& cs)
  {
    if (cs.is_default_case ())
      os_src << "else";
    else
      os_src << "else if (Case( ";

    octave::tree_expression *label = cs.case_label ();

    if (label)
      label->accept (*this);

    if (!cs.is_default_case ())
      os_src << "))";

    octave::tree_statement_list *list = cs.commands ();

    increment_indent_level (os_src);

    os_src <<"\n{\n";

    increment_indent_level (os_src);

    if (list)
      {
        list->accept (*this);
      }

    decrement_indent_level (os_src);

    os_src << "}\n";

    decrement_indent_level (os_src);
  }

  void
  code_generator::visit_switch_case_list (octave::tree_switch_case_list& lst)
  {
    octave::tree_switch_case_list::iterator p = lst.begin ();

    os_src << "if (0)\n";

    increment_indent_level (os_src);

    os_src << "{}\n";

    decrement_indent_level (os_src);

    while (p != lst.end ())
      {
        octave::tree_switch_case *elt = *p++;

        if (elt)
          elt->accept (*this);
      }
  }

  void
  code_generator::visit_switch_command (octave::tree_switch_command& cmd)
  {
    os_src << "\n{\n";

    increment_indent_level (os_src);

    os_src << "auto Case = Switch (";

    octave::tree_expression *expr = cmd.switch_value ();

    if (expr)
      expr->accept (*this);
    else
      error ("missing value in switch command");

    os_src << ");\n";

    octave::tree_switch_case_list *list = cmd.case_list ();

    if (list)
      {
        list->accept (*this);
      }

    decrement_indent_level (os_src);

    os_src << "}\n";
  }

  void
  code_generator::visit_try_catch_command (octave::tree_try_catch_command& cmd)
  {
    octave::tree_statement_list *try_code = cmd.body ();

    octave::tree_statement_list *catch_code = cmd.cleanup ();

    octave::tree_identifier *expr_id = cmd.identifier ();

    if (expr_id)
      os_src << "TRY_CATCH_VAR(";
    else
      os_src << "TRY_CATCH(";

    increment_indent_level (os_src);

    os_src << "\n{\n";

    increment_indent_level (os_src);

    if (try_code)
      try_code->accept (*this);

    decrement_indent_level (os_src);

    os_src
      << "}\n"
      << ",\n"
      << "{\n";

    increment_indent_level (os_src);

    if (catch_code)
      catch_code->accept (*this);

    decrement_indent_level (os_src);

    os_src << "}";

    decrement_indent_level (os_src);

    if (expr_id)
      {
        os_src
          << ", "
          << mangle(expr_id->name());
      }
    os_src << ")\n";
  }

  void
  code_generator::visit_unwind_protect_command (octave::tree_unwind_protect_command& cmd)
  {
    bool loopflag = false;
    bool loop_propagates = loop_or_unwind.back () == unwinding_context;
    bool return_propagates = false;

    for (auto lu : loop_or_unwind)
      if (lu == looping_context)
        {
          loopflag = true;
        }
      else if (lu == unwinding_context)
        {
          return_propagates = true;
        }

    if (loopflag)
      os_src << "UNWIND_PROTECT_LOOP(" ;
    else
      os_src << "UNWIND_PROTECT(" ;

    loop_or_unwind.push_back(unwinding_context);

    increment_indent_level (os_src);

    os_src << "\n{\n" ;

    increment_indent_level (os_src);

    octave::tree_statement_list *unwind_protect_code = cmd.body ();

    if (unwind_protect_code)
      {
        unwind_protect_code->accept (*this);
      }

    octave::tree_statement_list *cleanup_code = cmd.cleanup ();

    decrement_indent_level (os_src);

    os_src << "}\n,\n{\n";

    increment_indent_level (os_src);

    loop_or_unwind.pop_back ();

    if (cleanup_code)
      cleanup_code->accept (*this);

    decrement_indent_level (os_src);

    os_src << "}, " << return_propagates ;

    if (loopflag)
      os_src << ", " << loop_propagates;

    os_src << ")\n";

    decrement_indent_level (os_src);
  }

  void
  code_generator::visit_while_command (octave::tree_while_command& cmd)
  {
    os_src
      << "WHILE( ";

    octave::tree_expression *expr = cmd.condition ();

    if (expr)
      expr->accept (*this);

    os_src
      << ",\n";

    increment_indent_level (os_src);

    os_src  << "{\n";

    increment_indent_level (os_src);

    octave::tree_statement_list *list = cmd.body ();

    loop_or_unwind.push_back(looping_context);

    if (list)
      {
        list->accept (*this);
      }

    loop_or_unwind.pop_back ();

    decrement_indent_level (os_src);

    os_src << "})\n";

    decrement_indent_level (os_src);
  }
  void
  code_generator::visit_do_until_command (octave::tree_do_until_command& cmd)
  {
    loop_or_unwind.push_back(looping_context);

    octave::tree_statement_list *list = cmd.body ();

    os_src
      << "DO_UNTIL(";

    increment_indent_level (os_src);

    os_src  << "\n{\n";

    increment_indent_level (os_src);

    if (list)
      list->accept (*this);

    loop_or_unwind.pop_back ();

    octave::tree_expression *expr = cmd.condition ();

    decrement_indent_level (os_src);

    os_src << "}\n";

    decrement_indent_level (os_src);

    os_src << ", ";

    if (expr)
      expr->accept (*this);

    os_src << ")\n";
  }

  std::string
  code_generator::mangle (const std::string& str)
  {
    return str + "_";
  }

  void
  code_generator::declare_and_define_variables()
  {
    delimiter sep;

    const auto& scope = traversed_scopes.front()[0].front()->symbols();

    auto scope_searcher = traversed_scopes.front()[0].front();

    static const std::map<std::string,std::string> special_functions ({
      {"nargin", "NARGIN"},
      {"nargout", "NARGOUT"},
      {"isargout", "ISARGOUT"},
      {"narginchk", "NARGINCHK"},
      {"nargoutchk", "NARGOUTCHK"}
    });

    for(const auto& symbol : scope[(int)symbol_type::ordinary])
      {
        os_src
          << "Symbol "
          << mangle(symbol->name) ;

        auto f = special_functions.find(symbol->name);

        if (f != special_functions.end())
          {
            if (symbol->fcn.is_function())
              {
                octave_function* fcn = symbol->fcn.function_value(true);

                if
                (
                  fcn
                  &&
                  (symbol->fcn.is_builtin_function() || is_system_fcn_file(fcn))
                )
                  {
                  }
                else if (symbol->file )
                  {
                    os_src
                      << " = Copy (";

                    os_src
                      << mangle(symbol->file->name)
                      << symbol->file->id;

                    os_src
                      << "::"
                      << mangle(symbol->name)
                      << "make())";
                  }
              }
          }
        else if (symbol->file )
          {
            os_src
              << " = Copy (";

            os_src
              << mangle(symbol->file->name)
              << symbol->file->id;

            os_src
              << "::"
              << mangle(symbol->name)
              << "make())";
          }

        os_src << ";\n";
      }

    static std::vector<std::string> special_names  =
    {
      "nargin",
      "nargout",
      "isargout",
      "narginchk",
      "nargoutchk"
    };

    for (auto& name : special_names)
			{
			  if (scope_searcher->contains(name))
				os_src
					<< ([name]  () mutable {for (auto & c: name) c = toupper(c); return name;})()
					<< "_MAKER;\n";
			}
  }


  void
  code_generator::declare_and_define_handle_variables()
  {
    delimiter sep;

    const auto current_scope = traversed_scopes.front()[1].front();

    const auto& scope = current_scope->symbols();

    traversed_scopes.front()[1].pop_front();

    static const std::map<std::string,std::string> special_functions ({
      {"nargin", "NARGIN"},
      {"nargout", "NARGOUT"},
      {"isargout", "ISARGOUT"},
      {"narginchk", "NARGINCHK"},
      {"nargoutchk", "NARGOUTCHK"}
    });

    for(const auto& symbol : scope[(int)symbol_type::ordinary])
      {
        os_src
          << "Symbol "
          << mangle(symbol->name) ;

        auto f = special_functions.find(symbol->name);

        if (f != special_functions.end())
          {
            if (symbol->fcn.is_function())
              {
                octave_function* fcn = symbol->fcn.function_value(true);

                if
                (
                  fcn
                  &&
                  (symbol->fcn.is_builtin_function() || is_system_fcn_file(fcn))
                )
                  {
                  }
                else if (symbol->file)
                  {
                    os_src
                      << " = Copy (";

                    os_src
                      << mangle(symbol->file->name)
                      << symbol->file->id;

                    os_src
                      << "::"
                      << mangle(symbol->name)
                      << "make())";
                  }
              }
          }
        else if (symbol->file )
          {
            os_src
              << " = Copy (";

            os_src
              << mangle(symbol->file->name)
              << symbol->file->id;

            os_src
              << "::"
              << mangle(symbol->name)
              << "make())";
          }

        os_src << ";\n";
      }

    static std::vector<std::string> special_names  =
    {
      "nargin",
      "nargout",
      "isargout",
      "narginchk",
      "nargoutchk"
    };

    for (auto& name : special_names)
			{
			  if (current_scope->contains(name))
				os_src
					<< ([name]  () mutable {for (auto & c: name) c = toupper(c); return name;})()
					<< "_MAKER;\n";
			}
  }

  void
  code_generator::define_nested_functions(octave_user_function& fcnn)
  {
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
                traversed_scopes.front()[0].pop_front();

                auto mangled_name = mangle(nm);

                os_src
                  << mangled_name
                  << "= ";

                nested_fcn_names.push_back (mangled_name);

                nesting_context ++;

                ufc.accept(*this);

                nesting_context --;

                nested_fcn_names.pop_back ();

                decrement_indent_level (os_src);

                os_src
                  << "\n";
              }
          }
      }
  }

  std::string
  code_generator::init_persistent_variables()
  {
    delimiter sep;

    std::stringstream ss;

    const auto& scope = traversed_scopes.front()[0].front()->symbols();

    const auto& persistent_vars = scope[(int)symbol_type::persistent];

    if (!persistent_vars.empty())
      {
        ss
          << "Ptr per_rhs[] = {";
        ss << sep();

        for(const auto& symbol : persistent_vars)
          {
           ss << sep(", ")
              << "persistents."
              << mangle(symbol->name);
          }

        ss
          << "};\n";

        ss
          << "Ptr per_lhs[] = {";

        ss << sep();

        for(const auto& symbol : persistent_vars)
          {
            ss << sep(", ")
              << mangle(symbol->name);
          }

        ss
          << "};\n";
        ss
          << "AssignByRefInit(per_lhs, per_rhs);\n";
      }

    return ss.str();
  }

  void
  code_generator::declare_persistent_variables()
  {
    delimiter sep;

    const auto& scope = traversed_scopes.front()[0].front()->symbols();

    const auto& persistent_vars = scope[(int)symbol_type::persistent];

    if (!persistent_vars.empty())
      {
        os_src
          << "static struct { Symbol ";

        os_src << sep();

        for(const auto& symbol : persistent_vars)
          {
            os_src << sep(", ")
              << mangle(symbol->name);
          }

        os_src
          << ";} persistents;\n";
      }
  }

  void
  code_generator::generate_header(  )
  {
    if (mode != gm_compact)
      {
        os_hdr
          << "#pragma once\n"
          << "namespace coder{struct Symbol;}\n"
          << "using namespace coder;\n";
      }

    os_hdr
      << "namespace "
      << mangle(m_file->name) << m_file->id
      << "\n{\n";

    increment_indent_level (os_hdr);

    if (m_file->type == file_type::global)
      {
        for (const auto& var : m_file->local_functions)
          os_hdr
            << "Symbol& "
            << mangle(var.name())  << "make();\n";
      }
    else if(m_file->type == file_type::m)
      {
        os_hdr
          << "const Symbol& "
          << mangle(m_file->name) << "make();\n";

        for (const auto& sub: m_file->fcn.user_function_value()->subfunctions())
          {
            const auto& nm = sub.first;

            const auto& f = sub.second;

            octave_function *fcn = f.function_value (true);

            if (fcn && fcn->is_user_function())
              {
                octave_user_function& ufc = *(fcn->user_function_value());

                if (!ufc.is_nested_function())
                  {
                    os_hdr
                      << "const Symbol& "
                      << mangle(nm) << "make();\n";
                  }
              }
          }
      }
    else if(m_file->type == file_type::oct)
      {
        os_hdr << "const Symbol& " << mangle(m_file->name) << "make();\n";

        for (const auto& var : m_file->local_functions)
          {
            os_hdr
              << "const Symbol& "
              << mangle(var.name())  << "make();\n";
          }
      }
    else
      {
        os_hdr << "const Symbol& " << mangle(m_file->name) << "make();\n";
      }

    decrement_indent_level (os_hdr);

    os_hdr << "}\n";
  }

  void
  code_generator::generate_partial_source( )
  {
    os_prt
      << "namespace coder{struct Symbol;}\n"
      << "using namespace coder;\n"
      << "namespace "
      << mangle(m_file->name) << m_file->id
      << "\n{\n" ;

    increment_indent_level (os_prt);

    os_prt
      << "const Symbol& "
      << mangle(m_file->name)
      << "make(){Symbol* sym = nullptr; return *sym;}\n";

    if(m_file->type == file_type::m)
      for (const auto& sub: m_file->fcn.user_function_value()->subfunctions())
        {
          const auto& nm = sub.first;

          const auto& f = sub.second;

          octave_function *fcn = f.function_value (true);

          if (fcn && fcn->is_user_function())
          {
            octave_user_function& ufc = *(fcn->user_function_value());

            if (!ufc.is_nested_function())
            {
              os_prt
                << "const Symbol& "
                << mangle(nm)
                << "make(){Symbol* sym; return *sym;}\n";
            }
          }
        }

    decrement_indent_level (os_prt);

    os_prt << "}\n";
  }

  bool code_generator::has_nested_function(octave_user_function& ufcn)
  {
    for (const auto& sub: ufcn.subfunctions())
      {
        const auto& f = sub.second;

        octave_function *fcn = f.function_value (true);

        if (fcn && fcn->is_user_function())
          {
            octave_user_function& ufc = *(fcn->user_function_value());

            if (ufc.is_nested_function())
              {
                return true;
              }
          }
      }

    return false;
  }

  void
  code_generator::generate_full_source( )
  {
    auto inclusion = [&](const coder_file_ptr &f)->void
    {
      os_src
        << "#include "
        << "\""
        << mangle (lowercase (f->name)) << f->id
        << ".h"
        << "\"\n" ;
    };
    if (mode != gm_compact)
      {
        os_src << "#include " << quote("coder.h") << "\n";

        inclusion(m_file);

        for ( const auto&  dep: dependency_graph.at(m_file))
          inclusion(dep);

        os_src << "using namespace coder;\n";
      }

    os_src
      << "namespace "
      << mangle(m_file->name) << m_file->id
      << "\n{\n" ;

    increment_indent_level (os_src);

    if (m_file->type == file_type::global)
      {
        for (const auto& var : m_file->local_functions)
          {
            os_src
              << "Symbol& "
              << mangle(var.name())
              << "make()\n{";

            increment_indent_level (os_src);

            os_src
              << "static Symbol "
              << mangle(var.name())
              << "; return "
              << mangle(var.name())  ;

            decrement_indent_level (os_src);

            os_src <<";}\n";
          }
      }
    else if (m_file->type == file_type::oct)
      {
        auto define_oct = [&](const std::string& sym,
                              const std::string& fcn,
                              const std::string& file,
                              const std::string& path)
        {
          os_src
            << "const Symbol& "
            << sym
            << "make() { static const Symbol "
            << sym
            << " ("
            << fcn
            << ", "
            << file
            << ", "
            << path
            << ", "
            << "file_type::oct"
            << "); return "
            << sym
            << ";}\n";
        };

        define_oct(mangle(m_file->name), quote(m_file->name), quote(m_file->name),
          quote(undo_string_escapes1(m_file->path)));

        for (const auto& var : m_file->local_functions)
          define_oct(mangle(var.name()), quote(var.name()), quote(m_file->name),
            quote(undo_string_escapes1(m_file->path)));
      }
    else
      {
        if (m_file->type == file_type::m || m_file->type == file_type::cmdline)
          {
            os_src << "static Constant& Const(int);\n";
          }

        os_src
          << "const Symbol& "
          << mangle(m_file->name)
          << "make()\n{\n";

        increment_indent_level (os_src);

        os_src
          << "static const Symbol "
          << mangle(m_file->name) ;

        os_src << "(";

        if(m_file->type == file_type::m || m_file->type == file_type::cmdline)
          {
            octave_user_function *fcn = m_file->fcn.user_function_value (true);

            bool has_nested = has_nested_function (*fcn);

            nesting_context += has_nested;

            if (has_nested)
              {
                nested_fcn_names.push_back (mangle(m_file->name));
              }

            fcn->accept(*this);

            if (has_nested)
              {
                nested_fcn_names.pop_back ();
              }

            nesting_context -= has_nested;
          }
        else
          {
            os_src
              << quote(m_file->name)
              << ", "
              << quote(m_file->name)
              << ", "
              << quote(undo_string_escapes1(m_file->path))
              << ", ";

            if(m_file->type == file_type::mex)
              {
                os_src << "file_type::mex";
              }
            else if ( m_file->type == file_type::package)
              {
                os_src << "file_type::package" ;
              }
            else if ( m_file->type == file_type::classdef)
              {
                os_src << "file_type::classdef";
              }
          }

        os_src
          << ");\nreturn "
          << mangle(m_file->name) ;

        os_src  << ";\n";

        decrement_indent_level (os_src);

        os_src << "}\n";

        if(m_file->type == file_type::m )
          for (const auto& sub: m_file->fcn.user_function_value()->subfunctions())
            {
              const auto& nm = sub.first;

              const auto& f = sub.second;

              octave_function *fcn = f.function_value (true);

              if (fcn && fcn->is_user_function())
                {
                  octave_user_function& ufc = *(fcn->user_function_value());

                  if (!ufc.is_nested_function())
                    {
                      bool has_nested = has_nested_function (ufc);

                      nesting_context += has_nested;

                      if (has_nested)nested_fcn_names.push_back (mangle(nm));

                      traversed_scopes.pop_front();

                      os_src
                        << "const Symbol& "
                        << mangle(nm)
                        << "make()\n{\n";

                      increment_indent_level (os_src);

                      os_src
                        << "static const Symbol "
                        << mangle(nm) ;

                      os_src
                        << "(";

                      fcn->accept(*this);

                      os_src
                        << ");\nreturn ";

                      os_src
                        << mangle(nm)
                        << ";\n";

                      decrement_indent_level (os_src);

                      os_src << "}\n";

                      if (has_nested) nested_fcn_names.pop_back ();

                      nesting_context -= has_nested;
                    }
                }
            }

        if (m_file->type == file_type::m || m_file->type == file_type::cmdline)
          {
            os_src
              << "static Constant& Const(int i)\n{\n";

            increment_indent_level (os_src);

            os_src
              << "static Constant consts[] = \n{\n";

            increment_indent_level (os_src);

            if (constant_map.size())
              {
                using eltype = std::pair<std::string, int>;

                std::vector<eltype> map_vec(constant_map.begin(),constant_map.end());

                std::sort(map_vec.begin(), map_vec.end(),
                  [](const eltype& a, const eltype& b)
                  {
                    return a.second < b.second;
                  }
                );

                std::string sep = "  ";

                for(const auto& str: map_vec)
                  {
                    os_src << sep << "Constant(" << str.first << ")" << "\n";
                    sep = ", ";
                  }
              }

            decrement_indent_level (os_src);

            os_src
              << "};\n"
              << "return consts[i];\n";

            decrement_indent_level (os_src);

            os_src << "}\n";
          }
      }

    decrement_indent_level (os_src);

    os_src << "}\n";
  }

  void
  code_generator::generate_builtin_header( )
  {
    octave::symbol_table& octave_symtab = octave::interpreter::the_interpreter ()->get_symbol_table();

    const string_vector bif = octave_symtab.built_in_function_names ();

    auto n = bif.numel();

    if (mode != gm_compact)
      {
        os_hdr
          << "#pragma once\n"
          << "namespace coder {struct Symbol;}\n"
          << "using namespace coder;\n";
      }

    os_hdr
      << "namespace "
      << "builtins_0"
      << "\n{\n";

    increment_indent_level (os_hdr);

    for(octave_idx_type i = 0; i < n; ++i)
      {
        if (bif(i) == "__init_qt__")
          continue;

        if (bif(i) == "__shutdown_qt__")
          continue;

        if(!(bif(i).size() > 5 && bif(i).substr(0,5) == "meta."))
          {
            os_hdr
              << "const Symbol& "
              << mangle(bif(i)) << "make();\n";
          }
      }

    os_hdr << "const Symbol& " << mangle("meta") << "make();\n";

    decrement_indent_level (os_hdr);

    os_hdr
      << "}\n";
  }

  void
  code_generator::generate_builtin_source( )
  {
    os_src << "#include " << quote("builtin-defun-decls.h") << "\n";

    if (mode != gm_compact)
      {
        os_src << "#include " << quote("coder.h") << "\n";

        os_src << "using namespace coder;\n";
      }

    octave::symbol_table& octave_symtab = octave::interpreter::the_interpreter ()->get_symbol_table();

    const string_vector bif = octave_symtab.built_in_function_names ();

    auto n = bif.numel();

    static const std::map<std::string,std::string> aliases ({
      {"J"           ,"I"},
      {"__keywords__","iskeyword"},
      {"chdir"       , "cd"},
      {"dbnext"      , "dbstep"},
      {"exit"        , "quit"},
      {"gammaln"     , "lgamma"},
      {"isbool"      , "islogical"},
      {"inf"         , "Inf"},
      {"nan"         , "NaN"},
      {"i"           , "I"},
      {"j"           , "I"},
      {"ifelse"      , "merge"},
      {"alias"       , "name"},
      {"inverse"     , "inv"},
      {"lower"       , "tolower"},
      {"upper"       , "toupper"},
      {"home"        , "clc"},
      {"putenv"      , "setenv"}
    });

    os_src
      << "namespace "
      << "builtins_0"
      << "\n{\n";

    increment_indent_level (os_src);

    for (octave_idx_type i = 0; i < n; ++i)
      {
        if (bif(i) == "__init_qt__")
          continue;

        if (bif(i) == "__shutdown_qt__")
          continue;

        if (bif(i) == "feval")
          {
            os_src
              << "const Symbol& feval_make() { static const Symbol feval_ (fcn2ov(call_feval)); return feval_;}\n";

              continue;
          }

        if (! (bif(i).size() > 5 && bif(i).substr(0,5) == "meta."))
          {
            os_src << "const Symbol& ";

            auto f = aliases.find(bif(i));

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

            if (f != aliases.end())
              {
                os_src
                  << mangle(f->first)
                  << "make() { static const Symbol "
                  << mangle(f->first)
                  << " (fcn2ov("
                  << TOSTRING(OCTAVE_DEPR_NS)
                  << " F"
                  << f->second
                  << " )); return "
                  << mangle(f->first)
                  << ";}\n";
              }
            else
              {
                os_src
                  << mangle(bif(i))
                  << "make() { static const Symbol "
                  << mangle(bif(i))
                  << " (fcn2ov("
                  << TOSTRING(OCTAVE_DEPR_NS)
                  << " F"
                  << bif(i)
                  << " )); return "
                  << mangle(bif(i))
                  << ";}\n";
              }
          }
      }

    os_src
      << "const Symbol& "
      << mangle("meta")
      << "make() { static const Symbol "
      << mangle("meta")
      << " ("
      << quote("meta")
      << ", "
      << quote("builtins")
      << ", "
      << quote("")
      << ", "
      << "file_type::package"
      << "); return "
      << mangle("meta")
      << ";}\n";

    decrement_indent_level (os_src);

    os_src
      << "}\n";
  }

  void code_generator::generate(bool iscyclic)
  {
    if (m_file->type == file_type::builtin)
      {
        if (os_hdr.good())
          generate_builtin_header();

        if (os_src.good())
          generate_builtin_source();
      }
    else
      {
        if (os_hdr.good())
          generate_header();

        if (iscyclic && os_prt.good())
          generate_partial_source();

        if (os_src.good())
          generate_full_source();
      }

    if (os_hdr_ext.good())
      os_hdr_ext << os_hdr.str();

    if (os_src_ext.good())
      os_src_ext << os_src.str();

    if (iscyclic && os_prt_ext.good())
      os_prt_ext << os_prt.str();
  }
}
