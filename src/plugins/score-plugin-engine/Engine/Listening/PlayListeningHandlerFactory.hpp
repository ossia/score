#pragma once

#include <Explorer/Listening/ListeningHandlerFactory.hpp>

namespace Execution
{
class PlayListeningHandlerFactory final : public Explorer::ListeningHandlerFactory
{
  SCORE_CONCRETE("5332e60c-2e29-490a-a12e-c289c5262c57")
public:
  std::unique_ptr<Explorer::ListeningHandler>
  make(const Explorer::DeviceDocumentPlugin& plug, const score::DocumentContext& ctx) override;
};
}
