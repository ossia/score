set(CMAKE_BUILD_TYPE Debug)
set(ISCORE_COTIRE False)
set(DEPLOYMENT_BUILD False)
set(ISCORE_COVERAGE False)

set(CMAKE_CXX_CLANG_TIDY "/usr/bin/clang-tidy"
  "-checks=-*,clang-analyzer-*,performance-*"
  # "-checks=*,-readability-*,-google-*,-llvm-header-guard,-cppcoreguidelines-*,-llvm-namespace-comment,-misc-macro-parentheses,-llvm-include-order,-modernize-*"
  "-header-filter=(ossia/*|base/*)")
include(all-plugins)
