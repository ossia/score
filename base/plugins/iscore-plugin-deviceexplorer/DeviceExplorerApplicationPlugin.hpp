#pragma once
#include <QString>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>

#include <iscore/plugins/documentdelegate/plugin/DocumentPlugin.hpp>

namespace iscore
{

class Document;
} // namespace iscore
struct VisitorVariant;

namespace Explorer
{
class ApplicationPlugin final : public iscore::GUIApplicationContextPlugin
{
public:
  ApplicationPlugin(const iscore::GUIApplicationContext& app);

protected:
  void on_newDocument(iscore::Document& doc) override;
  void on_documentChanged(
      iscore::Document* olddoc, iscore::Document* newdoc) override;
};
}
