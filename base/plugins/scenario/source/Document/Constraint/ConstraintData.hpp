#pragma once
#include <tools/SettableIdentifier.hpp>
class ConstraintModel;

struct ConstraintData {
   id_type<ConstraintModel> id{0};
   int x{0};
   int y{0};
   int dDate{0};
   double relativeY{0.0};
};
