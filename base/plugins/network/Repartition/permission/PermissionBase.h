#pragma once
#include "../group/Group.h"
#include "../client/Client.h"

class PermissionBase
{
    public:
        PermissionBase(id_type<Group> groupId, id_type<Client> clientId):
            m_groupId(groupId),
            m_clientId(clientId)
        {
        }

        virtual ~PermissionBase() = default;
        bool operator==(const PermissionBase& p)
        {
            return p.m_groupId == m_groupId && p.m_clientId == m_clientId;
        }

        virtual bool listens() const = 0;
        virtual bool writes()  const = 0;

    private:
        id_type<Group> m_groupId;
        id_type<Client> m_clientId;
};
