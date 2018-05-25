#pragma once
#include <Media/Input/InputMetadata.hpp>
#include <wobjectdefs.h>
#include <Process/Process.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score_plugin_media_export.h>
#include <vector>
namespace Media
{
namespace Input
{
class SCORE_PLUGIN_MEDIA_EXPORT ProcessModel final
    : public Process::ProcessModel
    , public Nano::Observer
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Media::Input::ProcessModel)
  W_OBJECT(ProcessModel)

public:
  explicit ProcessModel(
      const TimeVal& duration,
      const Id<Process::ProcessModel>& id,
      QObject* parent);

  ~ProcessModel() override;

  template <typename Impl>
  explicit ProcessModel(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
    init();
  }

  void init()
  {
    m_outlets.push_back(outlet.get());
  }

  std::unique_ptr<Process::Outlet> outlet;

  int startChannel() const;
  int numChannel() const;

public:
  void setStartChannel(int startChannel); W_SLOT(setStartChannel);
  void setNumChannel(int numChannel); W_SLOT(setNumChannel);

public:
  void inputsChanged() W_SIGNAL(inputsChanged);
  void startChannelChanged(int startChannel) W_SIGNAL(startChannelChanged, startChannel);
  void numChannelChanged(int numChannel) W_SIGNAL(numChannelChanged, numChannel);

  PROPERTY(int, numChannel READ numChannel WRITE setNumChannel NOTIFY numChannelChanged)
  PROPERTY(int, startChannel READ startChannel WRITE setStartChannel NOTIFY startChannelChanged)

private:
  int m_startChannel{};
  int m_numChannel{};
};
}
}
