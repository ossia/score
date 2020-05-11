#include "Inspector.hpp"

#include <score/document/DocumentContext.hpp>
#include <score/widgets/JS/JSEdit.hpp>

#include <QFormLayout>
#include <QLabel>

namespace Gfx::Mesh
{
InspectorWidget::InspectorWidget(
    const Gfx::Mesh::Model& object,
    const score::DocumentContext& context,
    QWidget* parent)
    : InspectorWidgetDelegate_T{object, parent}
{
  auto lay = new QHBoxLayout{this};
  auto edit = new JSEdit;
  lay->addWidget(edit);

  edit->setPlainText(object.fragment());
  connect(edit, &JSEdit::textChanged, this, [&object, &context, edit] {
    context.dispatcher.submit<ChangeMeshFragmentShader>(
        object, edit->toPlainText());
  });
  connect(
      edit,
      &JSEdit::editingFinished,
      this,
      [&object, &context](const QString& txt) {
        context.dispatcher.submit<ChangeMeshFragmentShader>(object, txt);
        context.dispatcher.commit();
      });
}

InspectorWidget::~InspectorWidget() {}
}
