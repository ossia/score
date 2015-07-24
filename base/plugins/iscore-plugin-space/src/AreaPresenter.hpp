#pragma once
#include <iscore/tools/NamedObject.hpp>
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


    private:
        void on_areaChanged();

        const AreaModel& m_model;
        AreaView* m_view{};

};
