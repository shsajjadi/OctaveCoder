#include "coder_runtime.h"

namespace coder_compiler
{
  const std::string& runtime_header()
  {
    static const std::string s = R"header(

#include "version.h"
#include <functional>
#include <initializer_list>
#include <type_traits>
#include <utility>

#if OCTAVE_MAJOR_VERSION < 6
extern int buffer_error_messages;
#endif

class octave_value_list;

class octave_function;

namespace octave
{
  class tree_evaluator;

  class interpreter;

  class execution_exception;
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

  typedef void (*stateless_function) (coder_value_list&, const octave_value_list&, int);

  struct Symbol;

  typedef const Symbol& (*function_maker) ();

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

  octave_base_value* fcn2ov(stateless_function f);

  octave_base_value* stdfcntoov (const std::function<void(coder_value_list&, const octave_value_list&, int)>& fcn);

  octave_base_value* stdfcntoov (std::function<void(coder_value_list&, const octave_value_list&, int)>&& fcn);

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
      : m_sym (nullptr), m_black_hole (false), m_type (""),
        m_nel (1)
    { }

    explicit coder_lvalue (octave_base_value*& sr)
      : m_sym (&sr), m_black_hole (false),
        m_type (""), m_nel (1)
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
                              const octave_value& rhs, coder_value_list& idx) ;

    void numel (octave_idx_type n) { m_nel = n; }

    octave_idx_type numel (void) const { return m_nel; }

    void set_index (const char *t,
                                 coder_value_list& i, coder_value_list& idx);

    void clear_index (coder_value_list& idx) ;

    const char *index_type (void) const { return m_type; }

    bool index_is_empty (coder_value_list& idx) const;

    void do_unary_op (int op, coder_value_list& idx) ;

    coder_value value (coder_value_list& idx) const;

  private:

    mutable octave_base_value** m_sym;

    bool m_black_hole;

    const char *m_type;

    octave_idx_type m_nel;

    friend class for_loop_rep;
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

  struct Endindex
  {
    Endindex(octave_base_value* object, const char *type, const coder_value_list* idx, int position, int num):
      indexed_object(object),
      idx_type(type),
      idx_list(idx),
      index_position(position),
      num_indices(num)
      {}

    Endindex()=default;

    coder_value compute_end() const;

    mutable octave_base_value *indexed_object = nullptr;
    const char *idx_type = "";
    const coder_value_list* idx_list = nullptr;
    int index_position = 0;
    int num_indices = 0;
  };

  struct Expression
  {
    Expression () = default;

    virtual coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    virtual void evaluate_n(coder_value_list& output,int nargout=1, const Endindex& endkey=Endindex(), bool short_circuit=false) ;

    virtual coder_lvalue lvalue(coder_value_list&){return coder_lvalue();}

    virtual bool is_Symbol() {return false;}

    virtual bool is_Tilde() {return false;}

    operator bool();
  };

  struct LightweightExpression : public Expression
  {
    LightweightExpression()=default;
    LightweightExpression(LightweightExpression const&)=delete;
    LightweightExpression(LightweightExpression &&)=delete;
    LightweightExpression& operator=(LightweightExpression const&)=delete;
    LightweightExpression& operator=(LightweightExpression &&)=delete;
  };

  struct Symbol : Expression
  {
    explicit Symbol(const char *fcn_name, const char *file_name, const char *path, file_type type );
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
    evaluate_n(coder_value_list& output,int nargout=1,
      const Endindex& endkey=Endindex(), bool short_circuit=false);

    coder_lvalue lvalue(coder_value_list&);

    void
    call (coder_value_list& output, int nargout, const octave_value_list& args);

    bool
    is_defined () const;

    bool
    is_function () const;

    mutable octave_base_value* value;

    Symbol* reference;
  };

  struct Index : LightweightExpression
  {
    Index(Ptr arg, const char *type, Ptr_list_list&& arg_list)
    : base(arg),idx_type(type),arg_list(arg_list){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    void evaluate_n(coder_value_list& output,int nargout=1, const Endindex& endkey=Endindex(), bool short_circuit=false) ;

    coder_lvalue
    lvalue (coder_value_list&);

    Ptr base;

    const char *idx_type;

    Ptr_list_list& arg_list;
  };

  struct Null : LightweightExpression
  {
    Null () = default;

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);
  };

  struct NullStr : LightweightExpression
  {
    NullStr () = default;

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);
  };

  using NullDqStr = NullStr;

