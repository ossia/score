#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <src/Area/ValMap.hpp>
#include <ginac/ex.h>
class QGraphicsItem;

namespace Space
{
class AreaModel;
class GenericAreaView;

class AreaPresenter : public NamedObject
{
    public:
        using model_type = AreaModel;
        using view_type = GenericAreaView;
        AreaPresenter(
                QGraphicsItem* view,
                const AreaModel &model,
                QObject* parent);

        ~AreaPresenter();

        const Id<AreaModel>& id() const;

        virtual void update() = 0;
        virtual void on_areaChanged(ValMap) = 0;

        // Useful for subclasses
        template<typename T>
        const typename T::model_type& model(T*) const
        { return static_cast<const typename T::model_type&>(m_model); }
        template<typename T>
        typename T::view_type& view(T*) const
        { return static_cast<typename T::view_type&>(*m_view); }

    private:
        const AreaModel& m_model;
        QGraphicsItem* m_view{};

};
}
