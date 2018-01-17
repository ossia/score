#include "MidiDrop.hpp"
#include <ModernMIDI/midi_file_reader.h>
#include <Midi/MidiNote.hpp>
#include <QMimeData>
#include <QByteArray>
#include <QFile>
#include <QUrl>
namespace Midi
{

bool DropMidiInSenario::drop(
    const Scenario::TemporalScenarioPresenter& pres, QPointF pos, const QMimeData* mime)
{
  qDebug() << mime->formats();
  if (mime->formats().contains("audio/midi") || mime->formats().contains("audio/x-midi"))
  {
    auto data = mime->data("audio/midi");
    qDebug() << data;
    qDebug() << mime->text();
  }
  else if(mime->formats().contains("text/uri-list"))
  {
    if(mime->urls().empty())
      return false;
    qDebug() << mime->urls();
    QFile f(mime->urls().first().toLocalFile());
    if(f.open(QIODevice::ReadOnly))
    {
      auto dat = f.readAll();
      mm::MidiFileReader reader;
      std::vector<uint8_t> raw;
      for(auto v : dat)
      {
        raw.push_back(v);
      }
      reader.parse(raw);
      std::vector<Midi::NoteData> midi_dat;
      std::map<int, mm::MidiMessage> notes;

      int tick = 0;
      for(auto& t : reader.tracks)
      {
        for(auto& ev : t)
        {
          switch(ev->m->getMessageType())
          {
            case mm::MessageType::NOTE_ON:
            {
              qDebug() << ev->tick;
              notes.insert({ev->m->data[1], *ev->m});
              break;
            }
            case mm::MessageType::NOTE_OFF:
            {
              qDebug() << ev->tick;
              notes.erase(ev->m->data[1]);
              break;
            }
            default:
              break;
          }
        }
      }
    }
  }
    /*
    Mime<Process::ProcessData>::Deserializer des{*mime};
    Process::ProcessData p = des.deserialize();

    RedoMacroCommandDispatcher<Scenario::Command::AddProcessInNewBoxMacro> m{
        pres.context().context.commandStack};

    // Create a box.
    const Scenario::ProcessModel& scenar = pres.model();
    Scenario::Point pt = pres.toScenarioPoint(pos);

    // 5 seconds.
    // TODO instead use a percentage of the currently displayed view
    TimeVal t = std::chrono::seconds{5};

    // Create the beginning
    auto start_cmd = new Scenario::Command::CreateTimeSync_Event_State{
        scenar, pt.date, pt.y};
    m.submitCommand(start_cmd);

    // Create a box with the duration of the longest song
    auto box_cmd
        = new Scenario::Command::CreateInterval_State_Event_TimeSync{
            scenar, start_cmd->createdState(), pt.date + t, pt.y};
    m.submitCommand(box_cmd);
    auto& interval = scenar.interval(box_cmd->createdInterval());

    // Create process
    auto process_cmd
        = new Scenario::Command::AddOnlyProcessToInterval{interval, p.key, p.customData};
    m.submitCommand(process_cmd);

    // Create a new slot
    auto slot_cmd = new Scenario::Command::AddSlotToRack{interval};
    m.submitCommand(slot_cmd);

    // Add a new layer in this slot.
    auto& proc = interval.processes.at(process_cmd->processId());
    auto layer_cmd = new Scenario::Command::AddLayerModelToSlot{
        SlotPath{interval, int(interval.smallView().size() - 1)},
        proc};

    m.submitCommand(layer_cmd);

    // Finally we show the newly created rack
    auto show_cmd = new Scenario::Command::ShowRack{interval};
    m.submitCommand(show_cmd);

    m.commit();
    return true;
    */


  return false;
}

}

