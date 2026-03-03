#include "SceneInspectorWidget.hpp"

#include <Inspector/InspectorLayout.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/tools/Bind.hpp>

#include <QLineEdit>

#include <ClipLauncher/Commands/SetSceneProperties.hpp>
#include <ClipLauncher/ProcessModel.hpp>
#include <ClipLauncher/SceneModel.hpp>

namespace ClipLauncher
{

const ProcessModel& SceneInspectorWidget::parentProcess() const
{
  return *safe_cast<const ProcessModel*>(m_scene.parent());
}

SceneInspectorWidget::SceneInspectorWidget(
    const SceneModel& scene, const score::DocumentContext& ctx, QWidget* parent)
    : InspectorWidgetBase{scene, ctx, parent, tr("Scene")}
    , m_scene{scene}
    , m_ctx{ctx}
{
  auto w = new QWidget;
  auto lay = new Inspector::Layout{w};

  // Name
  auto nameEdit = new QLineEdit{scene.name()};
  connect(nameEdit, &QLineEdit::editingFinished, this, [this, nameEdit] {
    if(nameEdit->text() != m_scene.name())
    {
      CommandDispatcher<> disp{m_ctx.commandStack};
      disp.submit<SetSceneName>(parentProcess(), m_scene, nameEdit->text());
    }
  });
  con(scene, &SceneModel::nameChanged, nameEdit, &QLineEdit::setText);
  lay->addRow(tr("Name"), nameEdit);

  updateAreaLayout({w});
}

SceneInspectorWidget::~SceneInspectorWidget() = default;

} // namespace ClipLauncher
