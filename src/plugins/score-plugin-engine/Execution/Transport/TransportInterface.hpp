#pragma once
#include <score/plugins/Interface.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <ossia-qt/time_value.hpp>
#include <verdigris>

#include <QObject>

#include <score_plugin_engine_export.h>

namespace Execution
{
class SCORE_PLUGIN_ENGINE_EXPORT TransportInterface
    : public QObject
    , public score::InterfaceBase
{
  W_OBJECT(TransportInterface)
  SCORE_INTERFACE(TransportInterface, "2f5845f1-548e-4729-86b7-4ae919b8e7e3")

public:
  explicit TransportInterface();
  virtual ~TransportInterface();

  virtual void setup() = 0;
  virtual void teardown() = 0;

  virtual void requestPlay() = 0;
  virtual void requestPause() = 0;
  virtual void requestStop() = 0;
  virtual void requestTransport(ossia::time_value t) = 0;

  void play()
  E_SIGNAL(SCORE_PLUGIN_ENGINE_EXPORT, play);
  void pause()
  E_SIGNAL(SCORE_PLUGIN_ENGINE_EXPORT, pause);
  void stop()
  E_SIGNAL(SCORE_PLUGIN_ENGINE_EXPORT, stop);
  void transport(ossia::time_value t)
  E_SIGNAL(SCORE_PLUGIN_ENGINE_EXPORT, transport, t);
};

class SCORE_PLUGIN_ENGINE_EXPORT TransportInterfaceList final
    : public score::InterfaceList<TransportInterface>
{
};

class DirectTransport : public TransportInterface
{
  SCORE_CONCRETE("73453569-b453-4cad-b12b-4b71c61cf9a7")

public:
  DirectTransport();
  ~DirectTransport();
  void setup() override;
  void teardown() override;

  void requestPlay() override;
  void requestPause() override;
  void requestStop() override;
  void requestTransport(ossia::time_value t) override;
};
}

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
  void setup() override;
  void teardown() override;

  void requestPlay() override;
  void requestPause() override;
  void requestStop() override;
  void requestTransport(ossia::time_value t) override;

  std::shared_ptr<ossia::jack_client> m_client;
};
}
#endif