  struct NullSqStr : LightweightExpression
  {
    NullSqStr()=default;

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);
  };

  struct string_literal_sq : Expression
  {
    explicit string_literal_sq(const char *str) :str(str){  }

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    const char *str;
  };

  struct string_literal_dq : Expression
  {
    explicit string_literal_dq(const char *str) :str(str){  }

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    const char *str;
  };

  using string_literal = string_literal_sq;

  struct double_literal : Expression
  {
    explicit double_literal(double d) :val(d){  }

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    double val;
  };

  struct int8_literal : Expression
  {
    explicit int8_literal(unsigned long long int d) :val(d){  }

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    unsigned long long int val;
  };

  struct int16_literal : Expression
  {
    explicit int16_literal(unsigned long long int d) :val(d){  }

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    unsigned long long int val;
  };

  struct int32_literal : Expression
  {
    explicit int32_literal(unsigned long long int d) :val(d){  }

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    unsigned long long int val;
  };

  struct int64_literal : Expression
  {
    explicit int64_literal(unsigned long long int d) :val(d){  }

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    unsigned long long int val;
  };

  struct uint8_literal : Expression
  {
    explicit uint8_literal(unsigned long long int d) :val(d){  }

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    unsigned long long int val;
  };

  struct uint16_literal : Expression
  {
    explicit uint16_literal(unsigned long long int d) :val(d){  }

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    unsigned long long int val;
  };

  struct uint32_literal : Expression
  {
    explicit uint32_literal(unsigned long long int d) :val(d){  }

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    unsigned long long int val;
  };

  struct uint64_literal : Expression
  {
    explicit uint64_literal(unsigned long long int d) :val(d){  }

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    unsigned long long int val;
  };

  struct complex_literal : Expression
  {
    explicit complex_literal(double d) :val(d){ }

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

  inline int8_literal operator"" _i8(unsigned long long int val)
  {
    return int8_literal(val);
  }

  inline int16_literal operator"" _i16(unsigned long long int val)
  {
    return int16_literal(val);
  }

  inline int32_literal operator"" _i32(unsigned long long int val)
  {
    return int32_literal(val);
  }

  inline int64_literal operator"" _i64(unsigned long long int val)
  {
    return int64_literal(val);
  }

  inline uint8_literal operator"" _ui8(unsigned long long int val)
  {
    return uint8_literal(val);
  }

  inline uint16_literal operator"" _ui16(unsigned long long int val)
  {
    return uint16_literal(val);
  }

  inline uint32_literal operator"" _ui32(unsigned long long int val)
  {
    return uint32_literal(val);
  }

  inline uint64_literal operator"" _ui64(unsigned long long int val)
  {
    return uint64_literal(val);
  }

  inline complex_literal operator"" _i(unsigned long long int val)
  {
    return complex_literal(double(val));
  }

  inline complex_literal operator"" _i(long double d)
  {
    return complex_literal(double(d));
  }

  inline string_literal_sq operator"" __( const char *str, std::size_t sz)
  {
    return string_literal_sq (str);
  }

  inline string_literal_sq operator"" _sq( const char *str, std::size_t sz)
  {
    return string_literal_sq (str);
  }

  inline string_literal_dq operator"" _dq( const char *str, std::size_t sz)
  {
    return string_literal_dq (str);
  }

  struct Plus : public LightweightExpression
  {
    Plus(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Minus : public LightweightExpression
  {
    Minus(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Uplus : public LightweightExpression
  {
    Uplus(Ptr rhs) : a(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
  };

  struct Uminus : public LightweightExpression
  {
    Uminus(Ptr rhs) : a(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
  };

  struct Preinc : public LightweightExpression
  {
    Preinc(Ptr rhs) : a(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
  };

  struct Predec : public LightweightExpression
  {
    Predec(Ptr rhs) : a(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
  };

  struct Postinc : public LightweightExpression
  {
    Postinc(Ptr lhs) : a(lhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
  };

  struct Postdec : public LightweightExpression
  {
    Postdec(Ptr lhs) : a(lhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
  };

  struct Not : public LightweightExpression
  {
    Not(Ptr rhs) : a(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
  };

  struct Power : public LightweightExpression
  {
    Power(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Mpower : public LightweightExpression
  {
    Mpower(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Transpose : public LightweightExpression
  {
    Transpose(Ptr lhs) : a(lhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
  };

  struct Ctranspose : public LightweightExpression
  {
    Ctranspose(Ptr lhs) : a(lhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
  };

  struct Mtimes : public LightweightExpression
  {
    Mtimes(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Mrdivide : public LightweightExpression
  {
    Mrdivide(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Mldivide : public LightweightExpression
  {
    Mldivide(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Times : public LightweightExpression
  {
    Times(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Rdivide : public LightweightExpression
  {
    Rdivide(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Colon : public LightweightExpression
  {
    Colon(Ptr base, Ptr limit) : base(base),limit(limit){}

    Colon(Ptr base, Ptr inc, Ptr limit) : base(base), inc(inc), limit(limit){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr base;
    Ptr inc;
    Ptr limit;
  } ;

  struct Lt : public LightweightExpression
  {
    Lt(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Gt : public LightweightExpression
  {
    Gt(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Le : public LightweightExpression
  {
    Le(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Ge : public LightweightExpression
  {
    Ge(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Eq : public LightweightExpression
  {
    Eq(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Ne : public LightweightExpression
  {
    Ne(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct And : public LightweightExpression
  {
    And(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Or : public LightweightExpression
  {
    Or(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct AndAnd : public LightweightExpression
  {
    AndAnd(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct OrOr : public LightweightExpression
  {
    OrOr(Ptr lhs, Ptr rhs) : a(lhs),b(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr a;
    Ptr b;
  };

  struct Assign : public LightweightExpression
  {
    Assign(Ptr lhs, Ptr rhs) : lhs(lhs),rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr lhs;
    Ptr rhs;
  };

  struct AssignAdd : public LightweightExpression
  {
    AssignAdd(Ptr lhs, Ptr rhs) : lhs(lhs),rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr lhs;
    Ptr rhs;
  };

  struct AssignSub : public LightweightExpression
  {
    AssignSub(Ptr lhs, Ptr rhs) : lhs(lhs),rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr lhs;
    Ptr rhs;
  };

  struct AssignMul : public LightweightExpression
  {
    AssignMul(Ptr lhs, Ptr rhs) : lhs(lhs),rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr lhs;
    Ptr rhs;
  };

  struct AssignDiv : public LightweightExpression
  {
    AssignDiv(Ptr lhs, Ptr rhs) : lhs(lhs),rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr lhs;
    Ptr rhs;
  };

  struct AssignLdiv : public LightweightExpression
  {
    AssignLdiv(Ptr lhs, Ptr rhs) : lhs(lhs),rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr lhs;
    Ptr rhs;
  };

  struct AssignPow : public LightweightExpression
  {
    AssignPow(Ptr lhs, Ptr rhs) : lhs(lhs),rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr lhs;
    Ptr rhs;
  };

  struct AssignElMul : public LightweightExpression
  {
    AssignElMul(Ptr lhs, Ptr rhs) : lhs(lhs),rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr lhs;
    Ptr rhs;
  };

  struct AssignElDiv : public LightweightExpression
  {
    AssignElDiv(Ptr lhs, Ptr rhs) : lhs(lhs),rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr lhs;
    Ptr rhs;
  };

  struct AssignElLdiv : public LightweightExpression
  {
    AssignElLdiv(Ptr lhs, Ptr rhs) : lhs(lhs),rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr lhs;
    Ptr rhs;
  };

  struct AssignElPow : public LightweightExpression
  {
    AssignElPow(Ptr lhs, Ptr rhs) : lhs(lhs),rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr lhs;
    Ptr rhs;
  };

  struct AssignElAnd : public LightweightExpression
  {
    AssignElAnd(Ptr lhs, Ptr rhs) : lhs(lhs),rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr lhs;
    Ptr rhs;
  };

  struct AssignElOr : public LightweightExpression
  {
    AssignElOr(Ptr lhs, Ptr rhs) : lhs(lhs),rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr lhs;
    Ptr rhs;
  };

  struct MultiAssign : public LightweightExpression
  {
    MultiAssign(Ptr_list&& lhs, Ptr rhs) : lhs(lhs),rhs(rhs){}

    void evaluate_n(coder_value_list& output,int nargout=1, const Endindex& endkey=Endindex(), bool short_circuit=false) ;

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false) ;

    Ptr_list& lhs;
    Ptr rhs;
  };

  struct TransMul : public LightweightExpression
  {
    TransMul(Ptr lhs, Ptr rhs) : op_lhs(lhs),op_rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr op_lhs;
    Ptr op_rhs;
  };

  struct MulTrans : public LightweightExpression
  {
    MulTrans(Ptr lhs, Ptr rhs) : op_lhs(lhs),op_rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr op_lhs;
    Ptr op_rhs;
  };

  struct HermMul : public LightweightExpression
  {
    HermMul(Ptr lhs, Ptr rhs) : op_lhs(lhs),op_rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr op_lhs;
    Ptr op_rhs;
  };

  struct MulHerm : public LightweightExpression
  {
    MulHerm(Ptr lhs, Ptr rhs) : op_lhs(lhs),op_rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr op_lhs;
    Ptr op_rhs;
  };

  struct TransLdiv : public LightweightExpression
  {
    TransLdiv(Ptr lhs, Ptr rhs) : op_lhs(lhs),op_rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr op_lhs;
    Ptr op_rhs;
  };

  struct HermLdiv : public LightweightExpression
  {
    HermLdiv(Ptr lhs, Ptr rhs) : op_lhs(lhs),op_rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr op_lhs;
    Ptr op_rhs;
  };

  struct NotAnd : public LightweightExpression
  {
    NotAnd(Ptr lhs, Ptr rhs) : op_lhs(lhs),op_rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr op_lhs;
    Ptr op_rhs;
  };

  struct NotOr : public LightweightExpression
  {
    NotOr(Ptr lhs, Ptr rhs) : op_lhs(lhs),op_rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr op_lhs;
    Ptr op_rhs;
  };

  struct AndNot : public LightweightExpression
  {
    AndNot(Ptr lhs, Ptr rhs) : op_lhs(lhs),op_rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr op_lhs;
    Ptr op_rhs;
  };

  struct OrNot : public LightweightExpression
  {
    OrNot(Ptr lhs, Ptr rhs) : op_lhs(lhs),op_rhs(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr op_lhs;
    Ptr op_rhs;
  };

  struct Handle : public LightweightExpression
  {
    Handle(Ptr rhs, const char* nm) : op_rhs(rhs) ,name(nm), fmaker(nullptr){}

    Handle(function_maker rhs, const char* nm) : op_rhs() ,name(nm), fmaker(rhs){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr op_rhs;
    const char* name;
    function_maker fmaker;
  };

  struct Anonymous : public LightweightExpression
  {
    Anonymous(octave_base_value* arg);

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    coder_value value;
  };

  struct NestedHandle : public LightweightExpression
  {
    NestedHandle(Ptr arg, const char* name);

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    coder_value value;
  };

  struct Matrixc : public LightweightExpression
  {
    Matrixc(Ptr_list_list&& expr) : mat(expr){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr_list_list& mat;
  };

  struct Cellc : public LightweightExpression
  {
    Cellc(Ptr_list_list&& expr) : cel(expr){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    Ptr_list_list& cel;
  };

  struct Constant : public LightweightExpression
  {
    Constant(Ptr expr) : val(expr->evaluate(1)){}
    Constant(Constant&&other) :
    val(std::move(other.val)){}

    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);

    coder_value val;
  };

  struct Switch
  {
    Switch() = delete;

    Switch(Ptr expr) : val(expr->evaluate(1)){}

    bool operator()(Ptr label);

    coder_value val;
  };

  struct End : public LightweightExpression
  {
    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);
  };

  struct MagicColon : public LightweightExpression
  {
    coder_value evaluate(int nargout=0, const Endindex& endkey=Endindex(), bool short_circuit=false);
  };

  struct Tilde : public LightweightExpression
  {
    coder_lvalue lvalue(coder_value_list&);
    bool is_Tilde () {return true;}
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

  void make_return_list (coder_value_list&, Ptr_list&& ret_list, int nargout);
  void make_return_list (coder_value_list&,Ptr_list&& ret_list, int nargout, Ptr varout);
  void make_return_list (coder_value_list&,Ptr varout);
  void make_return_list (coder_value_list&);
  void make_return_val (coder_value_list&,Ptr expr, int nargout);
  void make_return_val (coder_value_list&);

  inline void AssignByRef( Ptr lhs, Ptr rhs)
  {
    static_cast<Symbol&>(lhs.get()).get_reference() = static_cast<Symbol&>(rhs.get()).get_reference();
  }

template <int size>
    using array = Ptr(&)[size];

template <int size>
  void AssignByRef( array<size>& lhs, array<size>& rhs)
  {
    Ptr* in = rhs;

    for (Ptr out: lhs )
      {
        AssignByRef(out,*in++);
      }
  }

  inline void AssignByVal( Ptr lhs, Ptr rhs)
  {
    static_cast<Symbol&>(lhs.get()) = static_cast<Symbol&>(rhs.get());
  }

template <int size>
  void AssignByVal( array<size>& lhs, array<size>& rhs)
  {
    Ptr* in = rhs;

    for (Ptr out: lhs )
      {
        AssignByVal(out,*in++);
      }
  }

template <int size>
  void AssignByRefInit( array<size>& lhs, array<size>& rhs)
  {
    Ptr* in = rhs;

    for (Ptr out: lhs )
      {
        auto& in_sym = static_cast<Symbol&>(in->get());

        if (in_sym.is_defined() && !in_sym.is_function())
          AssignByRef(out,*in++);
      }
  }

template <int size>
  void AssignByRefCon( array<size>& lhs, array<size>& rhs,const Ptr_list& macr)
  {
    Ptr* sym_name = rhs;

    auto fcn_impl = macr.begin();

    for (Ptr out: lhs )
      {
        auto& name = static_cast<Symbol&>(sym_name->get());

        if(name.is_defined() )
          {
            if(name.is_function())
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

template <int size>
  void AssignByValCon( array<size>& lhs, array<size>& rhs,const Ptr_list& macr)
  {
    Ptr* sym_name = rhs;

    auto fcn_impl = macr.begin ();

    for (Ptr out: lhs )
      {
        auto& name = static_cast<Symbol&>(sym_name->get());
        if(name.is_defined() )
          {
            if(name.is_function())
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

  void SetEmpty(Symbol& id);

  bool isargout1 (int nargout, double k);

  void
  call_narginchk (coder_value_list& output, int nargin, const octave_value_list& arg);

  void
  call_nargoutchk (coder_value_list& output, int nargout, const octave_value_list& arg, int nout);

  void
  call_nargin (coder_value_list& output, int nargin, const octave_value_list& arg, int nout);

  void
  call_nargout (coder_value_list& output, int nargout, const octave_value_list& arg, int nout);

  void
  call_isargout (coder_value_list& output, int nargout, const octave_value_list& arg, int nout);

  void recover_from_execution_excep ();

  void recover_from_execution_and_interrupt_excep ();

  void assign_try_id (Ptr expr_id);

  enum LoopType
  {
    range_loop,
    scalar_loop,
    matrix_loop,
    undefined_loop
  };

  struct unwind_ex {};
  struct break_unwind:unwind_ex {};
  struct continue_unwind:unwind_ex {};
  struct normal_unwind:unwind_ex {};
  struct return_unwind:unwind_ex {};

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
    for_iterator(for_loop_rep *dis, octave_idx_type i): dis(dis), i(i) {}
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
    struct_iterator(struct_loop_rep *dis, octave_idx_type i): dis(dis), i(i) {}
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
    for_loop()=delete;
    for_loop(for_loop const&)=delete;
    for_loop(for_loop &&)=delete;
    for_loop& operator=(for_loop const&)=delete;
    for_loop& operator=(for_loop &&)=delete;
    for_loop (Ptr lhs, Ptr expr, bool fast_loop);
    for_iterator begin() ;
    for_iterator end() ;
    ~for_loop();
  };

  class struct_loop
  {
    struct_loop_rep* rep;
  public:
    struct_loop()=delete;
    struct_loop(struct_loop const&)=delete;
    struct_loop(struct_loop &&)=delete;
    struct_loop& operator=(struct_loop const&)=delete;
    struct_loop& operator=(struct_loop &&)=delete;
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

#if OCTAVE_MAJOR_VERSION >=6

#define TRY_CATCH( try_code , catch_code )\
{\
  try\
    {\
      try_code\
    }\
  catch (unwind_ex)\
    { throw; }\
  catch (...)\
    {\
      recover_from_execution_excep ();\
      {catch_code}\
    }\
}

#define TRY_CATCH_VAR( try_code , catch_code , expr_id )\
{\
  try\
    {\
      try_code\
    }\
  catch (unwind_ex)\
    { throw; }\
  catch (...)\
    {\
      recover_from_execution_excep ();\
      assign_try_id (expr_id);\
      {catch_code}\
    }\
}

#else

#define TRY_CATCH( try_code , catch_code )\
{\
  unwindprotect buferrtmp (buffer_error_messages);\
  buffer_error_messages++;\
  try\
    {\
      try_code\
    }\
  catch (unwind_ex)\
    { throw; }\
  catch (...)\
    {\
      recover_from_execution_excep ();\
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
  catch (unwind_ex)\
    { throw; }\
  catch (...)\
    {\
      recover_from_execution_excep ();\
      assign_try_id (expr_id);\
      {catch_code}\
    }\
}

#endif

#define UNWIND_PROTECT(unwind_protect_code , cleanup_code , return_propagates)\
{\
  try\
    {\
      try\
        {\
          {unwind_protect_code}\
        }\
      catch (...)\
        {\
          throw;\
        }\
      throw normal_unwind {};\
    }\
  catch (...)\
    {\
      try\
        {\
          try\
            {\
              throw;\
            }\
          catch (unwind_ex)\
            {\
              throw;\
            }\
          catch (...)\
            {\
              recover_from_execution_and_interrupt_excep ();\
              throw;\
            }\
        }\
      catch (...)\
        {\
          {cleanup_code}\
          try\
            {\
              throw;\
            }\
          catch (normal_unwind)\
            {\
            }\
          catch (return_unwind)\
            {\
              if (return_propagates) throw; else goto Return;\
            }\
          catch (...)\
            {\
              throw;\
            }\
        }\
    }\
}

#define UNWIND_PROTECT_LOOP(unwind_protect_code , cleanup_code , return_propagates , loop_propagates)\
{\
  try\
    {\
      try\
        {\
          {unwind_protect_code}\
        }\
      catch (...)\
        {\
          throw;\
        }\
      throw normal_unwind {};\
    }\
  catch (...)\
    {\
      try\
        {\
          try\
            {\
              throw;\
            }\
          catch (unwind_ex)\
            {\
              throw;\
            }\
          catch (...)\
            {\
              recover_from_execution_and_interrupt_excep ();\
              throw;\
            }\
        }\
      catch (...)\
        {\
          {cleanup_code}\
          try\
            {\
              throw;\
            }\
          catch (normal_unwind)\
            {\
            }\
          catch (return_unwind)\
            {\
              if (return_propagates) throw; else goto Return;\
            }\
          catch (break_unwind)\
            {\
              if (loop_propagates) throw; else break;\
            }\
          catch (continue_unwind)\
            {\
              if (loop_propagates) throw; else continue;\
            }\
          catch (...)\
            {\
              throw;\
            }\
        }\
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
  [nargin](coder_value_list& output, const octave_value_list& arg, int nout)->void\
    {\
      return call_narginchk (output, nargin, arg);\
    }));\
})()

#define NARGOUTCHK Symbol(fcn2ov(\
  [nargout](coder_value_list& output, const octave_value_list& arg, int nout)->void \
  {\
    return call_nargoutchk (output, nargout, arg, nout);\
  }))

#define NARGIN (\
  [&args]()\
  {\
    int nargin = ovl_length(args);\
    return Symbol (fcn2ov ( \
      [nargin](coder_value_list& output, const octave_value_list& arg, int nout)->void \
      {\
        return call_nargin (output, nargin, arg, nout);\
      }));\
  })()

#define NARGOUT Symbol(fcn2ov(\
  [nargout](coder_value_list& output, const octave_value_list& arg, int nout)->void \
  {\
    return call_nargout (output, nargout, arg, nout);\
  }))

#define ISARGOUT Symbol(fcn2ov(\
  [nargout](coder_value_list& output, const octave_value_list& arg, int nout)->void\
  {\
    return call_isargout (output, nargout, arg, nout);\
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

#include "error.h"
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
#include "quit.h"

#if defined (CODER_BUILDMODE_NOT_SINGLE)
#include "coder.h"
#endif

#if OCTAVE_MAJOR_VERSION >= 7
  #define OCTAVE_DEPR_NS octave::
  #define OCTAVE_RANGE octave::range<double>
#else
  #define OCTAVE_DEPR_NS ::
  #define OCTAVE_RANGE ::Range
#endif


namespace coder
{
  // don't do the trick!
  // https://stackoverflow.com/a/3173080/6579744

  template<typename Tag, typename Tag::type M>
  struct Rob {
    friend typename Tag::type get(Tag) {
      return M;
    }
  };

  #define GETMEMBER(alias, classtype, membertype, membername)\
  struct alias { \
    typedef membertype classtype::*type;\
    friend type get(alias);\
  };\
  template struct Rob<alias, &classtype::membername>;

  GETMEMBER(octave_base_value_count, octave_base_value, octave::refcount<octave_idx_type>, count)

#if OCTAVE_MAJOR_VERSION >= 7
  static void grab (octave_base_value * val)
  {
    ++(val->*get(octave_base_value_count ()));
  }
#else
  static void grab (octave_base_value * val)
  {
    val->grab ();
  }
#endif
  coder_value::~coder_value()
  {
    if (val)
      octave_value (val, false);
  }

  coder_value::coder_value (coder_value&& v)
  :val(v.val)
  {
    v.val = nullptr;
  }

  coder_value& coder_value::operator= (coder_value&& v)
  {
    if (val)
      octave_value (val, false);

    val = v.val;

    v.val = nullptr;

    return *this;
  }

  coder_value::coder_value(const octave_value& v)
  {
    val = v.internal_rep ();

    grab (val);
  }

  class value_list_pool
  {
  public:
    using List = std::list<octave_value_list>;
    using iterator = List::iterator;

    value_list_pool ()
    : cache(capacity)
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

          if (sz < (octave_idx_type)capacity && cache[sz].size () < capacity )
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

    size_t max_capacity ()
    {
      return capacity;
    }

  private:

    const size_t capacity = 10;

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

  class coder_value_list
  {
   public:
    coder_value_list (bool multiassign = false):
    m_list (),
    used_in_multiassign (multiassign)
    {
    }

    coder_value_list (octave_idx_type n, bool multiassign = false):
    used_in_multiassign (multiassign)
    {
      m_list = Pool::alloc (n);
    }

    ~coder_value_list ()
    {
      Pool::free (m_list);
    }

    octave_value_list& back ()
    {
      return m_list.back ();
    }

    octave_value_list & front ()
    {
      return m_list.front ();
    }

    void clear ()
    {
      Pool::free (m_list);
    }

    octave_value& operator () (octave_idx_type i)
    {
      return back () (i);
    }

    void append (const octave_value& val)
    {
      auto new_list = Pool::alloc (1);
      new_list.back ()(0) = val;
      m_list.splice (m_list.end () , new_list);
    }

    void append (coder_value_list&& other)
    {
      m_list.splice (m_list.end (), other.m_list);
    }

    void append (const octave_value_list& val)
    {
      auto new_list = Pool::alloc (0);
      new_list.back () = val;
      m_list.splice (m_list.end (), new_list);
    }

    void append_first (coder_value_list& other)
    {
      m_list.splice (m_list.end (), other.m_list, other.m_list.begin ());
    }

    bool empty ()const
    {
      return m_list.empty ();
    }

    size_t size ()const
    {
      return m_list.size ();
    }

    const std::list<octave_value_list>& list () const
    {
      return m_list;
    }

    operator std::list<octave_value_list>& ()
    {
      return m_list;
    }

    operator const std::list<octave_value_list>& () const
    {
      return m_list;
    }

    bool is_multi_assigned () const
    {
      return used_in_multiassign;
    }

  private:
    std::list<octave_value_list> m_list;
    bool used_in_multiassign;
  };

  class coder_function_base
  {
  public:
      virtual void call (coder_value_list& , int, const octave_value_list&) = 0;
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

#if OCTAVE_MAJOR_VERSION >= 6
    octave_value_list
    execute(octave::tree_evaluator& tw, int nargout = 0,
      const octave_value_list& args = octave_value_list ())
    {
      return call (tw, nargout, args);
    }
#endif

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

#if OCTAVE_MAJOR_VERSION >= 6
    octave_value_list
    execute(octave::tree_evaluator& tw, int nargout = 0,
      const octave_value_list& args = octave_value_list ())
    {
      return call (tw, nargout, args);
    }
#endif

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

    typedef void (*fcn) (coder_value_list&, const octave_value_list&, int);

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
    call (coder_value_list& output, int nargout = 0,
      const octave_value_list& args = octave_value_list ())
    {
      f(output, args, nargout);
    }

    octave_value_list
    call(octave::tree_evaluator& tw, int nargout = 0,
      const octave_value_list& args = octave_value_list ())
    {
      coder_value_list result;

      f(result, args, nargout);

      octave_value_list retval = result.back ();

      retval.make_storable_values ();

      if (retval.length () == 1 && retval.xelem (0).is_undefined ())
        retval.clear ();

      return retval;
    }

#if OCTAVE_MAJOR_VERSION >= 6
    octave_value_list
    execute(octave::tree_evaluator& tw, int nargout = 0,
      const octave_value_list& args = octave_value_list ())
    {
      return call (tw, nargout, args);
    }
#endif

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
    call (coder_value_list& output, int nargout = 0,
      const octave_value_list& args = octave_value_list ())
    {
      s(output, args, nargout);
    }

    octave_value_list
    call(octave::tree_evaluator& tw, int nargout = 0,
      const octave_value_list& args = octave_value_list ())
    {
      coder_value_list result;

      s(result, args, nargout);

      octave_value_list retval = result.back ();

      retval.make_storable_values ();

      if (retval.length () == 1 && retval.xelem (0).is_undefined ())
        retval.clear ();

      return retval;
    }

#if OCTAVE_MAJOR_VERSION >= 6
    octave_value_list
    execute(octave::tree_evaluator& tw, int nargout = 0,
      const octave_value_list& args = octave_value_list ())
    {
      return call (tw, nargout, args);
    }
#endif

  private:

    std::function<void(coder_value_list&, const octave_value_list&, int)> s;
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

  octave_base_value* fcn2ov(stateless_function f)
  {
    return new coder_stateless_function(f);
  }

  octave_base_value* stdfcntoov (const std::function<void(coder_value_list&, const octave_value_list&,int)>& fcn)
  {
    return new coder_stateful_function(fcn);
  }

  octave_base_value* stdfcntoov (std::function<void(coder_value_list&, const octave_value_list&,int)>&& fcn)
  {
    return new coder_stateful_function(std::move (fcn));
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

    grab (*m_sym);
  }

  void
  coder_lvalue::assign (int op,
                            const octave_value& rhs, coder_value_list& idx)
  {
    coder_value_list& m_idx = idx;

    if (! is_black_hole ())
      {
        octave_value tmp(*m_sym);

        if (tmp.is_function ())
          tmp = octave_value ();

        if (m_idx.empty ())
          tmp.assign ((octave_value::assign_op)op, rhs);
        else
          tmp.assign ((octave_value::assign_op)op, m_type, m_idx.list (), rhs);

        *m_sym = tmp.internal_rep ();

        grab (*m_sym);
      }
  }

  void
  coder_lvalue::set_index (const char *t,
                                 coder_value_list& i, coder_value_list& idx)
  {
    coder_value_list& m_i = i;

    coder_value_list& m_idx = idx;

    if (! m_idx.empty ())
      {
        error ("invalid index expression in assignment");
      }

    m_type = t;

    m_idx.clear ();

    m_idx.append (std::move (m_i));
  }

  void coder_lvalue::clear_index (coder_value_list& idx)
  {
    coder_value_list& m_idx = idx;

    m_type = "";

    m_idx.clear ();
  }

  bool
  coder_lvalue::index_is_empty (coder_value_list& idx) const
  {
    coder_value_list& m_idx = idx;

    bool retval = false;

    if (m_idx.size () == 1)
      {
        const octave_value_list& tmp = m_idx.front ();

        retval = (tmp.length () == 1 && tmp(0).isempty ());
      }

    return retval;
  }

  void
  coder_lvalue::do_unary_op (int op, coder_value_list& idx)
  {
    coder_value_list& m_idx = idx;

    if (! is_black_hole ())
      {
        octave_value tmp(*m_sym);
#if OCTAVE_MAJOR_VERSION >= 7
        if (m_idx.empty ())
          tmp.non_const_unary_op ((octave_value::unary_op)op);
        else
          tmp.non_const_unary_op ((octave_value::unary_op)op, m_type, m_idx);
#else
        if (m_idx.empty ())
          tmp.do_non_const_unary_op ((octave_value::unary_op)op);
        else
          tmp.do_non_const_unary_op ((octave_value::unary_op)op, m_type, m_idx);
#endif
        *m_sym = tmp.internal_rep ();

        grab (*m_sym);
      }
  }

  coder_value
  coder_lvalue::value (coder_value_list& idx) const
  {
    coder_value_list& m_idx = idx;

    octave_value retval;

    if (! is_black_hole ())
      {
        if (m_idx.empty ())
          {
            grab (*m_sym);

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

    octave_value expr_result;

    if (! indexed_object)
      error ("invalid use of end");

    if (! indexed_object->is_defined())
      err_invalid_inquiry_subscript ();

    const coder_value_list& index_list = *idx_list;

    if (index_list.empty ())
      {
        expr_result = octave_value(indexed_object, true);
      }
    else
      {
        try
          {
            octave_value_list tmp
              = indexed_object->subsref ({idx_type, index_list.size()}, index_list, 1);

            expr_result = tmp.length () ? tmp(0) : octave_value ();

            if (expr_result.is_cs_list ())
              err_indexed_cs_list ();
          }
        catch (octave::index_exception&)
          {
            error ("error evaluating partial expression for END");
          }
      }

    if (indexed_object->isobject ())
      {
        coder_value_list args{octave_idx_type(3)};

        args(2) = num_indices;

        args(1) = index_position + 1;

        args(0) = expr_result;

        std::string class_name = indexed_object->class_name ();

        octave::symbol_table& symtab = octave::interpreter::the_interpreter () ->get_symbol_table ();

        octave_value meth = symtab.find_method ("end", class_name);

        if (meth.is_defined ())
          {
            auto result = octave::feval (meth.function_value (), args.back (), 1);

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
      retval = octave_value(dv(index_position));
    else
      retval = octave_value (1.0);

    return (retval);
  }

  coder_value
  Expression::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return octave_value();
  }

  void
  Expression::evaluate_n(coder_value_list& output, int nargout, const Endindex& endkey, bool short_circuit)
  {
    output.append (octave_value(evaluate(nargout,endkey,short_circuit),false));
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

    grab (value);

    reference = this;
  }

  Symbol::Symbol (const Symbol& other)
  {
    value = other.get_value ();

    if (value)
      grab (value);

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
          octave_value (value, false);

        value = tmp;

        if (value )
          grab(value);
      }

    reference = this;

    return *this;
  }

  Symbol& Symbol::operator=(Symbol&& other)
  {
    if (value )
      octave_value (value, false);

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
      octave_value (value, false);
  }

  Symbol::Symbol(const char *fcn_name, const char *file_name,
  const char *path, file_type type )
  : reference (this)
  {
    switch (type)
      {
      case file_type::oct:
        {
          octave::dynamic_loader& dyn_loader
            = octave::interpreter::the_interpreter ()->get_dynamic_loader ();

          using octave::sys::file_ops::concat;

          std::string fullname = concat (path, std::string(file_name) + ".oct");

          octave_function *tmpfcn
            = dyn_loader.load_oct (fcn_name, fullname, false);

          if (tmpfcn)
            {
              value = tmpfcn;
            }
          else
            {
              error("cannot find %s.oct>%s in %s ", file_name, fcn_name, path);
            }

          break;
        }

      case file_type::mex:
        {
          octave::dynamic_loader& dyn_loader
            = octave::interpreter::the_interpreter ()->get_dynamic_loader ();

          using octave::sys::file_ops::concat;

          std::string fullname = concat (path, std::string (file_name) + ".mex");

          octave_function *tmpfcn
            = dyn_loader.load_mex (fcn_name, fullname, false);

          if (tmpfcn)
            {
              value = tmpfcn;
            }
          else
            {
              error("cannot find %s.mex>%s in %s ", file_name, fcn_name, path);
            }

          break;
        }
      case file_type::classdef:
        {
          auto& cdm = octave::interpreter::the_interpreter ()->get_cdef_manager ();
#if OCTAVE_MAJOR_VERSION >= 6
          octave_value klass_meth;
#else
          octave_function* klass_meth = nullptr;
#endif
          auto klass = cdm.find_class (fcn_name, false, true);

          if(klass.ok ())
            klass_meth =  klass.get_constructor_function();
          else
            error("cannot find class %s in path %s", fcn_name, path);
#if OCTAVE_MAJOR_VERSION >= 6
          if (klass_meth.is_defined ())
            {
              value = klass_meth.internal_rep ();

              grab (value);
            }
#else
          if (klass_meth)
            value = klass_meth;
#endif
          else
            error("cannot find class %s in path %s", fcn_name, path);

          break;
        }
      case file_type::package:
        {
          auto& cdm = octave::interpreter::the_interpreter ()->get_cdef_manager ();

          auto pack = cdm.find_package (fcn_name, false, true);

#if OCTAVE_MAJOR_VERSION >= 6
          octave_value pack_sym = cdm.find_package_symbol (fcn_name);
#else
          octave_function* pack_sym = cdm.find_package_symbol (fcn_name);
#endif

#if OCTAVE_MAJOR_VERSION >= 6
          if (pack_sym.is_defined ())
            {
              value = pack_sym.internal_rep ();

              grab (value);
            }
#else
          if (pack_sym)
            value = pack_sym;
#endif
          else
            error("cannot find package %s in path %s", fcn_name, path);

          break;
        }
      }
  }

  coder_value
  Symbol::evaluate(int nargout, const Endindex& endkey, bool short_circuit)
  {
    octave_base_value* value = get_value();

    if (value && value->is_defined ())
      {
        octave_function *fcn = nullptr;

        if (value->is_function())
          {
            fcn = value->function_value (true);

            coder_function_base* generated_fcn = dynamic_cast<coder_function_base *> (fcn);

            coder_value retval;

            if (generated_fcn)
              {
                coder_value_list result;

                coder_value_list first_args {octave_idx_type(0)};

                generated_fcn->call (result, nargout, first_args.back () );

                octave_value_list& retlist = result.back ();

                if(retlist.empty ())
                  {
                    retval = coder_value (octave_value ());
                  }
                else
                  retval = coder_value (retlist(0));

                return retval;
              }
            else if (! value->is_package () )
              {
                octave::tree_evaluator& ev = octave::interpreter::the_interpreter () -> get_evaluator ();

                coder_value_list first_args {octave_idx_type(0)};

                octave_value_list retlist =  fcn->call (ev, nargout, first_args.back ());

                if(retlist.empty ())
                  {
                    retval = coder_value (octave_value ());
                  }
                else
                  retval = coder_value (retlist(0));

                return retval;
              }
          }

        grab (value);

        return coder_value(value);
      }
    else
      {
        error("evaluating undefined variable");
      }
  }

  void
  Symbol::evaluate_n (coder_value_list& output, int nargout, const Endindex& endkey, bool short_circuit)
  {
    octave_base_value* value = get_value();

    if (value && value->is_defined())
      {
        octave_function *fcn = nullptr;

        if (value->is_function())
          {
            fcn = value->function_value (true);

            coder_function_base* generated_fcn = dynamic_cast<coder_function_base *> (fcn);

            if (generated_fcn)
              {
                coder_value_list first_args {octave_idx_type(0)};

                generated_fcn->call (output, nargout, first_args.back ());
              }
            else
              {
                octave::tree_evaluator& ev = octave::interpreter::the_interpreter () -> get_evaluator ();

                coder_value_list args {octave_idx_type(0)};

                output.append (fcn->call (ev, nargout, args.back ()));
              }
          }
        else
          {
            output.append (octave_value(value, true));
          }
      }
    else
      {
        error("multi assignment from undefined variable");
      }
  }

  coder_lvalue
  Symbol::lvalue(coder_value_list& idx)
  {
    return coder_lvalue(get_value());
  }

  void
  Symbol::call (coder_value_list& output, int nargout, const octave_value_list& args)
  {
    coder_function_base* fcn = dynamic_cast<coder_function_base *> (value);

    if (fcn)
      fcn->call (output, nargout, args);
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

  coder_value_list
  convert_to_const_vector (const Ptr_list& arg_list, Endindex endkey)
  {
    const int len = arg_list.size ();

    coder_value_list result_list {octave_idx_type(len)};

    octave_value_list& args  = result_list.back ();

    auto p = arg_list.begin ();

    int j = 0;

    octave_idx_type nel = 0;

    bool contains_cs_list = false;

    endkey.num_indices = len;

    for (int k = 0; k < len; k++)
      {
        endkey.index_position = k;

        auto& elt = *p++;

        octave_value tmp (elt->evaluate(1, endkey, false), false);

        if (tmp.is_defined ())
          {
            if (tmp.is_cs_list ())
              {
                const octave_idx_type n = tmp.numel();

                if (n == 0)
                  continue;
                else if (n  == 1)
                  {
                    result_list(j++) = tmp.list_value ()(0);
                  }
                else
                  {
                    contains_cs_list = true;

                    result_list(j++) = tmp;
                  }

                nel+=n;
              }
            else
              {
                result_list(j++) = tmp;

                nel++;
              }
          }
      }

    if (nel != len || contains_cs_list)
      {
        coder_value_list new_result {octave_idx_type(nel)};

        octave_idx_type s = 0;

        for (octave_idx_type i = 0; i < j; i++)
          {
            octave_value& val = args.xelem(i);

            if (val.is_cs_list ())
              {
                octave_value_list ovl = val.list_value ();

                const octave_idx_type ovlen = ovl.length ();

                for (octave_idx_type m = 0; m < ovlen; m++)
                  new_result (s++) = ovl.xelem (m);
              }
            else
              {
                new_result (s++) = val;
              }
          }

        assert (s == nel);

        return new_result;
      }

    return result_list;
  }

  coder_value_list
  make_value_list (const Ptr_list& m_args,
                   const Endindex& endkey=Endindex())
  {
    if (m_args.size() > 0)
      {
        return convert_to_const_vector (m_args ,endkey);
      }

    return coder_value_list {octave_idx_type(0)};
  }

  octave_value
  get_struct_index (const Ptr_list& arg, const Endindex& endkey)
  {
    coder_value val ( (*arg.begin())->evaluate (1, endkey));

    if (! val.val->is_string ())
      error ("dynamic structure field names must be strings");

    return octave_value (val, false);
  }

  coder_value
  Index::evaluate ( int nargout, const Endindex& endkey, bool short_circuit)
  {
    coder_value_list result;

    evaluate_n (result, nargout, endkey, short_circuit);

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

    return retval;
  }

  void
  Index::evaluate_n(coder_value_list& output, int nargout, const Endindex& endkey, bool short_circuit)
  {
    coder_value_list& retval = output;

    const char *type = idx_type;

    auto& args = arg_list;

    assert ( args.size () > 0);

    auto p_args = args.begin ();

    const int n = args.size ();

    int beg = 0;

    octave_value base_expr_val;

    auto& expr = base;

    coder_value_list idx ;

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
                coder_value_list first_args;

                coder_function_base* generated_fcn = dynamic_cast<coder_function_base *> (fcn);

                if (generated_fcn)
                  {
                    if (p_args->size() > 0)
                      {
                        const Endindex &endindex = val->is_function_handle ()
                          ? Endindex{val, type, &idx, 0, n}
                          : endkey;

                        first_args.append (convert_to_const_vector( *p_args, endindex));
                      }
                    else
                      {
                        first_args.append (coder_value_list{octave_idx_type(0)});
                      }

                    generated_fcn->call (retval, nargout, first_args.back () );
                  }
                else if (! val->is_function_handle ())
                  {
                    if (p_args->size() > 0)
                      {
                          first_args.append (convert_to_const_vector( *p_args, endkey));
                      }
                    else
                      {
                        first_args.append (coder_value_list {octave_idx_type(0)});
                      }

                    try
                      {
                        octave::tree_evaluator& ev = octave::interpreter::the_interpreter () -> get_evaluator ();

                        retval.append (fcn->call (ev, nargout, first_args.back ()));
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
                      base_expr_val = retval(0);
                  }
                else
                  {
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

    octave_value partial_expr_val = base_expr_val;

    for (int i = beg; i < n; i++)
      {
        if (i > beg)
          {
            if (! indexing_object)
              {
                try
                  {
                    octave_value_list tmp_list
                      = base_expr_val.subsref ({type+beg, size_t(i-beg)},
                                   idx, nargout);

                    partial_expr_val
                      = tmp_list.length () ? tmp_list(0) : octave_value ();

                    base_expr_val = partial_expr_val;

                    if (partial_expr_val.is_cs_list ())
                      err_indexed_cs_list ();

                    retval.clear ();

                    retval.append (partial_expr_val);

                    beg = i;

                    idx.clear ();

                    if (partial_expr_val.isobject ()
                      || partial_expr_val.isjava ()
                      || (partial_expr_val.is_classdef_meta ()
                        && ! partial_expr_val.is_package ()))
                      {
                        indexing_object = true;
                      }
                  }
                catch (octave::index_exception& e)
                  {
                    error ("indexing error");
                  }
              }
          }

        Endindex endindex {partial_expr_val.internal_rep(), type, &idx, i, n};

        switch (type[i])
          {
          case '(':
          case '{':
            {
              coder_value_list result = make_value_list (*p_args, endindex);

              idx.append (std::move(result));
            }
            break;

          case '.':
            {
              idx.append (get_struct_index (*p_args, endindex));
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
                retval.clear ();

                retval.append (base_expr_val.subsref ({type+beg, size_t(n-beg)},
                        idx, nargout));

                beg = n;

                idx.clear ();
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
                    retval.clear ();

                    if (idx.size () != 1)
                      error ("unexpected extra index at end of expression");

                    if (type[beg] != '(')
                      error ("invalid index type '%c' for function call",
                             type[beg]);

                    octave::tree_evaluator& ev = octave::interpreter::the_interpreter () -> get_evaluator ();

                    retval.append (fcn->call (ev, nargout, idx.front ()));
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
            coder_value_list final_args {octave_idx_type(0)};

            if (! idx.empty ())
              {
                if (n - beg != 1)
                  error ("unexpected extra index at end of Expression");

                if (type[beg] != '(')
                  error ("invalid index type '%c' for function call",
                         type[beg]);

                final_args.clear ();

                final_args.append_first(idx);
              }

            octave::tree_evaluator& ev = octave::interpreter::the_interpreter () -> get_evaluator ();

            retval.clear ();

            retval.append (fcn->call (ev, nargout, final_args.back ()));
          }
      }
  }

  coder_lvalue
  Index::lvalue (coder_value_list& in_idx)
  {
    Pool::bitidx ().clear ();

    coder_value_list& arg = in_idx;

    coder_lvalue retval;

    coder_value_list idx;

    auto& m_expr = base;

    auto& m_type = idx_type;

    auto& m_args = arg_list;

    const int n = m_args.size ();

    auto p_args = m_args.begin ();

    assert(m_expr->is_Symbol());

    coder_value_list arg1;

    retval = m_expr->lvalue (arg1);

    octave_value base_obj = octave_value(retval.value (arg1), false);

    octave_value tmp = base_obj;

    octave_idx_type tmpi = 0;

    coder_value_list tmpidx;

    coder_value_list resultidx;

    for (int i = 0; i < n; i++)
      {
        if (retval.numel () != 1)
          err_indexed_cs_list ();

        if (tmpi < i)
          {
            try
              {
                tmp = tmp.subsref ({m_type+tmpi, size_t(i-tmpi)}, tmpidx, true);
              }
            catch (octave::index_exception& e)
              {
                error("problems with range, invalid type etc.");
              }

            for (auto b : Pool::bitidx ())
              if (b)
                resultidx.append_first (idx);
              else
                resultidx.append_first (tmpidx);

            Pool::bitidx ().clear ();
          }

        Endindex endindex {base_obj.internal_rep(), m_type, &resultidx, i, n};

        switch (m_type[i])
          {
          case '(':
            {
              auto tidx = make_value_list (*p_args, endindex);

              if (i < n - 1)
                {
                  if (m_type[i+1] != '.')
                    error ("() must be followed by . or close the index chain");

                  tmpidx.append (std::move (tidx));

                  tmpi = i+1;

                  Pool::bitidx ().push_back (0);
                }
              else
                {
                  idx.append (std::move (tidx));

                  Pool::bitidx ().push_back (1);
                }
            }
            break;

          case '{':
            {
              auto tidx = make_value_list (*p_args, endindex);

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
#if OCTAVE_MAJOR_VERSION >= 6
              retval.numel (tmp.xnumel (tidx.back ()));
#else
              retval.numel (tmp.numel (tidx.back ()));
#endif
              tmpidx.append (std::move (tidx));

              tmpi = i;

              Pool::bitidx ().push_back (0);
            }
            break;

          case '.':
            {
              auto tidxlist = get_struct_index (*p_args, endindex);

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
#if OCTAVE_MAJOR_VERSION >= 6
                  retval.numel (tmp.xnumel (pidx));
#else
                  retval.numel (tmp.numel (pidx));
#endif
                  tmpi = i-1;

                  tmpidx.append (tidxlist);

                  Pool::bitidx ().push_back (0);
                }
              else
                {
                  if (tmp.is_undefined () || autoconv)
                    {
                      tmpi = i+1;

                      tmp = octave_value ();

                      idx.append (tidxlist);

                      Pool::bitidx ().push_back (1);
                    }
                  else
                    {
#if OCTAVE_MAJOR_VERSION >= 6
                      retval.numel (tmp.xnumel (octave_value_list ()));
#else
                      retval.numel (tmp.numel (octave_value_list ()));
#endif
                      tmpi = i;

                      tmpidx.append (tidxlist);

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
        resultidx.append_first (idx);
      else
        resultidx.append_first (tmpidx);

    Pool::bitidx ().clear ();

    retval.set_index (m_type, resultidx, arg);

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

  coder_value int8_literal::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return (octave_value(octave_int8(val)));
  }

  coder_value int16_literal::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return (octave_value(octave_int16(val)));
  }

  coder_value int32_literal::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return (octave_value(octave_int32(val)));
  }

  coder_value int64_literal::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return (octave_value(octave_int64(val)));
  }

  coder_value uint8_literal::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return (octave_value(octave_uint8(val)));
  }

  coder_value uint16_literal::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return (octave_value(octave_uint16(val)));
  }

  coder_value uint32_literal::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return (octave_value(octave_uint32(val)));
  }

  coder_value uint64_literal::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    return (octave_value(octave_uint64(val)));
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
#if OCTAVE_MAJOR_VERSION >= 7
    return (octave::binary_op (ti, op, left, right));
#else
    return (::do_binary_op (ti, op, left, right));
#endif
  }

  coder_value
  unary_expr(Ptr a, int nargout, const Endindex& endkey, bool short_circuit, octave_value::unary_op op)
  {
#if OCTAVE_MAJOR_VERSION >= 7
    octave_value val ( a->evaluate(nargout,endkey,short_circuit), false);

    if (val.get_count () == 1)
      return val.non_const_unary_op ( op);

    octave::type_info& ti = octave::interpreter::the_interpreter () ->get_type_info ();

    return (octave::unary_op (ti, op, val));
#else
    octave_value val ( a->evaluate(nargout,endkey,short_circuit), false);

    if (val.get_count () == 1)
      return val.do_non_const_unary_op ( op);

    octave::type_info& ti = octave::interpreter::the_interpreter () ->get_type_info ();

    return (::do_unary_op (ti, op, val));
#endif
  }

  coder_value
  pre_expr(Ptr a, octave_value::unary_op op)
  {
    coder_value_list lst;

    coder_lvalue op_ref = a->lvalue (lst);

    op_ref.do_unary_op (op, lst);

    coder_value retval = op_ref.value (lst);

    return retval;
  }

  coder_value
  post_expr(Ptr a, octave_value::unary_op op)
  {
    coder_value_list lst;

    coder_lvalue op_ref = a->lvalue (lst);

    coder_value val = op_ref.value (lst);

    op_ref.do_unary_op (op, lst);

    return val;
  }

  coder_value
  trans_expr(Ptr a, int nargout, const Endindex& endkey, bool short_circuit, octave_value::unary_op op)
  {
    octave_value left ( a->evaluate(nargout,endkey,short_circuit), false);

    octave::type_info& ti = octave::interpreter::the_interpreter () ->get_type_info ();
#if OCTAVE_MAJOR_VERSION >= 7
    return (octave::unary_op (ti, op, left));
#else
    return (::do_unary_op (ti, op, left));
#endif
  }

  coder_value
  assign_expr(Ptr lhs, Ptr rhs, const Endindex& endkey, octave_value::assign_op op)
  {
    coder_value val;

    try
      {
        coder_value_list arg;

        coder_lvalue ult = lhs->lvalue (arg);

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

        ult.assign (op, rhs_val, arg);

        val = ult.value (arg);
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
#if OCTAVE_MAJOR_VERSION >= 7
            val = octave::binary_op (ti, op, a, b);
#else
            val = ::do_binary_op (ti, op, a, b);
#endif
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
#if OCTAVE_MAJOR_VERSION >= 7
    return (octave::colon_op (ov_base, ov_increment, ov_limit, true));
#else
    return (::do_colon_op (ov_base, ov_increment, ov_limit, true));
#endif
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
#if OCTAVE_MAJOR_VERSION >= 7
    return (octave::binary_op (ti, octave_value::op_el_and, left, right));
#else
    return (::do_binary_op (ti, octave_value::op_el_and, left, right));
#endif
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
#if OCTAVE_MAJOR_VERSION >= 7
    return (octave::binary_op (ti, octave_value::op_el_or, left, right));
#else
    return (::do_binary_op (ti, octave_value::op_el_or, left, right));
#endif
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
        coder_value_list arg;

        coder_lvalue ult = lhs->lvalue (arg);

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

        ult.assign (octave_value::op_asn_eq, rhs_val, arg);

        val = rhs_val;
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
  MultiAssign::evaluate_n(coder_value_list& output, int nargout, const Endindex& endkey, bool short_circuit)
  {
    const int max_lhs_sz = 3;

    const int l_sz = lhs.size();

    const bool small_lhs = l_sz <= max_lhs_sz;

    std::vector<coder_lvalue > lvalue_list_vec;

    std::vector<coder_value_list> lvalue_arg_vec ;

    coder_lvalue lvalue_list_arr[max_lhs_sz] ={};

    coder_value_list  lvalue_arg_arr [max_lhs_sz] = {};

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

            lvalue_list_vec.emplace_back (out->lvalue (lvalue_arg_vec.back ()));
          }
        else
          {
            lvalue_list_arr[c] = out->lvalue (lvalue_arg_arr[c]);
            c++;
          }
      }

    int n_out = 0;

    int n_blackhole = 0;

    bool has_cs_out = false;

    coder_lvalue* lvalue_list = small_lhs ? lvalue_list_arr : lvalue_list_vec.data ();

    coder_value_list* lvalue_arg = small_lhs ? lvalue_arg_arr : lvalue_arg_vec.data ();

    for (int i = 0 ; i < l_sz; i++)
      {
        octave_idx_type nel = lvalue_list[i].numel ();

        has_cs_out = has_cs_out || (nel != 1);

        n_out += nel;

        n_blackhole += lvalue_list[i].is_black_hole();
      }

    coder_value_list rhs_val1 (true);

    rhs->evaluate_n (rhs_val1, n_out , endkey);

    bool rhs_cs_list = rhs_val1.back().length () == 1
                    && rhs_val1.back()(0).is_cs_list ();

    const octave_value_list& rhs_val = (rhs_cs_list
                                       ? rhs_val1.back()(0).list_value ()
                                       : rhs_val1.back());
    octave_idx_type k = 0;

    octave_idx_type j = 0;

    const octave_idx_type n = rhs_val.length ();

    size_t ai = 0;

    coder_value_list& retval = output;

    if (l_sz  == 1 && lvalue_list[0].numel () == 0 && n > 0)
      {
        coder_lvalue& ult = lvalue_list[0];

        coder_value_list& arg = lvalue_arg[0];

        if (! strcmp (ult.index_type () , "{") && ult.index_is_empty (arg)
            && ult.is_undefined ())
          {
            ult.define (Cell (1, 1));

            ult.clear_index (arg);

            coder_value_list idx ;

            idx.append (octave_value (1));

            ult.set_index ("{", idx, arg);

            ult.assign (octave_value::op_asn_eq, octave_value (rhs_val), arg);

            if (n == 1 )
              {
                if (nargout > 0)
                  {
                    if (! rhs_cs_list)
                      {
                        retval.append (std::move (rhs_val1));
                      }
                    else
                      {
                        retval.append (rhs_val(0));
                      }
                  }
                else
                  {
                    retval.append (coder_value_list {octave_idx_type(0)});
                  }
              }

            return;
          }
      }

    octave_value valarg;

    if (has_cs_out)
      valarg = octave_value (octave_value_list ());

    const int nargout_retval = std::min (n_out-n_blackhole, nargout);

    if (nargout_retval < n)
      {
        retval.append (coder_value_list{octave_idx_type(nargout_retval)});
      }

    for (int ii = 0 ; ii < l_sz; ii++)
      {
        coder_lvalue& ult = lvalue_list[ii];

        octave_idx_type nel = ult.numel ();

        coder_value_list& arg = lvalue_arg[ai++];

        if (nel != 1)
          {
            if (k + nel > n)
              error ("some elements undefined in return list");

            octave_value_list ovl = rhs_val.slice (k, nel);

            ult.assign (octave_value::op_asn_eq, octave_value (ovl), arg);

            if (nargout_retval < n)
              for (octave_idx_type i = k; i < nel && j < nargout_retval; i++)
                retval(j++) = rhs_val(i);

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
                      error ("element number %s undefined in return list",
                             std::to_string (k+1).c_str ());

                    ult.assign (octave_value::op_asn_eq, tmp, arg);

                    if (nargout_retval < n && j < nargout_retval)
                      retval (j++) = tmp;

                    k++;
                  }
              }
            else
              {
                if (! ult.is_black_hole ())
                  error ("element number %s undefined in return list", std::to_string (k+1).c_str ());

                k++;
                continue;
              }
          }
      }

    if (nargout_retval >= n)
      {
        retval.clear ();

        retval.append (std::move (rhs_val1));
      }
  }

  coder_value
  MultiAssign::evaluate(int nargout, const Endindex& endkey, bool short_circuit)
  {
    coder_value_list result;

    evaluate_n(result, nargout, endkey, short_circuit);

    octave_value_list& vlist = result.back ();

    coder_value retval;

    if(vlist.empty ())
      retval = coder_value (octave_value ());
    else
      retval = coder_value (vlist (0));

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

  octave_value
  eval_dot_separated_names (Index& index_expr)
  {
    auto& args = index_expr.arg_list;

    auto& expr = index_expr.base;

    Endindex endindex;

    coder_value_list idx;

    octave_value base_expr_val = octave_value(expr->evaluate ( 1, endindex), false);

    for (auto& p_args: args)
      {
        auto struct_idx = get_struct_index (p_args, endindex);

        idx.append (struct_idx);
      }

    octave_value_list tmp_list
      = base_expr_val.subsref (index_expr.idx_type, idx, 1);

    return tmp_list.length () ? tmp_list(0) : octave_value ();
  }

  static void err_invalid_fcn_handle (const std::string& name)
  {
    error ("invalid function handle, unable to find function for @%s",
           name.c_str ());
  }

  coder_value
  Handle::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    if (fmaker)
      {
#if OCTAVE_MAJOR_VERSION >= 6
        return coder_value(new octave_fcn_handle (octave_value(fmaker ().get_value(), true)));
#else
        return coder_value(new octave_fcn_handle (octave_value(fmaker ().get_value(), true), name));
#endif
      }

    auto rhs = op_rhs.get ();

    if (rhs.is_Symbol ())
      {
#if OCTAVE_MAJOR_VERSION >= 6
        return coder_value(new octave_fcn_handle (octave_value(static_cast<Symbol&>(op_rhs.get()).get_value(), true)));
#else
        return coder_value(new octave_fcn_handle (octave_value(static_cast<Symbol&>(op_rhs.get()).get_value(), true), name));
#endif
      }
#if OCTAVE_MAJOR_VERSION >= 6
    Index& index_expr = static_cast<Index&> (op_rhs.get ());

    std::string type = ".";

    auto& args = index_expr.arg_list;

    auto& expr = index_expr.base;

    Endindex endindex;

    bool have_object = false;

    octave_value partial_expr_val = octave_value(expr->evaluate (1, endindex), false);

    if (partial_expr_val.is_classdef_object ())
      {
        if (args.size () != 1)
          err_invalid_fcn_handle (name);

        have_object = true;
      }

    size_t i = 0;

    for (auto& p_args: args)
      {
        if (partial_expr_val.is_package ())
          {
            if (have_object)
              err_invalid_fcn_handle (name);

            coder_value_list arg_list;

            arg_list.append (get_struct_index (p_args, endindex));

            try
              {
                octave_value_list tmp_list
                  = partial_expr_val.subsref (type, arg_list, 0);

                partial_expr_val
                  = tmp_list.length () ? tmp_list(0) : octave_value ();

                if (partial_expr_val.is_cs_list ())
                  err_invalid_fcn_handle (name);
              }
            catch (octave::index_exception&)
              {
                err_invalid_fcn_handle (name);
              }
          }
        else if (have_object || partial_expr_val.is_classdef_meta ())
          {
            if (++i != args.size () )
              err_invalid_fcn_handle (name);

            octave_value arg = get_struct_index (*std::prev (args.end ()), endindex);

            return coder_value (new octave_fcn_handle (octave_value (fcn2ov (
              [=](coder_value_list& output, const octave_value_list& args, int nargout)->void
              {
                coder_value_list arg_list;

                arg_list.append (arg);

                auto n = args.length ();

                coder_value_list second_arg {octave_idx_type(n)};

                for (octave_idx_type i = 0; i < n; i++)
                  second_arg(i) = args (i);

                arg_list.append (std::move (second_arg));

                octave_value base_expr = partial_expr_val;

                output.append (base_expr.subsref (".(", arg_list, nargout));
              }))));
          }
        else
          err_invalid_fcn_handle (name);
      }

    err_invalid_fcn_handle (name);
#endif
    return coder_value {octave_value ()};
  }

#if OCTAVE_MAJOR_VERSION >= 6
  Anonymous::Anonymous(octave_base_value* arg) : value(new octave_fcn_handle (octave_value(arg, false))){}
#else
  Anonymous::Anonymous(octave_base_value* arg) : value(new octave_fcn_handle (octave_value(arg, false), "<coderanonymous>")){}
#endif

  coder_value
  Anonymous::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    grab(value.val);
    return  coder_value(value.val);
  }

#if OCTAVE_MAJOR_VERSION >= 6
  NestedHandle::NestedHandle(Ptr arg, const char* name) : value(new octave_fcn_handle (octave_value(static_cast<Symbol&>(arg.get()).get_value(), true))){}
#else
  NestedHandle::NestedHandle(Ptr arg, const char* name) : value(new octave_fcn_handle (octave_value(static_cast<Symbol&>(arg.get()).get_value(), true), name)){}
#endif

  coder_value
  NestedHandle::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    grab(value.val);
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
                error ("undefined element in matrix list");
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

        auto hcat = OCTAVE_DEPR_NS Fhorzcat(octave_value_list(current_row),1);

        rows(i++) = hcat.empty()? octave_value(::Matrix()) : hcat(0);
      }

    octave_value_list retval = OCTAVE_DEPR_NS Fvertcat(rows,1);

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

        auto hcat = OCTAVE_DEPR_NS Fhorzcat(current_row,1);

        rows(i++) = hcat.empty()? octave_value(::Cell()) : hcat(0);
      }

    octave_value_list retval = OCTAVE_DEPR_NS Fvertcat(rows,1);

    if (retval.empty())
      return (octave_value(::Cell()));

    return  (retval(0));
  }
  coder_value
  Constant::evaluate( int nargout, const Endindex& endkey, bool short_circuit)
  {
    grab(val.val);
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
  Tilde::lvalue(coder_value_list& idx)
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

        coder_value_list arg;

        coder_lvalue ult = id->lvalue (arg);

        octave_value init_val ( expr->evaluate (1), false);

        ult.assign (octave_value::op_asn_eq, init_val, arg);

        retval = true;
      }
    else if (elt.size()==1)
      {
        coder_value_list lst;

        coder_lvalue ref = (*elt.begin())->lvalue (lst);

        ref.define (octave_value ());
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

        bool is_tilde = elt.begin ()->get ().is_Tilde ();

        if (is_tilde)
          continue;

        if (i < args.length ())
          {
            if (args(i).is_defined () && args(i).is_magic_colon ())
              {
                if (! eval_decl_elt (elt))
                  error ("no default value for argument %d", i+1);
              }
            else
              {
                coder_value_list lst;

                coder_lvalue ref = (*elt.begin())->lvalue (lst);

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
    coder_value_list lst;

    coder_lvalue ref = varargin->lvalue (lst);

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
        coder_value_list lst;

        coder_lvalue ref = varargin->lvalue (lst);

        ref.define (octave_value(Cell()));
      }
  }

  void
  make_return_list(coder_value_list& output, Ptr_list&& ret_list, int nargout)
  {
    if(ret_list.size() == 0)
      {
        output.append (coder_value_list {octave_idx_type(0)});

        return;
      }

    int n_retval = std::max(nargout,1);

    coder_value_list retval1 {octave_idx_type(nargout)};

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

    if (nargout == 1 && ! output.is_multi_assigned () && retval(0).is_undefined ())
      error_with_id ("Octave:undefined-function", "%s", "undefined value returned from function");

    output.append (std::move (retval1));
  }

  void
  make_return_list(coder_value_list& output, Ptr_list&& ret_list, int nargout, Ptr varout)
  {
    int len = ret_list.size();

    make_return_list(output, std::move(ret_list), nargout);

    octave_value_list & retval = output.back ();

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
  }

  void
  make_return_list(coder_value_list& output, Ptr varout)
  {
    Expression& ex = varout;

    Symbol* sym = static_cast<Symbol*>(&ex);

    const octave_base_value* val = sym->get_value();

    if (! val->is_defined ())
      {
        output.append (coder_value_list {octave_idx_type(0)});

        return;
      }

    if (! val->iscell ())
      error("varargout must be a cell array object");

    output.append (val->list_value ());
  }

  void
  make_return_list(coder_value_list& output)
  {
    output.append (coder_value_list {octave_idx_type(0)});
  }

  void
  make_return_val (coder_value_list& output, Ptr expr, int nargout)
  {
    output.append (octave_value (expr->evaluate (nargout), false));
  }

  void
  make_return_val (coder_value_list& output)
  {
    output.append (coder_value_list {octave_idx_type(0)});
  }

  void SetEmpty(Symbol& id)
  {
    octave_value tmp_val{Matrix ()};

    octave_base_value * val = tmp_val.internal_rep ();

    grab (val);

    octave_base_value*& id_val = id.get_value ();

    if (id_val)
      octave_value (id_val, false);

    id_val = val;
  }

  bool isargout1 (int nargout, double k)
  {
    if (k != octave::math::round (k) || k <= 0)
      error ("isargout: K must be a positive integer");

    return (k == 1 || k <= nargout) ;
  }

  void
  call_narginchk (coder_value_list& output, int nargin, const octave_value_list& arg)
  {
    if (arg.length () != 2)
      {
#if OCTAVE_MAJOR_VERSION >= 6
       OCTAVE_DEPR_NS Ffeval (*octave::interpreter::the_interpreter (),
          ovl (octave_value ("help"), octave_value ("narginchk")), 0);
#else
       OCTAVE_DEPR_NS  Ffeval (ovl (octave_value ("help"), octave_value ("narginchk")), 0);
#endif
        output.append (coder_value_list {octave_idx_type(0)});
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

    output.append (coder_value_list {octave_idx_type(0)});
  }

  void
  call_nargoutchk (coder_value_list& output, int nargout, const octave_value_list& arg, int nout)
  {
    octave_value_list input = arg;

    octave_value msg;

    const int minargs=0, maxargs=1, nargs=2, outtype=3;

    auto val = [&](int v) {return input (v).idx_type_value ();};

    auto str = [&](int v) {return input (v).string_value ();};

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
            msg = OCTAVE_DEPR_NS Fstruct (ovl (
              octave_value ("message"),
              octave_value (message),
              octave_value ("identifier"),
              octave_value (id)
            ), 1) (0);

            if (message.empty ())
              {
                msg = OCTAVE_DEPR_NS Fresize (ovl (msg, octave_value (0.0), octave_value (1.0)), 1) (0);
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
#if OCTAVE_MAJOR_VERSION >= 6
        OCTAVE_DEPR_NS Ffeval (*octave::interpreter::the_interpreter (),
          ovl (octave_value ("help"), octave_value ("nargoutchk")), 0);
#else
        OCTAVE_DEPR_NS Ffeval (ovl (octave_value ("help"), octave_value ("nargoutchk")), 0);
#endif
      }

    output.append (msg);
  }

  void
  call_nargin (coder_value_list& output, int nargin, const octave_value_list& arg, int nout)
  {
    auto& result = output;

    if (arg.length() > 0)
      {
        result.append(OCTAVE_DEPR_NS Fnargin(*octave::interpreter::the_interpreter (), arg, nout));
      }
    else
      {
        result.append (octave_value(double(nargin)));
      }
  }

  void
  call_nargout (coder_value_list& output, int nargout, const octave_value_list& arg, int nout)
  {
    auto& result = output;

    if (arg.length() > 0)
      {
        result.append (OCTAVE_DEPR_NS Fnargout(*octave::interpreter::the_interpreter (), arg, nout));
      }
    else
      {
        result.append (octave_value(double(nargout)));
      }
  }

  //FIXME : always returns true!
  void
  call_isargout (coder_value_list& output, int nargout, const octave_value_list& arg, int nout)
  {
    auto& result = output;

    result.append (octave_value (false));

    octave_value& retval = result(0);

    if (arg.length () != 1)
      {
#if OCTAVE_MAJOR_VERSION >= 6
        OCTAVE_DEPR_NS Ffeval (*octave::interpreter::the_interpreter (),
          ovl (octave_value ("help"), octave_value ("isargout")), 0);
#else
        OCTAVE_DEPR_NS Ffeval (ovl (octave_value ("help"), octave_value ("isargout")), 0);
#endif
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
  }

  void recover_from_execution_excep ()
  {
    try
      {
        throw;
      }
    catch (const octave::execution_exception& ee)
      {
#if OCTAVE_MAJOR_VERSION >= 6
        auto& es = octave::interpreter::the_interpreter ()->get_error_system ();

        es.save_exception (ee);

        octave::interpreter::the_interpreter ()-> recover_from_exception ();
#else
        octave::interpreter::recover_from_exception ();
#endif
      }
  }

  void recover_from_execution_and_interrupt_excep ()
  {
    try
      {
        throw;
      }
    catch (const octave::execution_exception& ee)
      {
#if OCTAVE_MAJOR_VERSION >= 6
        auto& es = octave::interpreter::the_interpreter ()->get_error_system ();

        es.save_exception (ee);

        octave::interpreter::the_interpreter ()-> recover_from_exception ();
#else
        octave::interpreter::recover_from_exception ();
#endif
      }
    catch (const octave::interrupt_exception&)
      {
#if OCTAVE_MAJOR_VERSION >= 6
        octave::interpreter::the_interpreter ()-> recover_from_exception ();
#else
        octave::interpreter::recover_from_exception ();
#endif
      }
  }

  void assign_try_id (Ptr expr_id)
  {
    coder_value_list idx;
    coder_lvalue ult = expr_id->lvalue (idx);
    octave_scalar_map err;
#if OCTAVE_MAJOR_VERSION >= 6
    auto& es = octave::interpreter::the_interpreter ()->get_error_system ();
    err.assign ("message", es.last_error_message ());
    err.assign ("identifier", es.last_error_id ());
    err.assign ("stack", es.last_error_stack ());
#else
    err.assign ("message", last_error_message ());
    err.assign ("identifier", last_error_id ());
    err.assign ("stack", last_error_stack ());
#endif
    ult.assign (octave_value::op_asn_eq, err, idx);
  }

  template<typename T, typename S>
  T Dynamic_cast (S *s)
  {
    static const int id = std::remove_pointer<T>::type::static_type_id ();

    if (s->type_id () == id)
      {
        return static_cast<T> (s);
      }

    return nullptr;
  }

  class for_loop_rep
  {
  public:
    for_loop_rep (Ptr lhs, Ptr expr, bool fast_loop):
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
      ult(lhs->lvalue (ult_idx)),
      blackhole (ult.is_black_hole ()),
      m_fast_loop (fast_loop),
      m_first_loop (true)
    {
      switch (looptype)
        {
        case range_loop :
          {
            rng = val.range_value ();

            ult.assign (octave_value::op_asn_eq, Matrix (), ult_idx);

            steps = rng.numel ();

            break;
          }

        case scalar_loop :
          {
            ult.assign (octave_value::op_asn_eq, val, ult_idx);

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

                ult.assign (octave_value::op_asn_eq, Matrix (), ult_idx);
              }
            else
              {
                ult.assign (octave_value::op_asn_eq, val, ult_idx);

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
      if (blackhole)
        return;

      if (looptype == range_loop)
        {
          auto rhs = rng.elem (i);

          auto& m_sym = *ult.m_sym;

          if (m_first_loop)
            {
              m_first_loop = false;

              octave_value val {m_sym};

              val = rhs;

              m_sym = val.internal_rep ();

              grab (m_sym);

              return;
            }

          if (ult_idx.empty () && (m_fast_loop || Dynamic_cast<octave_scalar *>(m_sym)))
            {
              if (m_sym->*get(octave_base_value_count ()) == 1)
                static_cast<octave_base_scalar<double> *>(m_sym)->scalar_ref() = rhs;
              else
                {
                  octave_value val {m_sym};

                  val = rhs;

                  m_sym = val.internal_rep ();

                  grab (m_sym);
                }

              return;
            }

          ult.assign (octave_value::op_asn_eq, rhs, ult_idx);
        }
      else if (looptype == matrix_loop)
        {
          static_cast<octave_scalar*>(base_val)->scalar_ref() = i + 1;
#if OCTAVE_MAJOR_VERSION >= 7
          octave_value tmp = val.index_op (idx);
#else
          octave_value tmp = val.do_index_op (idx);
#endif
          ult.assign (octave_value::op_asn_eq, tmp, ult_idx);
        }
    }

    for_iterator begin()  { return for_iterator (this,0); }

    for_iterator end()  { return for_iterator (this, steps); }

  private:

    octave_value val;

    const LoopType looptype;

    octave_base_value* base_val;

    OCTAVE_RANGE rng;

    octave_idx_type steps;

    octave_value_list idx;

    coder_lvalue ult;

    coder_value_list ult_idx;

    const bool blackhole;

    const bool m_fast_loop;

    bool m_first_loop;
  };

  class struct_loop_rep
  {

  public:
    struct_loop_rep (Ptr v ,Ptr k, Ptr expr)
    {
      rhs = (expr)->evaluate (1);

      if (! rhs.val->isstruct ())
        error ("in statement 'for [X, Y] = VAL', VAL must be a structure");

      val_ref = (v)->lvalue (idxs[0]);

      key_ref = (k)->lvalue (idxs[1]);

      keys = rhs.val->map_keys ();

      is_scalar_struct = Dynamic_cast <octave_scalar_struct*> (rhs.val);

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

    void set_loop_val (octave_idx_type i)
    {
      const std::string &key = keys.xelem(i);

      if (is_scalar_struct)
        {
          const octave_value& val_lst = tmp_scalar_val.contents(i) ;

          val_ref.assign (octave_value::op_asn_eq, val_lst, idxs[0] );
        }
      else
        {
          const Cell& val_lst = tmp_val.contents(i);

          octave_base_matrix<Cell>* val_mat = static_cast<octave_base_matrix<Cell>*>(val.internal_rep ());

          val_mat->matrix_ref () = val_lst;

          val_ref.assign (octave_value::op_asn_eq, val, idxs[0]);
        }

      key_ref.assign (octave_value::op_asn_eq, key, idxs[1]);
    }

    struct_iterator begin()  { return struct_iterator (this,0); }

    struct_iterator end()  { return struct_iterator (this, nel); }

  private:

    string_vector keys;

    octave_map tmp_val;

    octave_scalar_map tmp_scalar_val;

    coder_value_list idxs [2] ;

    coder_lvalue val_ref;

    coder_lvalue key_ref;

    coder_value rhs;

    octave_idx_type nel;

    octave_value val;

    bool is_scalar_struct;

  };

  for_loop::for_loop (Ptr lhs, Ptr expr, bool fast_loop)
  : rep (new for_loop_rep(lhs, expr, fast_loop))
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
