#include "EffectSlider.hpp"
#include <ossia/network/domain/domain.hpp>
#include <State/Expression.hpp>
#include <score/widgets/DoubleSlider.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/TextLabel.hpp>
#include <Device/Node/NodeListMimeSerialization.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/value/value_conversion.hpp>
#include <QMenu>
#include <QLabel>
#include <QDrag>
#include <QVBoxLayout>
#include <QAction>
#include <QContextMenuEvent>
#include <Device/Address/AddressSettings.hpp>
#include <State/MessageListSerialization.hpp>
#include <Process/Dataflow/Port.hpp>

namespace Media
{
namespace Effect
{
/*
// TODO move me
class AddressLabel : public QLabel
{
        const ossia::net::node_base& m_node;
    public:
        AddressLabel(const ossia::net::node_base& node, QString str, QWidget* parent):
            QLabel{std::move(str), parent},
            m_node{node}
        {

        }

    private:
        void mousePressEvent(QMouseEvent* event) override
        {
            if (event->button() == Qt::LeftButton)
            {
                auto as = Engine::ossia_to_score::ToFullAddressSettings(m_node);

                auto drag = new QDrag(this);
                auto mimeData = new QMimeData;
                {
                    Mime<Device::FullAddressSettings>::Serializer s{*mimeData};
                    s.serialize(as);
                }
                {
                    Mime<State::MessageList>::Serializer s{*mimeData};
                    s.serialize({State::Message{State::AddressAccessor{as.address}, as.value}});
                }
                drag->setMimeData(mimeData);

                drag->setPixmap(grab());
                drag->setHotSpot(rect().center());

                drag->exec();
            }
        }

};
*/
EffectSlider::EffectSlider(Process::ControlInlet& fx, bool is_output, QWidget* parent):
  QWidget{parent},
  m_param{fx}
{
  const auto& dom = fx.domain().get();

  if(auto f = dom.maybe_min<float>()) m_min = *f;
  if(auto f = dom.maybe_max<float>()) m_max = *f;

  auto lay = new score::MarginLess<QVBoxLayout>;
  /*
  lay->addWidget(new AddressLabel{
                     m_param,
                     QString::fromStdString(
                      ossia::get_value_or(ossia::net::get_description(m_param), "nothing")),
                     this});
  */
  lay->addWidget(new TextLabel{fx.customData()});
  m_slider = new score::DoubleSlider{this};
  m_slider->setEnabled(!is_output);
  m_slider->setForegroundRole(QPalette::AlternateBase);
  lay->addWidget(m_slider);
  this->setLayout(lay);

  auto cur_val = ossia::convert<float>(fx.value());
  scaledValue = (cur_val - m_min) / (m_max - m_min);
  m_slider->setValue(scaledValue);

  if(!is_output)
  {
      connect(m_slider, &score::DoubleSlider::valueChanged,
              this, [=] (double v)
      {
          // TODO undo ???
          // v is between 0 - 1
          const auto& cur = m_param.value();
          auto exp = float(m_min + (m_max - m_min) * v);
          if(ossia::convert<float>(cur) != exp)
            m_param.setValue(float{exp});
      });
  }
  con(m_param, &Process::ControlInlet::valueChanged,
          this, [=] (const ossia::value& val) {

    if(auto v = val.target<float>())
    {
      auto scaled = (*v - m_min) / (m_max - m_min);
      if(scaled != m_slider->value()) // TODO qFuzzyCompare instead
         m_slider->setValue(scaled);
    }
  });

  m_addAutomAction = new QAction{tr("Add automation"), this};
  connect(m_addAutomAction, &QAction::triggered,
          this, [=] () {
    /*
    auto& ossia_addr = *m_param.get_parameter();
    auto& dom = ossia_addr.get_domain();
    auto addr = State::parseAddress(QString::fromStdString(ossia::net::address_string_from_node(ossia_addr)));

    if(addr)
    {
      auto min = dom.convert_min<double>();
      auto max = dom.convert_max<double>();
      emit createAutomation(std::move(*addr), min, max);
    }*/
  });
  // TODO show tooltip with current value
}

EffectSlider::~EffectSlider()
{
}

void EffectSlider::contextMenuEvent(QContextMenuEvent* event)
{
  QMenu* contextMenu = new QMenu{this};

  contextMenu->addAction(m_addAutomAction);
  contextMenu->exec(event->globalPos());

  contextMenu->deleteLater();
}

}
}
