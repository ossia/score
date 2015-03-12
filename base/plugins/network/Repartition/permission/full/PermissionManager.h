#pragma once
#include <vector>
#include "Permission.h"

#include "../../Iterable.h"
class PermissionManager : public Iterable<Permission>
{
    public:
        /*
        void changePermission(Client& client,
                              Group& group,
                              Permission::Category cat,
                              Permission::Enablement enablement)
        {
            (*this)(group, client).setPermission(cat, enablement);
        }

        bool getPermission(Client& client,
                           Group& group,
                           Permission::Category cat)
        {
            return (*this)(group, client).getPermission(cat);
        }
        */
};

