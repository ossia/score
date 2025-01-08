#pragma once

#include <Protocols/Settings/Model.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <libremidi/api.hpp>
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
