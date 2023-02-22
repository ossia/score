
#include <clang/AST/QualTypeNames.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>

#include <cstdio>
#include <iostream>
#include <span>

using namespace clang::ast_matchers;
using namespace clang::tooling;
const DeclarationMatcher methodMatcher{
    cxxRecordDecl(hasDeclContext(namespaceDecl(isExpansionInMainFile()))).bind("class")};

std::string main_class_name;
std::string main_file_name;

std::vector<std::string> inputs;
bool inputs_recursive = false;
std::vector<std::string> outputs;
bool outputs_recursive = false;
std::vector<std::string> messages;
bool messages_recursive = false;

std::string to_member(const std::vector<std::string>& vec)
{
  assert(vec.size() > 0);
  std::string r;
  for(int i = 0; i < std::ssize(vec) - 1; i++)
  {
    r += vec[i];
    r += '.';
  }
  r += vec.back();
  return r;
}

class MethodPrinter final : public clang::ast_matchers::MatchFinder::MatchCallback
{
public:
  static bool is_recursive(clang::RecordDecl* record)
  {
    for(auto decls : record->decls())
    {
      if(llvm::isa<clang::EnumDecl>(decls))
      {
        if(auto e = llvm::cast<clang::EnumDecl>(decls))
        {
          for(auto m : e->enumerators())
          {
            if(m->getNameAsString() == "recursive_group")
            {
              return true;
            }
          }
        }
      }
    }
    return false;
  }

  int k = 0;
  std::vector<std::string> member_stack;
  void printRecursively(clang::FieldDecl* field, std::vector<std::string>& portlist)
  {
    auto field_name = field->getNameAsString();
    member_stack.push_back(field_name);

    if(clang::CXXRecordDecl* record = field->getType()->getAsCXXRecordDecl())
    {
      // clang::QualType qt = field->getType();

      // clang::LangOptions lo;
      // clang::PrintingPolicy p{lo};
      // std::string VarType
      //     = clang::TypeName::getFullyQualifiedName(qt, field->getASTContext(), p);
      // std::cerr << "field: " << field_name << " of type : " << VarType << std::endl;
      if(is_recursive(record))
      {
        for(const auto& ff : record->fields())
        {
          ++k;
          printRecursively(ff, portlist);
          --k;
        }
      }
      else
      {
        portlist.push_back(to_member(member_stack));
      }
    }
    member_stack.pop_back();
  }

  void run(const clang::ast_matchers::MatchFinder::MatchResult& Result) override
  {
    if(auto md = Result.Nodes.getNodeAs<clang::CXXRecordDecl>("class"))
    {
      if(md->getQualifiedNameAsString() == main_class_name)
      {
        for(const auto& field : md->fields())
        {
          auto field_name = field->getNameAsString();

          if(field_name == "inputs")
          {
            auto record = field->getType()->getAsCXXRecordDecl();
            for(const auto& ff : record->fields())
            {
              if(clang::CXXRecordDecl* field_record
                 = ff->getType()->getAsCXXRecordDecl())
                if(is_recursive(field_record))
                  inputs_recursive = true;

              printRecursively(ff, ::inputs);
            }
          }
          else if(field_name == "outputs")
          {
            auto record = field->getType()->getAsCXXRecordDecl();
            for(const auto& ff : record->fields())
            {
              if(clang::CXXRecordDecl* field_record
                 = ff->getType()->getAsCXXRecordDecl())
                if(is_recursive(field_record))
                  outputs_recursive = true;

              printRecursively(ff, ::outputs);
            }
          }
          else if(field_name == "messages")
          {
            auto record = field->getType()->getAsCXXRecordDecl();
            for(const auto& ff : record->fields())
            {
              if(clang::CXXRecordDecl* field_record
                 = ff->getType()->getAsCXXRecordDecl())
                if(is_recursive(field_record))
                  messages_recursive = true;

              printRecursively(ff, ::messages);
            }
          }
        }
      }
    }
  }
};

