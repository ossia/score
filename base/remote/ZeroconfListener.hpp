#pragma once
#include <servus/servus.h>
#include <QTimer>
#include <RemoteContext.hpp>

namespace RemoteUI
{
//! Periodically checks for new OSCQuery devices on the network
class ZeroconfListener final
    : public servus::Listener
{
public:
  ZeroconfListener(Context& ctx);
  ~ZeroconfListener() override;

private:
  void instanceAdded(const std::string& instance) final override;
  void instanceRemoved(const std::string& instance) final override;

  Context& context;
  servus::Servus service;
  QTimer timer;
};
}
