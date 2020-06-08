#include <cctype>
#include <fstream>
#include <iostream>

#include <octave/builtin-defun-decls.h>
#include <octave/dir-ops.h>
#include <octave/file-ops.h>
#include <octave/file-stat.h>
#include <octave/ls-mat5.h>
#include <octave/oct-env.h>
#include <octave/oct.h>
#include <octave/pager.h>

#include "build_system.h"
#include "code_generator.h"
#include "coder_file.h"
#include "coder_runtime.h"
#include "coder_symtab.h"
#include "dgraph.h"
#include "octave_refactored.h"
#include "semantic_analyser.h"

namespace coder_compiler
{

  std::string
  mangle (const std::string& str)
  {
    return str + "_";
  }

  void
  printgraph(const dgraph& G)
  {
    std::cout << "\n{";

    for(const auto& n: G)
      {
        std::cout
          << "{"
          << n.first->name
          << ","
          << "{";

        for(const auto& m: n.second)
          {
             std::cout
              << m->name
              << " ";
          }

        std::cout << "}} ";
      }

    std::cout << "}\n";
  }

  build_system::build_system (
    build_mode mode,
    const std::string& cache_dir
  )
  :
    analyser(),
    cache_updated(false),
    new_or_updated_files(),
    cache_index(),
    dyn_oct_list(),
    cache_directory(cache_dir),
    mode(mode)       
  {
    if (mode == bm_static || mode == bm_dynamic)
      {
        for (const auto& dir : {"lib", "bin", "include", "src", "tmp"})
          {
            std::string dirname = octave::sys::file_ops::concat (cache_directory, dir);

            dirname = octave::sys::file_ops::tilde_expand (dirname);

            octave::sys::file_stat fs (dirname);

            if (!(fs && fs.exists () && fs.is_dir ()))
              {
                std::string msg;

                int status = octave::sys::mkdir (dirname, 0777, msg);

                if (status < 0)
                  error("%s", msg.c_str());
              }
          }

        read_cache_index ();

        if (cache_index.nfields () == 0)
          {
            auto schema = make_schema ();

            cache_index = octave_map (schema.first);

            dyn_oct_list = octave_map (schema.second);
          }

        analyser.build_dependency_graph(cache_index);
      }
  }

  void
  build_system::update_cache_index ()
  {
    if (new_or_updated_files.size () == 0)
      return;

    if (new_or_updated_files.size () > 0)
      {
        if (! cache_index.nfields ())
          {
            auto p = make_schema();

            cache_index = octave_map (p.first);
          }

        Cell& names = cache_index.contents ("name");

        Cell& ids =   cache_index.contents ("id");

        octave_idx_type n = names.numel ();

        std::map<std::string, octave_idx_type> files_in_cache;

        for (octave_idx_type i = 0 ; i < n; i++)
          {
            std::string key = mangle (names (i).string_value ()) + std::to_string (ids (i).int_value ());

            files_in_cache[key] = i;
          }

        std::vector<coder_file_ptr> non_cached_files;

        for (const auto& file : new_or_updated_files)
          {
            std::string key = mangle (file->name) + std::to_string (file->id);

            auto f = files_in_cache.find (key);

            if (f != files_in_cache.end ())
              {
                analyser.write_dep (file, scalar_map_view( f->second, cache_index));
              }
            else
              {
                non_cached_files.push_back (file);
              }
          }

        if (non_cached_files.size () > 0)
          {
            octave_idx_type old_sz = cache_index.numel();

            octave_idx_type new_sz = old_sz + non_cached_files.size ();

            cache_index.resize (dim_vector (1, new_sz));

            size_t j = 0;

            for (octave_idx_type i = old_sz; i < new_sz; i++)
              {
                analyser.write_dep (non_cached_files[j++], scalar_map_view( i,cache_index));
              }
          }
      }
  }

  void
  build_system::update_cache_index (const std::map<std::string, coder_file_ptr>& oct_map)
  {
    update_cache_index ();

    dyn_oct_list = analyser.write_oct_list (oct_map);
  }

