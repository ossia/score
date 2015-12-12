#pragma once
#include <Scenario/Document/DisplayedElements/DisplayedElementsModel.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessFocusManager.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateModelInterface.hpp>

#include <QPointer>

#include <iscore/selection/Selection.hpp>
#include <iscore/serialization/VisitorInterface.hpp>

class BaseScenario;
class ConstraintModel;
class DataStream;
class FullViewConstraintViewModel;
class JSONObject;
class LayerModel;
class LayerPresenter;
class QObject;

class ISCORE_PLUGIN_SCENARIO_EXPORT ScenarioDocumentModel final : public iscore::DocumentDelegateModelInterface
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
            init();
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


    signals:
        void focusMe();
        void setFocusedPresenter(LayerPresenter*);
        void displayedConstraintChanged();

    public slots:
        void on_viewModelDefocused(const LayerModel* vm);
        void on_viewModelFocused(const LayerModel* vm);

    private:
        void init();
        void initializeNewDocument(const FullViewConstraintViewModel* viewmodel);

        ProcessFocusManager m_focusManager;
        QPointer<ConstraintModel> m_focusedConstraint{};
        BaseScenario* m_baseScenario{};

        QMetaObject::Connection m_constraintConnection;

    public:
        DisplayedElementsModel displayedElements;
};