#define LAFAJOL_SOURCE_DIR "/home/jcelerier/ossia/score"
std::string list_inputs(std::string file, std::span<char*> orig_argv)
{
  std::vector<const char*> argv{"/usr/bin/clang++", file.c_str(), "--"};
  if(orig_argv.empty())
  {
    std::vector<const char*> argv_default{
        "-x",
        "c++",
        "-std=c++20",
        "-fsyntax-only",
        "-I" LAFAJOL_SOURCE_DIR "/3rdparty/avendish/include",
        "-I" LAFAJOL_SOURCE_DIR "/3rdparty/DSPFilters/DSPFilters/include/",
        "-I" LAFAJOL_SOURCE_DIR "/3rdparty/Gamma/",
        "-I" LAFAJOL_SOURCE_DIR "/3rdparty/libossia/3rdparty/brigand/include",
        "-I" LAFAJOL_SOURCE_DIR "/3rdparty/libossia/3rdparty/concurrentqueue",
        "-I" LAFAJOL_SOURCE_DIR "/3rdparty/libossia/3rdparty/dr_libs",
        "-I" LAFAJOL_SOURCE_DIR "/3rdparty/libossia/3rdparty/fmt/include",
        "-I" LAFAJOL_SOURCE_DIR "/3rdparty/libossia/3rdparty/libremidi/include",
        "-I" LAFAJOL_SOURCE_DIR "/3rdparty/libossia/3rdparty/mdspan/include",
        "-I" LAFAJOL_SOURCE_DIR "/3rdparty/libossia/3rdparty/nano-signal-slot/include",
        "-I" LAFAJOL_SOURCE_DIR "/3rdparty/libossia/3rdparty/rapidjson/include",
        "-I" LAFAJOL_SOURCE_DIR "/3rdparty/libossia/3rdparty/readerwriterqueue",
        "-I" LAFAJOL_SOURCE_DIR "/3rdparty/libossia/3rdparty/rnd/include",
        "-I" LAFAJOL_SOURCE_DIR "/3rdparty/libossia/3rdparty/spdlog/include",
        "-I" LAFAJOL_SOURCE_DIR
        "/3rdparty/libossia/3rdparty/SmallFunction/smallfun/include",
        "-I" LAFAJOL_SOURCE_DIR "/3rdparty/libossia/3rdparty/span/include",
        "-I" LAFAJOL_SOURCE_DIR "/3rdparty/libossia/3rdparty/tuplet/include",
        "-I" LAFAJOL_SOURCE_DIR "/3rdparty/libossia/3rdparty/unordered_dense/include",
        "-I" LAFAJOL_SOURCE_DIR "/3rdparty/libossia/3rdparty/mparkvariant/include",
        "-I" LAFAJOL_SOURCE_DIR "/3rdparty/libossia/3rdparty/rubberband/rubberband/",
        "-I" LAFAJOL_SOURCE_DIR "/3rdparty/libossia/3rdparty/r8brain-free-src",
        "-I" LAFAJOL_SOURCE_DIR "/3rdparty/libossia/3rdparty/",
        "-I" LAFAJOL_SOURCE_DIR "/3rdparty/libossia/src",
        "-I" LAFAJOL_SOURCE_DIR "/3rdparty/avendish/include",
        "-I/usr/include/qt",
        "-I/usr/include/qt/QtCore",
        "-I/usr/include/qt/QtGui",
        "-I/usr/include/qt/QtWidgets",
        "-resource-dir",
        "/usr/lib/clang/14.0.6"};
    argv.insert(argv.end(), argv_default.begin(), argv_default.end());
  }
  else
  {
    argv.insert(argv.end(), orig_argv.begin(), orig_argv.end());
  }
  int argc = std::size(argv);
  llvm::cl::OptionCategory cat("", "");

  auto optionsParser = CommonOptionsParser::create(argc, argv.data(), cat);

  ClangTool tool(optionsParser->getCompilations(), optionsParser->getSourcePathList());

  MethodPrinter printer;
  MatchFinder finder;

  finder.addMatcher(methodMatcher, &printer);
  tool.run(newFrontendActionFactory(&finder).get());

  return "main_class_name";
}

#include <fmt/format.h>

#include <fstream>
#include <iostream>
struct generator
{
  std::ofstream out_file_pre{"/tmp/foreach.hpp"};
  std::ofstream out_file_main{"/tmp/test.hpp"};
  const std::vector<std::string>& inputs;
  const std::vector<std::string>& outputs;
  const std::vector<std::string>& messages;

  const std::string inputs_typename
      = fmt::format("avnd::inputs_type_refl<{0}>::type", main_class_name);
  const std::string outputs_typename
      = fmt::format("avnd::outputs_type_refl<{0}>::type", main_class_name);
  const std::string messages_typename
      = fmt::format("avnd::messages_type_refl<{0}>::type", main_class_name);

