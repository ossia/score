#pragma once
#include <iscore/plugins/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <iscore/selection/SelectionStack.hpp>
#include "ProcessFocusManager.hpp"

class BaseElementPresenter;
class FullViewConstraintViewModel;
class ConstraintModel;
class ProcessModel;
class LayerModel;

class BaseElementModel : public iscore::DocumentDelegateModelInterface
{
        Q_OBJECT

    public:
        BaseElementModel(QObject* parent);
        BaseElementModel(const VisitorVariant& data, QObject* parent);
        virtual ~BaseElementModel() = default;

        void serialize(const VisitorVariant&) const override;
        void setNewSelection(const Selection& s) override;


        ConstraintModel* baseConstraint() const
        { return m_baseConstraint; }

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
        ConstraintModel* m_baseConstraint {};
        const ConstraintModel* m_displayedConstraint {};

        ProcessFocusManager m_focusManager;
};

