#pragma once
#include "Document/Constraint/ViewModels/AbstractConstraintPresenter.hpp"

class FullViewConstraintViewModel;
class FullViewConstraintView;
class RackPresenter;
class RackModel;
class QGraphicsItem;
#include <iscore/selection/SelectionDispatcher.hpp>


namespace iscore
{
    class SerializableCommand;
}
class ProcessPresenter;

/**
 * @brief The FullViewConstraintPresenter class
 *
 * Présenteur : reçoit signaux depuis modèle et vue et présenteurs enfants.
 * Exemple : cas d'un process ajouté : le modèle reçoit la commande addprocess, émet un signal, qui est capturé par le présenteur qui va instancier le présenteur nécessaire en appelant la factory.
 */
class FullViewConstraintPresenter : public AbstractConstraintPresenter
{
        Q_OBJECT

    public:
        using layer_type = FullViewConstraintViewModel;
        using view_type = FullViewConstraintView;

        FullViewConstraintPresenter(const FullViewConstraintViewModel& viewModel,
                                    QGraphicsItem* parentobject,
                                    QObject* parent);
        virtual ~FullViewConstraintPresenter();

        void on_pressed(const QPointF &);

    private:
        iscore::SelectionDispatcher m_selectionDispatcher;

};

