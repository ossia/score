#pragma once
#include <QObject>
#include <iscore/selection/Selection.hpp>
#include <QApplication>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <Process/ScenarioModel.hpp>

class TemporalConstraintPresenter;
class TemporalScenarioPresenter;
namespace iscore
{
    class SelectionDispatcher;
}
class ScenarioModel;

class ScenarioSelectionManager : public QObject
{
    public:
        ScenarioSelectionManager(TemporalScenarioPresenter* parent);

        void deselectAll();

        template<typename T>
        Selection filterSelections(T* pressedModel, Selection sel)
        {
            auto cumulation = QApplication::keyboardModifiers().testFlag(Qt::ControlModifier);
            if(!cumulation)
            {
                sel.clear();
            }

            // If the pressed element is selected
            if(pressedModel->selection.get())
            {
                if(cumulation)
                {
                    sel.removeAll(pressedModel);
                }
                else
                {
                    sel.push_back(pressedModel);
                }
            }
            else
            {
                sel.push_back(pressedModel);
            }

            return sel;
        }

        void setSelectionArea(const QRectF& area);
        void selectConstraint(TemporalConstraintPresenter* cstr);
    private:
        void focus();
        TemporalScenarioPresenter* m_presenter{};
        iscore::SelectionDispatcher m_selectionDispatcher;
        ScenarioModel* m_scenario{};
};
