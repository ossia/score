#pragma once

#include "ChangeGroup.hpp"
#include "source/Document/Constraint/ConstraintModel.hpp"

class ChangeConstraintGroup : public ChangeGroup<ConstraintModel>
{
    public:
        template<typename... Args>
        ChangeConstraintGroup(Args&&... args):
            ChangeGroup<ConstraintModel>{std::forward<Args>(args), "ChangeConstraintGroup", "ChangeConstraintGroup_desc"}
        {

        }
}
