#include "coder_runtime.h"

namespace coder_compiler
{
  const std::string& runtime_header()
  {
    static const std::string s = R"header(

#include "octave-config.h"
#include <type_traits>
#include <functional>


extern int buffer_error_messages;

class octave_value_list;

class octave_function;

namespace octave
{
  class tree_evaluator;

  class interpreter;
}

class octave_value;

class octave_value_list;

class octave_base_value;

namespace coder
{
  class coder_value_list;

  typedef octave_value_list (*coder_function_fcn) (const octave_value_list&, int);

  typedef octave_value_list (*coder_method_meth) (octave::interpreter&,
                                     const octave_value_list&, int);

  typedef void (*stateless_function) (const octave_value_list&, int);

  struct coder_value
  {
    mutable octave_base_value* val;

    coder_value ()
    : val (nullptr)
    {}

    coder_value (const coder_value& v)=delete;
    coder_value (coder_value&&);
    coder_value& operator= (coder_value&&);

    coder_value (const octave_value& v);

    explicit coder_value (octave_base_value* b)
    : val (b)
    {}

    operator octave_base_value* ()
    {
      octave_base_value* temp = val;

      val = nullptr;

      return temp;
    }

    ~coder_value();
  };

  octave_base_value* fcn2ov(coder_function_fcn f);

  octave_base_value* fcn2ov(coder_method_meth m);

  octave_base_value* fcn2ov(stateless_function m);

  octave_base_value* stdfcntoov (std::function<void(const octave_value_list&,int)> fcn);

  template <typename F, typename std::enable_if<
            !std::is_convertible<F,stateless_function>::value, int>::type = 0 >
  octave_base_value* fcn2ov(F&& fun)
  {
    return stdfcntoov(std::forward<F> (fun));
  }

  enum class file_type
  {
    oct,
    mex,
    classdef,
    package
  };

  class coder_lvalue
  {
  public:

    coder_lvalue (void)
      : m_sym (nullptr), m_black_hole (false), m_type (),
        m_nel (1)
    { }

    explicit coder_lvalue (octave_base_value*& sr)
      : m_sym (&sr), m_black_hole (false),
        m_type (), m_nel (1)
    { }

    coder_lvalue (const coder_lvalue&) = default;

    coder_lvalue& operator = (const coder_lvalue&) = default;

    ~coder_lvalue (void) = default;

    bool is_black_hole (void) const { return m_black_hole; }

    void mark_black_hole (void) { m_black_hole = true; }

    bool is_defined (void) const;

    bool is_undefined (void) const;

    void define (const octave_value& v) ;

    void assign (int op,
                              const octave_value& rhs, void* idx) ;

    void numel (octave_idx_type n) { m_nel = n; }

    octave_idx_type numel (void) const { return m_nel; }

    void set_index (const std::string& t,
                                 void* i, void* idx);

    void clear_index (void* idx) ;

    std::string index_type (void) const { return m_type; }

    bool index_is_empty (void* idx) const;

    void do_unary_op (int op, void* idx) ;

    coder_value value (void* idx) const;

  private:

    mutable octave_base_value** m_sym;

    bool m_black_hole;

    std::string m_type;

    octave_idx_type m_nel;
  };

  template <typename Expression>
  struct proxy_ptr
  {
    proxy_ptr(Expression&& ref) noexcept : p(std::addressof(ref)) {}
    proxy_ptr(Expression& ref) noexcept : p(std::addressof(ref)) {}
    proxy_ptr() noexcept : p(nullptr) {}
    proxy_ptr(const proxy_ptr&) noexcept = default;
    operator Expression& () const noexcept { return *p; }
    Expression& get() const noexcept { return *p; }
    bool is_valid() {return p != nullptr;}
    Expression* operator->() const
    { return p; }
    Expression* p;
  };

  class Expression;

  using Ptr = proxy_ptr<Expression>;
  using Ptr_list = std::initializer_list<Ptr>;
  using Ptr_list_list = std::initializer_list<Ptr_list>;

  struct ArgList
  {
    ArgList(Ptr_list&& list, bool magic_end = false): list(list), m_has_magic_end(magic_end){}

    bool has_magic_end () const
    {
      return m_has_magic_end;
    }

    size_t size() const
    {
      return list.size();
    }

    Ptr_list& list;

    bool m_has_magic_end;
  };

  struct Endindex
  {
    Endindex(octave_base_value* object, int position, int num):
      indexed_object(object),
      index_position(position),
      num_indices(num)
      {}

    Endindex()=default;

    coder_value compute_end() const;

  private:
    mutable octave_base_value *indexed_object = nullptr;
    int index_position = 0;
    int num_indices = 0;
  };

  struct Expression
  {
    Expression () = default;

    virtual coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);


    virtual void evaluate_n(int nargout=1, const Endindex& endkey=Endindex(), bool short_circuit=false) ;

    virtual coder_lvalue lvalue(void*){return coder_lvalue();}

    virtual bool is_Symbol() {return false;}

    operator bool();
  };

  struct Symbol : Expression
  {
    explicit Symbol(const std::string &fcn_name, const std::string &file_name, const std::string& path, file_type type );
    explicit Symbol(octave_base_value* arg):value(arg),reference(this){}

    Symbol();

    Symbol(const Symbol& other);

    Symbol (Symbol&& other);

    Symbol& operator=(const Symbol& other);
    Symbol& operator=(Symbol&& other) ;
    ~Symbol() ;


    bool is_Symbol() {return true;}

    Symbol*& get_reference()
    {
      return reference;
    }

    octave_base_value*& get_value() const
    {
      const Symbol* ptr = this;

      while (ptr != ptr->reference)
        {
          ptr = ptr->reference;
        }

      return  ptr->value;
    }

    coder_value
    evaluate(int nargout=0,
    const Endindex& endkey=Endindex(), bool short_circuit=false);

    void
    evaluate_n(int nargout=1,
      const Endindex& endkey=Endindex(), bool short_circuit=false);

    coder_lvalue lvalue(void*);

    void
    call (int nargout, const octave_value_list& args);

    bool
    is_defined () const;

    bool
    is_function () const;

    mutable octave_base_value* value;

    Symbol* reference;
  };

  struct Index : Expression
  {

    Index(Ptr arg, const std::string& type, std::initializer_list<ArgList>&& arg_list)
    :	base(arg),idx_type(type),arg_list(arg_list){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    void evaluate_n(int nargout=1, const Endindex& endkey=Endindex(), bool short_circuit=false) ;

    coder_lvalue
    lvalue (void*);

    Ptr base;

    std::string idx_type;

    std::initializer_list<ArgList>& arg_list;
  };

  struct Null : Expression
  {
    Null () = default;

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

  };

  struct NullStr : Expression
  {
    NullStr () = default;

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);
  };

  using NullDqStr = NullStr;

  struct NullSqStr : Expression
  {
    NullSqStr()=default;

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

  };

  struct string_literal_sq : Expression
  {
    explicit string_literal_sq(const std::string&  str) :str(str){	}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);


    std::string str;
  };

  struct string_literal_dq : Expression
  {
    explicit string_literal_dq(const std::string&  str) :str(str){	}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    std::string str;
  };

  using string_literal = string_literal_sq;

  struct double_literal : Expression
  {
    explicit double_literal(double d) :val(d){	}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    double val;
  };

  struct complex_literal : Expression
  {
    explicit complex_literal(double d) :val(d){	}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    double val;
  };

  inline double_literal operator"" __(long double d)
  {
    return double_literal(double(d));
  }

  inline double_literal operator"" __(unsigned long long int val)
  {
    return double_literal(double(val));
  }

  inline complex_literal operator"" _i(unsigned long long int val)
  {
    return complex_literal(double(val));
  }
  inline complex_literal operator"" _j(unsigned long long int val)
  {
    return complex_literal(double(val));
  }

  inline complex_literal operator"" _I(unsigned long long int val)
  {
    return complex_literal(double(val));
  }

  inline complex_literal operator"" _J(unsigned long long int val)
  {
    return complex_literal(double(val));
  }

  inline complex_literal operator"" _i(long double d)
  {
    return complex_literal(double(d));
  }

  inline complex_literal operator"" _j(long double d)
  {
    return complex_literal(double(d));
  }

  inline complex_literal operator"" _I(long double d)
  {
    return complex_literal(double(d));
  }

  inline complex_literal operator"" _J(long double d)
  {
    return complex_literal(double(d));
  }

  inline string_literal_sq operator"" __( const char *  str, std::size_t sz)
  {
    return string_literal_sq(std::string(str, sz));
  }

  inline string_literal_sq operator"" _sq( const char *  str, std::size_t sz)
  {
    return string_literal_sq(std::string(str, sz));
  }

  inline string_literal_dq operator"" _dq( const char *  str, std::size_t sz)
  {
    return string_literal_dq(std::string(str, sz));
  }