  bool
  build_system::write_cache_index () const
  {
    using octave::sys::file_ops::concat;

    octave_scalar_map result;

    result.setfield ("files", cache_index);

    result.setfield ("dyn_oct_list", dyn_oct_list);

    std::ios::openmode omode = std::ios::out | std::ios::binary;

    std::ofstream os(concat(cache_directory ,"cache_index.mat" ), omode);

    return write_mat (os, octave_value (result));
  }

  void
  build_system::read_cache_index ()
  {
    using octave::sys::file_ops::concat;

    std::ios::openmode omode = std::ios::in | std::ios::binary;

    std::string filename = "cache_index.mat";

    std::ifstream is(concat(cache_directory , filename ), omode);

    if (is.good ())
      {
        octave_value val ;

        bool global = false;

        bool swap = false;

        read_mat5_binary_file_header (is, swap, false, filename);

        read_mat5_binary_element (is, filename, swap, global, val);

        cache_index = val.subsref(".",{ovl("files")}).map_value ();

        dyn_oct_list = val.subsref(".",{ovl("dyn_oct_list")}).map_value ();
      }
  }

  void
  build_system::run (
    const std::vector<std::string>& sym_names,
    const std::vector<std::string>& out_names,
    const std::vector<std::string>& out_dirs,
    bool upgrade_cache,
    bool debug,
    bool keepcc,
    bool verbose,
    const std::string& compiler_options
  )
  {
    struct build_option
    {
      coder_file_ptr file;
      std::string    sym_name;
      bool           mkoct;
      bool           mkoct_bridge;
      std::string    out_name ;
      std::string    out_dir  ;
    };

    if (! (sym_names.size() == out_names.size() && out_names.size() == out_dirs.size()))
      return;

    std::vector <build_option> options;

    std::map<std::string, coder_file_ptr> cached_oct_list;

    std::set<std::string> updated_octs;

    bool new_octs_added = false;

    if (mode == bm_dynamic)
      cached_oct_list = analyser.read_oct_list (dyn_oct_list);

    auto  out_name_it = out_names.begin ();

    auto  out_dir_it = out_dirs.begin ();

    for (const std::string& fname : sym_names)
      {
        octave::symbol_table& octave_symtab = octave::interpreter::the_interpreter ()->get_symbol_table();

        octave_value fcn_val = octave_symtab.find_function (fname, octave_value_list ());

        auto start_node = analyser.analyse (fname, fcn_val);

        if (! start_node)
          {
            warning ("coder: cannot create/upgrade oct file from unresoved symbol \"%s\"", fname.c_str ());

            continue;
          }

        if (mode == bm_dynamic)
          {
            updated_octs.insert(fname);

            auto it = cached_oct_list.find (fname);

            if (it != cached_oct_list.end ())
              {
                if (it->second != start_node)
                  {
                    new_octs_added = true;

                    cached_oct_list[fname] = start_node;

                    options.push_back ({start_node, fname, true, true, *out_name_it++, *out_dir_it++});
                  }
                else
                  {
                    options.push_back ({start_node, fname, true, false, *out_name_it++, *out_dir_it++});
                  }
              }
            else
              {
                new_octs_added = true;

                cached_oct_list[fname] = start_node;

                options.push_back ({start_node, fname, true, true, *out_name_it++, *out_dir_it++});
              }
          }
        else
          {
            options.push_back ({start_node, fname, true, false, *out_name_it++, *out_dir_it++});
          }
      }

    size_t n_octs = options.size ();

    if (mode == bm_dynamic && upgrade_cache)
      for (auto& oct : cached_oct_list)
        {
          const std::string& fname = oct.first;

          coder_file_ptr& file = oct.second;

          if (! file )
            continue;

          if (updated_octs.count(fname) == 0)
            {
              octave::symbol_table& octave_symtab = octave::interpreter::the_interpreter ()->get_symbol_table();

              octave_value fcn_val = octave_symtab.find_function (fname, octave_value_list ());

              auto start_node = analyser.analyse (fname, fcn_val);

              if (! start_node)
                {
                  warning ("coder: cannot create/upgrade oct file from unresoved symbol \"%s\"", fname.c_str ());

                  continue;
                }

              bool resolved_to_different_name = start_node != file;

              if (resolved_to_different_name)
                {
                  file = start_node;

                  new_octs_added = true;
                }

              options.push_back ({start_node, fname, false, resolved_to_different_name, "", ""});
            }
        }

    size_t i = 0;

    for (const auto& opt : options)
      {
        if (verbose)
          {
            if (i++ < n_octs)
              octave_stdout << "\nbuilding " << opt.out_name << ".oct ...\n";
            else
              octave_stdout << "\nupdating dependencies of \"" << opt.sym_name << "\" ...\n";
          }
        build (opt.file, opt.sym_name, opt.mkoct, opt.mkoct_bridge, opt.out_name, opt.out_dir, debug, keepcc, verbose, compiler_options);
      }

    if (mode == bm_dynamic )
      {
        if (new_octs_added)
          {
            update_cache_index (cached_oct_list);
          }
        else
          update_cache_index ();
      }
    else if (mode == bm_static)
      update_cache_index ();

    if (mode == bm_static || mode == bm_dynamic)
      write_cache_index ();
  }

