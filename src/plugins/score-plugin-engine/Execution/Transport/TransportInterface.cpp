#include "TransportInterface.hpp"

#include <wobjectimpl.h>

W_OBJECT_IMPL(Execution::TransportInterface)
namespace Execution
{

TransportInterface::TransportInterface() { }

TransportInterface::~TransportInterface() { }

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

}