  struct Plus : public Expression
  {
    Plus(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Minus : public Expression
  {
    Minus(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Uplus : public Expression
  {
    Uplus(Ptr rhs) : a(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
  };

  struct Uminus : public Expression
  {
    Uminus(Ptr rhs) : a(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
  };

  struct Preinc : public Expression
  {
    Preinc(Ptr rhs) : a(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
  };

  struct Predec : public Expression
  {
    Predec(Ptr rhs) : a(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
  };

  struct Postinc : public Expression
  {
    Postinc(Ptr lhs) : a(lhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
  };

  struct Postdec : public Expression
  {
    Postdec(Ptr lhs) : a(lhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
  };

  struct Not : public Expression
  {
    Not(Ptr rhs) : a(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
  };

  struct Power : public Expression
  {
    Power(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Mpower : public Expression
  {
    Mpower(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Transpose : public Expression
  {
    Transpose(Ptr lhs) : a(lhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
  };

  struct Ctranspose : public Expression
  {
    Ctranspose(Ptr lhs) : a(lhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
  };

  struct Mtimes : public Expression
  {
    Mtimes(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Mrdivide : public Expression
  {
    Mrdivide(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Mldivide : public Expression
  {
    Mldivide(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Times : public Expression
  {
    Times(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Rdivide : public Expression
  {
    Rdivide(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Colon : public Expression
  {
    Colon(Ptr base, Ptr limit) : base(base),limit(limit){}

    Colon(Ptr base, Ptr inc, Ptr limit) : base(base), inc(inc), limit(limit){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr base;
    Ptr inc;
    Ptr limit;
  } ;

  struct Lt : public Expression
  {
    Lt(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Gt : public Expression
  {
    Gt(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Le : public Expression
  {
    Le(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Ge : public Expression
  {
    Ge(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Eq : public Expression
  {
    Eq(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Ne : public Expression
  {
    Ne(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct And : public Expression
  {
    And(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Or : public Expression
  {
    Or(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct AndAnd : public Expression
  {
    AndAnd(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct OrOr : public Expression
  {
    OrOr(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Assign : public Expression
  {
    Assign(Ptr lhs, Ptr rhs) : lhs(lhs),rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr lhs;
    Ptr rhs;
  };

  struct AssignAdd : public Expression
  {
    AssignAdd(Ptr lhs, Ptr rhs) : lhs(lhs),rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr lhs;
    Ptr rhs;
  };

  struct AssignSub : public Expression
  {
    AssignSub(Ptr lhs, Ptr rhs) : lhs(lhs),rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr lhs;
    Ptr rhs;
  };

  struct AssignMul : public Expression
  {
    AssignMul(Ptr lhs, Ptr rhs) : lhs(lhs),rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr lhs;
    Ptr rhs;
  };

  struct AssignDiv : public Expression
  {
    AssignDiv(Ptr lhs, Ptr rhs) : lhs(lhs),rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr lhs;
    Ptr rhs;
  };

  struct AssignLdiv : public Expression
  {
    AssignLdiv(Ptr lhs, Ptr rhs) : lhs(lhs),rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr lhs;
    Ptr rhs;
  };

  struct AssignPow : public Expression
  {
    AssignPow(Ptr lhs, Ptr rhs) : lhs(lhs),rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr lhs;
    Ptr rhs;
  };

  struct AssignElMul : public Expression
  {
    AssignElMul(Ptr lhs, Ptr rhs) : lhs(lhs),rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr lhs;
    Ptr rhs;
  };

  struct AssignElDiv : public Expression
  {
    AssignElDiv(Ptr lhs, Ptr rhs) : lhs(lhs),rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr lhs;
    Ptr rhs;
  };

  struct AssignElLdiv : public Expression
  {
    AssignElLdiv(Ptr lhs, Ptr rhs) : lhs(lhs),rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr lhs;
    Ptr rhs;
  };

  struct AssignElPow : public Expression
  {
    AssignElPow(Ptr lhs, Ptr rhs) : lhs(lhs),rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr lhs;
    Ptr rhs;
  };

  struct AssignElAnd : public Expression
  {
    AssignElAnd(Ptr lhs, Ptr rhs) : lhs(lhs),rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr lhs;
    Ptr rhs;
  };

  struct AssignElOr : public Expression
  {
    AssignElOr(Ptr lhs, Ptr rhs) : lhs(lhs),rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr lhs;
    Ptr rhs;
  };

  struct MultiAssign : public Expression
  {
    MultiAssign(Ptr_list&& lhs, Ptr rhs) : lhs(lhs),rhs(rhs){}

    void evaluate_n(int nargout=1, const Endindex& endkey=Endindex(), bool short_circuit=false) ;

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false) ;

    Ptr_list& lhs;
    Ptr rhs;
  };

  struct TransMul : public Expression
  {
    TransMul(Ptr lhs, Ptr rhs) : op_lhs(lhs),op_rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr op_lhs;
    Ptr op_rhs;
  };

  struct MulTrans : public Expression
  {
    MulTrans(Ptr lhs, Ptr rhs) : op_lhs(lhs),op_rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr op_lhs;
    Ptr op_rhs;
  };

  struct HermMul : public Expression
  {
    HermMul(Ptr lhs, Ptr rhs) : op_lhs(lhs),op_rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr op_lhs;
    Ptr op_rhs;
  };

  struct MulHerm : public Expression
  {
    MulHerm(Ptr lhs, Ptr rhs) : op_lhs(lhs),op_rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr op_lhs;
    Ptr op_rhs;
  };

  struct TransLdiv : public Expression
  {
    TransLdiv(Ptr lhs, Ptr rhs) : op_lhs(lhs),op_rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr op_lhs;
    Ptr op_rhs;
  };

  struct HermLdiv : public Expression
  {
    HermLdiv(Ptr lhs, Ptr rhs) : op_lhs(lhs),op_rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr op_lhs;
    Ptr op_rhs;
  };

  struct NotAnd : public Expression
  {
    NotAnd(Ptr lhs, Ptr rhs) : op_lhs(lhs),op_rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr op_lhs;
    Ptr op_rhs;
  };

  struct NotOr : public Expression
  {
    NotOr(Ptr lhs, Ptr rhs) : op_lhs(lhs),op_rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr op_lhs;
    Ptr op_rhs;
  };

  struct AndNot : public Expression
  {
    AndNot(Ptr lhs, Ptr rhs) : op_lhs(lhs),op_rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr op_lhs;
    Ptr op_rhs;
  };

  struct OrNot : public Expression
  {
    OrNot(Ptr lhs, Ptr rhs) : op_lhs(lhs),op_rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr op_lhs;
    Ptr op_rhs;
  };

  struct Handle : Expression
  {
    Handle(Ptr rhs, const char* nm) : op_rhs(rhs) ,name(nm){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr op_rhs;
    const char* name;
  };

  struct Anonymous : Expression
  {
    Anonymous(octave_base_value* arg);

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);


    coder_value value;
  };

  struct NestedHandle : Expression
  {
    NestedHandle(Ptr arg, const char* name);

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);


    coder_value value;
  };

  struct Matrixc : Expression
  {
    Matrixc(Ptr_list_list&& expr) : mat(expr){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr_list_list& mat;
  };

  struct Cellc : Expression
  {
    Cellc(Ptr_list_list&& expr) : cel(expr){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr_list_list& cel;
  };

  struct Constant : Expression
  {
    Constant(Ptr expr) : val(expr->evaluate(1)){}
    Constant(Constant&&other) :
    val(std::move(other.val)){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    coder_value val;
  };

  struct Switch
  {
    Switch(Ptr expr) : val(expr->evaluate(1)){}

    bool operator()(Ptr label);

    coder_value val;
  };

  struct End : public Expression
  {
    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);
  };

  struct MagicColon : public Expression
  {
    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);
  };

  struct Tilde : public Expression
  {
    coder_lvalue lvalue(void*);
  };

  bool
  eval_decl_elt (const Ptr_list& elt);

  void
  define_parameter_list_from_arg_vector
    (Ptr_list_list&& param_list, const octave_value_list& args);

  void
  define_parameter_list_from_arg_vector(Ptr varargin, const octave_value_list& args);

  void
  define_parameter_list_from_arg_vector
    (Ptr_list_list&& param_list, Ptr varargin, const octave_value_list& args);

	void make_return_list (Ptr_list&& ret_list, int nargout);
	void make_return_list (Ptr_list&& ret_list, int nargout, Ptr varout);
	void make_return_list (Ptr varout);
	void make_return_list ();
	void make_return_val (Ptr expr, int nargout);
  void make_return_val ();

  inline void AssignByRef( Ptr lhs, Ptr rhs)
  {
    static_cast<Symbol&>(lhs.get()).get_reference() = static_cast<Symbol&>(rhs.get()).get_reference();
  }

  inline void AssignByRef( Ptr_list& lhs, Ptr_list& rhs)
  {
    auto in = rhs.begin();

    for (auto& out: lhs )
      {
        AssignByRef(out,*in++);
      }
  }

  inline void AssignByVal( Ptr lhs, Ptr rhs)
  {
    static_cast<Symbol&>(lhs.get()) = static_cast<Symbol&>(rhs.get());
  }

  inline void AssignByVal( Ptr_list& lhs, Ptr_list& rhs)
  {
    auto in = rhs.begin();

    for (auto& out: lhs )
      {
        AssignByVal(out,*in++);
      }
  }

  void AssignByRefInit( Ptr_list& lhs, const Ptr_list& rhs);
  void AssignByRefCon( Ptr_list& lhs, const Ptr_list& rhs,const Ptr_list& macr);
  void AssignByValCon( Ptr_list& lhs, const Ptr_list& rhs,const Ptr_list& macr);

  void SetEmpty(Symbol& id);


  bool isargout1 (int nargout, double k);

  void
  call_narginchk (int nargin, const octave_value_list& arg);

  void
  call_nargoutchk (int nargout, const octave_value_list& arg, int nout);

  void
  call_nargin (int nargin, const octave_value_list& arg, int nout);

  void
  call_nargout (int nargout, const octave_value_list& arg, int nout);

  void
  call_isargout (int nargout, const octave_value_list& arg, int nout);

  void recover_from_excep ();

  void throw_exec_excep ();

  void assign_try_id (Ptr expr_id);

  enum LoopType
  {
    range_loop,
    scalar_loop,
    matrix_loop,
    undefined_loop
  };

  enum class unwind_ex
  {
    normal_unwind,
    exception_unwind,
    continue_unwind,
    break_unwind,
    return_unwind
  };

  struct unwindprotect
  {
    unwindprotect (int& var) : val(var), ptr(&var){}
    ~unwindprotect () {*ptr = val;}

    int val;
    int* ptr;
  };

  class for_loop_rep;

  class struct_loop_rep;

  class for_iterator
  {
  public:
    for_iterator(for_loop_rep *dis, octave_idx_type i): i(i), dis(dis){}
    for_iterator operator++() { ++i; return *this; }
    bool operator!=(const for_iterator & other) const
    {return i != other.i; }
    octave_idx_type operator*() const;
  private:
    for_loop_rep* dis;
    octave_idx_type i;
  };

  class struct_iterator
  {
  public:
    struct_iterator(struct_loop_rep *dis, octave_idx_type i): i(i), dis(dis){}
    struct_iterator operator++() { ++i; return *this; }
    bool operator!=(const struct_iterator & other) const
    {return i != other.i; }
    octave_idx_type operator*() const;
  private:
    struct_loop_rep* dis;
    octave_idx_type i;
  };

  class for_loop
  {
    for_loop_rep* rep;
  public:
    for_loop (Ptr lhs, Ptr expr);
    for_iterator begin() ;
    for_iterator end() ;
    ~for_loop();
  };

  class struct_loop
  {
    struct_loop_rep* rep;
  public:
    struct_loop (Ptr v, Ptr k, Ptr expr);
    struct_iterator begin() ;
    struct_iterator end() ;
    ~struct_loop ();
  };

  void call_error(const char*);

  octave_idx_type ovl_length (const octave_value_list& args);
}

#define GLOBAL(X)\
{\
	if (! globals_0::X##make().is_defined ()) {\
		SetEmpty( globals_0::X##make() );\
	} \
	AssignByRef( X , globals_0::X##make() );\
}

#define GLOBALASSIGN(X,Y)\
{\
	if (! globals_0::X##make().is_defined ()) {\
		Assign( globals_0::X##make() , Y ).evaluate();\
	}\
	AssignByRef( X , globals_0::X##make() );\
}

#define PERSISTENT(X)\
{\
	if (! persistents.X.is_defined ()) {\
		if ( X.is_defined ())\
			call_error("can't make variable X persistent");\
		SetEmpty( persistents.X );\
	}\
	AssignByRef( X , persistents.X );\
}

#define PERSISTENTASSIGN(X,Y)\
{\
	if (! persistents.X.is_defined ()) {\
		if ( X.is_defined ())\
			call_error("can't make variable X persistent");\
		Assign( persistents.X , Y ).evaluate();\
	}\
	AssignByRef( X , persistents.X );\
}

#define TRY_CATCH( try_code , catch_code )\
{\
  unwindprotect buferrtmp (buffer_error_messages);\
  buffer_error_messages++;\
  try\
  {\
    try_code\
  }\
  catch (...)\
  {\
    recover_from_excep ();\
    {catch_code}\
  }\
}

#define TRY_CATCH_VAR( try_code , catch_code , expr_id )\
{\
  unwindprotect buferrtmp (buffer_error_messages);\
  buffer_error_messages++;\
  try\
  {\
    try_code\
  }\
  catch (...)\
  {\
    recover_from_excep ();\
    assign_try_id (expr_id);\
    {catch_code}\
  }\
}

#define UNWIND_PROTECT_LOOP(unwind_protect_code , cleanup_code)\
{\
  try\
  {\
    try\
    {\
      unwind_protect_code\
    }\
    catch(unwind_ex e)\
    {\
      throw e;\
    }\
    catch (...)\
    {\
      throw unwind_ex::exception_unwind;\
    }\
    throw unwind_ex::normal_unwind;\
  }\
  catch(unwind_ex e)\
  {\
    recover_from_excep ();\
    cleanup_code\
    if (e == unwind_ex::return_unwind)\
      goto Return;\
    else if (e == unwind_ex::break_unwind)\
      break;\
    else if (e == unwind_ex::continue_unwind)\
      continue;\
    else if (e == unwind_ex::exception_unwind)\
      throw_exec_excep ();\
  }\
}

#define UNWIND_PROTECT(unwind_protect_code , cleanup_code)\
{\
  try\
  {\
    try\
    {\
      unwind_protect_code\
    }\
    catch(unwind_ex e)\
    {\
      throw e;\
    }\
    catch (...)\
    {\
      throw unwind_ex::exception_unwind;\
    }\
    throw unwind_ex::normal_unwind;\
  }\
  catch(unwind_ex e)\
  {\
    recover_from_excep ();\
    cleanup_code\
    if (e == unwind_ex::return_unwind)\
      goto Return;\
    else if (e == unwind_ex::exception_unwind)\
      throw_exec_excep ();\
  }\
}

#define WHILE(condition , loop_body)\
{\
	bool con = bool(condition);\
	while (con)\
	{\
		loop_body\
		con = bool(condition);\
	}\
}

#define DO_UNTIL(loop_body , condition )\
{\
	bool con;\
	do {\
		loop_body\
		con = bool(condition);\
	} while ( !con );\
}

#define NARGINCHK ([&args]()\
{\
  int nargin = ovl_length(args);\
  return Symbol(fcn2ov(\
  [nargin](const octave_value_list& arg, int nout)->void\
    {\
      return call_narginchk (nargin, arg);\
    }));\
})()

#define NARGOUTCHK Symbol(fcn2ov(\
  [nargout](const octave_value_list& arg, int nout)->void \
  {\
    return call_nargoutchk (nargout, arg, nout);\
  }))

#define NARGIN (\
  [&args]()\
  {\
    int nargin = ovl_length(args);\
    return Symbol (fcn2ov ( \
      [nargin](const octave_value_list& arg, int nout)->void \
      {\
        return call_nargin (nargin, arg, nout);\
      }));\
  })()

#define NARGOUT Symbol(fcn2ov(\
  [nargout](const octave_value_list& arg, int nout)->void \
  {\
    return call_nargout (nargout, arg, nout);\
  }))

#define ISARGOUT Symbol(fcn2ov(\
  [nargout](const octave_value_list& arg, int nout)->void\
  {\
    return call_isargout (nargout, arg, nout);\
  }))

#define DEFCODER_DLD(name, interp, args, nargout, doc, fcn_body)        \
  extern "C"                                                            \
  OCTAVE_EXPORT                                                         \
  octave_function *                                                     \
  G ## name (const octave::dynamic_library& shl, bool relative)         \
  {                                                                     \
    check_version (OCTAVE_API_VERSION, #name);                          \
    auto F ## name = [](octave::interpreter& interp,                    \
      const octave_value_list& args, int nargout) -> octave_value_list  \
      { fcn_body };                                                     \
    octave_dld_function *fcn                                            \
      = octave_dld_function::create ( F ## name, shl, #name, doc);      \
    if (relative)                                                       \
      fcn->mark_relative ();                                            \
    return fcn;                                                         \
  }


    )header"; return s;
  }


  const std::string& runtime_source()
  {
    static const std::string s = R"source(

#include "ov-null-mat.h"
#include "ov-bool.h"
#include "Matrix.h"
#include "parse.h"
#include "dynamic-ld.h"
#include "file-ops.h"
#include "builtin-defun-decls.h"
#include "oct-env.h"
#include "ov-typeinfo.h"
#include "lo-array-errwarn.h"
#include "ov-fcn-handle.h"
#include "ov-cs-list.h"
#include "ov-struct.h"

namespace coder
{
  coder_value::~coder_value()
  {
    if (val)
      val->release ();
  }

  coder_value::coder_value (coder_value&& v)
  :val(v.val)
  {
    v.val = nullptr;
  }

  coder_value& coder_value::operator= (coder_value&& v)
  {
    if (val)
      val->release ();

    val = v.val;

    v.val = nullptr;

    return *this;
  }

  coder_value::coder_value(const octave_value& v)
  {
    val = v.internal_rep ();

    val->grab ();
  }

  class value_list_pool
  {
  public:
    using List = std::list<octave_value_list>;
    using iterator = List::iterator;

    value_list_pool ()
    :result (), cache(capacity)
    {
      for (size_t i = 0 ; i < capacity; i++)
        for (size_t j = 0 ; j < capacity; j++)
          cache[i].emplace_back (octave_value_list (i));
    }

    List alloc (size_t sz)
    {
      if (sz < capacity && ! cache[sz].empty ())
        {
          List retval;

          retval.splice (retval.end (), cache[sz], cache[sz].begin ());

          return retval;
        }

      return {octave_value_list (sz)};
    }

    void free (List& list)
    {
      for (iterator it = list.begin(); it != list.end (); )
        {
          const octave_idx_type sz = it->length ();

          if (sz < capacity && cache[sz].size () < capacity )
            {
              for (octave_idx_type i = 0 ;i < sz; i++)
                (*it) (i) = octave_value ();

              cache[sz].splice (cache[sz].end (), list, it++);
            }
          else if (cache[0].size () < capacity)
            {
              it->clear ();

              cache[0].splice (cache[0].end (), list, it++);
            }
          else
            it = list.erase (it);
        }
    }

    void push_result (List& list)
    {
      free (result);
      result.splice (result.end (), list);
    }

    List pop_result ()
    {
      List retval;

      retval.splice (retval.end (), result);

      return retval;
    }

    size_t max_capacity ()
    {
      return capacity;
    }

  private:

    const size_t capacity = 10;

    std::list<octave_value_list> result;

    std::vector<std::list<octave_value_list>> cache;
  };

  namespace Pool
  {
    static value_list_pool& pool ()
    {
      static value_list_pool list_pool;

      return list_pool;
    }

    static value_list_pool::List alloc (size_t sz = 0)
    {
      return pool ().alloc (sz);
    }

    static void free (value_list_pool::List& list)
    {
      return pool ().free (list);
    }

    static value_list_pool::List pop_result ()
    {
      return pool ().pop_result ();
    }

    static void push_result (value_list_pool::List& list)
    {
      return pool ().push_result (list);
    }

    static size_t max_capacity ()
    {
      return pool ().max_capacity ();
    }
    static std::vector<char> create_bitidx()
    {
      std::vector<char> idx;

      idx.reserve (max_capacity ());

      return idx;
    }
    static std::vector<char>& bitidx ()
    {
      static std::vector<char> idx = create_bitidx();

      return idx;
    }
  }

  class coder_function_base
  {
  public:
      virtual void call (int, const octave_value_list&) = 0;
  };

  class
  coder_function : public octave_function
  {
  public:

    typedef octave_value_list (*fcn) (const octave_value_list&, int);

    coder_function():f(nullptr){}

    coder_function (fcn fun, const std::string& nm="",
               const std::string& ds = "") :
               octave_function(nm,ds),
               f(fun)
               {}

    octave_function * function_value (bool = false) { return this; }

    octave_value
    subsasgn (const std::string& type,
                            const std::list<octave_value_list>& idx,
                            const octave_value& rhs)
    {
      octave_value retval;
      return retval.subsasgn (type, idx, rhs);
    }

    octave_value_list
    call(octave::tree_evaluator& tw, int nargout = 0,
      const octave_value_list& args = octave_value_list ())
    {
      octave_value_list retval;

      retval = f(args, nargout);

      retval.make_storable_values ();

      if (retval.length () == 1 && retval.xelem (0).is_undefined ())
        retval.clear ();

      return retval;
    }

  private:

      fcn f;
      DECLARE_OV_TYPEID_FUNCTIONS_AND_DATA
  };
  DEFINE_OV_TYPEID_FUNCTIONS_AND_DATA (coder_function,
                                       "coder_function",
                                       "coder_function");

  class
  coder_method : public octave_function
  {
  public:

    typedef octave_value_list (*meth) (octave::interpreter&,
                                     const octave_value_list&, int);

    coder_method():m(nullptr){}

    coder_method (meth fun, const std::string& nm="",
               const std::string& ds = "") :
               octave_function(nm,ds),
               m(fun )
               {}

    octave_function * function_value (bool = false) { return this; }

    octave_value
    subsasgn (const std::string& type,
                            const std::list<octave_value_list>& idx,
                            const octave_value& rhs)
    {
      octave_value retval;
      return retval.subsasgn (type, idx, rhs);
    }

    octave_value_list
    call(octave::tree_evaluator& tw, int nargout = 0,
      const octave_value_list& args = octave_value_list ())
    {
      octave_value_list retval;

      auto& interp = *octave::interpreter::the_interpreter ();

      retval = m(interp, args, nargout);

      retval.make_storable_values ();

      if (retval.length () == 1 && retval.xelem (0).is_undefined ())
        retval.clear ();

      return retval;
    }

  private:

      meth m;
      DECLARE_OV_TYPEID_FUNCTIONS_AND_DATA
  };

  DEFINE_OV_TYPEID_FUNCTIONS_AND_DATA (coder_method,
                                       "coder_method",
                                       "coder_method");

  class
  coder_stateless_function : public coder_function_base ,public octave_function
  {
  public:

    typedef void (*fcn) (const octave_value_list&, int);

    coder_stateless_function():f(){}


    coder_stateless_function(fcn fun):
    f(fun)
    {  }

    octave_function * function_value (bool = false) { return this; }

    octave_value
    subsasgn (const std::string& type,
                            const std::list<octave_value_list>& idx,
                            const octave_value& rhs)
    {
      octave_value retval;
      return retval.subsasgn (type, idx, rhs);
    }

    void
    call (int nargout = 0,
      const octave_value_list& args = octave_value_list ())
    {
      f(args, nargout);
    }

    octave_value_list
    call(octave::tree_evaluator& tw, int nargout = 0,
      const octave_value_list& args = octave_value_list ())
    {
      f(args, nargout);

      auto result =  Pool::pop_result ();

      octave_value_list retval = result.back ();

      Pool::free (result);

      retval.make_storable_values ();

      if (retval.length () == 1 && retval.xelem (0).is_undefined ())
        retval.clear ();

      return retval;
    }

  private:

      fcn f;
      DECLARE_OV_TYPEID_FUNCTIONS_AND_DATA
  };

  DEFINE_OV_TYPEID_FUNCTIONS_AND_DATA (coder_stateless_function,
                                         "coder_stateless_function",
                                         "coder_stateless_function");
  class
  coder_stateful_function : public coder_function_base ,public octave_function
  {
  public:

    coder_stateful_function():s(){}

    template <typename F, typename std::enable_if<
              !std::is_convertible<F,coder_stateless_function::fcn>::value, int>::type = 0 >

    coder_stateful_function(F&& fun):
    s(std::forward<F> (fun))
    {  }

    octave_function * function_value (bool = false) { return this; }

    octave_value
    subsasgn (const std::string& type,
                            const std::list<octave_value_list>& idx,
                            const octave_value& rhs)
    {
      octave_value retval;
      return retval.subsasgn (type, idx, rhs);
    }

    void
    call (int nargout = 0,
      const octave_value_list& args = octave_value_list ())
    {
      s(args, nargout);
    }

    octave_value_list
    call(octave::tree_evaluator& tw, int nargout = 0,
      const octave_value_list& args = octave_value_list ())
    {
      s(args, nargout);

      auto result =  Pool::pop_result ();

      octave_value_list retval = result.back ();

      Pool::free (result);

      retval.make_storable_values ();

      if (retval.length () == 1 && retval.xelem (0).is_undefined ())
        retval.clear ();

      return retval;
    }

  private:

      std::function<void(const octave_value_list&,int)> s;
      DECLARE_OV_TYPEID_FUNCTIONS_AND_DATA
  };

  DEFINE_OV_TYPEID_FUNCTIONS_AND_DATA (coder_stateful_function,
                                         "coder_stateful_function",
                                         "coder_stateful_function");

  octave_base_value* fcn2ov(coder_function::fcn f)
  {
    return new coder_function(f);
  }

  octave_base_value* fcn2ov(coder_method::meth m)
  {
    return new coder_method(m);
  }

  typedef void (*stateless_function) (const octave_value_list&, int);

  octave_base_value* fcn2ov(stateless_function f)
  {
    return new coder_stateless_function(f);
  }

  octave_base_value* stdfcntoov (std::function<void(const octave_value_list&,int)> fcn)
  {
    return new coder_stateful_function(fcn);
  }

  bool
  coder_lvalue::is_defined (void) const
  {
    return ! is_black_hole () && (*m_sym)->is_defined () && ! (*m_sym)->is_function ();
  }

  bool
  coder_lvalue::is_undefined (void) const
  {
    return is_black_hole () || ! (*m_sym)->is_defined () || (*m_sym)->is_function ();
  }

  void coder_lvalue::define (const octave_value& v  )
  {
    octave_value tmp(*m_sym);

    tmp.assign (octave_value::op_asn_eq, v);

    *m_sym = tmp.internal_rep ();

    (*m_sym)->grab ();
  }

  void
  coder_lvalue::assign (int op,
                            const octave_value& rhs, void* idx)
  {
    std::list<octave_value_list>& m_idx = *static_cast<std::list<octave_value_list>*>(idx);

    if (! is_black_hole ())
      {
        octave_value tmp(*m_sym);

        if (m_idx.empty ())
          tmp.assign ((octave_value::assign_op)op, rhs);
        else
          tmp.assign ((octave_value::assign_op)op, m_type, m_idx, rhs);

        *m_sym = tmp.internal_rep ();

        (*m_sym)->grab ();
      }
  }

  void
  coder_lvalue::set_index (const std::string& t,
                                 void* i, void* idx)
  {
    std::list<octave_value_list>& m_i = *static_cast<std::list<octave_value_list>*>(i);

    std::list<octave_value_list>& m_idx = *static_cast<std::list<octave_value_list>*>(idx);

    if (! m_idx.empty ())
      error ("invalid index expression in assignment");

    m_type = t;

    Pool::free (m_idx);

    m_idx.splice (m_idx.end (), m_i);
  }

  void coder_lvalue::clear_index (void* idx)
  {
    std::list<octave_value_list>& m_idx = *static_cast<std::list<octave_value_list>*>(idx);

    m_type = ""; Pool::free (m_idx);
  }

  bool
  coder_lvalue::index_is_empty (void* idx) const
  {
    std::list<octave_value_list>& m_idx = *static_cast<std::list<octave_value_list>*>(idx);

    bool retval = false;

    if (m_idx.size () == 1)
      {
        octave_value_list tmp = m_idx.front ();

        retval = (tmp.length () == 1 && tmp(0).isempty ());
      }

    return retval;
  }

  void
  coder_lvalue::do_unary_op (int op, void* idx)
  {
    std::list<octave_value_list>& m_idx = *static_cast<std::list<octave_value_list>*>(idx);

    if (! is_black_hole ())
      {
        octave_value tmp(*m_sym);

        if (m_idx.empty ())
          tmp.do_non_const_unary_op ((octave_value::unary_op)op);
        else
          tmp.do_non_const_unary_op ((octave_value::unary_op)op, m_type, m_idx);

        *m_sym = tmp.internal_rep ();

        (*m_sym)->grab ();
      }
  }

  coder_value
  coder_lvalue::value (void* idx) const
  {
    std::list<octave_value_list>& m_idx = *static_cast<std::list<octave_value_list>*>(idx);

    octave_value retval;

    if (! is_black_hole ())
      {
        if (m_idx.empty ())
          {
            (*m_sym)->grab ();

            return coder_value(*m_sym);
          }
        else
          {
            if ((*m_sym)->is_constant ())
              retval = (*m_sym)->subsref (m_type, m_idx);
            else
              {
                octave_value_list t = (*m_sym)->subsref (m_type, m_idx, 1);

                if (t.length () > 0)
                  retval = t(0);
              }
          }
      }

    return retval;

  }

  coder_value
  Endindex::compute_end() const
  {
    octave_value retval;

    if (! indexed_object)
      error ("invalid use of end");

    if (! indexed_object->is_defined())
      err_invalid_inquiry_subscript ();

    if (indexed_object->isobject ())
      {
        octave_value_list args;

        args(2) = num_indices;

        args(1) = index_position + 1;

        args(0) = octave_value(indexed_object, true);

        std::string class_name = indexed_object->class_name ();

        octave::symbol_table& symtab = octave::interpreter::the_interpreter () ->get_symbol_table ();

        octave_value meth = symtab.find_method ("end", class_name);

        if (meth.is_defined ())
          {
            auto result = octave::feval (meth.function_value (), args, 1);

            if(! result.empty ())
              {
                return (result(0));
              }

            return (retval);
          }
      }

    dim_vector dv = indexed_object->dims ();

    int ndims = dv.ndims ();

    if (num_indices < ndims)
      {
        for (int i = num_indices; i < ndims; i++)
          dv(num_indices-1) *= dv(i);

        if (num_indices == 1)
          {
            ndims = 2;
            dv.resize (ndims);
            dv(1) = 1;
          }
        else
          {
            ndims = num_indices;
            dv.resize (ndims);
          }
      }

    if (index_position < ndims)
      retval = dv(index_position);
    else
      retval = 1;

    return (retval);
  }

  coder_value
  Expression::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return octave_value();
  }

  void
  Expression::evaluate_n( int nargout, const Endindex& endkey, bool short_circuit)
  {
    auto retval = Pool::alloc (1);

    retval.back ()(0) = octave_value(evaluate(nargout,endkey,short_circuit),false);

    Pool::push_result (retval);
  }

  Expression::operator bool ()
  {
    bool expr_value = false;

    octave_value retval (evaluate ( 1, Endindex(), true),false);

    if (retval.is_defined ())
      return retval.is_true ();
    else
      error ("undefined value used in conditional Expression");

    return expr_value;
  }

  Symbol::Symbol ()
  {
    octave_value tmp;

    value = tmp.internal_rep ();

    value->grab ();

    reference = this;
  }

  Symbol::Symbol (const Symbol& other)
  {
    value = other.get_value ();

    if (value)
      value->grab ();

    reference = this;
  }

  Symbol::Symbol (Symbol&& other)
  {
    if (other.reference != &other)
      {
        reference = other.reference;
      }
    else
      {
        reference = this;
      }

    value = other.value;

    other.value = nullptr;
  }

  Symbol& Symbol::operator=(const Symbol& other)
  {
    octave_base_value* tmp = other.get_value ();

    if (tmp != value)
      {
        if (value )
          value->release ();

        value = tmp;

        if (value )
          value->grab();
      }

    reference = this;

    return *this;
  }

  Symbol& Symbol::operator=(Symbol&& other)
  {
    if (value )
      value->release ();

    value = other.value;

    other.value = nullptr;

    if (other.reference != &other)
      {
        reference = other.reference;
      }
    else
      {
        reference = this;
      }

    return *this;
  }

  Symbol::~Symbol ()
  {
    if (value)
      value->release ();
  }

  Symbol::Symbol(const std::string &fcn_name, const std::string &file_name,
  const std::string& path, file_type type )
  : reference (this)
  {
    switch (type)
      {
      case file_type::oct:
        {
          octave::dynamic_loader& dyn_loader
            = octave::interpreter::the_interpreter ()->get_dynamic_loader ();

          using octave::sys::file_ops::concat;

          std::string fullname = concat (path, file_name + ".oct");

          octave_function *tmpfcn
            = dyn_loader.load_oct (fcn_name, fullname, false);

          if (tmpfcn)
            {
              value = tmpfcn;
            }
          else
            {
              error("cannot find %s.oct>%s in %s ", file_name.c_str (), fcn_name.c_str (), path.c_str ());
            }

          break;
        }

      case file_type::mex:
        {
          octave::dynamic_loader& dyn_loader
            = octave::interpreter::the_interpreter ()->get_dynamic_loader ();

          using octave::sys::file_ops::concat;

          std::string fullname = concat (path, file_name + ".mex");

          octave_function *tmpfcn
            = dyn_loader.load_mex (fcn_name, fullname, false);

          if (tmpfcn)
            {
              value = tmpfcn;
            }
          else
            {
              error("cannot find %s.mex>%s in %s ", file_name.c_str (), fcn_name.c_str (), path.c_str ());
            }

          break;
        }
      case file_type::classdef:
        {
          cdef_manager& cdm = octave::interpreter::the_interpreter ()->get_cdef_manager ();

          octave_function* klass_meth = nullptr;

          cdef_class klass = cdm.find_class (fcn_name, false, true);

          if(klass.ok ())
            klass_meth =  klass.get_constructor_function();
          else
            error("cannot find class %s in path %s", fcn_name.c_str (), path.c_str ());

          if (klass_meth)
            value = klass_meth;
          else
            error("cannot find class %s in path %s", fcn_name.c_str (), path.c_str ());

          break;
        }
      case file_type::package:
        {
          cdef_manager& cdm = octave::interpreter::the_interpreter ()->get_cdef_manager ();

          cdef_package pack = cdm.find_package (fcn_name, false, true);

          octave_function* pack_sym = cdm.find_package_symbol (fcn_name);

          if (pack_sym)
            value = pack_sym;
          else
            error("cannot find package %s in path %s", fcn_name.c_str (), path.c_str ());

          break;
        }
      }
  }

  coder_value
  Symbol::evaluate( int nargout,  const Endindex& endkey, bool short_circuit)
  {
    octave_base_value* value = get_value();

    if (value && value->is_defined ())
      {
        octave_function *fcn = nullptr;

        if (value->is_function())
          {
            fcn = value->function_value (true);

            coder_function_base* generated_fcn = dynamic_cast<coder_function_base*> (fcn);

            coder_value retval;

            if (generated_fcn)
              {
                auto first_args = Pool::alloc();

                generated_fcn->call (nargout, first_args.back () );

                Pool::free (first_args);

                auto result = Pool::pop_result();

                octave_value_list& retlist = result.back ();

                if(retlist.empty ())
                  {
                    retval =  coder_value (octave_value ());
                  }
                else
                  retval =  coder_value (retlist(0));

                Pool::free (result);

                return retval;
              }
            else if (! value->is_package () )
              {
                octave::tree_evaluator& ev = octave::interpreter::the_interpreter () -> get_evaluator ();

                auto first_args = Pool::alloc();

                octave_value_list retlist =  fcn->call (ev, nargout, first_args.back ());

                Pool::free (first_args);

                if(retlist.empty ())
                  {
                    retval =  coder_value (octave_value ());
                  }
                else
                  retval =  coder_value (retlist(0));

                return retval;
              }
          }

        value->grab ();

        return coder_value(value);
      }
    else
      {
        error("evaluating undefined variable");
      }
  }

  void
  Symbol::evaluate_n( int nargout,  const Endindex& endkey, bool short_circuit)
  {
    octave_base_value* value = get_value();

    if (value && value->is_defined())
      {
        octave_function *fcn = nullptr;

        if (value->is_function())
          {
            fcn = value->function_value (true);

            coder_function_base* generated_fcn = dynamic_cast<coder_function_base*> (fcn);

            octave_value_list retval;

            if (generated_fcn)
              {
                auto first_args = Pool::alloc();

                generated_fcn->call (nargout, first_args.back ());

                Pool::free (first_args);
              }
            else
              {
                octave::tree_evaluator& ev = octave::interpreter::the_interpreter () -> get_evaluator ();

                auto args = Pool::alloc ();

                auto result = Pool::alloc ();

                result.back () = fcn->call (ev, nargout, args.back ());

                Pool::free (args);

                Pool::push_result (result);
              }
          }
        else
          {
            auto result = Pool::alloc (1);

            result.back ()(0) = octave_value(value, true);

            Pool::push_result (result);
          }
      }
    else
      {
        error("multi assignment from undefined variable");
      }
  }

  coder_lvalue
  Symbol::lvalue(void* idx)
  {
    if (reference != this)
      {
        octave_base_value* val = get_value();

        if (!val->is_defined() || val->is_function())
          {
            (*this) = *reference;
          }
      }

    return coder_lvalue(get_value());
  }

  void
  Symbol::call (int nargout, const octave_value_list& args)
  {
    coder_function_base* fcn = dynamic_cast<coder_function_base*>(value);

    if (fcn)
      fcn->call (nargout, args);
  }

  bool
  Symbol::is_defined () const
  {
    octave_base_value* val = get_value ();

    return val && val->is_defined ();
  }

  bool
  Symbol::is_function () const
  {
    octave_base_value* val = get_value ();

    return val && val->is_function ();
  }

  void
  convert_to_const_vector (const ArgList& arg_list,
                                         octave_base_value* object, Endindex endkey)
  {
    const bool stash_object = (arg_list.has_magic_end()
                         && ! (object->is_function ()
                               || object->is_function_handle ()));

    const int len = arg_list.size ();

    std::list<octave_value_list> result_list = Pool::alloc ( len);

    octave_value_list& args  = result_list.back ();

    auto p = arg_list.list.begin ();

    int j = 0;

    octave_idx_type nel = 0;

    bool contains_cs_list = false;

    for (int k = 0; k < len; k++)
      {
        const Endindex& endk = stash_object ?
          Endindex{object, k, len} :
          endkey;

        auto& elt = *p++;

        octave_value tmp ( elt->evaluate(1, endk ,  false), false);

        if (tmp.is_defined ())
          {
            if (tmp.is_cs_list ())
              {
                const octave_idx_type n = tmp.numel();

                if (n == 0)
                  continue;
                else if (n  == 1)
                  {
                    args(j++) = tmp.list_value ()(0);
                  }
                else
                  {
                    contains_cs_list = true;

                    args(j++) = tmp;
                  }

                nel+=n;
              }
            else
              {
                args(j++) = tmp;

                nel++;
              }
          }
      }

    if (nel != len || contains_cs_list)
      {
        auto new_result = Pool::alloc (nel);

        octave_idx_type s = 0;

        for (octave_idx_type i = 0; i < j; i++)
          {
            octave_value& val = args.xelem(i);

            if (val.is_cs_list ())
              {
                octave_value_list ovl = val.list_value ();

                const octave_idx_type ovlen = ovl.length ();

                for (octave_idx_type m = 0; m < ovlen; m++)
                  new_result.back () (s++) = ovl.xelem (m);
              }
            else
              {
                new_result.back ()(s++) = val;
              }
          }

        assert (s == nel);

        Pool::free(result_list);

        Pool::push_result(new_result);

        return;
      }

    Pool::push_result(result_list);
  }

  void
  make_value_list (const ArgList& m_args,
                   const octave_value &object,
                   const Endindex& endkey=Endindex())

  {
    if (m_args.size() > 0)
      {
        return convert_to_const_vector (m_args, object.internal_rep () ,endkey);
      }

    auto result = Pool::alloc();

    return Pool::push_result(result);
  }

  std::string
  get_struct_index (const ArgList& arg)
  {
    coder_value val ( (*arg.list.begin())->evaluate (1));

    if (! val.val->is_string ())
      error ("dynamic structure field names must be strings");

    return val.val->string_value ();
  }

  coder_value
  Index::evaluate ( int nargout, const Endindex& endkey, bool short_circuit)
  {
    evaluate_n(nargout, endkey, short_circuit);

    auto result = Pool::pop_result ();

    octave_value_list& vlist = result.back ();

    coder_value retval;

    if (vlist.empty ())
      {
        octave_value tmp;
        retval =  coder_value (tmp);
      }
    else
      {
        retval = coder_value(vlist(0));
      }

    Pool::free (result);

    return retval;
  }

  void
  Index::evaluate_n( int nargout, const Endindex& endkey, bool short_circuit)
  {
    std::list<octave_value_list> retval;

    const std::string& type = idx_type;

    auto& args = arg_list;

    assert ( args.size () > 0);

    auto p_args = args.begin ();

    const int n = args.size ();

    int beg = 0;

    octave_value base_expr_val;

    auto& expr = base;

    if (expr->is_Symbol () && type[beg] == '(')
      {
        Symbol& id = static_cast<Symbol&>(expr.get());

        if (true)
          {
            octave_function *fcn = nullptr;

            octave_base_value* val = id.get_value();

            fcn = val->function_value (true);

            if (fcn)
              {
                std::list<octave_value_list> first_args;

                coder_function_base* generated_fcn = dynamic_cast<coder_function_base*> (fcn);

                if (generated_fcn)
                  {
                    if (p_args->size() > 0)
                      {
                          convert_to_const_vector( *p_args, val, endkey) ;

                          first_args = Pool::pop_result();
                      }
                    else
                      {
                        first_args = Pool::alloc();
                      }

                    generated_fcn->call (nargout, first_args.back () );

                    Pool::free (first_args);

                    Pool::free (retval);

                    retval = Pool::pop_result();
                  }
                else if (! val->is_function_handle ())
                  {
                    if (p_args->size() > 0)
                      {
                          convert_to_const_vector( *p_args, val, endkey) ;

                          first_args = Pool::pop_result();
                      }
                    else
                      {
                        first_args = Pool::alloc();
                      }

                    try
                      {
                        octave::tree_evaluator& ev = octave::interpreter::the_interpreter () -> get_evaluator ();

                        Pool::free(retval);

                        retval = Pool::alloc();

                        retval.back () =  fcn->call (ev, nargout, first_args.back ());

                        Pool::free(first_args);
                      }
                    catch (octave::index_exception& e)
                      {
                        error ("function indexing error");
                      }
                  }
                beg++;
                p_args++;

                if (n > beg)
                  {
                    if (retval.back ().length () == 0)
                      error ("indexing undefined value");
                    else
                      base_expr_val = retval.back ()(0);
                  }
                else
                  {
                    Pool::push_result(retval);

                    return;
                  }
              }
          }
      }

    if (base_expr_val.is_undefined ())
      base_expr_val = octave_value(expr->evaluate ( 1, endkey), false);

    bool indexing_object = (base_expr_val.isobject ()
                || base_expr_val.isjava ()
                || (base_expr_val.is_classdef_meta ()
                  && ! base_expr_val.is_package ()));

    std::list<octave_value_list> idx ;

    octave_value partial_expr_val = base_expr_val;

    for (int i = beg; i < n; i++)
      {
        if (i > beg)
          {
            auto &al = *p_args;

            if (! indexing_object || (al.has_magic_end ()))
              {
                try
                  {
                    octave_value_list tmp_list
                      = base_expr_val.subsref (type.substr (beg, i-beg),
                                   idx, nargout);

                    partial_expr_val
                      = tmp_list.length () ? tmp_list(0) : octave_value ();

                    if (! indexing_object)
                      {
                        base_expr_val = partial_expr_val;

                        if (partial_expr_val.is_cs_list ())
                          err_indexed_cs_list ();

                        Pool::free (retval);

                        retval = Pool::alloc(1);

                        retval.front()(0) = partial_expr_val;

                        beg = i;

                        Pool::free(idx);

                        if (partial_expr_val.isobject ()
                          || partial_expr_val.isjava ()
                          || (partial_expr_val.is_classdef_meta ()
                            && ! partial_expr_val.is_package ()))
                          {
                            indexing_object = true;
                          }
                      }
                  }
                catch (octave::index_exception& e)
                  {
                    error ("indexing error");
                  }
              }
          }

        switch (type[i])
          {
          case '(':
          case '{':
            {
              make_value_list (*p_args, partial_expr_val,endkey);

              auto result = Pool::pop_result ();

              idx.splice (idx.end () , result);
            }
            break;

          case '.':
            {
              auto struct_idx = Pool::alloc (1);

              struct_idx.back ()(0) = get_struct_index (*p_args);

              idx.splice (idx.end () , struct_idx);
            }
            break;

          default:
            panic_impossible ();
          }

        p_args++;
      }

    if (! idx.empty ())
      {
        if (! base_expr_val.is_function ()
          || base_expr_val.is_classdef_meta ())
          {
            try
              {
                Pool::free (retval);

                retval = Pool::alloc();

                retval.back () = base_expr_val.subsref (type.substr (beg, n-beg),
                        idx, nargout);

                beg = n;

                Pool::free (idx);
              }
            catch (octave::index_exception& e)
              {
                error ("indexing error");
              }
          }
        else
          {
            octave_function *fcn = base_expr_val.function_value ();

            if (fcn)
              {
                try
                  {
                    Pool::free (retval);

                    retval = Pool::alloc ();
                    octave::tree_evaluator& ev = octave::interpreter::the_interpreter () -> get_evaluator ();
                    retval.back () = fcn->call (ev, nargout, idx);
                  }
                catch (octave::index_exception& e)
                  {
                    error ("indexing error");
                  }
              }
          }
      }

    octave_value val = (retval.back ().length () ? retval.back()(0) : octave_value ());

    if (val.is_function ())
      {
        octave_function *fcn = val.function_value (true);

        if (fcn)
          {
            auto final_args = Pool::alloc ();

            if (! idx.empty ())
              {
                if (n - beg != 1)
                  error ("unexpected extra index at end of Expression");

                if (type[beg] != '(')
                  error ("invalid index type '%c' for function call",
                         type[beg]);

                Pool::free (final_args);

                final_args.splice(final_args.end(), idx, idx.begin ());
              }

            octave::tree_evaluator& ev = octave::interpreter::the_interpreter () -> get_evaluator ();

            Pool::free (retval);
            retval = Pool::alloc ();
            retval.back () = fcn->call (ev, nargout, final_args);
          }
      }

    Pool::free (idx);

    Pool::push_result (retval);
  }

  coder_lvalue
  Index::lvalue (void* in_idx)
  {
    std::list<octave_value_list>& arg = *static_cast<std::list<octave_value_list>*>(in_idx);

    coder_lvalue retval;

    std::list<octave_value_list> idx;

    std::string tmp_type;

    auto& m_expr = base;

    auto& m_type = idx_type;

    auto& m_args = arg_list;

    const int n = m_args.size ();

    auto p_args = m_args.begin ();

    assert(m_expr->is_Symbol());

    std::list<octave_value_list> arg1;

    retval = m_expr->lvalue (&arg1);

    octave_value tmp = octave_value(retval.value (&arg1), false);

    Pool::free (arg1);

    octave_idx_type tmpi = 0;

    std::list<octave_value_list> tmpidx;

    std::list<octave_value_list> resultidx;

    for (int i = 0; i < n; i++)
      {
        if (retval.numel () != 1)
          err_indexed_cs_list ();

        if (tmpi < i)
          {
            try
              {
                tmp = tmp.subsref (m_type.substr (tmpi, i-tmpi), tmpidx, true);
              }
            catch (octave::index_exception& e)
              {
                error("problems with range, invalid type etc.");
              }

            for (auto b : Pool::bitidx ())
              if (b)
                resultidx.splice (resultidx.end (), idx, idx.begin ());
              else
                resultidx.splice (resultidx.end (), tmpidx, tmpidx.begin ());

            Pool::bitidx ().clear ();
          }

        switch (m_type[i])
          {
          case '(':
            {
              make_value_list (*p_args, tmp);

              auto tidx = Pool::pop_result ();

              if (i < n - 1)
                {
                  if (m_type[i+1] != '.')
                    error ("() must be followed by . or close the index chain");

                  tmpidx.splice (tmpidx.end () , tidx);

                  tmpi = i+1;

                  Pool::bitidx ().push_back (0);
                }
              else
                {
                  idx.splice (idx.end () , tidx);

                  Pool::bitidx ().push_back (1);
                }

            }
            break;

          case '{':
            {
              make_value_list (*p_args, tmp);

              auto tidx = Pool::pop_result ();

              if (tmp.is_undefined ())
                {
                  if (tidx.back ().has_magic_colon ())
                    err_invalid_inquiry_subscript ();

                  tmp = Cell ();
                }
              else if (tmp.is_zero_by_zero ()
                       && (tmp.is_matrix_type () || tmp.is_string ()))
                {
                  tmp = Cell ();
                }

              retval.numel (tmp.numel (tidx.back ()));

              tmpidx.splice (tmpidx.end () , tidx);

              tmpi = i;

              Pool::bitidx ().push_back (0);
            }
            break;

          case '.':
            {
              octave_value tidx = get_struct_index (*p_args);

              auto tidxlist = Pool::alloc(1);

              tidxlist.back ()(0) = get_struct_index (*p_args);

              bool autoconv = (tmp.is_zero_by_zero ()
                               && (tmp.is_matrix_type () || tmp.is_string ()
                                   || tmp.iscell ()));

              if (i > 0 && m_type[i-1] == '(')
                {
                  octave_value_list& pidx = Pool::bitidx ().back () ? idx.back () : tmpidx.back ();

                  if (tmp.is_undefined ())
                    {
                      if (pidx.has_magic_colon ())
                        err_invalid_inquiry_subscript ();

                      tmp = octave_map ();
                    }
                  else if (autoconv)
                    tmp = octave_map ();

                  retval.numel (tmp.numel (pidx));

                  tmpi = i-1;

                  tmpidx.splice (tmpidx.end (), tidxlist);

                  Pool::bitidx ().push_back (0);
                }
              else
                {
                  if (tmp.is_undefined () || autoconv)
                    {
                      tmpi = i+1;

                      tmp = octave_value ();

                      idx.splice (idx.end (), tidxlist);

                      Pool::bitidx ().push_back (1);
                    }
                  else
                    {
                      retval.numel (tmp.numel (octave_value_list ()));

                      tmpi = i;

                      tmpidx.splice (tmpidx.end (), tidxlist);

                      Pool::bitidx ().push_back (0);
                    }
                }
            }
            break;

          default:
            panic_impossible ();
          }

        octave_value_list& lastidx = Pool::bitidx ().back () ? idx.back () : tmpidx.back ();

        if (lastidx.empty ())
          error ("invalid empty index list");

        p_args++;
      }
    for (auto b : Pool::bitidx ())
      if (b)
        resultidx.splice (resultidx.end (), idx, idx.begin ());
      else
        resultidx.splice (resultidx.end (), tmpidx, tmpidx.begin ());

    Pool::bitidx ().clear ();

    retval.set_index (m_type, &resultidx, &arg);

    return retval;
  }

  coder_value
  Null::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return octave_null_matrix::instance;
  }

  coder_value
  NullStr::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return (octave_null_str::instance);
  }
  coder_value
  NullSqStr::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return octave_null_sq_str::instance;;
  }
  coder_value
  string_literal_sq::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return (octave_value(str));
  }
  coder_value
  string_literal_dq::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return (octave_value(str,'"'));
  }
  coder_value double_literal::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return (octave_value(double(val)));
  }

  coder_value complex_literal::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return (octave_value(Complex (0.0, val)));
  }

  coder_value
  binary_expr (Ptr a, Ptr b, int nargout, const Endindex& endkey, bool short_circuit, octave_value::binary_op op)
  {
    octave_value left ( a->evaluate(nargout,endkey,short_circuit), false);

    octave_value right ( b->evaluate(nargout,endkey,short_circuit), false);

    octave::type_info& ti = octave::interpreter::the_interpreter ()->get_type_info ();

    return (::do_binary_op (ti, op, left, right));
  }

	coder_value
  unary_expr(Ptr a, int nargout, const Endindex& endkey, bool short_circuit, octave_value::unary_op op)
  {
    octave_value val ( a->evaluate(nargout,endkey,short_circuit), false);

    if (val.get_count () == 1)
      return val.do_non_const_unary_op ( op);

    octave::type_info& ti = octave::interpreter::the_interpreter () ->get_type_info ();

    return (::do_unary_op (ti, op, val));
	}

	coder_value
  pre_expr(Ptr a, octave_value::unary_op op)
  {
    std::list<octave_value_list> lst;

    coder_lvalue op_ref = a->lvalue (&lst);

    op_ref.do_unary_op (op, &lst);

    coder_value retval = op_ref.value (&lst);

    Pool::free (lst);

    return retval;
	}

	coder_value
  post_expr(Ptr a, octave_value::unary_op op)
  {
    std::list<octave_value_list> lst;

    coder_lvalue op_ref = a->lvalue (&lst);

    coder_value val = op_ref.value (&lst);

    op_ref.do_unary_op (op, &lst);

    Pool::free (lst);

    return val ;
	}

	coder_value
  trans_expr(Ptr a, int nargout, const Endindex& endkey, bool short_circuit, octave_value::unary_op op)
  {
    octave_value left ( a->evaluate(nargout,endkey,short_circuit), false);

    octave::type_info& ti = octave::interpreter::the_interpreter () ->get_type_info ();

    return (::do_unary_op (ti, op, left));
	}

	coder_value
  assign_expr(Ptr lhs, Ptr rhs, const Endindex& endkey, octave_value::assign_op op)
  {
		coder_value val ;

		try
      {
        std::list<octave_value_list> arg;

        coder_lvalue ult = lhs->lvalue (&arg);

        if (ult.numel () != 1)
          err_invalid_structure_assignment ();

        octave_value rhs_val ( rhs->evaluate (1,endkey), false);

        if (rhs_val.is_undefined ())
          error ("value on right hand side of assignment is undefined");

        if (rhs_val.is_cs_list ())
          {
            const octave_value_list lst = rhs_val.list_value ();

            if (lst.empty ())
              error ("invalid number of elements on RHS of assignment");

            rhs_val = lst(0);
          }

        ult.assign (op, rhs_val, &arg);

        val = ult.value (&arg);

        Pool::free (arg);
		  }
		catch (octave::index_exception& e)
      {
        std::string msg = e.message ();

        error_with_id (e.err_id (), "%s", msg.c_str ());
      }

    return val;
	}

  coder_value
  compound_binary_expr (Ptr op_lhs, Ptr op_rhs, const Endindex& endkey, octave_value::compound_binary_op op)
  {
    octave_value val;

    octave_value a ( op_lhs->evaluate(1,endkey), false);

    if (a.is_defined ())
      {
        octave_value b ( op_rhs->evaluate (1,endkey), false);

        if (b.is_defined ())
          {
            octave::type_info& ti = octave::interpreter::the_interpreter () ->get_type_info ();

            val = ::do_binary_op (ti, op, a, b);
          }
      }

    return val;
  }

  coder_value
  Plus::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return binary_expr (a, b, nargout,endkey,short_circuit,octave_value::op_add);
  }

  coder_value
  Minus::evaluate(int nargout, const Endindex& endkey, bool short_circuit)
  {
    return binary_expr (a, b, nargout,endkey,short_circuit,octave_value::op_sub);
  }

  coder_value
  Uplus::evaluate(int nargout, const Endindex& endkey, bool short_circuit)
  {
    return unary_expr (a, nargout,endkey,short_circuit,octave_value::op_uplus);
  }

  coder_value
  Uminus::evaluate(int nargout, const Endindex& endkey, bool short_circuit)
  {
    return unary_expr (a, nargout,endkey,short_circuit,octave_value::op_uminus);
  }

  coder_value
  Preinc::evaluate(int nargout, const Endindex& endkey, bool short_circuit)
  {
    return pre_expr (a, octave_value::op_incr);
  }

  coder_value
  Predec::evaluate(int nargout, const Endindex& endkey, bool short_circuit)
  {
    return pre_expr (a, octave_value::op_decr);
  }

  coder_value
  Postinc::evaluate(int nargout, const Endindex& endkey, bool short_circuit)
  {
    return post_expr (a, octave_value::op_incr);
  }

  coder_value
  Postdec::evaluate(int nargout, const Endindex& endkey, bool short_circuit)
  {
    return post_expr (a, octave_value::op_decr);
  }

  coder_value
  Not::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return unary_expr (a, nargout,endkey,short_circuit,octave_value::op_not);
  }

  coder_value
  Power::evaluate(int nargout, const Endindex& endkey, bool short_circuit)
  {
    return binary_expr (a,b, nargout,endkey,short_circuit,octave_value::op_el_pow);
  }

  coder_value
  Mpower::evaluate(int nargout, const Endindex& endkey, bool short_circuit)
  {
    return binary_expr (a,b, nargout,endkey,short_circuit,octave_value::op_pow);
  }

  coder_value
  Transpose::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return trans_expr (a, nargout,endkey,short_circuit,octave_value::op_transpose);
  }

  coder_value
  Ctranspose::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return trans_expr (a, nargout,endkey,short_circuit,octave_value::op_hermitian);
  }

  coder_value
  Mtimes::evaluate(int nargout, const Endindex& endkey, bool short_circuit)
  {
    return binary_expr (a,b, nargout,endkey,short_circuit,octave_value::op_mul);
  }

  coder_value
  Mrdivide::evaluate(int nargout, const Endindex& endkey, bool short_circuit)
  {
    return binary_expr (a,b, nargout,endkey,short_circuit,octave_value::op_div);
  }

  coder_value
  Mldivide::evaluate(int nargout, const Endindex& endkey, bool short_circuit)
  {
    return binary_expr (a,b, nargout,endkey,short_circuit,octave_value::op_ldiv);
  }

  coder_value
  Times::evaluate(int nargout, const Endindex& endkey, bool short_circuit)
  {
    return binary_expr (a,b, nargout,endkey,short_circuit,octave_value::op_el_mul);
  }

  coder_value
  Rdivide::evaluate(int nargout, const Endindex& endkey, bool short_circuit)
  {
    return binary_expr (a,b, nargout,endkey,short_circuit,octave_value::op_el_div);
  }

  coder_value
  Colon::evaluate(int nargout, const Endindex& endkey, bool short_circuit)
  {
    octave_value ov_base ( base->evaluate( 1,endkey,short_circuit), false);

    octave_value ov_increment ( inc.is_valid() ?
      octave_value(inc->evaluate(1,endkey,short_circuit), false) :
      octave_value(1.0));

    octave_value ov_limit ( limit->evaluate(1,endkey,short_circuit), false);

    return (::do_colon_op (ov_base, ov_increment, ov_limit, true));
  }

  coder_value
  Lt::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return binary_expr (a,b, nargout,endkey,short_circuit,octave_value::op_lt);
  }

  coder_value
  Gt::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return binary_expr (a,b, nargout,endkey,short_circuit,octave_value::op_gt);
  }

  coder_value
  Le::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return binary_expr (a,b, nargout,endkey,short_circuit,octave_value::op_le);
  }

  coder_value
  Ge::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return binary_expr (a,b, nargout,endkey,short_circuit,octave_value::op_ge);
  }

  coder_value
  Eq::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return binary_expr (a,b, nargout,endkey,short_circuit,octave_value::op_eq);
  }

  coder_value
  Ne::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return binary_expr (a,b, nargout,endkey,short_circuit,octave_value::op_ne);
  }

  coder_value
  And::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    if (short_circuit)
      {
        octave_value left ( a->evaluate( 1,endkey,short_circuit), false);

        if (left.ndims () == 2 && left.rows () == 1 && left.columns () == 1)
          {
            bool a_true = left.is_true ();

            if (!a_true)
              {
                return coder_value(new octave_bool(false));
              }

            octave_value right ( b->evaluate( 1,endkey,short_circuit), false);

            return coder_value(new octave_bool(right.is_true ()));
          }
      }

    octave_value left ( a->evaluate( 1,endkey,short_circuit), false);

    octave_value right ( b->evaluate( 1,endkey,short_circuit), false);

    octave::type_info& ti = octave::interpreter::the_interpreter () ->get_type_info ();

    return (::do_binary_op (ti, octave_value::op_el_and, left, right));
  }

  coder_value
  Or::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    if (short_circuit)
      {
        octave_value left ( a->evaluate( 1,endkey,short_circuit), false);

        if (left.ndims () == 2 && left.rows () == 1 && left.columns () == 1)
          {
            bool a_true = left.is_true ();

            if (a_true)
              {
                return coder_value(new octave_bool(true));
              }

            octave_value right ( b->evaluate( 1,endkey,short_circuit), false);

            return coder_value(new octave_bool(right.is_true ()));
          }
      }

    octave_value left ( a->evaluate( 1,endkey,short_circuit), false);

    octave_value right ( b->evaluate( 1,endkey,short_circuit), false);

    octave::type_info& ti = octave::interpreter::the_interpreter () ->get_type_info ();

    return (::do_binary_op (ti, octave_value::op_el_or, left, right));
  }

  coder_value
  AndAnd::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    octave_value left ( a->evaluate( 1,endkey,short_circuit), false);

    bool a_true = left.is_true ();

    if (!a_true)
      {
        return coder_value(new octave_bool(false));
      }

    octave_value right ( b->evaluate( 1,endkey,short_circuit), false);

    return coder_value(new octave_bool(right.is_true ()));
  }

  coder_value
  OrOr::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    octave_value left ( a->evaluate( 1,endkey,short_circuit), false);

    bool a_true = left.is_true ();

    if (a_true)
      {
        return coder_value(new octave_bool(true));
      }

    octave_value right ( b->evaluate( 1,endkey,short_circuit), false);

    return coder_value(new octave_bool(right.is_true ()));
  }

  coder_value
  Assign::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    octave_value val;
    try
      {
        std::list<octave_value_list> arg;
        coder_lvalue ult = lhs->lvalue (&arg);

        if (ult.numel () != 1)
          err_invalid_structure_assignment ();

        octave_value rhs_val ( rhs->evaluate ( 1,endkey), false);

        if (rhs_val.is_undefined ())
          error ("value on right hand side of assignment is undefined");

        if (rhs_val.is_cs_list ())
          {
            const octave_value_list lst = rhs_val.list_value ();

            if (lst.empty ())
              error ("invalid number of elements on RHS of assignment");

            rhs_val = lst(0);
          }

        ult.assign (octave_value::op_asn_eq, rhs_val, &arg);

        val = rhs_val;

        Pool::free (arg);
      }
    catch (octave::index_exception& e)
      {
        std::string msg = e.message ();

        error_with_id (e.err_id (), "%s", msg.c_str ());
      }

    return (val);
  }

  coder_value
  AssignAdd::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return assign_expr (lhs, rhs, endkey,octave_value::op_add_eq);
  }

  coder_value
  AssignSub::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return assign_expr (lhs, rhs, endkey,octave_value::op_sub_eq);
  }

  coder_value
  AssignMul::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return assign_expr (lhs, rhs, endkey,octave_value::op_mul_eq);
  }

  coder_value
  AssignDiv::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return assign_expr (lhs, rhs, endkey, octave_value::op_div_eq);
  }

  coder_value
  AssignLdiv::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return assign_expr (lhs, rhs, endkey,octave_value::op_ldiv_eq);
  }

  coder_value
  AssignPow::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return assign_expr (lhs, rhs, endkey,octave_value::op_pow_eq);
  }

  coder_value
  AssignElMul::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return assign_expr (lhs, rhs, endkey,octave_value::op_el_mul_eq);
  }

  coder_value
  AssignElDiv::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return assign_expr (lhs, rhs, endkey,octave_value::op_el_div_eq);
  }

  coder_value
  AssignElLdiv::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return assign_expr (lhs, rhs, endkey,octave_value::op_el_ldiv_eq);
  }

  coder_value
  AssignElPow::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return assign_expr (lhs, rhs, endkey,octave_value::op_el_pow_eq);
  }

  coder_value
  AssignElAnd::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return assign_expr (lhs, rhs, endkey,octave_value::op_el_and_eq);
  }


  coder_value
  AssignElOr::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return assign_expr (lhs, rhs, endkey,octave_value::op_el_or_eq);
  }

  void
  MultiAssign::evaluate_n( int nargout, const Endindex& endkey, bool short_circuit)
  {
    const int max_lhs_sz = 3;

    const int l_sz = lhs.size();

    const bool small_lhs = l_sz <= max_lhs_sz;

    std::vector<coder_lvalue > lvalue_list_vec;

    std::vector<std::list<octave_value_list> > lvalue_arg_vec ;

    coder_lvalue lvalue_list_arr[max_lhs_sz] ={};

    std::list<octave_value_list>  lvalue_arg_arr [max_lhs_sz] = {};

    if (! small_lhs)
      {
        lvalue_list_vec.reserve(lhs.size());

        lvalue_arg_vec.reserve(lhs.size());
      }

    int c = 0;

    for (auto& out: lhs)
      {
        if (! small_lhs)
          {
            lvalue_arg_vec.emplace_back ();

            lvalue_list_vec.emplace_back (out->lvalue (&lvalue_arg_vec.back ()));
          }
        else
          {
            lvalue_list_arr[c] = out->lvalue (&lvalue_arg_arr[c]);
            c++;
          }
      }

    int n_out = 0;

    int n_blackhole = 0;

    bool has_cs_out = false;

    coder_lvalue* lvalue_list = small_lhs ? lvalue_list_arr : lvalue_list_vec.data ();

    std::list<octave_value_list>* lvalue_arg = small_lhs ? lvalue_arg_arr : lvalue_arg_vec.data ();

    for (int i = 0 ; i < l_sz; i++)
      {
        octave_idx_type nel = lvalue_list[i].numel ();

        has_cs_out = has_cs_out || (nel != 1);

        n_out += nel;

        n_blackhole += lvalue_list[i].is_black_hole();
      }

    rhs->evaluate_n (n_out , endkey);

    auto rhs_val1 = Pool::pop_result();

    bool rhs_cs_list = rhs_val1.back().length () == 1
                    && rhs_val1.back()(0).is_cs_list ();

    const octave_value_list& rhs_val = (rhs_cs_list
                                       ? rhs_val1.back()(0).list_value ()
                                       : rhs_val1.back());
    octave_idx_type k = 0;

    octave_idx_type j = 0;

    const octave_idx_type n = rhs_val.length ();

    size_t ai = 0;

    std::list<octave_value_list> retval;

    if (l_sz  == 1 && lvalue_list[0].numel () == 0 && n > 0)
      {
        coder_lvalue& ult = lvalue_list[0];
        std::list<octave_value_list>& arg = lvalue_arg[0];

        if (ult.index_type () == "{" && ult.index_is_empty (&arg)
            && ult.is_undefined ())
          {
            ult.define (Cell (1, 1));

            ult.clear_index (&arg);

            std::list<octave_value_list> idx = Pool::alloc(1);

            idx.back ()(0) = octave_value (1);

            ult.set_index ("{", &idx, &arg);

            ult.assign (octave_value::op_asn_eq, octave_value (rhs_val), &arg);

            if (n == 1 )
              {
              if (nargout > 0)
                {
                  if (! rhs_cs_list)
                    {
                      Pool::push_result (rhs_val1);
                    }
                  else
                    {
                      retval = Pool::alloc(1);

                      retval.back()(0) = rhs_val(0);

                      Pool::push_result (retval);

                      Pool::free (rhs_val1);
                    }
                }
              else
                {
                  retval = Pool::alloc();

                  Pool::push_result (retval);

                  Pool::free (rhs_val1);
                }
              }

            Pool::free (arg);

            return;
          }
      }

    octave_value valarg;

    if (has_cs_out)
      valarg = octave_value (octave_value_list ());

    const int nargout_retval = std::min (n_out-n_blackhole, nargout);

    if (nargout_retval < n)
      retval = Pool::alloc (nargout_retval);

    for (int ii = 0 ; ii < l_sz; ii++)
      {
        coder_lvalue& ult = lvalue_list[ii];

        octave_idx_type nel = ult.numel ();

        std::list<octave_value_list>& arg = lvalue_arg[ai++];

        if (nel != 1)
          {
            if (k + nel > n)
              error ("some elements undefined in return list");

            octave_value_list ovl = rhs_val.slice (k, nel);

            *static_cast<octave_cs_list*> (valarg.internal_rep ()) = octave_cs_list (ovl);

            ult.assign (octave_value::op_asn_eq, valarg, &arg);

            if (nargout_retval < n)
              for (octave_idx_type i = k; i < nel && j < nargout_retval; i++)
                retval.back () (j++) = rhs_val(i);

            k += nel;
          }
        else
          {
            if (k < n)
              {
                if (ult.is_black_hole ())
                  {
                    k++;
                    continue;
                  }
                else
                  {
                    const octave_value& tmp = rhs_val (k);

                    if (tmp.is_undefined ())
                      error ("element number %lld undefined in return list",
                             k+1);

                    ult.assign (octave_value::op_asn_eq, tmp, &arg);

                    if (nargout_retval < n && j < nargout_retval)
                      retval.back () (j++) = tmp;

                    k++;
                  }
              }
            else
              {
                if (! ult.is_black_hole ())
                  error ("element number %lld undefined in return list", k+1);

                k++;
                continue;
              }
          }

        Pool::free (arg);
      }

    if (nargout_retval < n)
      {
        Pool::push_result (retval);

        Pool::free (rhs_val1);
      }
    else
      {
        Pool::push_result (rhs_val1);

        Pool::free (retval);
      }
  }

  coder_value
  MultiAssign::evaluate(int nargout, const Endindex& endkey, bool short_circuit)
  {
    evaluate_n(nargout, endkey, short_circuit);

    auto result = Pool::pop_result ();

    octave_value_list& vlist = result.back ();

    coder_value retval;

    if(vlist.empty ())
      retval = coder_value (octave_value ());
    else
      retval = coder_value (vlist (0));

    Pool::free (result);

    return retval;
  }




  coder_value
  TransMul::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return compound_binary_expr (op_lhs, op_rhs, endkey, octave_value::op_trans_mul);
  }

  coder_value
  MulTrans::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return compound_binary_expr (op_lhs, op_rhs, endkey, octave_value::op_mul_trans);
  }

  coder_value
  HermMul::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return compound_binary_expr (op_lhs, op_rhs, endkey, octave_value::op_herm_mul);
  }

  coder_value
  MulHerm::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return compound_binary_expr (op_lhs, op_rhs, endkey, octave_value::op_mul_herm);
  }

  coder_value
  TransLdiv::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return compound_binary_expr (op_lhs, op_rhs, endkey, octave_value::op_trans_ldiv);
  }

  coder_value
  HermLdiv::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return compound_binary_expr (op_lhs, op_rhs, endkey, octave_value::op_herm_ldiv);
  }

  coder_value
  NotAnd::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return compound_binary_expr (op_lhs, op_rhs, endkey, octave_value::op_el_not_and);
  }