  void generate_has_recursive_groups_impl(std::string kind_type, bool recursive)
  {
    out_file_main << fmt::format(
        R"_(
template <>
struct has_recursive_groups_impl<{0}>
{{
  static constexpr bool value = {1};
}};
)_",
        kind_type, recursive);
  }

  void generate_has_recursive_groups()
  {
    if(!inputs.empty())
      generate_has_recursive_groups_impl(inputs_typename, inputs_recursive);

    if(!outputs.empty())
      generate_has_recursive_groups_impl(outputs_typename, outputs_recursive);

    if(!messages.empty())
      generate_has_recursive_groups_impl(messages_typename, messages_recursive);
  }

  std::string vec_to_comma_list(const std::vector<std::string>& vec, std::string prefix)
  {
    std::string ret;
    if(vec.empty())
      return ret;
    for(auto& v : vec)
    {
      ret += prefix;
      ret += v;
      ret += ", ";
    }
    ret.resize(ret.size() - 2);
    return ret;
  }

  void generate_tie_as_tuple_impl(std::string name, std::string list)
  {

    // 0: fully qualified object name e.g. examples::helpers::GradientScrub
    // 1: accessors
    // 2: input/output/message

    out_file_main << fmt::format(
        R"_(
template <>
constexpr auto tie_as_tuple<{0}>({0} &v) {{
  return tpl::tie({1});
}}
)_",
        name, list);
    /*
    out_file << fmt::format(
        R"_(
template <>
constexpr auto structure_to_tpltuple<avnd::{2}_type_refl<{0}>::type>(const avnd::{2}_type_refl<{0}>::type &v) noexcept {{
  return tpl::tie({1});
}}
)_",
        main_class_name, list, name);
        */
  }
  void generate_tie_as_tuple()
  {
    if(!inputs.empty())
      generate_tie_as_tuple_impl(inputs_typename, vec_to_comma_list(inputs, "v."));

    if(!outputs.empty())
      generate_tie_as_tuple_impl(outputs_typename, vec_to_comma_list(outputs, "v."));

    if(!messages.empty())
      generate_tie_as_tuple_impl(messages_typename, vec_to_comma_list(messages, "v."));
  }

  void generate_get(int k, const std::string& accessor, std::string kind)
  {
    // 0: index
    // 1: fully qualified object name e.g. examples::helpers::GradientScrub
    // 2: accessor
    // 3: input / output / message
    auto t = fmt::format(
        R"_(
template <>
constexpr auto& get<{0}, {1}>({1} &&v) {{
  return v.{2};
}}

template <>
constexpr auto& get<{0}, {1}&>({1} &v) {{
  return v.{2};
}}

)_",
        k, kind, accessor);
    out_file_main << t;
  }

  void generate_get()
  {
    {
      int k = 0;
      for(auto& v : ::inputs)
      {
        generate_get(k, v, inputs_typename);
        k++;
      }
    }
    {
      int k = 0;
      for(auto& v : ::outputs)
      {
        generate_get(k, v, outputs_typename);
        k++;
      }
    }
    {
      int k = 0;
      for(auto& v : ::messages)
      {
        generate_get(k, v, messages_typename);
        k++;
      }
    }
  }

  std::string nth_type(std::string kind, std::string v)
  {
    return fmt::format("std::decay_t<decltype(std::declval<{0}&>().{1})>", kind, v);
  }

  // avnd::typelist<avnd::pfr::tuple_element_t<{2}, avnd::{1}_type_refl<{0}>::type>
  std::string vec_to_typelist(const std::vector<std::string>& vec, std::string kind)
  {
    std::string ret;
    if(vec.empty())
      return ret;

    int index = 0;
    for(auto& v : vec)
    {
      ret += nth_type(kind, v);
      // ret += fmt::format(
      //     "avnd::pfr::tuple_element_t<{2}, avnd::{1}_type_refl<{0}>::type>", main_class_name,
      //     kind, index);
      ret += ", ";
      index++;
    }
    ret.resize(ret.size() - 2);
    return ret;
  }

  void generate_tuple_element_impl(
      std::string simple_kind, std::string kind, const std::vector<std::string>& vec)
  {
    int k = 0;
    for(auto& v : vec)
    {
      out_file_main << fmt::format(
          R"_(
template<>
struct tuple_element<{0}, {1}>
{{
  using type = {2};
}};
)_",
          k++, kind, nth_type(kind, v));
    }
  }

  void generate_tuple_element()
  {
    if(!inputs.empty())
      generate_tuple_element_impl("inputs", inputs_typename, inputs);

    if(!outputs.empty())
      generate_tuple_element_impl("outputs", outputs_typename, outputs);

    if(!messages.empty())
      generate_tuple_element_impl("messages", messages_typename, messages);
  }
  void generate_tuple_size_impl(std::string kind, const std::vector<std::string>& vec)
  {
    // 0: class name
    // 1: kind
    // 2: vec size
    // 3: typelist
    // 4: tuple_element

    std::string typelist = vec_to_typelist(vec, kind);
    auto t = fmt::format(
        R"_(
  template <>
  struct pfr_impl_helper<{0}> {{
    using tuple_size = std::integral_constant<std::size_t, {1}>;

    using as_typelist = avnd::typelist<{2}>;

    template<std::size_t N>
    using tuple_element_t = typename tuple_element<N, {0}>::type;
  }};
  )_",
        kind, vec.size(), typelist);
    out_file_main << t;

    out_file_main << fmt::format(
        R"_(
  template <>
  inline constexpr size_t tuple_size_v<{0}> = {1};
)_",
        kind, vec.size());
  }

  std::string make_call_sequence(const std::vector<std::string>& vec)
  {
    std::string r;
    for(auto& elt : vec)
    {
      r += "f(v.";
      r += elt;
      r += ");\n";
    }
    return r;
  }

  std::string make_n_call_sequence(const std::vector<std::string>& vec)
  {
    std::string r;
    int i = 0;
    for(auto& elt : vec)
    {
      r += fmt::format("f(v.{}, avnd::field_index<{}>{{}});\n", elt, i++);
    }
    return r;
  }
  void generate_for_each_field_ref_impl(
      std::string kind_t, const std::vector<std::string>& vec)
  {
    out_file_pre << fmt::format(
        R"_(
    template <class F>
    constexpr void for_each_field_ref(const {0}& v, F&& f)
    {{
       {1}
    }}
    template <class F>
    constexpr void for_each_field_ref({0}& v, F&& f)
    {{
       {1}
    }}
    template <class F>
    constexpr void for_each_field_ref({0}&& v, F&& f)
    {{
       {1}
    }}
  )_",
        kind_t, make_call_sequence(vec));

    out_file_pre << fmt::format(
        R"_(
    template <class F>
    constexpr void for_each_field_ref_n(const {0}& v, F&& f)
    {{
       {1}
    }}
    template <class F>
    constexpr void for_each_field_ref_n({0}& v, F&& f)
    {{
       {1}
    }}
    template <class F>
    constexpr void for_each_field_ref_n({0}&& v, F&& f)
    {{
       {1}
    }}
  )_",
        kind_t, make_n_call_sequence(vec));
  }

  void generate_for_each_field_ref()
  {
    if(!inputs.empty())
      generate_for_each_field_ref_impl(inputs_typename, inputs);

    if(!outputs.empty())
      generate_for_each_field_ref_impl(outputs_typename, outputs);

    if(!messages.empty())
      generate_for_each_field_ref_impl(messages_typename, messages);
  }

  void generate_tuple_size()
  {
    if(!inputs.empty())
      generate_tuple_size_impl(inputs_typename, inputs);

    if(!outputs.empty())
      generate_tuple_size_impl(outputs_typename, outputs);

    if(!messages.empty())
      generate_tuple_size_impl(messages_typename, messages);
  }

  void generate_header()
  {
    constexpr auto templat = R"_(

    template <typename T>
    struct {0}_type_refl
    {{
      using type = dummy;
    }};

    template <{0}_is_type T>
    struct {0}_type_refl<T>
    {{
      using type = typename std::decay_t<T>::{0};
    }};

    template <{0}_is_value T>
    struct {0}_type_refl<T>
    {{
      using type = std::remove_reference_t<decltype(std::declval<std::decay_t<T>>().{0})>;
    }};
    )_";
    out_file_pre << fmt::format(templat, "inputs");
    out_file_pre << fmt::format(templat, "outputs");
    out_file_pre << fmt::format(templat, "messages");
  }

  void generate()
  {
    // Prelude
    out_file_pre << "namespace avnd {\n";
    generate_header();
    out_file_pre << "}\n";

    out_file_pre << "namespace avnd {\n";
    generate_for_each_field_ref();
    out_file_pre << "}\n";

    // Template specializations
    out_file_main << "namespace avnd {\n";
    generate_has_recursive_groups();
    out_file_main << "}\n";

    out_file_main << "namespace avnd::pfr::detail {\n";
    generate_tie_as_tuple();
    out_file_main << "}\n";

    out_file_main << "namespace avnd::pfr {\n";
    out_file_main << "template<std::size_t N, typename T> struct tuple_element;\n";
    generate_get();
    generate_tuple_element();
    generate_tuple_size();
    out_file_main << "}\n";
  }
};

int main(int argc, char* argv[])
{
  main_class_name = argv[1];
  main_file_name = argv[2];
  std::string out_file = argv[3];
  std::string out_file_foreach = out_file;
  out_file_foreach.resize(out_file_foreach.size() - 4);
  out_file_foreach.append(".pre.hpp");

  {
    // look for -- : everything after goes to clang
    int clang_arg_start = -1;
    for(int i = 4; i < argc; i++)
    {
      if(std::string_view{argv[i]} == "--")
      {
        clang_arg_start = i + 1;
        break;
      }
    }
    if(clang_arg_start == -1)
      list_inputs(main_file_name, {});
    else
      list_inputs(main_file_name, std::span<char*>(argv + clang_arg_start, argv + argc));
  }

  generator r{
      .out_file_pre{out_file_foreach},
      .out_file_main{out_file},
      .inputs = ::inputs,
      .outputs = ::outputs,
      .messages = ::messages};
  r.generate();
  sync();
}
