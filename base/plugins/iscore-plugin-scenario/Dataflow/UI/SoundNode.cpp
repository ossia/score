#include "SoundNode.hpp"
#include <Pd/UI/NodeItem.hpp>

namespace Dataflow
{

SoundNode::SoundNode(DocumentPlugin &doc, Media::Sound::ProcessModel &proc, Id<Process::Node> c, QObject *parent):
    Process::Node{c, parent}
  , process{proc}
{
    connect(&proc, &Media::Sound::ProcessModel::fileChanged,
            this, [=] { this->outletsChanged(); });

    auto itm = new NodeItem{doc.context(), *this};
    itm->setParentItem(doc.window.view.contentItem());
    itm->setPosition(QPointF(100, 100));
    ui = itm;
}

QString SoundNode::getText() const { return process.file().name(); }

std::size_t SoundNode::audioInlets() const { return 0; }

std::size_t SoundNode::messageInlets() const { return 0; }

std::size_t SoundNode::midiInlets() const { return 0; }

std::size_t SoundNode::audioOutlets() const { return 1; }

std::size_t SoundNode::messageOutlets() const{ return 0; }

std::size_t SoundNode::midiOutlets() const{ return 0; }

std::vector<Process::Port> SoundNode::inlets() const { return {}; }

std::vector<Process::Port> SoundNode::outlets() const {
  std::vector<Process::Port> v(1);

  v[0].type = Process::PortType::Audio;
  v[0].propagate = true;

  return v;
}

SoundNode::~SoundNode()
{
    cleanup();
}

std::vector<Id<Process::Cable> > SoundNode::cables() const
{ return m_cables; }

void SoundNode::addCable(Id<Process::Cable> c)
{ m_cables.push_back(c); }

void SoundNode::removeCable(Id<Process::Cable> c)
{ m_cables.erase(ossia::find(m_cables, c)); }




SoundGraphNode::SoundGraphNode()
{
}

void SoundGraphNode::setSound(AudioArray vec)
{
    m_data = std::move(vec);
    m_outlets.clear();
    for(std::size_t i = 0; i < m_data.size(); i++)
        m_outlets.push_back(ossia::make_outlet<ossia::audio_port>());
}

SoundGraphNode::~SoundGraphNode()
{

}

void SoundGraphNode::run(ossia::execution_state &e)
{
    if(m_data.empty())
        return;
    const std::size_t len = m_data[0].size();

    ossia::audio_port& ap = *m_outlets[0]->data.target<ossia::audio_port>();
    ap.samples.resize(m_data.size());
    for(std::size_t i = 0; i < m_data.size(); i++)
    {
        if(m_date > m_prev_date)
        {
            std::size_t max_N = std::min(m_date.impl, (int64_t)len);
            ap.samples[i].resize(max_N - m_prev_date);
            for(std::size_t j = m_prev_date; j < max_N; j++)
            {
                ap.samples[i][j - m_prev_date] = m_data[i][j];
            }
        }
        else
        {
            // TODO play backwards
        }

    }

}

SoundExecComponent::SoundExecComponent(
        Engine::Execution::ConstraintComponent &parentConstraint,
        Media::Sound::ProcessModel &element,
        const DocumentPlugin &df,
        const Engine::Execution::Context &ctx,
        const Id<iscore::Component> &id,
        QObject *parent)
    : Engine::Execution::ProcessComponent_T<Media::Sound::ProcessModel, ossia::node_process>{
          parentConstraint,
          element,
          ctx,
          id, "Executor::SoundComponent", parent}
    , m_df{df}
{
    m_ossia_process = std::make_shared<ossia::node_process>(m_df.execGraph);
    node = std::make_shared<SoundGraphNode>();

    con(element, &Media::Sound::ProcessModel::fileChanged,
        this, [this] { this->recompute(); });
}

void SoundExecComponent::recompute()
{
    system().executionQueue.enqueue(
                [n=std::dynamic_pointer_cast<SoundGraphNode>(this->node)
                ,data=process().file().data()
                ]
    {
        n->setSound(std::move(data));
    });
}

SoundExecComponent::~SoundExecComponent()
{
    if(node) node->clear();
}

}
