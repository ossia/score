#pragma once
#include <memory>
#include <Space/computation.hpp>

#include <iscore/tools/IdentifiedObject.hpp>

class SpaceModel;
// Maps addresses / values to the parameter of an Computation
class ComputationModel : public IdentifiedObject<ComputationModel>
{
        Q_OBJECT
    public:
        using Computation = std::function<double()>;
        ComputationModel(
                const QString& name,
                const Computation& comp,
                const SpaceModel& space,
                const Id<ComputationModel>&,
                QObject* parent);


        //void setComputation(std::unique_ptr<spacelib::computation> &&ar);

        void setName(const QString& n)
        {
            m_name = n;
        }

        const QString& name()
        { return m_name; }
        const auto& computation() const
        { return m_fun; }


        const auto& space() const
        { return m_space; }

    private:
        QString m_name; // e.g. dist_a1_a2
        Computation m_fun;
        const SpaceModel& m_space;
        //std::unique_ptr<spacelib::computation> m_computation;

};
