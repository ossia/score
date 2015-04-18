#pragma once

#include "ChangeGroup.hpp"
#include "source/Document/Event/EventModel.hpp"

class ChangeEventGroup : public ChangeGroup<EventModel>
{
    public:
        template<typename... Args>
        ChangeEventGroup(Args&&... args):
            ChangeGroup<EventModel>{std::forward<Args>(args), "ChangeEventGroup", "ChangeEventGroup_desc"}
        {

        }
}
