cninja_optional(compiler)

if("clang" STREQUAL "${CNINJA_COMPILER}" OR "gcc" STREQUAL "${CNINJA_COMPILER}")
  string(APPEND CMAKE_CXX_FLAGS_INIT
    " -Wall"
    " -Wextra"
    " -Wno-unused-parameter"
    " -Wno-unknown-pragmas"
    " -Wnon-virtual-dtor"
    " -pedantic"
    " -Woverloaded-virtual"
    " -Wno-missing-declarations"
    " -Wredundant-decls"
    " -Werror=return-type"
    " -Werror=trigraphs"
  )

  if ("clang" STREQUAL "${CNINJA_COMPILER}")
    string(APPEND CMAKE_CXX_FLAGS_INIT
      " -Wno-gnu-string-literal-operator-template"
      " -Wno-missing-braces"
      " -Werror=return-stack-address"
      " -Wmissing-field-initializers"
      " -Wno-gnu-statement-expression"
      " -Wno-four-char-constants"
      # " -Wweak-vtables"
      " -ftemplate-backtrace-limit=0"
    )
  elseif("gcc" STREQUAL "${CNINJA_COMPILER}")
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wno-error=shadow -Wno-error=switch -Wno-error=switch-enum -Wno-error=empty-body -Wno-error=overloaded-virtual -Wno-error=suggest-final-methods -Wno-error=suggest-final-types -Wno-error=suggest-override -Wno-error=maybe-uninitialized")
    string(APPEND CMAKE_CXX_FLAGS_INIT
      " -Wno-div-by-zero"
      " -Wsuggest-final-types"
      " -Wsuggest-final-methods"
      " -Wsuggest-override"
      " -Wpointer-arith"
      " -Wsuggest-attribute=noreturn"
      " -Wno-missing-braces"
      " -Wmissing-field-initializers"
      " -Wformat=2"
      " -Wno-format-nonliteral"
      " -Wpedantic"
      " -Werror=return-local-addr"
    )
    
    # -Wcast-qual is nice but requires more work...
    # -Wzero-as-null-pointer-constant  is garbage
    # Too much clutter :set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wswitch-enum -Wshadow  -Wsuggest-attribute=const  -Wsuggest-attribute=pure ")
  endif()

  if(APPLE)
    string(APPEND CMAKE_CXX_FLAGS_INIT
      " -Wno-auto-var-id"
      " -Wno-availability"
      " -Wno-deprecated-declarations"
      " -Wno-exceptions"
      " -Wno-auto-var-id"
      " -Wno-availability"
      " -Wno-deprecated-declarations"
      " -Wno-exceptions"
      " -Wno-extra-semi"
      " -Wno-gnu-folding-constant"
      " -Wno-gnu-zero-variadic-macro-arguments"
      " -Wno-inconsistent-missing-override"
      " -Wno-infinite-recursion"
      " -Wno-missing-method-return-type"
      " -Wno-non-virtual-dtor"
      " -Wno-nullability-completeness-on-arrays"
      " -Wno-nullability-extension"
      " -Wno-pedantic"
      " -Wno-sign-compare"
      " -Wno-switch"
      " -Wno-unguarded-availability-new"
      " -Wno-unknown-warning-option"
      " -Wno-unused-function"
      " -Wno-unused-local-typedef"
      " -Wno-unused-private-field"
      " -Wno-unused-variable"
      " -Wno-variadic-macros"
      " -Wno-zero-length-array"
    )
  endif()
elseif ("msvc" STREQUAL "${CNINJA_COMPILER}")
  string(APPEND CMAKE_CXX_FLAGS_INIT
    " -wd4180"
    " -wd4224"
    " -wd4068" # pragma mark -
    " -wd4250" # inherits via dominance
    " -wd4251" # DLL stuff
    " -wd4275" # DLL stuff
    " -wd4244" # return : conversion from foo to bar, possible loss of data
    " -wd4800" # conversion from int to bool, performance warning
    " -wd4503" # decorated name length exceeded
  )
endif()
