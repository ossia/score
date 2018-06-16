// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ActionManager.hpp"

#include <core/document/Document.hpp>

namespace score
{

ActionManager::ActionManager()
{
  onDocumentChange(std::make_unique<EnableActionIfDocument>());
}

ActionManager::~ActionManager()
{
}

void ActionManager::insert(Action val)
{
  SCORE_ASSERT(m_container.find(val.key()) == m_container.end());
  m_container.insert(std::make_pair(val.key(), std::move(val)));
}

void ActionManager::insert(std::vector<Action> vals)
{
  for (auto& val : vals)
  {
    insert(std::move(val));
  }
}

void ActionManager::reset(score::Document* doc)
{

  // Cleanup
  QObject::disconnect(focusConnection);
  QObject::disconnect(selectionConnection);

  MaybeDocument mdoc{};
  if (doc)
  {
    mdoc = &doc->context();

    focusConnection
        = con(doc->focusManager(), &FocusManager::changed, this, [=] {
            focusChanged(mdoc);
          });
    selectionConnection = con(
        doc->selectionStack(), &SelectionStack::currentSelectionChanged, this,
        [=](const auto&) { this->selectionChanged(mdoc); },
        Qt::QueuedConnection);
  }

  // Reset all the actions
  documentChanged(mdoc);
  focusChanged(mdoc);
  selectionChanged(mdoc);
}

void ActionManager::onDocumentChange(std::shared_ptr<ActionCondition> cond)
{
  SCORE_ASSERT(bool(cond));
  SCORE_ASSERT(m_docConditions.find(cond->key()) == m_docConditions.end());

  auto p = std::make_pair(cond->key(), std::move(cond));
  m_conditions.insert(p);
  m_docConditions.insert(std::move(p));
}

void ActionManager::onFocusChange(std::shared_ptr<ActionCondition> cond)
{
  SCORE_ASSERT(bool(cond));
  SCORE_ASSERT(m_focusConditions.find(cond->key()) == m_focusConditions.end());

  auto p = std::make_pair(cond->key(), std::move(cond));
  m_conditions.insert(p);
  m_focusConditions.insert(std::move(p));
}

void ActionManager::onSelectionChange(std::shared_ptr<ActionCondition> cond)
{
  SCORE_ASSERT(bool(cond));
  SCORE_ASSERT(
      m_selectionConditions.find(cond->key()) == m_selectionConditions.end());

  auto p = std::make_pair(cond->key(), std::move(cond));
  m_conditions.insert(p);
  m_selectionConditions.insert(std::move(p));
}

void ActionManager::onCustomChange(std::shared_ptr<ActionCondition> cond)
{
  SCORE_ASSERT(bool(cond));
  SCORE_ASSERT(
      m_customConditions.find(cond->key()) == m_customConditions.end());

  auto p = std::make_pair(cond->key(), std::move(cond));
  m_conditions.insert(p);
  m_customConditions.insert(std::move(p));
}

void ActionManager::documentChanged(MaybeDocument doc)
{
  for (auto& c_pair : m_docConditions)
  {
    ActionCondition& cond = *c_pair.second;
    cond.action(*this, doc);
  }
}

void ActionManager::focusChanged(MaybeDocument doc)
{
  for (auto& c_pair : m_focusConditions)
  {
    ActionCondition& cond = *c_pair.second;
    cond.action(*this, doc);
  }
}

void ActionManager::selectionChanged(MaybeDocument doc)
{
  for (auto& c_pair : m_selectionConditions)
  {
    ActionCondition& cond = *c_pair.second;
    cond.action(*this, doc);
  }
}

void ActionManager::resetCustomActions(MaybeDocument doc)
{
  for (auto& c_pair : m_customConditions)
  {
    ActionCondition& cond = *c_pair.second;
    cond.action(*this, doc);
  }
}
}
