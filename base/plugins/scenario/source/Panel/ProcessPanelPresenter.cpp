#include "ProcessPanelPresenter.hpp"
#include "ProcessPanelView.hpp"
#include "ProcessPanelModel.hpp"

#include <QApplication>
#include <Document/BaseElement/BaseElementModel.hpp>
#include <ProcessInterface/ProcessList.hpp>

#include <Document/BaseElement/Widgets/GraphicsProxyObject.hpp>
#include <ProcessInterface/ProcessSharedModelInterface.hpp>
#include <ProcessInterface/ProcessPresenterInterface.hpp>
#include <ProcessInterface/ProcessViewInterface.hpp>
#include <ProcessInterface/ProcessFactoryInterface.hpp>

#include <Document/Constraint/Box/BoxModel.hpp>
#include <Document/Constraint/Box/BoxPresenter.hpp>
#include <Document/Constraint/Box/BoxView.hpp>

#include <Document/Constraint/Box/Deck/DeckModel.hpp>
#include <Document/Constraint/Box/Deck/DeckPresenter.hpp>
#include <Document/Constraint/Box/Deck/DeckView.hpp>

#include <iscore/document/DocumentInterface.hpp>

#include <QDebug>
#include <QGraphicsScene>
QString ProcessPanelPresenter::modelObjectName() const
{
    return "ProcessPanelModel";
}

void ProcessPanelPresenter::on_modelChanged()
{
    auto panelmodel = static_cast<ProcessPanelModel*>(model());


    m_baseElementModel =
            iscore::IDocument::documentFromObject(model())
               ->findChild<BaseElementModel*>("BaseElementModel");

    if(!m_baseElementModel) return;

    connect(m_baseElementModel,  SIGNAL(focusedProcessChanged()),
            this, SLOT(on_focusedProcessChanged()));
}

void ProcessPanelPresenter::on_focusedProcessChanged()
{
    qDebug() << Q_FUNC_INFO;
    auto panelview = static_cast<ProcessPanelView*>(view());

    auto proc = m_baseElementModel->focusedProcess();
    if(proc)
    {
        auto gobj = new GraphicsProxyObject;

        auto boxm = new BoxModel(id_type<BoxModel>{0}, model());
        auto boxv = new BoxView(gobj);
        auto boxp = new BoxPresenter(boxm, boxv, model());

        auto deckm = new DeckModel(id_type<DeckModel>{0}, boxm);
        auto deckv = new DeckView(boxv);
        auto deckp = new DeckPresenter(deckm, deckv, boxp);

        boxm->addDeck(deckm);


        auto fact = ProcessList::getFactory(proc->processName());
        auto pvm = proc->makeViewModel(id_type<ProcessViewModelInterface>(1234), deckm);
        deckm->addProcessViewModel(pvm);

        auto view = fact->makeView("Temporal", gobj);
        auto pres = fact->makePresenter(pvm, view, deckp);

        panelview->scene()->addItem(gobj);
    }
}
