#pragma once
#include <QString>
#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>

namespace score
{

class Document;
} // namespace score
struct VisitorVariant;

namespace Explorer
{
class ApplicationPlugin final : public score::GUIApplicationPlugin
{
public:
  ApplicationPlugin(const score::GUIApplicationContext& app);

protected:
  void on_newDocument(score::Document& doc) override;
  void on_documentChanged(
      score::Document* olddoc, score::Document* newdoc) override;
};
}
