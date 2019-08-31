#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>
#include <score/model/ObjectRemover.hpp>

#include <Nodal/Metadata.hpp>

namespace Nodal
{
class Node final
    : public score::Entity<Node>
{
  W_OBJECT(Node)
  SCORE_SERIALIZE_FRIENDS

public:
  Node(
      std::unique_ptr<Process::ProcessModel> proc,
      const Id<Node>& id,
      QObject* parent);

  struct no_ownership { };
  Node(
      no_ownership,
      Process::ProcessModel& proc,
      const Id<Node>& id,
      QObject* parent);

  Node(DataStreamWriter& vis, QObject* parent)
    : score::Entity<Node>{vis, parent}
  {
    vis.writeTo(*this);
  }
  Node(DataStreamWriter&& vis, QObject* parent)
    : score::Entity<Node>{vis, parent}
  {
    vis.writeTo(*this);
  }
  Node(JSONObjectWriter& vis, QObject* parent)
    : score::Entity<Node>{vis, parent}
  {
    vis.writeTo(*this);
  }
  Node(JSONObjectWriter&& vis, QObject* parent)
    : score::Entity<Node>{vis, parent}
  {
    vis.writeTo(*this);
  }

  ~Node() override;

  QPointF position() const noexcept;
  QSizeF size() const noexcept;
  Process::ProcessModel& process() const noexcept;

  void setPosition(const QPointF& v);
  void setSize(const QSizeF& v);

  void release();

  void positionChanged(QPointF p) W_SIGNAL(positionChanged, p);
  void sizeChanged(QSizeF p) W_SIGNAL(sizeChanged, p);
  void processChanged(Process::ProcessModel* p) W_SIGNAL(processChanged, p);

  PROPERTY(QPointF, position READ position WRITE setPosition NOTIFY positionChanged)
  PROPERTY(QSizeF, size READ size WRITE setSize NOTIFY sizeChanged)
private:
  std::unique_ptr<Process::ProcessModel> m_impl;
  QPointF m_position{};
  QSizeF m_size{};
};

class Model final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Nodal::Model)
  W_OBJECT(Model)

public:
  std::unique_ptr<Process::Inlet> inlet;
  std::unique_ptr<Process::Outlet> outlet;

  Model(
      const TimeVal& duration, const Id<Process::ProcessModel>& id,
      QObject* parent);

  template <typename Impl>
  Model(Impl& vis, QObject* parent) : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
    init();
  }

  void init()
  {
    m_inlets.push_back(inlet.get());
    m_outlets.push_back(outlet.get());
  }

  ~Model() override;

  score::EntityMap<Node> nodes;

  void resetExecution()
  W_SIGNAL(resetExecution)

private:
  QString prettyName() const noexcept override;

  void setDurationAndScale(const TimeVal& newDuration) noexcept override;
  void setDurationAndGrow(const TimeVal& newDuration) noexcept override;
  void setDurationAndShrink(const TimeVal& newDuration) noexcept override;

  void startExecution() override;
  void stopExecution() override;
  void reset() override;
};

using ProcessFactory = Process::ProcessFactory_T<Nodal::Model>;

class NodeRemover : public score::ObjectRemover
{
  SCORE_CONCRETE("5e1c7e92-5beb-4313-92c8-f690089ff340")
  bool remove(const Selection& s, const score::DocumentContext& ctx) override;
};
}

W_REGISTER_ARGTYPE(Process::ProcessModel*)
