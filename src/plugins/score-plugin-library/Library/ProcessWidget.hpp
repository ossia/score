#pragma once
#include <Library/ProcessTreeView.hpp>
#include <Library/PresetListView.hpp>
#include <score/tools/std/Optional.hpp>

#include <QTreeView>
#include <QSplitter>

namespace score
{
struct GUIApplicationContext;
}
namespace Library
{
class ProcessesItemModel;
class PresetItemModel;

class ProcessWidget : public QWidget
{
public:
  ProcessWidget(const score::GUIApplicationContext& ctx, QWidget* parent);
  ~ProcessWidget();

private:
  ProcessesItemModel* m_processModel{};
  PresetItemModel* m_presetModel{};
  ProcessTreeView m_tv;
  PresetListView m_lv;
};
}
