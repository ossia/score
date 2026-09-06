#include "UIPlacement.hpp"

#include <QSettings>
#include <QStringList>

#include <algorithm>
#include <vector>

namespace Process
{
static void notifyChange();

const QStringList& UIPlacementSettings::names()
{
  static const QStringList list{
      QStringLiteral("Window"), QStringLiteral("Side panel"), QStringLiteral("Central")};
  return list;
}

QString UIPlacementSettings::toString(UIPlacement p)
{
  const int i = static_cast<int>(p);
  const auto& n = names();
  return (i >= 0 && i < n.size()) ? n[i] : n.front();
}

UIPlacement UIPlacementSettings::fromString(const QString& s, UIPlacement fallback)
{
  const int i = names().indexOf(s);
  return i == -1 ? fallback : static_cast<UIPlacement>(i);
}

UIPlacement UIPlacementSettings::scriptEditorPlacement()
{
  return fromString(
      QSettings{}.value(scriptEditorKey).toString(), defaultScriptEditorPlacement);
}

const QStringList& UIPlacementSettings::previewNames()
{
  static const QStringList list{QStringLiteral("Background"), QStringLiteral("None")};
  return list;
}

QString UIPlacementSettings::toString(PreviewSource p)
{
  const int i = static_cast<int>(p);
  const auto& n = previewNames();
  return (i >= 0 && i < n.size()) ? n[i] : n.front();
}

PreviewSource
UIPlacementSettings::previewFromString(const QString& s, PreviewSource fallback)
{
  const int i = previewNames().indexOf(s);
  return i == -1 ? fallback : static_cast<PreviewSource>(i);
}

PreviewSource UIPlacementSettings::scriptEditorPreview()
{
  return previewFromString(
      QSettings{}.value(scriptEditorPreviewKey).toString(), defaultScriptEditorPreview);
}

void UIPlacementSettings::setScriptEditorPreview(PreviewSource p)
{
  QSettings{}.setValue(scriptEditorPreviewKey, toString(p));
  notifyChange();
}

struct ChangeListener
{
  QPointer<QObject> context;
  std::function<void()> f;
};

static std::vector<ChangeListener>& changeListeners()
{
  static std::vector<ChangeListener> l;
  return l;
}

static void notifyChange()
{
  auto& l = changeListeners();
  std::erase_if(l, [](const ChangeListener& c) { return !c.context; });
  for(auto& c : l)
    c.f();
}

void UIPlacementSettings::addChangeListener(QObject* context, std::function<void()> f)
{
  changeListeners().push_back({context, std::move(f)});
}

void UIPlacementSettings::setScriptEditorPlacement(UIPlacement p)
{
  QSettings{}.setValue(scriptEditorKey, toString(p));
  notifyChange();
}

void UIPlacementSettings::setProcessUIPlacement(UIPlacement p)
{
  QSettings{}.setValue(processUIKey, toString(p));
  notifyChange();
}

UIPlacement UIPlacementSettings::processUIPlacement()
{
  return fromString(
      QSettings{}.value(processUIKey).toString(), defaultProcessUIPlacement);
}
}
