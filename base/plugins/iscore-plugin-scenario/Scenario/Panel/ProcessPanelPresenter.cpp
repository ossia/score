#include <Process/LayerModelPanelProxy.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Process/Tools/ProcessGraphicsView.hpp>
#include <QGraphicsScene>
#include <QObject>
#include <QSize>
#include <algorithm>

#include <Process/LayerModel.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>
#include <Process/TimeValue.hpp>
#include <Process/ZoomHelper.hpp>
#include "ProcessPanelPresenter.hpp"
#include "ProcessPanelView.hpp"
#include <Scenario/Document/ScenarioDocument/ProcessFocusManager.hpp>
#include <Process/Tools/ProcessPanelGraphicsProxy.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>
#include <iscore/plugins/panel/PanelModel.hpp>
#include <iscore/plugins/panel/PanelPresenter.hpp>
#include <iscore/tools/Todo.hpp>

#include "ProcessPanelId.hpp"
#include <iscore/widgets/GraphicsItem.hpp>

#include <Process/LayerView.hpp>

namespace iscore {
class PanelView;

}  // namespace iscore
namespace Scenario
{
ProcessPanelPresenter::ProcessPanelPresenter(
        const Process::ProcessList& plist,
        iscore::PanelView* view,
        QObject* parent):
    iscore::PanelPresenter{view, parent}
{
}

int ProcessPanelPresenter::panelId() const
{
    return PROCESS_PANEL_ID;
}

void ProcessPanelPresenter::on_modelChanged(
        iscore::PanelModel* oldm,
        iscore::PanelModel* newm)
{
    if(oldm)
    {
        auto old_bem = iscore::IDocument::try_get<ScenarioDocumentModel>(*iscore::IDocument::documentFromObject(oldm));
        if(old_bem)
        {
            disconnect(&old_bem->focusManager(),  &Process::ProcessFocusManager::sig_focusedViewModel,
                       this, &ProcessPanelPresenter::on_focusedViewModelChanged);
        }
    }

    if(!model())
    {
        cleanup();
        return;
    }

    auto bem = iscore::IDocument::try_get<ScenarioDocumentModel>(*iscore::IDocument::documentFromObject(model()));

    if(!bem)
        return;

    con(bem->focusManager(), &Process::ProcessFocusManager::sig_focusedViewModel,
        this, &ProcessPanelPresenter::on_focusedViewModelChanged);

    con(bem->focusManager(), &Process::ProcessFocusManager::sig_defocusedViewModel,
        this, [&] {
        on_focusedViewModelChanged(nullptr);
    } );

    on_focusedViewModelChanged(bem->focusManager().focusedViewModel());
}

void ProcessPanelPresenter::on_focusedViewModelChanged(const Process::LayerModel* theLM)
{
    if(theLM != m_layerModel)
    {
        m_layerModel = theLM;
        delete m_proxy;
        m_proxy = nullptr;
        auto v = static_cast<ProcessPanelView*>(view());
        v->setInnerWidget(nullptr);
        if(!m_layerModel)
            return;

        m_proxy = m_layerModel->make_panelProxy(this);
        if(m_proxy)
            v->setInnerWidget(m_proxy->widget());
    }
}


void ProcessPanelPresenter::cleanup()
{
    m_layerModel = nullptr;
    delete m_proxy;
    m_proxy = nullptr;
}
}
