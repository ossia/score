include(CheckCXXSourceCompiles)
include(CMakePushCheckState)

function(score_faust_detect_float_type output)
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_INCLUDES "${FAUST_INCLUDE_DIR}")
  set(CMAKE_REQUIRED_LIBRARIES ${FAUST_LIBRARIES})
  if(WIN32)
    list(APPEND CMAKE_REQUIRED_LIBRARIES ws2_32)
  endif()

  # A static-library try_compile would not resolve compute's symbol. This is a
  # link check, not a try_run, so it also works when cross-compiling.
  set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)
  foreach(float_type float double)
    string(TOUPPER "${float_type}" float_type_upper)
    set(result SCORE_FAUST_LINKS_${float_type_upper})
    # The selected library can be replaced or rebuilt at the same path.
    unset(${result} CACHE)
    unset(${result})
    set(source "
      #undef FAUSTFLOAT
      #define FAUSTFLOAT ${float_type}
      #include <faust/dsp/llvm-dsp.h>
      int main()
      {
        std::string error;
        const char* options[] = {sizeof(FAUSTFLOAT) == 4 ? \"-single\" : \"-double\"};
        auto* factory = createDSPFactoryFromString(
            \"score-precision\", \"process = _;\", 1, options, \"\", error, -1);
        if(!factory)
          return 1;
        auto* instance = factory->createDSPInstance();
        if(!instance)
        {
          deleteDSPFactory(factory);
          return 1;
        }
        instance->init(48000);
        FAUSTFLOAT input = 1, output = 0;
        FAUSTFLOAT* inputs[] = {&input};
        FAUSTFLOAT* outputs[] = {&output};
        // Qualify the virtual call: taking its address or calling via dsp's
        // vtable would not require the precision-specific exported symbol.
        instance->llvm_dsp::compute(1, inputs, outputs);
        delete instance;
        deleteDSPFactory(factory);
        return output == 1 ? 0 : 1;
      }")
    check_cxx_source_compiles("${source}" ${result})
  endforeach()
  cmake_pop_check_state()

  # Keep the client and libfaust on the same FAUSTFLOAT, including older Faust
  # releases with macro-dependent UI / JSON decoder symbol collisions.
  if(SCORE_FAUST_LINKS_FLOAT AND NOT SCORE_FAUST_LINKS_DOUBLE)
    set(${output} float PARENT_SCOPE)
  elseif(SCORE_FAUST_LINKS_DOUBLE AND NOT SCORE_FAUST_LINKS_FLOAT)
    set(${output} double PARENT_SCOPE)
  else()
    message(FATAL_ERROR
      "Cannot determine a unique libfaust sample type. Exactly one of "
      "llvm_dsp::compute(int, float**, float**) and "
      "llvm_dsp::compute(int, double**, double**) must link. "
      "Check FAUST_INCLUDE_DIR, FAUST_LIBRARY and libfaust's LLVM dependencies.")
  endif()
endfunction()
