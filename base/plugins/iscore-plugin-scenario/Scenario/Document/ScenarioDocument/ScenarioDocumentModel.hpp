#pragma once
#include <Scenario/Document/DisplayedElements/DisplayedElementsModel.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessFocusManager.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateModelInterface.hpp>

#include <QPointer>
#include <core/document/Document.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/serialization/VisitorInterface.hpp>

class JSONObject;
class DataStream;
namespace Process { class LayerModel; }
namespace Process { class LayerPresenter; }
class QObject;

namespace Scenario
{
class BaseScenario;
class ConstraintModel;
class FullViewConstraintViewModel;
class ISCORE_PLUGIN_SCENARIO_EXPORT ScenarioDocumentModel final :
        public iscore::DocumentDelegateModelInterface
{
        Q_OBJECT
        ISCORE_SERIALIZE_FRIENDS(Scenario::ScenarioDocumentModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(Scenario::ScenarioDocumentModel, JSONObject)
    public:
        ScenarioDocumentModel(
                const iscore::DocumentContext& ctx,
                QObject* parent);

        template<typename Impl>
        ScenarioDocumentModel(
                Deserializer<Impl>& vis,
                const iscore::DocumentContext& ctx,
                QObject* parent) :
            iscore::DocumentDelegateModelInterface {vis, parent},
            m_focusManager{ctx.document.focusManager()}
        {
            vis.writeTo(*this);
            init();
        }

        virtual ~ScenarioDocumentModel() = default;

        BaseScenario& baseScenario() const
        { return *m_baseScenario; }

        ConstraintModel& baseConstraint() const;

        void serialize(const VisitorVariant&) const override;
        void setNewSelection(const Selection& s) override;


        Process::ProcessFocusManager& focusManager()
        { return m_focusManager; }
        const Process::ProcessFocusManager& focusManager() const
        { return m_focusManager; }

        void setDisplayedConstraint(const ConstraintModel& constraint);

        void on_viewModelDefocused(const Process::LayerModel* vm);
        void on_viewModelFocused(const Process::LayerModel* vm);

    signals:
        void focusMe();
        void setFocusedPresenter(QPointer<Process::LayerPresenter>);
        void displayedConstraintChanged();

    private:
        void init();
        void initializeNewDocument(const FullViewConstraintViewModel* viewmodel);

        Process::ProcessFocusManager m_focusManager;
        QPointer<ConstraintModel> m_focusedConstraint{};
        BaseScenario* m_baseScenario{};

        QMetaObject::Connection m_constraintConnection;

    public:
        DisplayedElementsModel displayedElements;
};
}
