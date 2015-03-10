#pragma once
#include "Session.h"
#include "../client/LocalMaster.h"
#include "ZeroConfServerThread.h"

// Utiliser templates à la place ?
class MasterSession : public Session
{
        ZeroConfServerThread _zc_thread;
    public:
        template<typename... K>
        MasterSession(K&&... args):
            Session(std::forward<K>(args)...),
            _localMaster(new LocalMaster(9000, 0, "master"))
        {
            _zc_thread.start();
            _zc_thread.setPort(_localMaster->localPort());

            OscReceiver::connection_handler h = std::bind(&MasterSession::handle__session_connect,
                                                          this,
                                                          std::placeholders::_1,
                                                          std::placeholders::_2);

            _localMaster->receiver().setConnectionHandler("/session/connect", h);
            _localMaster->receiver().addHandler("/session/permission/update",
                                                &MasterSession::handle__session_permission_update,
                                                this);
            _localMaster->receiver().addHandler("/session/disconnect",
                                                &MasterSession::handle__session_disconnect,
                                                this);

            _localMaster->receiver().run();
        }

        virtual ~MasterSession()
        {
            _zc_thread.quit();
            while(_zc_thread.isRunning()) std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        // TODO mettre dans RemoteTrucBidule
        virtual void sendCommand(const char* parentName,
                                 const char* name,
                                 const char* data, int len) override
        {
            for(RemoteClient& rclt : clients())
            {
                rclt.send("/edit/command",
                          getId(),
                          _localMaster->getId(),
                          parentName,
                          name,
                          osc::Blob{data, len});
            }
        }

        virtual void sendUndoCommand() override
        {
            for(RemoteClient& rclt : clients())
            {
                rclt.send("/edit/undo",
                           getId(),
                           _localMaster->getId());
            }
        }

        virtual void sendRedoCommand() override
        {
            for(RemoteClient& rclt : clients())
            {
                rclt.send("/edit/redo",
                           getId(),
                           _localMaster->getId());
            }
        }

        virtual void sendLock(QByteArray arr) override
        {
            osc::Blob b{arr.constData(), arr.size()};
            for(RemoteClient& rclt : clients())
            {
                rclt.send("/edit/lock",
                           getId(),
                           _localMaster->getId(),
                           b);
            }
        }

        virtual void sendUnlock(QByteArray arr) override
        {
            osc::Blob b{arr.constData(), arr.size()};
            for(RemoteClient& rclt : clients())
            {
                rclt.send("/edit/unlock",
                           getId(),
                           _localMaster->getId(),
                           b);
            }
        }

        virtual LocalClient& getClient() override
        {
            return *_localMaster;
        }

        LocalMaster& getLocalMaster()
        {
            return *_localMaster;
        }

        /** Après ceux-là, informer sur réseau **/
        template<typename... K>
        Group& createGroup(K&&... args)
        {
            return private__createGroup(std::forward<K>(args)...);
        }

        template<typename... K>
        void removeGroup(K&&... args)
        {
            private__removeGroup(std::forward<K>(args)...);
        }


        template<typename... K>
        void muteGroup(K&&... args)
        {
            groups()(std::forward<K>(args)...).mute();
        }

        template<typename... K>
        void unmuteGroup(K&&... args)
        {
            groups()(std::forward<K>(args)...).unmute();
        }

    private:
        void pingAllClients()
        {

        }

        void send__client_ping(RemoteClient& rclt)
        {
            int stamp = std::chrono::milliseconds(std::time(NULL)).count();
            rclt.send("/client/ping",
                      getId(),
                      rclt.getId(),
                      stamp); // Ajouter timestamp

            rclt.setPingStamp(stamp);
        }

        void handle__session_connect(osc::ReceivedMessageArgumentStream args, std::string ip)
        {
            osc::int32 clientListenPort;
            const char* cname;

            args >> cname >> clientListenPort >> osc::EndMessage;
            std::string clientName(cname);
            OscSender clientSender(ip, clientListenPort);

            // Recherche si nom existe déjà
            if(clients().has(clientName))
            {
                clientSender.send(osc::MessageGenerator()("/session/clientNameIsTaken",
                                                          getId(),
                                                          cname));
                return;
            }

            createClient(clientName, std::move(clientSender));
        }

        void handle__session_disconnect(osc::ReceivedMessageArgumentStream args)
        {
            osc::int32 idSession, idClient;
            args >> idSession >> idClient;

            if(idSession != getId()) return;
            if(!clients().has(idClient)) return;
            auto& clt = client(idClient);

            for(RemoteClient& rclt : clients())
            {
                if(clt != rclt)
                {
                    rclt.send("/session/disconnect",
                              getId(),
                              clt.getId());
                }
            }

            removeClient(idClient);
        }

        // /session/permission/update sessionId clientId groupId category enablement
        void handle__session_permission_update(osc::ReceivedMessageArgumentStream args)
        {
            osc::int32 sessionId, clientId, groupId;
            osc::int32 cat;
            bool enablement;

            args >> sessionId >> clientId >> groupId >> cat >> enablement >> osc::EndMessage;

            if(sessionId != getId()) return;

            //TODO checks
            RemoteClient& client = this->client(clientId);
            Group& group = this->group(groupId);
            Permission& perm = localPermissions()(client, group);

            bool prevListens = perm.listens(),
                    prevWrites  = perm.writes();

            perm.setPermission(static_cast<Permission::Category>(cat),
                               static_cast<Permission::Enablement>(enablement));

            bool postListens = perm.listens(),
                    postWrites  = perm.writes();

            // Cas de changements :
            //	- !listens() devient  listens()
            //		=> ceux qui ont W sur group prennent connaissance de moi.
            //	-  listens() devient !listens()
            //		=> ceux qui ont W sur group m'oublient.
            //  - !writes()  devient  writes()
            //		=> je prends connaissance de ceux qui ont R / W / X sur groupe.
            //  -  writes()  devient !writes()
            //		=> j'oublie tout le monde sur ce groupe.
            //
            // Dans tous les cas on informe le maître, c'est lui qui informe les uns et les autres.
            //
            // Faire un RemotePermission avec juste listens() et writes()
            for(RemoteClient& rclt : clients())
            {
                if(rclt == client) continue;

                auto& rclt_perm = localPermissions()(rclt, group);
                if(rclt_perm.writes() && prevListens != postListens)
                {
                    if(!prevListens && postListens)
                        rclt.initConnectionTo(getId(), client);

                    rclt.send("/session/permission/listens",
                              getId(), client.getId(), group.getId(), postListens);
                }

                if(rclt_perm.listens() && !prevWrites && postWrites)
                {
                    client.initConnectionTo(getId(), rclt);
                    client.send("/session/permission/listens",
                                getId(), rclt.getId(), group.getId(), true);
                }
            }
            // Case write() => !write() is done locally by the concerned client.

            //TODO Dump the group data if !listens() -> listens().
        }


        template<typename... K>
        RemoteClient& createClient(K&&... args)
        {
            auto& client = private__createClient(std::forward<K>(args)...);

            // Send the session data
            client.send("/session/setSessionId", getId());
            client.send("/session/setSessionName", getId(), getName().c_str());
            client.send("/session/setMasterName", getId(), _localMaster->getName().c_str());

            // Send the id.
            client.send("/session/setClientId",
                        getId(),
                        client.getId());

            // Send the groups information.
            for(auto& group : groups())
            {
                client.send("/session/update/group",
                            (osc::int32) getId(),
                            (osc::int32) group.getId(),
                            group.getName().c_str(),
                            group.isMuted());
            }

            // Send isReady
            client.send("/session/isReady",
                        (osc::int32) getId());

            // Send information to others clients only when required.
            // (i. e. when acquiring write permission on a group)

            return client;
        }

        //TODO
        template<typename... K>
        void removeClient(K&&... args)
        {
            private__removeClient(std::forward<K>(args)...);
        }

        virtual void createRelatedPermissions(RemoteClient& client) override
        {
            for(Group& group : groups())
            {
                localPermissions().create(group, client);
            }
        }

        virtual void removeRelatedPermissions(RemoteClient &client) override
        {
            for(Group& group : groups())
            {
                localPermissions().remove(group, client);
            }
        }

        virtual void createRelatedPermissions(Group& group) override
        {
            for(RemoteClient& client : clients())
            {
                localPermissions().create(group, client);
            }
        }

        virtual void removeRelatedPermissions(Group& group) override
        {
            for(RemoteClient& client : clients())
            {
                localPermissions().remove(group, client);
            }
        }


        std::unique_ptr<LocalMaster> _localMaster{nullptr};
};
