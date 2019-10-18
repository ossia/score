// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <score_integration.hpp>

#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Application/ScenarioValidity.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Commands/Scenario/Deletions/RemoveMacro.hpp>

#include <Scenario/Commands/ClearSelection.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <QLocale>
#include <clocale>

// Taken from https://stackoverflow.com/a/1824900/1495627
template<typename T, typename F>
void for_each_pair(const T& vec, F f) {
  const auto begin = vec.cbegin();
  const auto end = vec.cend();
  for (auto i = begin; i != end; ++i) {
    for (auto j = i; ++j != end;) {
      f(*i, *j);
    }
  }
}

auto do_remove(
    const score::DocumentContext& ctx,
    const Scenario::ProcessModel& scenar,
    Selection sel)
{
  using validator = Scenario::ScenarioValidityChecker;
  struct {
    void submit(score::Command* cmd)
    {
      cmd->redo(ctx);
      cmds.push_back(cmd);
    }

    void redo()
    {
      for(auto it = std::begin(cmds); it != std::end(cmds); ++it)
        (*it)->redo(ctx);
    }
    void undo()
    {
      for(auto it = std::rbegin(cmds); it != std::rend(cmds); ++it)
        (*it)->undo(ctx);
    }
    const score::DocumentContext& ctx;
    std::vector<score::Command*> cmds;
  } macro{ctx, {}};

  Scenario::Command::setupRemoveMacro(scenar, sel, macro);

  validator::checkValidity(scenar);
  macro.undo();
  validator::checkValidity(scenar);
  macro.redo();
  validator::checkValidity(scenar);
  macro.undo();
  validator::checkValidity(scenar);
  return macro;
}

static void run_test()
{
  const auto& ctx = score::GUIAppContext();
  auto doc = ctx.docManager.loadFile(
      ctx, "testdata/scenario-removal-test.score");
  qApp->processEvents();

  auto& doc_pm = doc->model().modelDelegate();
  auto& scenario_dm = static_cast<Scenario::ScenarioDocumentModel&>(doc_pm);

  auto& scenar = static_cast<Scenario::ProcessModel&>(
      *scenario_dm.baseInterval().processes.begin());

  std::vector<ObjectPath> all_objects;
  qInstallMessageHandler(nullptr);

  for (auto& elt : scenar.intervals)
  {
    all_objects.push_back(score::IDocument::path(elt).unsafePath());
  }
  for (auto& elt : scenar.timeSyncs)
  {
    if(&elt != &scenar.startTimeSync())
      all_objects.push_back(score::IDocument::path(elt).unsafePath());
  }
  for (auto& elt : scenar.events)
  {
    if(&elt != &scenar.startEvent())
      all_objects.push_back(score::IDocument::path(elt).unsafePath());
  }
  for (auto& elt : scenar.states)
  {
    if(elt.eventId() != scenar.startEvent().id())
      all_objects.push_back(score::IDocument::path(elt).unsafePath());
  }

  for(auto& obj : all_objects)
  {
    Selection sel;
    sel.append(&obj.find<IdentifiedObjectAbstract>(doc->context()));

    Scenario::Command::RemoveSelection cmd(scenar, sel);

  }

  for(auto& obj : all_objects)
  {
    Selection sel;
    sel.append(&obj.find<IdentifiedObjectAbstract>(doc->context()));

    do_remove(doc->context(), scenar, sel);
  }

  for_each_pair(all_objects, [&] (const auto& obj1, const auto& obj2) {
    Selection sel;
    sel.append(&obj1.template find<IdentifiedObjectAbstract>(doc->context()));
    sel.append(&obj2.template find<IdentifiedObjectAbstract>(doc->context()));

    auto m0 = sel.at(0)->findChild<score::ModelMetadata*>(QString{}, Qt::FindDirectChildrenOnly);
    auto m1 = sel.at(1)->findChild<score::ModelMetadata*>(QString{}, Qt::FindDirectChildrenOnly);
    qDebug() << m0->getName() << sel.at(0)->id_val() << m1->getName() << sel.at(1)->id_val();

    do_remove(doc->context(), scenar, sel);
  });

  for_each_pair(all_objects, [&] (const auto& obj1, const auto& obj2) {
    Selection sel;
    sel.append(&obj2.template find<IdentifiedObjectAbstract>(doc->context()));
    sel.append(&obj1.template find<IdentifiedObjectAbstract>(doc->context()));

    qDebug() << sel.at(0)->objectName() << sel.at(0)->id_val() << sel.at(1)->objectName() << sel.at(1)->id_val();

    do_remove(doc->context(), scenar, sel);
  });

  ctx.docManager.forceCloseDocument(ctx, *doc);
  QApplication::processEvents();
}

SCORE_INTEGRATION_TEST(run_test)
