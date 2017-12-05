#pragma once
#include <Process/Process.hpp>
#include <Media/Step/Metadata.hpp>
#include <Process/TimeValue.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>
namespace Media
{
namespace Step
{
class Model final : public Process::ProcessModel
{
        SCORE_SERIALIZE_FRIENDS
        PROCESS_METADATA_IMPL(Media::Step::Model)

        Q_OBJECT
        Q_PROPERTY(std::size_t stepCount READ stepCount WRITE setStepCount NOTIFY stepCountChanged)
        Q_PROPERTY(TimeVal stepDuration READ stepDuration WRITE setStepDuration NOTIFY stepDurationChanged)
    public:
        explicit Model(
                const TimeVal& duration,
                const Id<Process::ProcessModel>& id,
                QObject* parent);

        explicit Model(
                const Model& source,
                const Id<Process::ProcessModel>& id,
                QObject* parent);

        ~Model() override;

        template<typename Impl>
        explicit Model(
                Impl& vis,
                QObject* parent) :
            Process::ProcessModel{vis, parent}
        {
            vis.writeTo(*this);
        }


        Process::Inlets inlets() const override
        {
          return {};
        }

        Process::Outlets outlets() const override
        {
          return {outlet.get()};
        }

        std::unique_ptr<Process::Inlet> inlet;
        std::unique_ptr<Process::Outlet> outlet;

        std::size_t stepCount() const { return m_stepCount; }
        TimeVal stepDuration() const { return m_stepDuration; }
        const std::vector<float>& steps() const { return m_steps; }

  signals:
        void stepCountChanged(std::size_t);
        void stepDurationChanged(TimeVal);
        void stepsChanged();

  public slots:
        void setStepCount(std::size_t s)
        {
          if(s != m_stepCount)
          {
            m_stepCount = s;
            m_steps.resize(s);
            emit stepCountChanged(s);
          }
        }

        void setStepDuration(TimeVal s)
        {
          if(s != m_stepDuration)
          {
            m_stepDuration = s;
            emit stepDurationChanged(s);
          }
        }

        void setSteps(std::vector<float> v)
        {
          if(m_steps != v)
          {
            m_steps = std::move(v);
            emit stepsChanged();
          }
        }

  private:
        std::vector<float> m_steps;
        std::size_t m_stepCount{8};
        TimeVal m_stepDuration{TimeVal::fromMsecs(400)};
};
}
}
