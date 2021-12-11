#pragma once
#include <YSFX/ProcessMetadata.hpp>
#include <Process/Process.hpp>

#include <score_plugin_ysfx_export.h>

#include <memory>
#include <verdigris>

#include <ysfx.h>

namespace YSFX
{
class Script;
class ProcessModel;
class SCORE_PLUGIN_YSFX_EXPORT ProcessModel final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(YSFX::ProcessModel)
  W_OBJECT(ProcessModel)
public:
  static constexpr bool hasExternalUI() noexcept { return false; }

  explicit ProcessModel(
      const TimeVal& duration,
      const QString& data,
      const Id<Process::ProcessModel>& id,
      QObject* parent);

  template <typename Impl>
  explicit ProcessModel(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  YSFX::Script* currentObject() const noexcept;
  ~ProcessModel() override;

  void setScript(const QString& path);
  QString script() const noexcept;


  std::shared_ptr<ysfx_t> fx;

private:
  QString m_script;
  bool m_isFile{};
};
}
