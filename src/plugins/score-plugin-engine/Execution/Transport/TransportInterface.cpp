#include "TransportInterface.hpp"
#include <Audio/Settings/Model.hpp>
#include <Audio/JackInterface.hpp>
#include <score/application/ApplicationContext.hpp>
#include <weak_libjack.h>
W_OBJECT_IMPL(Execution::TransportInterface)
namespace Execution
{

TransportInterface::TransportInterface()
{

}

TransportInterface::~TransportInterface()
{

}

DirectTransport::DirectTransport()
{

}

DirectTransport::~DirectTransport()
{

}

void DirectTransport::setup()
{
}

void DirectTransport::teardown()
{
}

void DirectTransport::requestPlay()
{
  play();
}

void DirectTransport::requestPause()
{
  pause();
}

void DirectTransport::requestTransport(ossia::time_value t)
{
  transport(t);
}

void DirectTransport::requestStop()
{
  stop();
}

#if defined(OSSIA_AUDIO_JACK)
JackTransport::JackTransport()
{

}

JackTransport::~JackTransport()
{

}

void JackTransport::setup()
{
  auto& ctx = score::AppContext();
  auto& clt = ctx.settings<Audio::Settings::Model>();
  if(clt.getDriver() == Audio::JackFactory::static_concreteKey())
  {
    auto jack_iface = (Audio::JackFactory*) ctx.interfaces<Audio::AudioFactoryList>().get(clt.getDriver());
    this->m_client = jack_iface->acquireClient();

    connect(jack_iface, &Audio::JackFactory::transportStateChanged,
            this, [this] (ossia::transport_status st) {
      switch(st)
      {
        case ossia::transport_status::stopped:
          stop();
          break;
        case ossia::transport_status::starting:
          play();
          break;
      }
    }, Qt::QueuedConnection);
  }
}

void JackTransport::teardown()
{
  this->m_client.reset();
}

void JackTransport::requestPlay()
{
  if(m_client && m_client->client)
  {
    jack_transport_start(m_client->client);
  }
  else
  {
    play();
  }
}

void JackTransport::requestPause()
{
  if(m_client && m_client->client)
  {
    jack_transport_stop(m_client->client);
  }
  else
  {
    pause();
  }
}

void JackTransport::requestTransport(ossia::time_value t)
{
  SCORE_TODO_("Implement transport request in frames for jack, needs access to modelToSamples");
}

void JackTransport::requestStop()
{
  if(m_client && m_client->client)
  {
    jack_transport_stop(m_client->client);
    jack_transport_locate(m_client->client, 0);
  }
  else
  {
    stop();
  }
}
#endif

}
