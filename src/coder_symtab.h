#pragma once

#include <functional>
#include <memory>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <octave/ov.h>

namespace coder_compiler
{
  struct coder_file;
  
  using coder_file_ptr = std::shared_ptr<coder_file>;
  
  enum class symbol_type 
  {
    ordinary,
    inherited,
    persistent,
    nested_fcn,
    num_symbol_types
    
  };
  
  struct coder_symbol
  {
    coder_symbol(const std::string& name = "",
      const octave_value& fcn = {}, const coder_file_ptr& file = {})
      :name(name), fcn(fcn), file(file){}
    
    std::string name;
    
    octave_value fcn;
    
    coder_file_ptr file;
  };
  
  using coder_symbol_ptr = std::shared_ptr<coder_symbol>;
  
  struct symscope;
  
  using symscope_ptr = std::shared_ptr<symscope>;
  
  struct symscope : std::enable_shared_from_this<symscope>
  {

  public:
  
    struct compare_symbol 
    {
      bool operator() (const coder_symbol_ptr& lhs,const  coder_symbol_ptr& rhs) const
      {
        return lhs->name < rhs->name; 
      }    
    };
    
    using symscope_ptr = std::shared_ptr<symscope>;
    
    symscope(const symscope&)=delete;
    
    symscope& operator=(const symscope& other)=delete;
    
    symscope(symscope&& other):
    
    m_symbols(std::move(other.m_symbols)),
    
    anon_fcn_scopes(std::move(other.anon_fcn_scopes)),
    
    nested_fcn_scopes(std::move(other.nested_fcn_scopes)),
    
    parent(other.parent == &other ? this : other.parent)
    {}
    
    symscope& operator=(symscope&& other)
    {
      m_symbols =std::move(other.m_symbols);
      
      anon_fcn_scopes = std::move(other.anon_fcn_scopes);
      
      nested_fcn_scopes=std::move(other.nested_fcn_scopes);
      
      parent = (other.parent == &other ? this : other.parent);
      
      return *this;
    }
    
    symscope(symscope* par) 
    : m_symbols((int)symbol_type::num_symbol_types), parent(par) 
    {} 
    
    symscope() 
    : m_symbols((int)symbol_type::num_symbol_types),parent(this) 
    {}
    
    symscope_ptr get_ptr() {return shared_from_this();}
       
    symscope_ptr  root() ;
    
    coder_symbol_ptr contains (const coder_symbol_ptr& name) const;
    
    coder_symbol_ptr contains (const coder_symbol_ptr& name, symbol_type type) const;
    
    coder_symbol_ptr contains (const std::string& name) const;
    
    coder_symbol_ptr contains (const std::string& name, symbol_type type) const;

    coder_symbol_ptr lookup_in_parent_scopes (const coder_symbol_ptr& name) const;
    
    coder_symbol_ptr lookup_in_parent_scopes (const coder_symbol_ptr& name, symbol_type type) const;
    
    coder_symbol_ptr lookup_in_parent_scopes (const std::string& name) const;
    
    coder_symbol_ptr lookup_in_parent_scopes (const std::string& name, symbol_type type) const;

    void insert_symbol( const coder_symbol_ptr & value , symbol_type type) ;
  
    bool erase_symbol(const coder_symbol_ptr & value , symbol_type type);
  
    symscope_ptr 
    close_current_scope ();
    
    symscope_ptr 
    open_new_anon_scope ();
    
    symscope_ptr 
    open_new_nested_scope ();
    
    void 
    dump () const;

    std::vector<std::deque<symscope_ptr> >
    traverse(std::vector<std::deque<symscope_ptr>> result) const;
    
    const std::vector <
      std::set<coder_symbol_ptr, symscope::compare_symbol>
      >& 
    symbols () const;    
      
  private:
      
    std::vector <
      std::set<coder_symbol_ptr, symscope::compare_symbol>
      >  m_symbols;
      
    std::deque<symscope_ptr > anon_fcn_scopes;
    
    std::deque<symscope_ptr > nested_fcn_scopes;
    
    symscope* parent;
  };

  class symtab 
  {
  public:
  
    symtab(const std::string& name = "")
      :m_name(name),m_size(1),hierarchy(new symscope()),
      m_currentscope(hierarchy)
      {}
    
    symtab(const symtab&) = delete;
    
    symtab& operator=(const symtab& ) =delete;
    
    symtab(symtab&& ) = default;
    
    symtab& operator=(symtab&& ) =default;
        
    coder_symbol_ptr 
    contains (const coder_symbol_ptr& name) const;
    
    coder_symbol_ptr 
    contains (const std::string& name) const;
    
    coder_symbol_ptr 
    lookup_in_parent_scopes (const coder_symbol_ptr& name) const;
    
    coder_symbol_ptr 
    lookup_in_parent_scopes (const std::string& name) const;
  
    void 
    insert_symbol( const coder_symbol_ptr& value , symbol_type type);

    bool 
    erase_symbol(const coder_symbol_ptr& value , symbol_type type) ;
    
    void 
    close_current_scope () ;
    
    void 
    open_new_anon_scope () ;
    
    void 
    open_new_nested_scope () ;
    
    symscope_ptr 
    current_scope();
    
    void 
    reset ();
    
    std::size_t 
    size() const ;
    
    std::string 
    name() const;
    
    void dump() const;
    
    std::vector<std::deque<symscope_ptr>> 
    traverse() const;
    
  private:
  
    std::string m_name;
    
    std::size_t m_size;
    
    symscope_ptr hierarchy;
    
    symscope_ptr m_currentscope;
  };
}
