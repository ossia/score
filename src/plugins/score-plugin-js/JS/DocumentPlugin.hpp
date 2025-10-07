#pragma once

#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPluginCreator.hpp>
#include <score/tools/Metadata.hpp>

namespace JS
{
class DocumentPlugin;
}

UUID_METADATA(
    , score::DocumentPluginFactory, JS::DocumentPlugin,
    "05e72689-e02c-4c9d-a0bf-fe84c32d3d96")

namespace JS
{
class DocumentPlugin final : public score::SerializableDocumentPlugin
{
  W_OBJECT(DocumentPlugin)
  SCORE_SERIALIZE_FRIENDS

  MODEL_METADATA_IMPL_HPP(DocumentPlugin)

public:
  explicit DocumentPlugin(const score::DocumentContext& ctx, QObject* parent);
  virtual ~DocumentPlugin();
  template <typename Impl>
  DocumentPlugin(const score::DocumentContext& ctx, Impl& vis, QObject* parent)
      : score::SerializableDocumentPlugin{ctx, vis, parent}
  {
    vis.writeTo(*this);
  }
  QString data;
  W_PROPERTY(QString, data MEMBER data)
};

using DocumentPluginFactory = score::DocumentPluginFactory_T<DocumentPlugin>;
}
