#pragma once
#include "Session.h"
#include "../client/LocalClient.h"
#include "../permission/view/PermissionViewManager.h"

#include <QDebug>
class ClientSession : public Session
{
        friend class ClientSessionBuilder;
    public:
        ClientSession(std::string name,
                      RemoteMaster* rm,
                      LocalClient* lc):
            Session(name, -1),
            _remoteMaster(rm),
            _localClient(lc)

        {
            getLocalClient().receiver().addHandler("/session/permission/listens",
                                                   &ClientSession::handle__session_permission_listens,
                                                   this);

            getLocalClient().receiver().addHandler("/connect/discover",
                                                   &ClientSession::handle__connect_discover,
                                                   this);

            getLocalClient().receiver().addHandler("/session/disconnect",
                                                   &ClientSession::handle__session_disconnect,
                                                   this);
        }

        virtual ~ClientSession()
        {
            disconnect();
        }

        ClientSession(ClientSession&&) = default;

        void disconnect()
        {
            if(_remoteMaster)
                _remoteMaster->send("/session/disconnect",
                                    getId(),
                                    _localClient->getId());
        }

        virtual LocalClient& getClient() override
        {
            return *_localClient;
        }

        LocalClient& getLocalClient()
        {
            return *_localClient;
        }

        RemoteMaster& getRemoteMaster()
        {
            return *_remoteMaster;
        }

        virtual void sendCommand(const char* parentName,
                                 const char* name,
                                 const char * data, int len) override
        {
            _remoteMaster->send("/edit/command",
                                getId(),
                                _localClient->getId(),
                                parentName,
                                name,
                                osc::Blob{data, len});
        }

        virtual void sendUndoCommand() override
        {
            _remoteMaster->send("/edit/undo",
                                getId(),
                                _localClient->getId());
        }

        virtual void sendRedoCommand() override
        {
            _remoteMaster->send("/edit/redo",
                                getId(),
                                _localClient->getId());
        }

        virtual void sendLock(QByteArray arr) override
        {
            osc::Blob b{arr.constData(), arr.size()};
            _remoteMaster->send("/edit/lock",
                                getId(),
                                _localClient->getId(),
                                b);
        }

        virtual void sendUnlock(QByteArray arr) override
        {
            osc::Blob b{arr.constData(), arr.size()};
            _remoteMaster->send("/edit/unlock",
                                getId(),
                                _localClient->getId(),
                                b);
        }


        void changePermission(Client& client,
                              Group& group,
                              Permission::Category cat,
                              Permission::Enablement enablement)
        {
            auto& perm = localPermissions()(client, group);

            bool preWrite = perm.writes();
            perm.setPermission(cat, enablement);
            bool postWrite = perm.writes();

            // /session/permission/update cat enablement
            _remoteMaster->send("/session/permission/update",
                                getId(),
                                client.getId(),
                                group.getId(),
                                static_cast<std::underlying_type<Permission::Category>::type>(cat),
                                static_cast<std::underlying_type<Permission::Enablement>::type>(enablement));

            if(preWrite && !postWrite)
            {
                // Remove all the known clients which are on the same group and not in other groups
                auto rclt_permSet = _remotePermissions.getFamily(group);
                for(auto& rclt_perm : rclt_permSet)
                {
                    auto& rclt = rclt_perm.get().getClient();
                    // If this client doesn't listen anywhere else...
                    for(auto& grp : groups())
                    {
                        if(grp == group) continue;
                        if(!_remotePermissions(grp, rclt).listens())
                        {
                            private__removeClient(rclt.getId()); // TODO by reference passing
                        }
                    }
                }
            }
        }


    private:
        // /session/permission/listens
        void handle__session_permission_listens(osc::ReceivedMessageArgumentStream args)
        {
            osc::int32 sessionId, clientId, groupId;
            bool enablement;

            args >> sessionId >> clientId >> groupId >> enablement >> osc::EndMessage;

            if(sessionId != getId()) return;

            RemoteClient& rclt = client(clientId); // test
            Group& grp = group(groupId);

            if(enablement)
                _remotePermissions.startsListening(rclt, grp);
            else
                _remotePermissions.stopsListening(rclt, grp);
        }

        void handle__session_disconnect(osc::ReceivedMessageArgumentStream args)
        {
            osc::int32 idSession, idClient;
            args >> idSession >> idClient;

            if(idSession != getId()) return;
            if(!clients().has(idClient)) return;

            private__removeClient(idClient);
        }

        // /connect/discover
        void handle__connect_discover(osc::ReceivedMessageArgumentStream args)
        { // nom id ip port
            osc::int32 sessionId;
            const char* name;
            osc::int32 id;
            const char* ip;
            osc::int32 port;

            args >> sessionId >> name >> id >> ip >> port >> osc::EndMessage;

            if(sessionId != getId()) return;

            private__createClient(id, std::string(name), std::string(ip), port);

        }

        // Creation of the permissions.
        virtual void createRelatedPermissions(RemoteClient& client) override
        {
            for(Group& group : groups())
            {
                _remotePermissions.create(group, client);
            }
        }

        virtual void createRelatedPermissions(Group& group) override
        {
            localPermissions().create(group, *_localClient.get());

            for(RemoteClient& client : clients())
            {
                _remotePermissions.create(group, client);
            }
        }

        virtual void removeRelatedPermissions(RemoteClient& client) override
        {
            for(Group& group : groups())
            {
                _remotePermissions.remove(group, client);
            }
        }

        virtual void removeRelatedPermissions(Group& group) override
        {
            for(RemoteClient& client : clients())
            {
                _remotePermissions.remove(group, client);
            }

            localPermissions().remove(group, *_localClient.get());
        }

        std::unique_ptr<RemoteMaster> _remoteMaster{nullptr};
        std::unique_ptr<LocalClient> _localClient{nullptr};

        // Holds the known permissions of other clients, so that
        // we know when they are no more useful
        PermissionViewManager _remotePermissions;
};

