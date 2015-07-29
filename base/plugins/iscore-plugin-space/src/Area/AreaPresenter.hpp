#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
class QGraphicsItem;
class AreaModel;
class AreaView;
class AreaPresenter : public NamedObject
{
    public:
        AreaPresenter(
                const AreaModel &model,
                QGraphicsItem* parentview,
                QObject* parent);

        const id_type<AreaModel>& id() const;

        void update();

    private:
        void on_areaChanged();

        const AreaModel& m_model;
        AreaView* m_view{};

};
