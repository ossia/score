#pragma once
#include <Execution/Transport/TransportInterface.hpp>

#include <ossia/audio/jack_protocol.hpp>
namespace Execution
{
#if defined(OSSIA_AUDIO_JACK)
class JackTransport : public TransportInterface
{
  SCORE_CONCRETE("c7fc1a0a-0d81-47d6-9216-6922af4fbc7b")

public:
  JackTransport();
  ~JackTransport();

  ossia::transport_info_fun transportUpdateFunction() override;

  void setup() override;
  void teardown() override;

  void requestPlay() override;
  void requestPause() override;
  void requestStop() override;
  void requestTransport(ossia::time_value t) override;

  std::shared_ptr<ossia::jack_client> m_client;
};
#endif
}
