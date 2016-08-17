#include <Midi/MidiProcess.hpp>
#include <Midi/MidiLayer.hpp>
#include <Midi/MidiPresenter.hpp>
#include <Midi/MidiView.hpp>

#include <iscore/document/DocumentInterface.hpp>
#include <iscore/document/DocumentContext.hpp>

namespace Midi
{
Presenter::Presenter(
        const Layer& layer,
        View* view,
        const Process::ProcessPresenterContext& ctx,
        QObject* parent):
    LayerPresenter{ctx, parent},
    m_layer{layer},
    m_view{view}
{
    putToFront();

    auto& model = layer.processModel();

    con(model, &ProcessModel::notesChanged,
        this, [&] ( ) {
        recompute();
    });
    model.notes.added.connect<Presenter, &Presenter::on_noteAdded>(this);
    model.notes.removing.connect<Presenter, &Presenter::on_noteRemoving>(this);

    for(auto& note : model.notes)
    {
        on_noteAdded(note);
    }
    recompute();
}

void Presenter::setWidth(qreal val)
{
    m_view->setWidth(val);
    recompute();
}

void Presenter::setHeight(qreal val)
{
    m_view->setHeight(val);
    recompute();
}

void Presenter::putToFront()
{
    m_view->setEnabled(true);
}

void Presenter::putBehind()
{
    m_view->setEnabled(false);
}

void Presenter::on_zoomRatioChanged(ZoomRatio)
{
}

void Presenter::parentGeometryChanged()
{
}

const Process::LayerModel& Presenter::layerModel() const
{
    return m_layer;
}

const Id<Process::ProcessModel>& Presenter::modelId() const
{
    return m_layer.processModel().id();
}

void Presenter::setupNote(NoteView& v)
{
    const auto note_height = m_view->height() / 127.;
    v.setX(v.note.start() * m_view->width());
    v.setWidth(v.note.duration() * m_view->width());
    v.setY(m_view->height() - v.note.pitch() * note_height);
    v.setHeight(note_height);
}

void Presenter::on_noteAdded(const Note& n)
{
    auto v = new NoteView{n, m_view};
    setupNote(*v);
    m_notes.push_back(v);
}

void Presenter::on_noteRemoving(const Note& n)
{
    auto it = find_if(m_notes, [&] (const auto& other) { return &other->note == &n; });
    if(it != m_notes.end())
    {
        delete *it;
        m_notes.erase(it);
    }
}

void Presenter::recompute()
{
    for(auto note : m_notes)
        setupNote(*note);
}

}
