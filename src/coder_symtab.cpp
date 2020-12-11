#include <iostream>

#include <octave/ov.h>

#include "coder_symtab.h"

namespace coder_compiler
{
	int level = 0;

	static std::string tab(int n)
  {
		std::string retval;

		for(int i=0;i< n;i++)
			retval+="  ";

		return retval;
	}

	symscope_ptr
  symscope::root()
  {
		symscope* scope = this;

		while(scope->parent != scope)
			scope = scope->parent;

    return scope->get_ptr();
	}

  coder_symbol_ptr
  symscope::contains (const coder_symbol_ptr& name) const
  {
    for (const auto& table: m_symbols)
      {
        auto it = table.find(name);

        if(it != table.end())
          return *it;
      }

    return coder_symbol_ptr();
  }

  coder_symbol_ptr
  symscope::contains (const coder_symbol_ptr& name, symbol_type type) const
  {
    const auto& table = m_symbols.at((int)type);
    {
      auto it = table.find(name);

      if(it != table.end())
        return *it;
    }

    return coder_symbol_ptr();
  }

  coder_symbol_ptr
  symscope::contains (const std::string& name) const
  {
    coder_symbol_ptr sym = coder_symbol_ptr(new coder_symbol(name));

    return contains(sym) ;
  }

  coder_symbol_ptr
  symscope::contains (const std::string& name, symbol_type type) const
  {
    coder_symbol_ptr sym = coder_symbol_ptr(new coder_symbol(name));

    return contains(sym,type) ;
  }

  coder_symbol_ptr
  symscope::lookup_in_parent_scopes (const coder_symbol_ptr& name) const
  {
    if(parent != this)
      {
        const symscope* scope = this;

        do
          {
            scope = scope->parent;

            auto sym = scope->contains(name);

            if (sym)
              return sym;
          }
        while(scope->parent != scope);
      }

    return coder_symbol_ptr();
  }

  coder_symbol_ptr
  symscope::lookup_in_parent_scopes (const coder_symbol_ptr& name, symbol_type type) const
  {
    if(parent != this)
      {
        const symscope* scope = this;

        do
          {
            scope = scope->parent;

            auto sym = scope->contains(name, type);

            if (sym)
              return sym;
          }
        while(scope->parent != scope);
      }

    return coder_symbol_ptr();
  }

  coder_symbol_ptr
  symscope::lookup_in_parent_scopes (const std::string& name) const
  {
    coder_symbol_ptr sym = coder_symbol_ptr(new coder_symbol(name));

    return lookup_in_parent_scopes(sym);
  }

  coder_symbol_ptr
  symscope::lookup_in_parent_scopes (const std::string& name, symbol_type type) const
  {
    coder_symbol_ptr sym = coder_symbol_ptr(new coder_symbol(name));

    return lookup_in_parent_scopes(sym,type);
  }

	void
  symscope::insert_symbol( const coder_symbol_ptr & value , symbol_type type)
  {
		 m_symbols.at((int)type).insert(value);
  }

  bool
  symscope::erase_symbol(const coder_symbol_ptr & value , symbol_type type)
  {
    return m_symbols[(int)type].erase(value);
  }

  symscope_ptr
  symscope::close_current_scope ()
  {
    return parent->get_ptr();
  }

  symscope_ptr
  symscope::open_new_anon_scope ()
  {
    anon_fcn_scopes.emplace_back(std::make_shared<symscope>(this));

    return anon_fcn_scopes.back();
  }

  symscope_ptr
  symscope::open_new_nested_scope ()
  {
    nested_fcn_scopes.emplace_back(std::make_shared<symscope>(this));

    return nested_fcn_scopes.back();
  }

  void symscope::dump () const
  {
    std::vector<std::string> symbol_type_str
    {
      "ordinary",
      "inherited",
      "persistent",
      "nested_fcn"
    };

    int i =0;

    std::cout << tab(++level) << "dumping:"<<"\n";

    ++level;

    for(auto& x : m_symbols)
      {
        std::cout<<tab(level) << symbol_type_str[i++]<<":\n";

        ++level;

        for(auto& y: x)
          {
            std::cout<< tab (level) << y->name<<std::endl;
          }

        level--;
      }

    level--;

    std::cout<<tab(++level)<<"dumping anonymous function:"<<"\n";

    for (auto& anon: anon_fcn_scopes)
      {
        anon->dump();
      }

    std::cout<<tab(level--)<<"end:dumping anonymous function:"<<"\n";

    std::cout<<tab(++level)<<"dumping nested function:"<<"\n";

    for (auto& nested: nested_fcn_scopes)
      {
        nested->dump();
      }

    std::cout<<tab(level--)<<"end:dumping nested function:"<<"\n";

    std::cout<<tab(level--)<<"end:dumping:"<<"\n";
  }

  std::vector<std::deque<symscope_ptr> >
  symscope::traverse(std::vector<std::deque<symscope_ptr>> result) const
  {
    for(auto& sc: nested_fcn_scopes)
      {
        result[0].push_back(sc);

        result = sc->traverse(std::move(result));
      }

    for(auto& sc: anon_fcn_scopes)
      {
        result[1].push_back(sc);

        result = sc->traverse(std::move(result));
      }

    return result;
  }

  const std::vector <
    std::set<coder_symbol_ptr, symscope::compare_symbol>
    >& symscope::symbols () const
  {
    return m_symbols;
  }

  coder_symbol_ptr symtab::contains (const coder_symbol_ptr& name) const
  {
    return m_currentscope->contains(name);
  }

  coder_symbol_ptr symtab::lookup_in_parent_scopes (const coder_symbol_ptr& name) const
  {
    return m_currentscope->lookup_in_parent_scopes(name);
  }

  coder_symbol_ptr symtab::contains (const std::string& name) const
  {
    return m_currentscope->contains(name);
  }

  coder_symbol_ptr symtab::lookup_in_parent_scopes (const std::string& name) const
  {
    return m_currentscope->lookup_in_parent_scopes(name);
  }

  void symtab::insert_symbol( const coder_symbol_ptr& value , symbol_type type)
  {
     m_currentscope->insert_symbol(value, type);
  }

  bool symtab::erase_symbol(const coder_symbol_ptr& value , symbol_type type)
  {
    return m_currentscope->erase_symbol(value, type);
  }

  void symtab::close_current_scope ()
  {
    m_currentscope = m_currentscope->close_current_scope() ;
  }

  void symtab::open_new_anon_scope ()
  {
    ++m_size;

    m_currentscope = m_currentscope->open_new_anon_scope();
  }

  void symtab::open_new_nested_scope ()
  {
    ++m_size;

    m_currentscope = m_currentscope->open_new_nested_scope();
  }

  symscope_ptr
  symtab::current_scope()
  {
    return m_currentscope;
  }

  void
  symtab::reset ()
  {
    m_currentscope = hierarchy;
  }

  std::size_t
  symtab::size() const
  {
    return m_size;
  }

  std::string
  symtab::name() const
  {
    return m_name;
  }

  void
  symtab::dump()const
  {
    hierarchy->dump();
  }

  std::vector<std::deque<symscope_ptr>>
  symtab::traverse() const
  {
    std::vector<std::deque<symscope_ptr>> result(2);

    result[0].push_back(hierarchy);

    return hierarchy->traverse(std::move(result));
  }
}
