#pragma once
#include <score/plugins/application/GUIApplicationPlugin.hpp>

namespace RemoteControl
{
class ApplicationPlugin final : public score::ApplicationPlugin
{
public:
  ApplicationPlugin(const score::ApplicationContext& app);

protected:
  void on_createdDocument(score::Document& doc) override;
};
}
