#pragma once
#include <score/application/GUIApplicationContext.hpp>
#include <score/command/CommandStackFacade.hpp>
#include <score/command/Dispatchers/OngoingCommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/selection/FocusManager.hpp>
class IdentifiedObjectAbstract;
namespace score
{
class Document;
class CommandStack;
class SelectionStack;
class ObjectLocker;
class DocumentPlugin;
struct SCORE_LIB_BASE_EXPORT DocumentContext
{
  friend class score::Document;
  static DocumentContext fromDocument(score::Document& d);

  const score::GUIApplicationContext& app;
  score::Document& document;
  const score::CommandStackFacade commandStack;
  score::SelectionStack& selectionStack;
  score::ObjectLocker& objectLocker;
  const score::FocusFacade focus;
  QTimer& updateTimer;
  QTimer& coarseUpdateTimer;
  QTimer& execTimer;

  OngoingCommandDispatcher& dispatcher;

  const std::vector<DocumentPlugin*>& pluginModels() const;

  template <typename T>
  T& model() const
  {
    return IDocument::modelDelegate<T>(document);
  }

  template <typename T>
  T& plugin() const
  {
    using namespace std;
    const auto& pms = this->pluginModels();
    auto it = find_if(begin(pms), end(pms), [&](DocumentPlugin* pm) {
      return dynamic_cast<T*>(pm);
    });

    SCORE_ASSERT(it != end(pms));
    return *safe_cast<T*>(*it);
  }

  template <typename T>
  T* findPlugin() const
  {
    using namespace std;
    const auto& pms = this->pluginModels();
    auto it = find_if(begin(pms), end(pms), [&](DocumentPlugin* pm) {
      return dynamic_cast<T*>(pm);
    });

    if (it != end(pms))
      return safe_cast<T*>(*it);
    return nullptr;
  }

protected:
  DocumentContext(score::Document& d);
  DocumentContext(const DocumentContext&) = default;
  DocumentContext(DocumentContext&&) = default;
  DocumentContext& operator=(const DocumentContext&) = default;
  DocumentContext& operator=(DocumentContext&&) = default;
};

using MaybeDocument = const score::DocumentContext*;
}
