#pragma once

#include <Protocols/Settings/Model.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <libremidi/backends.hpp>
#include <libremidi/libremidi.hpp>
namespace Protocols
{
inline libremidi::API getCurrentAPI()
{
  auto api
      = score::AppContext().settings<Protocols::Settings::Model>().getMidiApiAsEnum();
  if(api == libremidi::API::UNSPECIFIED)
    api = libremidi::midi1::default_api();
  return api;
}

}