  coder_value
  NotOr::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return compound_binary_expr (op_lhs, op_rhs, endkey, octave_value::op_el_not_or);
  }

  coder_value
  AndNot::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return compound_binary_expr (op_lhs, op_rhs, endkey, octave_value::op_el_and_not);
  }

  coder_value
  OrNot::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return compound_binary_expr (op_lhs, op_rhs, endkey, octave_value::op_el_or_not);
  }

  coder_value
  Handle::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return coder_value(new octave_fcn_handle (octave_value(static_cast<Symbol&>(op_rhs.get()).get_value(), true), name));
  }

  Anonymous::Anonymous(octave_base_value* arg) : value(new octave_fcn_handle (octave_value(arg))){}

  coder_value
  Anonymous::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    value.val->grab();
    return  coder_value(value.val);
  }

  NestedHandle::NestedHandle(Ptr arg, const char* name) : value(new octave_fcn_handle (octave_value(static_cast<Symbol&>(arg.get()).get_value(), true), name)){}

  coder_value
  NestedHandle::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    value.val->grab();
    return  coder_value(value.val);
  }
  coder_value
  Matrixc::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    octave_value_list rows(mat.size());

    octave_idx_type i = 0;

    for (auto& row : mat)
      {
        std::vector<octave_value> current_row;

        for (auto& col : row)
          {
            octave_value tmp ( col->evaluate(1,endkey), false);

            if (tmp.is_undefined ())
              {
                continue;
              }
            else
              {
                if (tmp.is_cs_list ())
                  {
                    octave_value_list tlst = tmp.list_value ();

                    for (octave_idx_type j = 0; j < tlst.length (); j++)
                      {
                        octave_value list_val = tlst(j);

                        if (list_val.is_undefined ())
                          continue;

                        current_row.push_back(list_val);
                      }
                  }
                else
                  current_row.push_back(tmp);
              }
          }

        if (current_row.empty ())
          current_row.push_back (::Matrix());

        auto hcat = Fhorzcat(octave_value_list(current_row),1);

        rows(i++) = hcat.empty()? octave_value(::Matrix()) : hcat(0);
      }

    octave_value_list retval = Fvertcat(rows,1);

    if (retval.empty())
      return (octave_value(::Matrix()));

    return  (retval(0));
  }

  coder_value
  Cellc::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    octave_value_list rows(cel.size());

    if(cel.size() == 1 && cel.begin()->size() == 0)
      return octave_value(::Cell());

    octave_idx_type i = 0;

    for (auto& row : cel)
      {
        std::vector<octave_value> current_row;

        for (auto& col : row)
          {
            octave_value tmp ( col->evaluate(1), false);

            if (tmp.is_undefined ())
              {
                current_row.push_back(::Cell());
              }
            else if (tmp.is_cs_list ())
              {
                octave_value_list tlst = tmp.list_value ();

                for (octave_idx_type j = 0; j < tlst.length (); j++)
                  {
                    octave_value list_val = tlst(j);

                    if (list_val.is_undefined ())
                      current_row.push_back(::Cell());

                    current_row.push_back(::Cell(list_val));
                  }
              }
            else
              current_row.push_back(::Cell(tmp));
          }

        if (current_row.empty ())
          current_row.push_back (::Cell());

        auto hcat = Fhorzcat(current_row,1);

        rows(i++) = hcat.empty()? octave_value(::Cell()) : hcat(0);
      }

    octave_value_list retval = Fvertcat(rows,1);

    if (retval.empty())
      return (octave_value(::Cell()));

    return  (retval(0));
  }
  coder_value
  Constant::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    val.val->grab();
    return  coder_value(val.val);
  }

  bool
  Switch::operator()(Ptr label)
  {
    octave_value label_value ( label->evaluate (1), false);

    if (label_value.is_defined ())
      {
        octave_value tmp (val.val, true);

        if (label_value.iscell ())
          {
            Cell cell (label_value.cell_value ());

            for (octave_idx_type i = 0; i < cell.rows (); i++)
              {
                for (octave_idx_type j = 0; j < cell.columns (); j++)
                  {
                    bool match = tmp.is_equal (cell(i,j));

                    if (match)
                      return true;
                  }
              }
          }
        else
          return tmp.is_equal (label_value);
      }

    return false;
  }

  coder_value
  End::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return endkey.compute_end();
  }

  coder_value
  MagicColon::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return (octave_value  (octave_value::magic_colon_t));
  }

  coder_lvalue
  Tilde::lvalue(void* idx)
  {
    coder_lvalue retval ;

    retval.mark_black_hole();

    return retval;
  }

  bool
  eval_decl_elt ( const Ptr_list& elt)
  {
    bool retval = false;

    if (elt.size()==2)
      {
        Ptr id ( *elt.begin() );

        Ptr expr (*std::next(elt.begin()));

        std::list<octave_value_list> arg;

        coder_lvalue ult = id->lvalue (&arg);

        octave_value init_val ( expr->evaluate (1), false);

        ult.assign (octave_value::op_asn_eq, init_val, &arg);

        retval = true;

        Pool::free (arg);
      }

    return retval;
  }

  void
  define_parameter_list_from_arg_vector
    (Ptr_list_list&& param_list, const octave_value_list& args)
  {
    int i = -1;

    for (auto& elt : param_list)
      {
        i++;

        if (i < args.length ())
          {
            if (args(i).is_defined () && args(i).is_magic_colon ())
              {
                if (! eval_decl_elt (elt))
                  error ("no default value for argument %d", i+1);
              }
            else
              {
                coder_lvalue ref = (*elt.begin())->lvalue (nullptr);

                ref.define (args(i));
              }
          }
        else
          eval_decl_elt (elt);
      }
  }

  void
  define_parameter_list_from_arg_vector(Ptr varargin, const octave_value_list& args)
  {
    coder_lvalue ref = varargin->lvalue (nullptr);

    ref.define (octave_value(args.cell_value()));
  }

  void
  define_parameter_list_from_arg_vector
    (Ptr_list_list&& param_list, Ptr varargin, const octave_value_list& args)
  {
    octave_idx_type num_named_args = param_list.size();

    define_parameter_list_from_arg_vector(std::move(param_list), args);


    if (args.length () > num_named_args)
      {
        octave_idx_type n = args.length () - num_named_args;

        octave_value_list retval = args.slice (num_named_args, n);

        define_parameter_list_from_arg_vector(varargin, retval);
      }
    else
      {
        coder_lvalue ref = varargin->lvalue (nullptr);

        ref.define (octave_value(Cell()));
      }
  }

  void
  make_return_list(Ptr_list&& ret_list, int nargout)
  {
    if(ret_list.size() == 0)
      {
        auto retval = Pool::alloc ();

        Pool::push_result (retval);

        return;
      }

    int n_retval = std::max(nargout,1);

    std::list<octave_value_list> retval1 = Pool::alloc (nargout);

    octave_value_list & retval = retval1.back ();

    int len = ret_list.size();

    int n = std::min(n_retval, len);

    auto ret = ret_list.begin();

    for (octave_idx_type i = 0; i < n; i++)
      {
        Expression& ex = *ret++;

        Symbol* sym = static_cast<Symbol*>(&ex);

        if (sym)
          retval(i) = octave_value(sym->get_value(), true);
      }

    Pool::push_result (retval1);
  }

  void
  make_return_list(Ptr_list&& ret_list, int nargout, Ptr varout)
  {
    int len = ret_list.size();

    make_return_list(std::move(ret_list), nargout);

    std::list<octave_value_list> retval1 = Pool::pop_result ();

    octave_value_list & retval = retval1.back ();

    Expression& ex = varout;

    Symbol* sym = static_cast<Symbol*>(&ex);

    const octave_base_value* val = sym->get_value();

    if (val->is_defined ())
      {
        int vlen=0;

        if (sym)
          vlen = val->numel ();

        if (! val->iscell ())
              error("varargout must be a cell array object");

        Cell vout = val->cell_value ();

        int n = std::min(nargout, len + vlen);

        int j = 0;

        for (int i = len; i < n; i++)
          retval(i) = vout(j++);
      }

    Pool::push_result (retval1);
  }

  void
  make_return_list(Ptr varout)
  {
    Expression& ex = varout;

    Symbol* sym = static_cast<Symbol*>(&ex);

    const octave_base_value* val = sym->get_value();

    if (! val->is_defined ())
      {
        auto retval = Pool::alloc ();
        Pool::push_result (retval);
        return;
      }

    if (! val->iscell ())
      error("varargout must be a cell array object");

    auto retval = Pool::alloc ();

    retval.back () = val->list_value ();

    Pool::push_result (retval);
  }

  void
  make_return_list()
  {
    auto retval = Pool::alloc ();

    Pool::push_result (retval);
  }

  void
  make_return_val (Ptr expr, int nargout)
  {
    auto retval = Pool::alloc (1);

    retval.back ()(0) = octave_value (expr->evaluate (nargout), false);

    Pool::push_result (retval);
  }

  void
  make_return_val ()
  {
    auto retval = Pool::alloc ();

    Pool::push_result (retval);
  }

  void AssignByRefInit( Ptr_list& lhs, const Ptr_list& rhs)
  {
    auto in = rhs.begin();

    for (auto& out: lhs )
      {
        auto& in_sym = static_cast<Symbol&>(in->get());

        if (in_sym.get_value()->is_defined() && !in_sym.get_value()->is_function())
          AssignByRef(out,*in++);
      }
  }

  void AssignByRefCon( Ptr_list& lhs, const Ptr_list& rhs,const Ptr_list& macr)
  {
    auto sym_name = rhs.begin();

    auto fcn_impl = macr.begin();

    for (auto& out: lhs )
      {
        auto& name = static_cast<Symbol&>(sym_name->get());

        if(name.get_value()->is_defined() )
          {
            if(name.get_value()->is_function())
              {
                AssignByVal(out, *fcn_impl);
              }
            else
              AssignByRef(out,*sym_name);
          }
        else
          AssignByRef(out,*sym_name);

        sym_name++;

        fcn_impl++;
      }
  }

  void AssignByValCon( Ptr_list& lhs, const Ptr_list& rhs,const Ptr_list& macr)
  {
    auto sym_name = rhs.begin();

    auto fcn_impl = macr.begin();

    for (auto& out: lhs )
      {
        auto& name = static_cast<Symbol&>(sym_name->get());
        if(name.get_value()->is_defined() )
          {
            if(name.get_value()->is_function())
              {
                AssignByVal(out,*fcn_impl);
              }
            else
              AssignByVal(out,*sym_name);
          }
        else
          AssignByVal(out,*sym_name);

        sym_name++;

        fcn_impl++;
      }
  }

  void SetEmpty(Symbol& id)
  {
    octave_value tmp_val{Matrix ()};

    octave_base_value * val = tmp_val.internal_rep ();

    val->grab ();

    octave_base_value*& id_val = id.get_value ();

    if (id_val)
      id_val->release ();

    id_val = val;
  }

  bool isargout1 (int nargout, double k)
  {
    if (k != octave::math::round (k) || k <= 0)
      error ("isargout: K must be a positive integer");

    return (k == 1 || k <= nargout) ;
  }

  void
  call_narginchk (int nargin, const octave_value_list& arg)
  {
    if (arg.length () != 2)
      {
        Ffeval (ovl (octave_value ("help"), octave_value ("narginchk")), 0);

        auto result = Pool::alloc ();

        Pool::push_result (result);
      }

    octave_value minargs = arg(0);

    octave_value maxargs = arg(1);

    if (! minargs.isnumeric () || minargs.numel () != 1)
      error ("narginchk: MINARGS must be a numeric scalar");

    else if (!maxargs.isnumeric () || maxargs.numel () != 1)
      error ("narginchk: MAXARGS must be a numeric scalar");

    else if (minargs.idx_type_value () > maxargs.idx_type_value ())
      error ("narginchk: MINARGS cannot be larger than MAXARGS");

    if (nargin < minargs.idx_type_value ())
      error ("narginchk: not enough input arguments");

    else if (nargin > maxargs.idx_type_value ())
      error ("narginchk: too many input arguments");

    auto result = Pool::alloc ();
    Pool::push_result (result);
  }

  void
  call_nargoutchk (int nargout, const octave_value_list& arg, int nout)
  {
    octave_value_list input = arg;

    auto result = Pool::alloc (1);

    octave_value& msg = result.back ()(0);

    const int minargs=0, maxargs=1, nargs=2, outtype=3;

    auto val = [&](int v) {return input (v).idx_type_value ();};

    auto str = [&](int v) {return input (v).string_value ();};

    int con = 0;

    if (nout == 1 && (arg.length () == 3 || arg.length () == 4))
      {
        if (input (minargs).numel ()!= 1 || input (maxargs).numel ()!= 1 || input (nargs).numel ()!= 1 )
          error ("nargoutchk: MINARGS, MAXARGS, and NARGS must be scalars");

        if (val (minargs) > val (maxargs))
          error ("nargoutchk: MINARGS must be <= MAXARGS");

        if (arg.length () == 3)
          input (outtype) = octave_value ("string");

        if (! (str (outtype) == "string" || str (outtype) == "struct"))
          error ("nargoutchk: output type must be either string or struct");

        std::string message , id;

        if (val (nargs) < val (minargs))
          {
            message = "not enough output arguments";

            id = "Octave:nargoutchk:not-enough-outputs";
          }
        else if (val (nargs) > val (minargs))
          {
            message = "too many output arguments";

            id = "Octave:nargoutchk:too-many-outputs";
          }

        if (str (outtype) == "string")
          {
            msg = octave_value (message);
          }
        else
          {
            msg = Fstruct (ovl (
              octave_value ("message"),
              octave_value (message),
              octave_value ("identifier"),
              octave_value (id)
            ), 1) (0);

            if (message.empty ())
              {
                msg = Fresize (ovl (msg, octave_value (0.0), octave_value (1.0)), 1) (0);
              }
          }
      }
    else if (nout == 0 && arg.length () == 2)
      {
        if (!  input (minargs).isnumeric () || input (minargs).numel () !=1 )
          error ("nargoutchk: MINARGS must be a numeric scalar");

        else if (!  input (maxargs).isnumeric () || input (maxargs).numel () !=1 )
          error ("nargoutchk: MAXARGS must be a numeric scalar");

        else if (val (minargs) > val (maxargs))
          error ("nargoutchk: MINARGS cannot be larger than MAXARGS");

        if (nargout < val (minargs))
          error ("nargoutchk: Not enough output arguments.");
        else if (nargout > val (maxargs))
          error ("nargoutchk: Too many output arguments.");
      }
    else
      {
        Ffeval (ovl (octave_value ("help"), octave_value ("nargoutchk")), 0);
      }

    Pool::push_result (result);
  }

  void
  call_nargin (int nargin, const octave_value_list& arg, int nout)
  {
    std::list<octave_value_list> result;

    if (arg.length() > 0)
      {
        result = Pool::alloc();
        result.back() = Fnargin(*octave::interpreter::the_interpreter (), arg, nout);
      }
    else
      {
        result = Pool::alloc(1);
        result.back()(0) =  octave_value(double(nargin));
      }

    Pool::push_result(result);
  }

  void
  call_nargout (int nargout, const octave_value_list& arg, int nout)
  {
    std::list<octave_value_list> result;

    if (arg.length() > 0)
      {
        result = Pool::alloc();
        result.back() = Fnargout(*octave::interpreter::the_interpreter (), arg, nout);
      }
    else
      {
        result = Pool::alloc(1);
        result.back()(0) =  octave_value(double(nargout));
      }

    Pool::push_result(result);
  }

  //FIXME : always returns true!
  void
  call_isargout (int nargout, const octave_value_list& arg, int nout)
  {
    auto result = Pool::alloc(1);

    result.back()(0) =  octave_value (false);

    octave_value& retval = result.back()(0);

    if (arg.length () != 1)
      {
        Ffeval (ovl (octave_value ("help"), octave_value ("isargout")), 0);
      }
    if (arg(0).is_scalar_type ())
      {
        double k = arg (0).double_value ();
        retval =  octave_value (isargout1 (nargout, k));
      }
    else if (arg (0).isnumeric ())
      {
        const NDArray ka = arg (0).array_value ();

        boolNDArray r (ka.dims ());

        for (octave_idx_type i = 0; i < ka.numel (); i++)
          r (i) = isargout1 (nargout, ka(i));

        retval = octave_value (r);
      }
    else
      err_wrong_type_arg ("isargout", arg (0));

    Pool::push_result (result);
  }

  void recover_from_excep ()
  {
    octave::interpreter::recover_from_exception ();
  }

  void throw_exec_excep ()
  {
    throw octave::execution_exception ();
  }

  void assign_try_id (Ptr expr_id)
  {
    std::list<octave_value_list> idx;
    coder_lvalue ult = expr_id->lvalue (&idx);
    octave_scalar_map err;
    err.assign ("message", last_error_message ());
    err.assign ("identifier", last_error_id ());
    err.assign ("stack", last_error_stack ());
    ult.assign (octave_value::op_asn_eq, err,&idx);
    Pool::free (idx);
  }

  class for_loop_rep
  {

  public:
    for_loop_rep (Ptr lhs, Ptr expr):
      val (expr->evaluate (1), false),
      looptype (
        val.is_range () ? range_loop
      : val.is_scalar_type () ? scalar_loop
      : val.is_matrix_type () || val.iscell () || val.is_string () || val.isstruct () ? matrix_loop
      : undefined_loop
      ),
      base_val(),
      rng (),
      steps(),
      idx (),
    ult(lhs->lvalue (&ult_idx))
    {
      switch (looptype)
        {
        case range_loop :
          {
            rng = val.range_value ();

            val = octave_value (0.0);

            base_val = val.internal_rep ();

            steps = rng.numel ();

            break;
          }

        case scalar_loop :
          {
            ult.assign (octave_value::op_asn_eq, val, &ult_idx);

            steps = 1;

            break;
          }
        case matrix_loop :
          {
            dim_vector dv = val.dims ().redim (2);

            octave_idx_type nrows = dv(0);

            steps = dv(1);

            if (val.ndims () > 2)
              val = val.reshape (dv);

            if (nrows > 0 && steps > 0)
              {
                idx = octave_value_list ();

                octave_idx_type iidx;

                if (nrows == 1)
                  {
                    idx.resize (1);

                    iidx = 0;
                  }
                else
                  {
                    idx.resize (2);

                    idx(0) = octave_value::magic_colon_t;

                    iidx = 1;
                  }
                idx (iidx) = iidx;

                base_val = idx.xelem (iidx).internal_rep ();
              }
            else
              {
                ult.assign (octave_value::op_asn_eq, val, &ult_idx);

                steps = 0;
              }
            break;
          }
        case undefined_loop :
          {
            error ("invalid type in for loop expression");

            break;
          }
        }
    }
    void set_loop_val (octave_idx_type i)
    {
      if (looptype == range_loop)
        {
          static_cast<octave_scalar*>(base_val)->scalar_ref() = rng.elem (i);

          ult.assign (octave_value::op_asn_eq, val, &ult_idx);
        }
      else if (looptype == matrix_loop)
        {
          static_cast<octave_scalar*>(base_val)->scalar_ref()  = i + 1;

          octave_value tmp = val.do_index_op (idx);

          ult.assign (octave_value::op_asn_eq, tmp, &ult_idx);
        }
    }

    ~for_loop_rep ()
    {
      Pool::free (ult_idx);
    }

    for_iterator begin()  { return for_iterator (this,0); }

    for_iterator end()  { return for_iterator (this, steps); }

  private:

    octave_value val;

    const LoopType looptype;

    octave_base_value* base_val;

    Range  rng;

    octave_idx_type steps;

    octave_value_list idx;

    coder_lvalue ult;

    std::list<octave_value_list> ult_idx;
  };

  class struct_loop_rep
  {

  public:
    struct_loop_rep (Ptr v ,Ptr k, Ptr expr)
    {
      rhs = (expr)->evaluate (1);

      if (! rhs.val->isstruct ())
        error ("in statement 'for [X, Y] = VAL', VAL must be a structure");

      val_ref = (v)->lvalue (&idxs[0]);

      key_ref = (k)->lvalue (&idxs[1]);

      keys = rhs.val->map_keys ();

      is_scalar_struct = dynamic_cast <octave_scalar_struct*> (rhs.val);

      if (is_scalar_struct)
        tmp_scalar_val = rhs.val->scalar_map_value ();
      else
        tmp_val = rhs.val->map_value ();

      nel = keys.numel ();

      if (nel > 0)
        {
          if (! is_scalar_struct)
            {
              const Cell& c = tmp_val.contents(0) ;

              val = c;
            }
        }
    }

    ~struct_loop_rep ()
    {
      Pool::free (idxs[0]);
      Pool::free (idxs[1]);
    }

    void set_loop_val (octave_idx_type i)
    {
      const std::string &key = keys.xelem(i);

      if (is_scalar_struct)
        {
          const octave_value& val_lst = tmp_scalar_val.contents(i) ;

          val_ref.assign (octave_value::op_asn_eq, val_lst, &idxs[0] );
        }
      else
        {
          const Cell& val_lst = tmp_val.contents(i);

          octave_base_matrix<Cell>* val_mat = static_cast<octave_base_matrix<Cell>*>(val.internal_rep ());

          val_mat->matrix_ref () = val_lst;

          val_ref.assign (octave_value::op_asn_eq, val, &idxs[0] );
        }

      key_ref.assign (octave_value::op_asn_eq, key, &idxs[1]);
    }

    struct_iterator begin()  { return struct_iterator (this,0); }

    struct_iterator end()  { return struct_iterator (this, nel); }

  private:

    string_vector keys;

    octave_map tmp_val;

    octave_scalar_map tmp_scalar_val;

    std::list<octave_value_list>  idxs [2] ;

    coder_lvalue val_ref;

    coder_lvalue key_ref;

    coder_value rhs;

    octave_idx_type nel;

    octave_value val;

    bool is_scalar_struct;

  };

  for_loop::for_loop (Ptr lhs, Ptr expr)
  : rep (new for_loop_rep(lhs, expr))
  {}

  for_iterator for_loop::begin()  { return rep->begin (); }

  for_iterator for_loop::end()  { return rep->end (); }

  for_loop::~for_loop ()
  {
    delete rep;
  }

  struct_loop::struct_loop (Ptr v, Ptr k, Ptr expr)
  : rep (new struct_loop_rep(v, k, expr))
  {}

  struct_iterator struct_loop::begin()  { return rep->begin (); }

  struct_iterator struct_loop::end()  { return rep->end (); }

  struct_loop::~struct_loop ()
  {
    delete rep;
  }

  octave_idx_type for_iterator::operator*() const { dis->set_loop_val (i); return i; }

  octave_idx_type struct_iterator::operator*() const{ dis->set_loop_val (i); return i; }

  void call_error(const char* str)
  {
    error ("%s", str);
  }
  octave_idx_type ovl_length (const octave_value_list& args)
  {
    return args.length ();
  }
}

    )source"; return s;
  }
}
