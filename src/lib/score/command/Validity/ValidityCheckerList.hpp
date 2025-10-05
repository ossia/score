#pragma once
#include <score/command/Validity/ValidityChecker.hpp>
#include <score/plugins/InterfaceList.hpp>

#include <score_lib_base_export.h>

namespace score
{
class SCORE_LIB_BASE_EXPORT ValidityCheckerList final
    : public InterfaceList<score::ValidityChecker>
{
public:
  ~ValidityCheckerList();
  DocumentValidator make(const score::Document& ctx);
};
}
