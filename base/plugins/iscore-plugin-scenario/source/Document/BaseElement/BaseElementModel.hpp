#pragma once
#include <iscore/plugins/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <iscore/selection/SelectionStack.hpp>
#include "ProcessFocusManager.hpp"
#include "DisplayedElements/DisplayedElementsModel.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
class BaseScenario;
class BaseElementPresenter;
class FullViewConstraintViewModel;
class ProcessModel;
class LayerModel;
class ConstraintModel;

class BaseElementModel : public iscore::DocumentDelegateModelInterface
{
        Q_OBJECT
        ISCORE_SERIALIZE_FRIENDS(BaseElementModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(BaseElementModel, JSONObject)
    public:
        BaseElementModel(QObject* parent);
        BaseElementModel(const VisitorVariant& data, QObject* parent);
        virtual ~BaseElementModel() = default;

        ConstraintModel& baseConstraint() const;

        void serialize(const VisitorVariant&) const override;
        void setNewSelection(const Selection& s) override;


        ProcessFocusManager& focusManager()
        { return m_focusManager; }

        void setDisplayedConstraint(const ConstraintModel *constraint);

        DisplayedElementsModel displayedElements;

    signals:
        void focusMe();
        void setFocusedPresenter(ProcessPresenter*);
        void displayedConstraintChanged();

    public slots:
        void on_viewModelDefocused(const LayerModel* vm);
        void on_viewModelFocused(const LayerModel* vm);

    private:
        void initializeNewDocument(const FullViewConstraintViewModel* viewmodel);

        ProcessFocusManager m_focusManager;
        BaseScenario* m_baseScenario{};

        QMetaObject::Connection m_constraintConnection;
};

