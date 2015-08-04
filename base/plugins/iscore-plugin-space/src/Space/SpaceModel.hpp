#pragma once

#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include <Space/space.hpp>
#include <Space/bounded_symbol.hpp>
// For now a simple min-max.



class DimensionModel
{
    public:
        DimensionModel(const QString& name):
            m_name{name},
            m_sym{GiNaC::symbol(name.toLatin1().constData()), spacelib::MinMaxDomain{}}
        {
        }

        const QString& name() const;
        spacelib::minmax_symbol& sym();
        const spacelib::minmax_symbol& sym() const;

    private:
        QString m_name;
        spacelib::minmax_symbol m_sym;
};

// A space has a number of dimensions
class SpaceModel : public IdentifiedObject<SpaceModel>
{
        Q_OBJECT
    public:
        SpaceModel(
                std::vector<DimensionModel>&& sp,
                const id_type<SpaceModel>& id,
                QObject* parent);

        const auto& space() const
        { return *m_space; }

        void addDimension(const DimensionModel& dim);
        void removeDimension(const QString& name);
        const auto& dimensions() const
        { return m_dimensions; }

    signals:
        void spaceChanged();

    private:
        void rebuildSpace();

        std::unique_ptr<spacelib::euclidean_space> m_space;
        std::vector<DimensionModel> m_dimensions;
};
