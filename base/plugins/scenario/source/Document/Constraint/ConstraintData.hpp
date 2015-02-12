#pragma once
#include <tools/SettableIdentifier.hpp>
#include <ProcessInterface/TimeValue.hpp>

class ConstraintModel;

struct ConstraintData {
   id_type<ConstraintModel> id{0};
   int x{0};
   int y{0};
   TimeValue dDate{0s};
   double relativeY{0.0};
};
