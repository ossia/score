set(CMAKE_BUILD_TYPE Debug CACHE INTERNAL "")
set(SCORE_PCH False)
set(DEPLOYMENT_BUILD False)
set(SCORE_COVERAGE False)
set(SCORE_AUDIO_PLUGINS True CACHE INTERNAL "")

set(CMAKE_CXX_CLANG_TIDY "/usr/bin/clang-tidy"
  "-checks=-*,clang-analyzer-*,performance-*"
  # "-checks=*,-readability-*,-google-*,-llvm-header-guard,-cppcoreguidelines-*,-llvm-namespace-comment,-misc-macro-parentheses,-llvm-include-order,-modernize-*"
  "-header-filter=(ossia/*|base/*)")
include(all-plugins)