  void
  build_system::build(
    const coder_file_ptr& start_node,
    const std::string& sym_name,
    bool mkoct,
    bool mkoct_bridge,
    const std::string& out_name,
    const std::string& out_dir,
    bool debug,
    bool keepcc,
    bool verbose,
    const std::string& compiler_options
  )
  {
    auto quote = [](const std::string& str) ->std::string
    {
      std::string result = str;

      for(char& c:result)
        if(c == '\\')
          c = '/';

      return "\"" + result + "\"";
    };

    auto call_mkoctfile = [] (const octave_value_list& args)
    {
      octave_value_list ret = octave::feval ("mkoctfile", args, 2);

      if (ret(1).int_value () != 0)
        error ("coder: compile error");
    };

    std::vector<coder_file_ptr> retval;

    using octave::sys::file_ops::concat;

    using octave::sys::file_ops::tilde_expand;

    bool isunix = F__octave_config_info__ (octave_value("unix"),1)(0).bool_value ();

    bool ispc = F__octave_config_info__ (octave_value("windows"),1)(0).bool_value ();

    bool ismac = F__octave_config_info__ (octave_value("mac"),1)(0).bool_value ();

    std::string  DL_LDFLAGS = F__octave_config_info__ (octave_value("DL_LDFLAGS"),1)(0).string_value ();

    std::string  SH_LDFLAGS = F__octave_config_info__ (octave_value("SH_LDFLAGS"),1)(0).string_value ();

    std::string libdir = tilde_expand(concat (cache_directory, "lib"));

    std::string srcdir = tilde_expand(concat (cache_directory, "src"));

    std::string incdir = tilde_expand(concat (cache_directory, "include"));

    std::string bindir = tilde_expand(concat (cache_directory, "bin"));

    std::string tmpdir = tilde_expand(concat (cache_directory, "tmp"));

    std::stringstream header ;

    std::stringstream source ;

    std::stringstream partial_source ;

    std::string shared_ext;

    if (isunix)
      shared_ext = ".so";
    else if (ispc)
      shared_ext = ".dll";
    else if (ismac)
      shared_ext = ".dylib";

    std::string dbg = debug ? "-g" : "-g0";

    std::string strp = debug ? "-O2" : "-s";

    std::string strpl = debug ? "" : ",-s";

    std::string coptions = "-O2";

    if (! compiler_options.empty ())
      coptions = compiler_options;

    auto generate = [&](const coder_file_ptr& file, bool iscyclic = false)->void
    {
      if ( analyser.should_generate (file))
        {
          std::string filename = mangle(lowercase(file->name)) + std::to_string(file->id);

          if (mode != bm_single)
            {
              std::fstream header (concat(incdir ,filename + ".h" ), header.in | header.out | header.trunc);

              std::fstream source (concat(srcdir ,filename + ".cpp" ), source.in | source.out | source.trunc);

              std::fstream partial_source ;

              if (iscyclic)
                partial_source = std::fstream(concat(srcdir ,filename + "-partial" + ".cpp" ), partial_source.in | partial_source.out | partial_source.trunc);

              code_generator cgen (file, analyser.dependency (), header, source, partial_source, gm_full);

              cgen.generate(iscyclic);
            }
          else
            {
              code_generator cgen (file, analyser.dependency (), header, source, partial_source, gm_compact);

              cgen.generate(false);
            }
        }
    };

    auto init = [&]()
    {
      std::string filename = "coder";

      std::string h = concat(incdir, filename + ".h" );

      std::string cpp = concat(srcdir, filename + ".cpp" );

      std::string obj = concat(libdir, filename + ".o" );

      std::string bin = concat(bindir, "lib" + filename + shared_ext );

      octave::sys::file_stat h_stat (h);

      octave::sys::file_stat c_stat (cpp);

      octave::sys::file_stat o_stat (obj);

      octave::sys::file_stat d_stat (bin);

      bool recompile = ! h_stat.exists () || ! c_stat.exists () || ! o_stat.exists () || (mode == bm_dynamic && ! d_stat.exists ());

      if (! h_stat.exists () || ! c_stat.exists ())
        {
            std::ofstream header(h);

            header << "#pragma once\n";

            header << runtime_header ();

            header.close ();

            std::ofstream source(cpp);

            source << "#include \"coder.h\"\n";

            source << runtime_source ();

            source.close ();
        }

      if (recompile)
        {
          if (verbose)
            octave_stdout << "  compiling coder runtime\n";

          call_mkoctfile (
            ovl(
            octave_value( quote("-o " + obj)),
            octave_value(quote("-std=gnu++11")),
            octave_value("-fPIC"),
            octave_value("-c"),
            octave_value(dbg),
            octave_value(quote("-I" + incdir)),
            octave_value (coptions),
            octave_value(quote(cpp))
           ));

          if (mode == bm_dynamic )
            {
              Fsetenv (ovl(octave_value("DL_LDFLAGS"), octave_value(SH_LDFLAGS) ));

              call_mkoctfile (
                ovl(
                octave_value(quote(obj)),
                octave_value("-Wl,--output," + quote(bin) + strpl)
                ));

              Fsetenv (ovl(octave_value("DL_LDFLAGS"), octave_value(DL_LDFLAGS) ));
            }
        }
    };

    auto compile =[&](const coder_file_ptr& file)
    {
      std::string filename = mangle(lowercase (file->name)) + std::to_string(file->id) ;

      std::string obj = concat(libdir, filename + ".o" );

      octave::sys::file_stat obj_stat (obj);

      file_time obj_time (obj);

      if (analyser.should_generate (file)
          || ! obj_stat.exists () )
        {
          if (verbose)
            {
              octave_stdout << "  compiling " << filename  << ".o\n";
            }

          std::string cpp = concat(srcdir, filename + ".cpp" );

          call_mkoctfile (
            ovl(
            octave_value( quote("-o " + obj)),
            octave_value(quote("-std=gnu++11")),
            octave_value("-fPIC"),
            octave_value("-c"),
            octave_value(dbg),
            octave_value(quote("-I" + incdir)),
            octave_value (coptions),
            octave_value(quote(cpp))
          ));
        }
    };

    auto forward_link = [&] (const coder_file_ptr& file)->bool
    {
      std::string filename = mangle(lowercase (file->name)) + std::to_string(file->id) ;

      std::string cpp = concat(srcdir, filename +  "-partial" + ".cpp" );

      std::string obj = concat(libdir, filename + ".o" );

      std::string tmpobj = concat(tmpdir, filename + ".o" );

      std::string bin = concat(bindir, "lib" + filename + shared_ext );

      octave::sys::file_stat bin_stat (bin);

      file_time obj_time (obj);

      file_time bin_time (bin);

      if (  analyser.should_generate (file)   // forward link (to avoid circular dependency problems when building in dynamic mode)
        ||  ! bin_stat.exists ()
        ||  obj_time.is_newer (bin_time.mtime ()))
        {
          call_mkoctfile (
            ovl(
            octave_value( quote("-o " + tmpobj)),
            octave_value(quote("-std=gnu++11")),
            octave_value("-fPIC"),
            octave_value("-c"),
            octave_value(dbg),
            octave_value(quote("-I" + incdir)),
            octave_value (coptions),
            octave_value(quote(cpp))
             )
          );

          Fsetenv (ovl(octave_value("DL_LDFLAGS"), octave_value(SH_LDFLAGS) ));

          call_mkoctfile (
            ovl( octave_value (quote(tmpobj)),
            octave_value("-Wl,--output," + quote(bin) + strpl)
            ) );

          Fsetenv (ovl(octave_value("DL_LDFLAGS"), octave_value(DL_LDFLAGS) ));

          return true;
        }

      return false;
    };

    auto link =[&](const coder_file_ptr& file, bool force = false)
    {
      std::string filename = mangle(lowercase (file->name)) + std::to_string(file->id) ;

      std::string bin = concat(bindir, "lib" + filename + shared_ext );

      std::string obj = concat(libdir, filename + ".o" );

      octave::sys::file_stat bin_stat (bin);

      file_time bin_time (bin);

      file_time obj_time (obj);

      if ( force ||
           analyser.should_generate (file)
        || ! bin_stat.exists ()
        || obj_time.is_newer (bin_time.mtime ())
        )
        {
          if (verbose)
            {
              octave_stdout << "  linking " << filename + shared_ext << "\n";
            }
          
          std::string dep_names = "-lcoder ";

          for(const auto& f: analyser.dependency ().at(file) )
            {
              dep_names += "-l" + mangle(lowercase (f->name)) + std::to_string(f->id) + " ";
            }

          Fsetenv (ovl(octave_value("DL_LDFLAGS"), octave_value(SH_LDFLAGS) ));

          call_mkoctfile (
            ovl(
            octave_value("-Wl,-o," + quote(bin) + strpl),
            octave_value(quote(obj)),
            octave_value(quote("-L" + bindir)),
            dep_names
            )
          );

          Fsetenv (ovl(octave_value("DL_LDFLAGS"), octave_value(DL_LDFLAGS) ));
        }
    };

    auto make_oct_bridge =[&](const coder_file_ptr& file, const std::string& sym_name)
    {
      std::string nsname = mangle(file->name) + std::to_string(file->id);

      std::string filename = lowercase (nsname);

      std::string bridge_nsname = mangle(sym_name) + std::to_string(1);

      std::string bridge_filename = lowercase (bridge_nsname);

      std::string h = concat(incdir ,bridge_filename + ".h" );

      std::string cpp = concat(srcdir ,bridge_filename + ".cpp" );

      std::string obj = concat(libdir ,bridge_filename + ".o" );

      std::string bin = concat(bindir, "lib" + bridge_filename + shared_ext );

      std::ofstream header (h);

      std::ofstream source (cpp);

      header
        << "#pragma once\n"
        << "namespace coder{struct Symbol;}\n"
        << "using namespace coder;\n"
        << "namespace "
        << bridge_nsname
        << "\n{\n"
        << "  const Symbol& "
        << mangle(sym_name) << "make();\n"
        << "}\n";

      header.close ();

      source
        << "#include" << quote("coder.h") << "\n"
        << "#include" << quote(bridge_filename + ".h") << "\n"
        << "#include" << quote(filename + ".h") << "\n"
        << "using namespace coder;\n"
        << "namespace "
        << bridge_nsname
        << "\n{\n"
        << "  const Symbol& "
        << mangle(sym_name) << "make()\n  {\n"
        << "    static const Symbol "
        << mangle(sym_name)
        << " (" << nsname  << "::" << mangle (sym_name) << "make()" << ");\n"
        << "    return " << mangle(sym_name) << ";\n"
        << "  }\n}\n";

      source.close ();

      call_mkoctfile (
        ovl(
        octave_value( quote("-o " + obj)),
        octave_value(quote("-std=gnu++11")),
        octave_value("-fPIC"),
        octave_value("-c"),
        octave_value(dbg),
        octave_value(quote("-I" + incdir)),
        octave_value (coptions),
        octave_value(quote(cpp))
         ));

      std::string dep_names = "-lcoder -l" + filename + " ";

      Fsetenv (ovl(octave_value("DL_LDFLAGS"), octave_value(SH_LDFLAGS) ));

      call_mkoctfile (
        ovl(octave_value("-Wl,-o," + quote(bin)+ strpl),
        octave_value(quote(obj)),
        octave_value(quote("-L" + bindir)),
        dep_names
        ));

      Fsetenv (ovl(octave_value("DL_LDFLAGS"), octave_value(DL_LDFLAGS) ));
    };

    auto mkoctfile  = [&](const coder_file_ptr& file,
      const std::string& sym_name,
      const std::string& out_name,
      const std::string& out_dir,
      const dgraph& dependency
      )
    {
      if (! file)
        return;

      std::string cc = concat (out_dir, out_name  + ".cc");

      std::string obj = concat (out_dir, out_name  + ".o");

      std::string oct = concat (out_dir, out_name + ".oct") ;

      if (mode == bm_single)
        {
          std::string nsname = mangle(file->name) + std::to_string(file->id);

          std::string filename = lowercase (nsname);

          std::ofstream oct_os(cc);

          oct_os
            << "#include <octave/oct.h>" << "\n"
            << "#include \"interpreter.h\"\n"
            << runtime_header()
            << runtime_source()
            << "\nusing namespace coder;\n"
            << header.str()
            << source.str()
            << "DEFCODER_DLD ("
            << out_name
            << ", interp, args, nargout,"
            << quote(out_name)
            << ",\n{\n"
            << "  return "
            << nsname
            << "::"
            <<  mangle(sym_name)
            << "make().get_value()->function_value()->call(interp.get_evaluator(),nargout,args);\n})";

          oct_os.close();

          call_mkoctfile (
            ovl(
            octave_value( quote("-o " + obj)),
            octave_value(quote("-std=gnu++11")),
            octave_value("-fPIC"),
            octave_value("-c"),
            octave_value(dbg),
            octave_value (coptions),
            octave_value(quote(cc))

             ));

          call_mkoctfile (
            ovl(
            octave_value("-o"),
            octave_value(quote(oct)),
            octave_value(quote (obj)),
            octave_value(strp)
             )
          );
        }
      else if (mode == bm_static)
        {
          std::string nsname = mangle(file->name) + std::to_string(file->id);

          std::string filename = lowercase (nsname);

          std::list<octave_value> obj_files;

          std::ofstream oct_os(cc);

          oct_os
            << "#include <octave/oct.h>" << "\n"
            << "#include \"interpreter.h\"" << "\n"
            << "#include" << quote("coder.h") << "\n"
            << "#include" << quote(filename + ".h") << "\n"
            << "DEFCODER_DLD ("
            << out_name
            << ", interp, args, nargout,"
            << quote(out_name)
            << ",\n{\n"
            << "  return "
            << nsname
            << "::"
            <<  mangle(sym_name)
            << "make().get_value()->function_value()->call(interp.get_evaluator(),nargout,args);\n})";

          oct_os.close ();

          obj_files.push_back (quote (obj));

          obj_files.push_back (quote (concat (libdir, "coder.o")));

          for (const auto& entry : dependency )
            {
              const coder_file_ptr& dfile = entry.first;

              obj_files.push_back (quote (concat ( libdir, mangle(lowercase (dfile->name)) + std::to_string(dfile->id) ) + ".o"));
            }

          call_mkoctfile(
            ovl(
            octave_value("-o"),
            octave_value(quote(obj)),
            octave_value("-c"),
            octave_value(quote("-std=gnu++11")),
            octave_value(dbg),
            octave_value(quote("-I" + incdir)),
            octave_value("-fPIC"),
            octave_value (coptions),
            octave_value(quote(cc))
            )
          );

          call_mkoctfile(
            ovl(
            octave_value("-o"),
            octave_value(quote(oct)))
            .append ( octave_value_list (obj_files))
            .append (octave_value(strp))
          );
        }
      else if (mode == bm_dynamic)
        {
          std::string bridge_nsname = mangle(sym_name) + std::to_string(1);

          std::string bridge_filename = lowercase (bridge_nsname);

          std::ofstream oct_os(cc);

          oct_os
            << "#include <octave/oct.h>" << "\n"
            << "#include \"interpreter.h\"" << "\n"
            << "#include" << quote("coder.h") << "\n"
            << "#include" << quote(bridge_filename + ".h") << "\n"
            << "DEFCODER_DLD ("
            << out_name
            << ", interp, args, nargout,"
            << quote(out_name)
            << ",\n{\n"
            << "  return "
            << bridge_nsname
            << "::"
            <<  mangle(sym_name)
            << "make().get_value()->function_value()->call(interp.get_evaluator(),nargout,args);\n})";

          oct_os.close ();

          call_mkoctfile(
            ovl(
            octave_value("-o"),
            octave_value(quote(obj)),
            octave_value("-c"),
            octave_value(quote("-std=gnu++11")),
            octave_value(dbg),
            octave_value(quote("-I" + incdir)),
            octave_value("-fPIC"),
            octave_value (coptions),
            octave_value(quote(cc))
            )
          );

          call_mkoctfile(
            ovl(
            octave_value(quote("-L" + bindir)),
            octave_value("-l" + bridge_filename),
            octave_value("-o"),
            octave_value(quote(oct)),
            octave_value(quote(obj)),
            octave_value(strp)
            )
          );
        }

      if (! keepcc)
        {
          octave::sys::unlink (obj);

          octave::sys::unlink (cc);
        }
    };

    auto callee = extract_subgraph (analyser.dependency (), start_node);

    auto cycles = conn_comp (callee, 2);

    auto merged = merge_nodes (callee, cycles);

    auto start_merged_node = start_node;

    for (const auto& c: cycles)
      if (c.second.count (start_node))
        {
          start_merged_node = c.first;

          break;
        }

    auto sorted_files = top_sort(merged, start_merged_node);

    for (const auto& file : sorted_files )
      {
        bool iscyclic = cycles.count(file);

        if (iscyclic)
          {
            for (const auto& c_file: cycles.at(file))
              {
                generate(c_file, iscyclic);
              }
          }
        else
          {
            generate (file, iscyclic);
          }
      }

    if (mode != bm_single)
      {
        init();

        for (const auto& file : sorted_files )
          {
            bool iscyclic = cycles.count (file);

            if (iscyclic) // if node is root of a  connected component
              {
                for (const auto& c_file: cycles.at (file))
                  {
                    compile (c_file);
                  }
              }
            else
              {
                compile(file);
              }
          }

        if (mode == bm_dynamic)
          {
            std::set<coder_file_ptr> new_dll;

            for (const auto& file : sorted_files )
              {
                bool iscyclic = cycles.count(file);

                if (iscyclic)
                  {
                    for (const auto& c_file: cycles.at(file))
                      {
                        bool b = forward_link (c_file);

                        if (b)
                          new_dll.insert (file);
                      }
                  }
              }

            for (const auto& file : sorted_files )
              {
                bool iscyclic = cycles.count(file);

                if (iscyclic)
                  {
                    for (const auto& c_file: cycles.at(file))
                      {
                        link (c_file, new_dll.count (c_file));
                      }
                  }
                else
                  {
                    link (file, new_dll.count (file));
                  }
              }
          }
      }

    if (mkoct_bridge)
      make_oct_bridge(start_node, sym_name);

    if (mkoct)
      {
        if (verbose)
          octave_stdout << "  linking " << out_name << ".oct\n";

        mkoctfile(
          start_node,
          sym_name,
          out_name,
          out_dir,
          callee
        );
      }

    for (const auto& file : sorted_files )
      {
        bool iscyclic = cycles.count(file);

        if (iscyclic)
          {
            for (const auto& c_file: cycles.at(file))
              {
                if (analyser.should_generate (c_file))
                  new_or_updated_files.insert (c_file);
              }
          }
        else
          {
            if (analyser.should_generate (file))
              new_or_updated_files.insert (file);
          }
      }

    for (const auto& file : sorted_files )
      {
        bool iscyclic = cycles.count(file);

        if (iscyclic)
          {
            for (const auto& c_file: cycles.at(file))
              {
                analyser.mark_as_generated (c_file);
              }
          }
        else
          {
            analyser.mark_as_generated (file);
          }
      }
  }
}
