#include "MidiNoteEditor.hpp"

#include <Midi/Commands/AddNote.hpp>
#include <Midi/Commands/RemoveNotes.hpp>
#include <Midi/MidiNote.hpp>
#include <Midi/MidiPresenter.hpp>
#include <Midi/MidiProcess.hpp>
#include <Midi/MidiView.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

#include <QGraphicsScene>
#include <QGraphicsView>

#include <Scenario/Document/ScenarioDocument/ProcessFocusManager.hpp>

namespace Midi
{
bool NoteEditor::copy(
    JSONReader& r,
    const Selection& s,
    const score::DocumentContext& ctx)
{
  if (!s.empty())
  {
    std::vector<Midi::NoteData> noteDataList;
    for (auto item : s)
    {
      if (auto model = qobject_cast<const Midi::Note*>(item.data()))
      {
        noteDataList.push_back(model->noteData());
      }
    }
    if (!noteDataList.empty())
    {
      r.stream.StartObject();
      r.obj["Notes"] = noteDataList;
      r.stream.EndObject();
      return true;
    }
  }
  return false;
}

bool NoteEditor::paste(
    QPoint pos,
    QObject* focusedObject,
    const QMimeData& mime,
    const score::DocumentContext& ctx)
{
  if (!focusedObject)
    return false;
  auto pres = qobject_cast<Midi::Presenter*>(focusedObject);
  if (!pres)
    return false;

  auto& mm = static_cast<const Midi::ProcessModel&>(pres->model());
  // Get the QGraphicsView
  auto views = pres->view().scene()->views();
  if (views.empty())
    return false;

  auto view = views.front();

  // Find where to paste in the scenario
  auto view_pt = view->mapFromGlobal(pos);
  auto scene_pt = view->mapToScene(view_pt);
  auto& mv = pres->view();
  auto mv_pt = mv.mapFromScene(scene_pt);

  auto obj = readJson(mime.data("text/plain"));
  if (!obj.IsObject())
    return false;
  JSONWriter w(obj);
  std::vector<NoteData> data;
  if (auto notes = w.obj.tryGet("Notes"))
    data = notes->to<std::vector<NoteData>>();
  else
    return false;
  if (data.empty())
    return false;
  //compute and apply copy offset
  auto offset = mv_pt.x() / mv.width();
  std::sort(
      data.begin(),
      data.end(),
      [](NoteData& a, NoteData& b) { return (a.m_start < b.m_start); });
  auto first_start = data[0].start();
  for (int i = 0; i < data.size(); i++)
  {
    data[i].setStart(data[i].start() - first_start + offset);
  }
  //TODO do pitch offset and keep note selected
  // Submit the paste command
  if (data.size() > 1)
  {
    auto cmd = new Midi::AddNotes(mm, data);
    CommandDispatcher<>{ctx.commandStack}.submit(cmd);
  }
  else
  {
    auto cmd = new Midi::AddNote(mm, data[0]);
    CommandDispatcher<>{ctx.commandStack}.submit(cmd);
  }
  return true;
}

bool NoteEditor::remove(const Selection& s, const score::DocumentContext& ctx)
{
  if (!s.empty())
  {
    std::vector<Id<Note>> noteIdList;
    for (auto item : s)
    {
      if (auto model = qobject_cast<const Midi::Note*>(item.data()))
      {
        if (auto parent
            = qobject_cast<const Midi::ProcessModel*>(model->parent()))
        {
          noteIdList.push_back(model->id());
        }
      }
      else
      {
        return false;
      }
    }

    if (!noteIdList.empty())
    {
      auto parent = qobject_cast<const Midi::ProcessModel*>(
          s.begin()->data()->parent());
      CommandDispatcher<>{ctx.commandStack}.submit<Midi::RemoveNotes>(
          *parent, noteIdList);
      return true;
    }
  }
  return false;
}
}
