#pragma once

#include <QMainWindow>
#include <QPair>
#include <QString>
#include <vector>

class QCloseEvent;
class QDockWidget;
class QEvent;
class QObject;
class QTabWidget;

#include <iscore/model/Identifier.hpp>
#include <iscore_lib_base_export.h>

namespace iscore
{
class DocumentModel;
class DocumentView;
class PanelView;
class PanelDelegate;
class Presenter;
struct ApplicationContext;
/**
 * @brief The main display of the application.
 */
class ISCORE_LIB_BASE_EXPORT View final : public QMainWindow
{
  Q_OBJECT
public:
  View(QObject* parent);

  void setPresenter(Presenter*);

  void addDocumentView(iscore::DocumentView*);
  void setupPanel(PanelDelegate* v);

  void closeDocument(iscore::DocumentView* doc);
  void restoreLayout();
  void closeEvent(QCloseEvent*) override;

signals:
  void activeDocumentChanged(const Id<DocumentModel>&);
  void closeRequested(const Id<DocumentModel>&);

  void activeWindowChanged();
  void sizeChanged(QSize);

public slots:
  void on_fileNameChanged(DocumentView* d, const QString& newName);

private:
  void changeEvent(QEvent*) override;
  void resizeEvent(QResizeEvent*) override;

  std::vector<QPair<PanelDelegate*, QDockWidget*>> m_leftPanels;
  std::vector<QPair<PanelDelegate*, QDockWidget*>> m_rightPanels;

  Presenter* m_presenter{};
  QTabWidget* m_tabWidget{};
};
}
