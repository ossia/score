#pragma once
#include <score/model/Entity.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <score_plugin_cliplauncher_export.h>

#include <verdigris>
namespace ClipLauncher
{

class SceneModel final : public score::Entity<SceneModel>
{
  W_OBJECT(SceneModel)
  SCORE_SERIALIZE_FRIENDS

public:
  SceneModel(const Id<SceneModel>& id, QObject* parent);

  template <typename Impl>
  SceneModel(Impl& vis, QObject* parent)
      : score::Entity<SceneModel>{vis, parent}
  {
    vis.writeTo(*this);
  }

  ~SceneModel() override;

  QString name() const noexcept { return m_name; }
  void setName(const QString& n);

  void nameChanged(const QString& n)
      E_SIGNAL(SCORE_PLUGIN_CLIPLAUNCHER_EXPORT, nameChanged, n)

private:
  QString m_name;
};

} // namespace ClipLauncher
