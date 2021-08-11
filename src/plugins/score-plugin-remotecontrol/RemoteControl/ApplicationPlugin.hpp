#pragma once
#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <RemoteControl/Http_server.hpp>
namespace RemoteControl
{
class ApplicationPlugin final : public score::GUIApplicationPlugin
{
public:
  ApplicationPlugin(const score::GUIApplicationContext& app);

protected:
  void on_createdDocument(score::Document& doc) override;
  Http_server m_server;
};
}
