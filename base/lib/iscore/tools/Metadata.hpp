#pragma once

#include <iscore/tools/std/ConstexprString.hpp>

#if !defined(USE_CONSTEXPR_STRING)
#define ISCORE_METADATA(TheClass) public: static const std::string className; private:
#define ISCORE_METADATA_IMPL(TheClass) const std::string TheClass::className{ #TheClass };
#else
#define ISCORE_METADATA(TheClass) public: static constexpr auto className = #TheClass##_S; private:
#define ISCORE_METADATA_IMPL(TheClass) template<> constexpr const char decltype(#TheClass##_S) :: _data[]; constexpr decltype(#TheClass##_S) TheClass :: className;
#endif
