#pragma once
#include <iscore/tools/IdentifiedObject.hpp>

class ConstraintModel;
class BoxModel;
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

        bool isBoxShown() const;
        const id_type<BoxModel>& shownBox() const;

        void hideBox();
        void showBox(const id_type<BoxModel>& boxId);

    signals:
        void boxRemoved();
        void boxHidden();
        void boxShown(const id_type<BoxModel>& boxId);

    public slots:
        virtual void on_boxRemoved(const id_type<BoxModel>& boxId);


    private:
        // A view model cannot be constructed without a model
        // hence we are safe with a pointer
        const ConstraintModel& m_model;

        id_type<BoxModel> m_shownBox {};
};
