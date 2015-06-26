#pragma once
#include <iscore/tools/IdentifiedObject.hpp>

class ConstraintModel;
class RackModel;
class AbstractConstraintViewModel : public IdentifiedObject<AbstractConstraintViewModel>
{
        Q_OBJECT

    public:
        AbstractConstraintViewModel(const id_type<AbstractConstraintViewModel>& id,
                                    const QString& name,
                                    const ConstraintModel& model,
                                    QObject* parent);

        template<typename DeserializerVisitor>
        AbstractConstraintViewModel(DeserializerVisitor&& vis,
                                    const ConstraintModel& model,
                                    QObject* parent) :
            IdentifiedObject<AbstractConstraintViewModel> {vis, parent},
            m_model {model}
        {
            vis.writeTo(*this);
        }

        virtual AbstractConstraintViewModel* clone(
                const id_type<AbstractConstraintViewModel>& id,
                const ConstraintModel& cm,
                QObject* parent) = 0;

        const ConstraintModel& model() const;

        bool isRackShown() const;
        const id_type<RackModel>& shownRack() const;

        void hideRack();
        void showRack(const id_type<RackModel>& rackId);

    signals:
        void rackRemoved();
        void rackHidden();
        void rackShown(const id_type<RackModel>& rackId);

    public slots:
        virtual void on_rackRemoved(const id_type<RackModel>& rackId);


    private:
        // A view model cannot be constructed without a model
        // hence we are safe with a pointer
        const ConstraintModel& m_model;

        id_type<RackModel> m_shownRack {};
};
