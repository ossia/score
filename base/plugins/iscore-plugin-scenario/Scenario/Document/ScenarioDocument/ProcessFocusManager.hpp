#pragma once
#include <QObject>
#include <QPointer>
#include <Process/Focus/FocusDispatcher.hpp>

namespace iscore { class FocusManager; }
namespace Process { class LayerModel; }
namespace Process { class LayerPresenter; }
namespace Process { class ProcessModel; }
namespace Scenario
{
class ScenarioDocumentPresenter;
}
// Keeps the focused elements in memory for use by the scenario application plugin.
// Note : focus should not be lost when switching documents. Hence, this
// should more be part of the per-document part.
namespace Process
{
class ProcessFocusManager final : public QObject
{
        Q_OBJECT

    public:
        ProcessFocusManager(iscore::FocusManager& fmgr):
            m_mgr{fmgr}
        {

        }

        const ProcessModel* focusedModel() const;
        const LayerModel* focusedViewModel() const;
        LayerPresenter* focusedPresenter() const;

        void focus(QPointer<Process::LayerPresenter>);
        void focus(Scenario::ScenarioDocumentPresenter*);

        void focusNothing();

    signals:
        void sig_focusedPresenter(LayerPresenter*);
        void sig_defocusedPresenter(LayerPresenter*);

        void sig_defocusedViewModel(const LayerModel*);
        void sig_focusedViewModel(const LayerModel*);

    private:
        void focusPresenter(LayerPresenter*);
        void defocusPresenter(LayerPresenter*);

        iscore::FocusManager& m_mgr;

        QPointer<const ProcessModel> m_currentModel{};
        QPointer<const LayerModel> m_currentViewModel{};
        QPointer<LayerPresenter> m_currentPresenter{};

        QMetaObject::Connection m_deathConnection;
};
}

