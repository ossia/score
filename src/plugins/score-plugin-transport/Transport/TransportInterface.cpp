#include <Transport/TransportInterface.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Transport::TransportInterface)
namespace Transport
{

TransportInterface::TransportInterface() { }

TransportInterface::~TransportInterface() { }

void TransportInterface::requestBeginScrub(ossia::time_value t)
{
  transport(t);
}
void TransportInterface::requestScrub(ossia::time_value t)
{
  transport(t);
}
void TransportInterface::requestEndScrub(ossia::time_value t)
{
  transport(t);
}

DirectTransport::DirectTransport() { }

DirectTransport::~DirectTransport() { }

ossia::transport_info_fun DirectTransport::transportUpdateFunction()
{
  return {};
}

void DirectTransport::setup() { }

void DirectTransport::teardown() { }

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

void DirectTransport::requestBeginScrub(ossia::time_value t)
{
  transport(t);
}
void DirectTransport::requestScrub(ossia::time_value t)
{
  transport(t);
}
void DirectTransport::requestEndScrub(ossia::time_value t)
{
  transport(t);
}
}
