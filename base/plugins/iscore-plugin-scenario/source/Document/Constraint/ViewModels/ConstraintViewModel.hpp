#pragma once
#include <iscore/tools/IdentifiedObject.hpp>

class ConstraintModel;
class RackModel;
class ConstraintViewModel : public IdentifiedObject<ConstraintViewModel>
{
        Q_OBJECT

    public:
        ConstraintViewModel(const id_type<ConstraintViewModel>& id,
                            const QString& name,
                            const ConstraintModel& model,
                            QObject* parent);

        template<typename DeserializerVisitor>
        ConstraintViewModel(DeserializerVisitor&& vis,
                            const ConstraintModel& model,
                            QObject* parent) :
            IdentifiedObject<ConstraintViewModel> {vis, parent},
            m_model {model}
        {
            vis.writeTo(*this);
        }

        virtual ConstraintViewModel* clone(
                const id_type<ConstraintViewModel>& id,
                const ConstraintModel& cm,
                QObject* parent) = 0;

        virtual QString type() const = 0;
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
