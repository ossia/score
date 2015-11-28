#pragma once
#include <iscore/plugins/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/selection/SelectionStack.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <QPointer>
#include <Scenario/Document/ScenarioDocument/ProcessFocusManager.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsModel.hpp>
class BaseScenario;
class ScenarioDocumentPresenter;
class FullViewConstraintViewModel;
class Process;
class LayerModel;
class ConstraintModel;

class ScenarioDocumentModel final : public iscore::DocumentDelegateModelInterface
{
        Q_OBJECT
        ISCORE_SERIALIZE_FRIENDS(ScenarioDocumentModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(ScenarioDocumentModel, JSONObject)
    public:
        ScenarioDocumentModel(QObject* parent);

        template<typename Impl>
        ScenarioDocumentModel(Deserializer<Impl>& vis, QObject* parent) :
            iscore::DocumentDelegateModelInterface {vis, parent}
        {
            vis.writeTo(*this);
        }

        virtual ~ScenarioDocumentModel() = default;

        BaseScenario& baseScenario() const
        { return *m_baseScenario; }

        ConstraintModel& baseConstraint() const;

        void serialize(const VisitorVariant&) const override;
        void setNewSelection(const Selection& s) override;


        ProcessFocusManager& focusManager()
        { return m_focusManager; }
        const ProcessFocusManager& focusManager() const
        { return m_focusManager; }

        void setDisplayedConstraint(const ConstraintModel& constraint);

        DisplayedElementsModel displayedElements;

    signals:
        void focusMe();
        void setFocusedPresenter(LayerPresenter*);
        void displayedConstraintChanged();

    public slots:
        void on_viewModelDefocused(const LayerModel* vm);
        void on_viewModelFocused(const LayerModel* vm);

    private:
        void initializeNewDocument(const FullViewConstraintViewModel* viewmodel);

        ProcessFocusManager m_focusManager;
        QPointer<ConstraintModel> m_focusedConstraint{};
        BaseScenario* m_baseScenario{};

        QMetaObject::Connection m_constraintConnection;
};

