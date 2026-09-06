#include "AboutDialog.hpp"

#include <score/widgets/Pixmap.hpp>

#include <core/presenter/AboutWidget.hpp>

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace score
{
AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
  setWindowTitle(tr("About ossia score"));
  resize(720, 640);

  // Same dark panel as the start screen
  setAutoFillBackground(true);
  QPalette pal = palette();
  pal.setColor(QPalette::Window, QColor{"#211f1f"});
  setPalette(pal);

  auto lay = new QVBoxLayout{this};
  lay->setContentsMargins(28, 20, 28, 20);
  lay->setSpacing(14);

  // Header: logo and name
  {
    auto header = new QHBoxLayout;
    header->setSpacing(16);
    auto logo = new QLabel{this};
    logo->setPixmap(
        QPixmap{":/about/logos/ossia.png"}.scaled(
            56, 56, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    header->addWidget(logo);

    auto title = new QLabel{QStringLiteral("ossia score"), this};
    // px, not pt: pt would shrink on macOS' 72 DPI.
    QFont titleFont("Montserrat");
    titleFont.setPixelSize(35);
    titleFont.setBold(true);
    title->setFont(titleFont);
    QPalette tp = title->palette();
    tp.setColor(QPalette::WindowText, QColor{"#03C3DD"});
    title->setPalette(tp);
    header->addWidget(title);
    header->addStretch();
    lay->addLayout(header);
  }

  lay->addWidget(new AboutWidget{AboutWidget::defaultStyle(), this}, 1);
}
}
