#pragma once
#include <Process/Dataflow/DataflowObjects.hpp>
#include <QQuickPaintedItem>
#include <QPainter>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/audio_parameter.hpp>
namespace Dataflow
{
class volume_node final : public ossia::graph_node
{
public:
  double volume{1.};
  volume_node()
  {
    m_inlets.push_back(ossia::make_inlet<ossia::audio_port>());
    m_outlets.push_back(ossia::make_outlet<ossia::audio_port>());
  }

  void run(ossia::token_request t, ossia::execution_state&) override
  {
    auto in = m_inlets[0]->data.target<ossia::audio_port>();
    auto out = m_outlets[0]->data.target<ossia::audio_port>();
    out->samples.resize(in->samples.size());
    for(std::size_t i = 0; i < in->samples.size(); i++)
    {
      auto& in_chan = in->samples[i];
      auto& out_chan = out->samples[i];
      for(std::size_t j = 0; j < in_chan.size(); j++)
      {
        out_chan[j] = in_chan[j] * volume;
      }
    }
  }
};
/*
class SliderUI final : public QQuickPaintedItem
{
    Q_OBJECT
public:
    SliderUI();

    void setValue(double v) { m_value = v; update(); }
Q_SIGNALS:
    void valueChanged(double);

private:
    void paint(QPainter *painter) override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

    void changeValue(double pos);
    double m_value{};
};*/

/*
class Slider final :
        public Process::Node
{
    Q_OBJECT
public:
    Slider(const score::DocumentContext& doc, Id<Node> c, QObject* parent);
    Slider(QObject* parent);

    void setVolume(double v);
    void preparePlay();

Q_SIGNALS:
    void volumeChanged(double);

private:
    QString getText() const override;
    std::size_t audioInlets() const override;
    std::size_t messageInlets() const override;
    std::size_t midiInlets() const override;
    std::size_t audioOutlets() const override;
    std::size_t messageOutlets() const override;
    std::size_t midiOutlets() const override;
    std::vector<Process::Port> inlets() const override;
    std::vector<Process::Port> outlets() const override;
    std::vector<Id<Process::Cable> > cables() const override;
    void addCable(Id<Process::Cable> c) override;
    void removeCable(Id<Process::Cable> c) override;

    double m_volume{};
    std::vector<Id<Process::Cable>> m_cables;
};
*/
}
