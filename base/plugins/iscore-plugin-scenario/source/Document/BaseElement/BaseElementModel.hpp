#pragma once
#include <iscore/plugins/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <iscore/selection/SelectionStack.hpp>
#include "ProcessFocusManager.hpp"

class BaseScenario;
class BaseElementPresenter;
class FullViewConstraintViewModel;
class ProcessModel;
class LayerModel;
class ConstraintModel;

class BaseElementModel : public iscore::DocumentDelegateModelInterface
{
        Q_OBJECT

    public:
        BaseElementModel(QObject* parent);
        BaseElementModel(const VisitorVariant& data, QObject* parent);
        virtual ~BaseElementModel() = default;

        ConstraintModel* baseConstraint() const;

        void serialize(const VisitorVariant&) const override;
        void setNewSelection(const Selection& s) override;

        void setDisplayedConstraint(const ConstraintModel*);

        ProcessFocusManager& focusManager()
        { return m_focusManager; }

    signals:
        void focusMe();
        void setFocusedPresenter(ProcessPresenter*);

    public slots:
        void on_viewModelDefocused(const LayerModel* vm);
        void on_viewModelFocused(const LayerModel* vm);

    private:
        void initializeNewDocument(const FullViewConstraintViewModel* viewmodel);
        const ConstraintModel* m_displayedConstraint {};

        ProcessFocusManager m_focusManager;
        BaseScenario* m_baseScenario{};
};

