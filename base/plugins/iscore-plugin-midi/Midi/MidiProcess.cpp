#include <Midi/MidiProcess.hpp>

namespace Midi
{
ProcessModel::ProcessModel(
        const TimeValue& duration,
        const Id<Process::ProcessModel>& id,
        QObject* parent):
    Process::ProcessModel{duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
{
    metadata.setName(QString("Midi.%1").arg(*this->id().val()));

    m_device = "tata";
    for(int i = 0; i < 10; i++)
    {
        auto n = new Note{Id<Note>(i), this};
        n->setPitch(64 + i);
        n->setStart(0.1 + i * 0.05);
        n->setDuration(0.1 + i * 0.05);
        notes.add(n);
    }
}

ProcessModel::ProcessModel(
        const ProcessModel& source,
        const Id<Process::ProcessModel>& id,
        QObject* parent):
    Process::ProcessModel{source.duration(), id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
{
    //TODO clone notes
}

ProcessModel::~ProcessModel()
{
}

void ProcessModel::setDurationAndScale(const TimeValue& newDuration)
{
    setDuration(newDuration);
    emit notesChanged();
}

void ProcessModel::setDurationAndGrow(const TimeValue& newDuration)
{
    auto ratio = duration() / newDuration;

    for(auto& note : notes)
        note.scale(ratio);

    setDuration(newDuration);
    emit notesChanged();
}

void ProcessModel::setDurationAndShrink(const TimeValue& newDuration)
{
    auto ratio = duration() / newDuration;
    auto inv_ratio = newDuration / duration();

    std::vector<Id<Note>> toErase;
    for(Note& n : notes)
    {
        if (n.end() >= inv_ratio)
        {
            toErase.push_back(n.id());
        }
        else
        {
            n.scale(ratio);
        }
    }

    for(auto& note : toErase)
    {
        notes.remove(note);
    }
    setDuration(newDuration);
    emit notesChanged();
}
}



template<>
void Visitor<Reader<DataStream>>::readFrom(
        const Midi::NoteData& n)
{
}

template<>
void Visitor<Writer<DataStream>>::writeTo(
        Midi::NoteData& n)
{
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(
        const Midi::NoteData& n)
{
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(
        Midi::NoteData& n)
{
}


template<>
void Visitor<Reader<DataStream>>::readFrom(
        const Midi::Note& n)
{
}

template<>
void Visitor<Writer<DataStream>>::writeTo(
        Midi::Note& n)
{
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(
        const Midi::Note& n)
{
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(
        Midi::Note& n)
{
}



template<>
void Visitor<Reader<DataStream>>::readFrom_impl(const Midi::ProcessModel& proc)
{
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Midi::ProcessModel& proc)
{
    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom_impl(const Midi::ProcessModel& proc)
{
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Midi::ProcessModel& proc)
{
}
