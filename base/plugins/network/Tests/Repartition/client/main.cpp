#include <iostream>
#include <session/ClientSessionBuilder.h>
#include <QCoreApplication>
// Client sans Zeroconf

class Test
{
    public:
        Test()
        {
            auto vec = ClientSessionBuilder::list();
            std::cerr << vec.size();

            ClientSessionBuilder builder(vec[0].remote_ip, vec[0].remote_port, "JeanMi", 7888);
            builder.join();
            sleep(2);
            auto session = builder.getBuiltSession();

            auto& g = *begin(session->groups());
            session->changePermission(session->getLocalClient(),
                                      g,
                                      Permission::Category::Write,
                                      Permission::Enablement::Enabled);
        }
};

using namespace std;
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    Test t;
    std::cerr << "Test ok" << std::endl;

    app.quit();
    return 0;

    //////////////////////////////////////////////////////
    /*
    	ClientSessionBuilder sessionBuilder(5678,
    										"Session Maitre",
    										"master",
    										"127.0.0.1",
    										9000,
    										"DatClient",
    										8900);

    	sessionBuilder.join();

    	while(!sessionBuilder.isReady())
    		std::this_thread::sleep_for(std::chrono::seconds(1));

    	std::unique_ptr<ClientSession> session{sessionBuilder.getBuiltSession()};
    	auto& g = *begin(session->groups());
    	session->changePermission(session->getLocalClient(),
    							 g,
    							 Permission::Category::Write,
    							 Permission::Enablement::Enabled);

    	std::this_thread::sleep_for(std::chrono::seconds(1));

    	///////////////////////////////////////////////////////

    	ClientSessionBuilder sessionBuilder2(5678,
    										"Session Maitre",
    										"master",
    										"127.0.0.1",
    										9000,
    										"OtherClient",
    										 8901);

    	sessionBuilder2.join();

    	while(!sessionBuilder2.isReady())
    		std::this_thread::sleep_for(std::chrono::seconds(1));

    	std::unique_ptr<ClientSession> session2{sessionBuilder2.getBuiltSession()};
    	auto& g2 = *begin(session2->groups());
    	session2->changePermission(session2->getLocalClient(),
    							 g2,
    							 Permission::Category::Read,
    							 Permission::Enablement::Enabled);

    	std::this_thread::sleep_for(std::chrono::seconds(2));
    	session2.reset();
    	std::this_thread::sleep_for(std::chrono::seconds(2));

    	return 0;
    */
}
