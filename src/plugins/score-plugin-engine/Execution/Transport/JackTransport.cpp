#include "JackTransport.hpp"

#include <score/application/ApplicationContext.hpp>
#include <Audio/JackInterface.hpp>
#include <Audio/Settings/Model.hpp>
#include <weak_libjack.h>
namespace Execution
{

#if defined(OSSIA_AUDIO_JACK)
JackTransport::JackTransport()
{

}

JackTransport::~JackTransport()
{

}

ossia::transport_info_fun JackTransport::transportUpdateFunction()
{
  auto& ctx = score::AppContext();
  auto& clt = ctx.settings<Audio::Settings::Model>();
  if(auto jack_iface = (Audio::JackFactory*) ctx.interfaces<Audio::AudioFactoryList>().get(clt.getDriver()))
  {
    // Only called during the execution, by buffer_tick, etc.
    // jack_iface->currentTransportInfo is only called during the execution by the jack thread,
    // before process()
    // so there is no need to have locks
    return [&info=jack_iface->currentTransportInfo] (const ossia::tick_transport_info& tick) {
      info = tick;
    };
  }
  else
  {
    return {};
  }
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
  if(m_client && m_client->client)
  {
    auto rate = jack_get_sample_rate(m_client->client);
    qDebug() << ossia::to_sample(t, rate);
    jack_transport_locate(m_client->client, ossia::to_sample(t, rate));
  }
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
