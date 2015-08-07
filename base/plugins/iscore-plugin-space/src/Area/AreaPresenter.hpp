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
                AreaView* view,
                const AreaModel &model,
                QObject* parent);

        const id_type<AreaModel>& id() const;

        virtual void update();

    protected:
        virtual void on_areaChanged();

    private:
        const AreaModel& m_model;
        AreaView* m_view{};

};
