#pragma once

#include <core/document/Document.hpp>

#include <score/model/Identifier.hpp>

#include <score_lib_base_export.h>


#include <QMainWindow>
#include <QPair>
#include <QString>

#include <verdigris>

#include <vector>

class QCloseEvent;
class QDockWidget;
class QEvent;
class QObject;
class QTabWidget;
class QLabel;
namespace score
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
class SCORE_LIB_BASE_EXPORT View final : public QMainWindow
{
  W_OBJECT(View)
public:
  explicit View(QObject* parent);
  ~View() override;

  void setPresenter(Presenter*);

  void addDocumentView(score::DocumentView*);
  void setupPanel(PanelDelegate* v);

  void closeDocument(score::DocumentView* doc);
  void restoreLayout();
  void closeEvent(QCloseEvent*) override;


public:
  void activeDocumentChanged(const Id<DocumentModel>& arg_1)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, activeDocumentChanged, arg_1)
  void closeRequested(const Id<DocumentModel>& arg_1)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, closeRequested, arg_1)

  void ready() E_SIGNAL(SCORE_LIB_BASE_EXPORT, ready)
  void sizeChanged(QSize arg_1)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, sizeChanged, arg_1)

public:
  void on_fileNameChanged(DocumentView* d, const QString& newName);
  W_SLOT(on_fileNameChanged);

private:
  bool event(QEvent* event) override;
  void changeEvent(QEvent*) override;
  void resizeEvent(QResizeEvent*) override;

  std::map<QWidget*, DocumentView*> m_documents;
  std::vector<QPair<PanelDelegate*, QDockWidget*>> m_leftPanels;
  std::vector<QPair<PanelDelegate*, QDockWidget*>> m_rightPanels;

  Presenter* m_presenter{};
  QTabWidget* m_tabWidget{};
  QTabWidget* m_bottomTabs{};
  QLabel* m_status{};

};
}
