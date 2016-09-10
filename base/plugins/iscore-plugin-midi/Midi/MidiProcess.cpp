#include <Midi/MidiProcess.hpp>

namespace Midi
{
ProcessModel::ProcessModel(
        const TimeValue& duration,
        const Id<Process::ProcessModel>& id,
        QObject* parent):
    Process::ProcessModel{duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
{
    metadata().setName(QString("Midi.%1").arg(*this->id().val()));

    m_device = "tata";
    for(int i = 0; i < 10; i++)
    {
        auto n = new Note{Id<Note>(i), this};
        n->setPitch(64 + i);
        n->setStart(0.1 + i * 0.05);
        n->setDuration(0.1 + (9 - i) * 0.05);
        n->setVelocity(i * 127. / 9.);
        notes.add(n);
    }
}

ProcessModel::ProcessModel(
        const ProcessModel& source,
        const Id<Process::ProcessModel>& id,
        QObject* parent):
    Process::ProcessModel{source.duration(), id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
{
    m_device = source.device();
    m_channel = source.channel();
    for(Note& note : source.notes)
    {
        notes.add(note.clone(note.id(), this));
    }
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
    m_stream << n.start << n.duration << n.pitch << n.velocity;
}

template<>
void Visitor<Writer<DataStream>>::writeTo(
        Midi::NoteData& n)
{
    m_stream >> n.start >> n.duration >> n.pitch >> n.velocity;
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(
        const Midi::NoteData& n)
{
    m_obj["Start"] = n.start;
    m_obj["Duration"] = n.duration;
    m_obj["Pitch"] = n.pitch;
    m_obj["Velocity"] = n.velocity;
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(
        Midi::NoteData& n)
{
    n.start = m_obj["Start"].toDouble();
    n.duration = m_obj["Duration"].toDouble();
    n.pitch = m_obj["Pitch"].toInt();
    n.velocity = m_obj["Velocity"].toInt();
}


template<>
void Visitor<Reader<DataStream>>::readFrom(
        const Midi::Note& n)
{
    readFrom(static_cast<const IdentifiedObject<Midi::Note>&>(n));

    m_stream << n.noteData();
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(
        Midi::Note& n)
{
    Midi::NoteData d;
    m_stream >> d;
    n.setData(d);
    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(
        const Midi::Note& n)
{
    readFrom(static_cast<const IdentifiedObject<Midi::Note>&>(n));
    readFrom(n.noteData());
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(
        Midi::Note& n)
{
    Midi::NoteData d;
    writeTo(d);
    n.setData(d);
}



template<>
void Visitor<Reader<DataStream>>::readFrom_impl(const Midi::ProcessModel& proc)
{
    m_stream << proc.device() << proc.channel();

    const auto& notes = proc.notes;

    m_stream << (int32_t)notes.size();
    for(const auto& n : notes)
    {
        readFrom(n);
    }

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Midi::ProcessModel& proc)
{
    m_stream >> proc.m_device >> proc.m_channel;
    int n;
    m_stream >> n;
    for (int i = 0; i < n; i++)
    {
        proc.notes.add(new Midi::Note{*this, &proc});
    }
    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom_impl(const Midi::ProcessModel& proc)
{
    m_obj["Device"] = proc.device();
    m_obj["Channel"] = proc.channel();
    m_obj["Notes"] = toJsonArray(proc.notes);
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Midi::ProcessModel& proc)
{
    proc.setDevice(m_obj["Device"].toString());
    proc.setChannel(m_obj["Channel"].toInt());

    for(const auto& json_vref : m_obj["Notes"].toArray())
    {
        auto note = new Midi::Note{
                Deserializer<JSONObject>{json_vref.toObject() },
                &proc};
        proc.notes.add(note);
    }
}
