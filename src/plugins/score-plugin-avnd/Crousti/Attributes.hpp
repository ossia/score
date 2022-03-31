#pragma once
#include <halp/controls.hpp>
/*
#include <Process/ProcessMetadata.hpp>
#include <ossia/detail/timed_vec.hpp>
#include <ossia/dataflow/exec_state_facade.hpp>

#include <array>
*/

#define meta_attribute_uuid(name, value) static consteval auto uuid() { return value; }
#define meta_attribute_name(unused, value) static consteval const char* name() { return value; }
#define meta_attribute_pretty_name(unused, value) static consteval const char* name() { return value; }
#define meta_attribute_script_name(name, value) static consteval const char* script_name() { { int value; (void) value; } return #value; }
#define meta_attribute_category(name, value) static consteval const char* category() { return #value; }
#define meta_attribute_author(name, value) static consteval const char* author() { return value; }
#define meta_attribute_kind(name, value) static consteval auto kind() { return Process::ProcessCategory::value; }
#define meta_attribute_description(name, value) static consteval const char* description() { return value; }
#define meta_attribute_event(name, value) static consteval bool is_event() { return value; }
#define meta_attribute_mimic_channels(name, value) static consteval auto mimic_channels() -> decltype( &decltype(inputs)::value ) { \
  static_assert(requires { &decltype(decltype(inputs)::value)::channels; }); \
  return &decltype(inputs)::value; \
}
#define meta_attribute_want_channels(name, value) static constexpr auto want_channels() { return value; }

#define meta_attribute(name, value) meta_attribute_ ## name(name, value)
//#define meta_control(type, ...) static constexpr auto control() -> decltype(type{__VA_ARGS__}){ return type{__VA_ARGS__}; }
//#define avnd_control(type, name, ...) type__VA_ARGS__}; }
#define meta_display(type, ...) static constexpr auto display() -> decltype(type{__VA_ARGS__}){ return type{__VA_ARGS__}; }

#define meta_attribute_widget(T) enum widget { T }
#define meta_attribute_range(Min, Max, Init) \
static consteval auto range() \
{ \
  struct \
  { \
    const decltype(Min) min = Min; \
    const decltype(Max) max = Max; \
    const decltype(Init) init = Init; \
  } c; \
  return c; \
}


#define OSCR_NUM_ARGS_(_10, _9, _8, _7, _6, _5, _4, _3, _2, _1, N, ...) N
#define OSCR_NUM_ARGS(...) OSCR_NUM_ARGS_(__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define OSCR_FOREACH(MACRO, ...) OSCR_FOREACH_(OSCR_NUM_ARGS(__VA_ARGS__), MACRO, __VA_ARGS__)
#define OSCR_FOREACH_(N, M, ...) OSCR_FOREACH__(N, M, __VA_ARGS__)
#define OSCR_FOREACH__(N, M, ...) OSCR_FOREACH_##N(M, __VA_ARGS__)
#define OSCR_FOREACH_1(M, A) M(A)
#define OSCR_FOREACH_2(M, A, ...) M(A)  OSCR_FOREACH_1(M, __VA_ARGS__)
#define OSCR_FOREACH_3(M, A, ...) M(A) OSCR_FOREACH_2(M, __VA_ARGS__)
#define OSCR_FOREACH_4(M, A, ...) M(A) OSCR_FOREACH_3(M, __VA_ARGS__)
#define OSCR_FOREACH_5(M, A, ...) M(A) OSCR_FOREACH_4(M, __VA_ARGS__)
#define OSCR_FOREACH_6(M, A, ...) M(A) OSCR_FOREACH_5(M, __VA_ARGS__)
#define OSCR_FOREACH_7(M, A, ...) M(A) OSCR_FOREACH_6(M, __VA_ARGS__)
#define OSCR_FOREACH_8(M, A, ...) M(A) OSCR_FOREACH_7(M, __VA_ARGS__)
#define OSCR_STRINGIFY_(X) #X
#define OSCR_STRINGIFY(X) OSCR_STRINGIFY_(X)
#define OSCR_STRINGIFY_ALL(...) OSCR_FOREACH(OSCR_STRINGIFY, __VA_ARGS__)

#define COMMA(X) X,
#define COMMA_OSCR_STRINGIFY(X) COMMA(OSCR_STRINGIFY(X))

#define STRING_LITERAL_ARRAY(...)  OSCR_FOREACH(COMMA_OSCR_STRINGIFY, __VA_ARGS__)

#define meta_enum(name, default_v, ...) \
  enum enum_type { __VA_ARGS__ }; \
  static const constexpr std::array<const char*, OSCR_NUM_ARGS(__VA_ARGS__)> choices() { \
    return { STRING_LITERAL_ARRAY(__VA_ARGS__) }; \
  } /* \
  meta_control(Control::Enum, name, default_v, choices()) */

