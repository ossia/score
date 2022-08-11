#pragma once

#include <Explorer/Listening/ListeningHandlerFactory.hpp>

namespace Explorer
{
class DefaultListeningHandlerFactory final : public ListeningHandlerFactory
{
  SCORE_CONCRETE("2e7498f9-ac41-4844-8f73-65e09669b582")
public:
  std::unique_ptr<Explorer::ListeningHandler>
  make(const DeviceDocumentPlugin& plug, const score::DocumentContext& ctx) override;
};
}
