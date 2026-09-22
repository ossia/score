#include "GfxApplicationPlugin.hpp"

#include <Process/PreviewSettings.hpp>

#include <Execution/DocumentPlugin.hpp>

#include <score/model/Skin.hpp>
#include <score/widgets/HelpInteraction.hpp>
#include <score/widgets/SetIcons.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QAction>
#include <QToolBar>

namespace Gfx
{

DocumentPlugin::DocumentPlugin(const score::DocumentContext& ctx, QObject* parent)
    : score::DocumentPlugin{ctx, "Gfx::DocumentPlugin", parent}
    , context{ctx}
{
  auto& exec_plug = ctx.plugin<Execution::DocumentPlugin>();
  exec_plug.registerAction(exec);
}

DocumentPlugin::~DocumentPlugin() { }


ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& app)
    : GUIApplicationPlugin{app}
{
  // Early: the canvas watchers have to be in place before a context can be lost.
}

score::GUIElements ApplicationPlugin::makeGUIElements()
{
  GUIElements e;

  auto bar = new QToolBar{QObject::tr("Graphics")};

  auto preview_act = new QAction{QObject::tr("Show shader previews"), bar};
  preview_act->setCheckable(true);
  preview_act->setChecked(Process::PreviewSettings::instance().enabled());
  score::setHelp(
      preview_act,
      QObject::tr("Render the shader previews in the library and the inspector"));
  setIcons(
      preview_act, QStringLiteral(":/icons/shader_preview_on.png"),
      QStringLiteral(":/icons/shader_preview_hover.png"),
      QStringLiteral(":/icons/shader_preview_off.png"),
      QStringLiteral(":/icons/shader_preview_disabled.png"));

  QObject::connect(preview_act, &QAction::toggled, preview_act, [](bool checked) {
    Process::PreviewSettings::instance().setEnabled(checked);
  });

  // The toggle is a view switch, not an edition tool: it takes the far end of
  // the row instead of sitting in the tools' run.
  auto spacer = new QWidget{bar};
  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  bar->addWidget(spacer);
  bar->addAction(preview_act);
  bar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  score::setSkinIconSize(bar, 24);

  e.toolbars.emplace_back(
      bar, StringKey<score::Toolbar>("Graphics"), Qt::TopToolBarArea, 900);

  return e;
}

void ApplicationPlugin::on_createdDocument(score::Document& doc)
{
  doc.model().addPluginModel(new DocumentPlugin{doc.context(), &doc.model()});
}

}
