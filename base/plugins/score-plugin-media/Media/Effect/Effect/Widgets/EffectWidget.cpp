#include "EffectWidget.hpp"
#include <QMenu>
#include <QPushButton>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMimeData>
#include <QDrag>
#include <QTimer>
#include <QApplication>
#include <score/widgets/DoubleSlider.hpp>
#include <Media/Effect/LocalTree/LocalTreeEffectComponent.hpp>
#include <Scenario/Commands/Interval/AddLayerInNewSlot.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Automation/AutomationModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
//#include <Media/Commands/CreateEffectAutomation.hpp>
#include <Automation/Commands/InitAutomation.hpp>
#include <Media/Effect/Effect/Widgets/EffectSlider.hpp>
#include <score/widgets/MarginLess.hpp>
namespace Media
{
namespace Effect
{


EffectWidget::EffectWidget(
        EffectModel& fx,
        const score::DocumentContext& doc,
        QWidget* parent):
    QFrame{parent},
    m_effect{fx},
    m_context{doc}
{
    // Setup ui
    setObjectName("EffectWidget");
    setStyleSheet("QFrame#EffectWidget { border: 1px solid black; border-radius: 10px; }");
    auto lay = new score::MarginLess<QVBoxLayout>;
    m_layout = new score::MarginLess<QGridLayout>;

    {
        auto title = new QWidget;
        auto title_lay = new score::MarginLess<QHBoxLayout>;
        title->setLayout(title_lay);

        auto label = new score::ReactiveLabel<score::ModelMetadataNameParameter>(fx.metadata(), this);
        label->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
        title_lay->addWidget(label);
        title_lay->addStretch();

        auto rm_but = new QPushButton{"x"};
        rm_but->setStyleSheet(
                    "QPushButton { "
                    "border-style: solid;"
                    "border-width: 2px;"
                    "border-color: black;"
                    "border-radius: 15px;}");
        connect(rm_but, &QPushButton::clicked,
                this, [&] () {
            this->m_timer.stop();

            score::clearLayout(this->m_layout);
            m_sliders.clear();
            emit removeRequested();
        });

        title_lay->addWidget(rm_but);

        lay->addWidget(title);
    }

    lay->addLayout(m_layout);

    this->setLayout(lay);
    lay->addStretch();

    setup();

    // Setup drag'n'drop
    setAcceptDrops(true);

}

void EffectWidget::on_createAutomation(const State::Address& addr, double min, double max)
{
  /*
    QObject* obj = &m_effect;
    while(obj)
    {
        auto parent = obj->parent();
        if(auto cst = dynamic_cast<Scenario::IntervalModel*>(parent))
        {
            RedoMacroCommandDispatcher<Commands::CreateEffectAutomation> macro{m_context.commandStack};
            auto make_cmd = new Scenario::Command::AddOnlyProcessToInterval{
                        *cst,
                        Metadata<ConcreteKey_k, Automation::ProcessModel>::get()};
            macro.submitCommand(make_cmd);

            auto lay_cmd = new Scenario::Command::AddLayerInNewSlot{*cst, make_cmd->processId()};
            macro.submitCommand(lay_cmd);

            auto& autom = safe_cast<Automation::ProcessModel&>(cst->processes.at(make_cmd->processId()));
            macro.submitCommand(new Automation::InitAutomation{autom, State::AddressAccessor{addr}, min, max});

            macro.commit();
            return;
        }
        else
        {
            obj = parent;
        }
    }
    */
}

void EffectWidget::setup()
{
    m_timer.stop();

    score::clearLayout(m_layout);
    m_sliders.clear();

    for(auto& param_addr : m_effect.inlets())
    {
      if(auto control = dynamic_cast<Process::ControlInlet*>(param_addr))
      {
        auto slider = new EffectSlider{*control, false, this};
        m_sliders.push_back(slider);
        connect(slider, &EffectSlider::createAutomation,
                this, &EffectWidget::on_createAutomation,
                Qt::QueuedConnection);
      }
    }

    /*
    for(auto& param_addr : m_effect.outlets())
    {
        auto slider = new EffectSlider{*std::get<2>(param_addr), true, this};
        m_sliders.push_back(slider);
    }
    */

    m_timer.setInterval(50);
    connect(&m_timer, &QTimer::timeout,
            this, &EffectWidget::updateSliders, Qt::UniqueConnection);
    m_timer.start();
    flow();
}

void EffectWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    reflow();
}

void EffectWidget::flow()
{
    if(m_sliders.empty())
        return;

    auto one_row = m_sliders[0]->height();
    auto max_row = this->height() - 50;
    auto row_count = std::min(1.f, std::round(std::floor(float(one_row) / float(max_row))));

    int cur_col = 0;
    int cur_row = 0;
    for(auto elt : m_sliders)
    {
        if(cur_row == row_count)
        {
            cur_row = 0;
            cur_col++;
        }

        m_layout->addWidget(elt, cur_row, cur_col);

        cur_row++;
    }
}

void EffectWidget::reflow()
{
    std::vector<QLayoutItem*> items;
    while(auto item = m_layout->takeAt(0))
    {
        items.push_back(item);
    }

    if(items.empty())
        return;

    auto one_row = std::min(30, m_sliders[0]->height());
    auto max_row = this->height();
    auto row_count = std::min(1.f, std::round(std::floor(float(one_row) / float(max_row))));

    int cur_col = 0;
    int cur_row = 0;
    for(auto elt : items)
    {
        if(cur_row == row_count)
        {
            cur_row = 0;
            cur_col++;
        }

        m_layout->addItem(elt, cur_row, cur_col);

        cur_row++;
    }
}

void EffectWidget::clear()
{
    score::clearLayout(m_layout);
    m_sliders.clear();
}


void EffectWidget::updateSliders()
{
  for(EffectSlider* widg : m_sliders)
  {
    if(widg->scaledValue != widg->m_slider->value()) // TODO qFuzzyCompare instead
       widg->m_slider->setValue(widg->scaledValue);
  }
}


void EffectWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {

        auto drag = new QDrag(this);
        auto mimeData = new QMimeData;

        // TODO proper MIME serialization.
        mimeData->setData("application/x-iscore-effectdrag",
                          score::marshall<DataStream>(make_path(m_effect)));
        drag->setMimeData(mimeData);
        QLabel label{m_effect.metadata().getLabel()};
        drag->setPixmap(label.grab());
        drag->setHotSpot(label.rect().center());

        drag->exec();
    }
}

void EffectWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if(!event->mimeData()->hasFormat("application/x-iscore-effectdrag"))
        return;

    auto path = score::unmarshall<Path<EffectModel>>(event->mimeData()->data("application/x-iscore-effectdrag"));
    EffectModel* res = path.try_find(m_context);
    if(!res)
        return;

    if(res == &m_effect)
        return;

    if(res->parent() != m_effect.parent())
        return;

    event->acceptProposedAction();
}

void EffectWidget::dragMoveEvent(QDragMoveEvent* event)
{
    // TODO : show a bar to indicate
    // graphically where the drop will occur.
}

void EffectWidget::dropEvent(QDropEvent* event)
{
    // Get the process
    auto path = score::unmarshall<Path<EffectModel>>(
                    event->mimeData()->data("application/x-iscore-effectdrag"));


    // Position
    emit sig_performInsert(path, m_effect, event->pos().x() < rect().center().x());

    // Accept
    event->acceptProposedAction();
}

}
}
