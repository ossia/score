#pragma once

#include <score/model/Identifier.hpp>

#include <core/document/Document.hpp>

#include <QActionGroup>
#include <QMainWindow>
#include <QPair>
#include <QString>
#include <QVBoxLayout>

#include <score_lib_base_export.h>

#include <QStackedWidget>
#include <vector>
#include <verdigris>

#include <score/widgets/MarginLess.hpp>

class QCloseEvent;
class QDockWidget;
class QEvent;
class QSplitter;
class QObject;
class QTabWidget;
class QLabel;
namespace score
{
class PanelStatus;
class FixedTabWidget;
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
  void activeDocumentChanged(const Id<DocumentModel>& arg_1) E_SIGNAL(
      SCORE_LIB_BASE_EXPORT,
      activeDocumentChanged,
      arg_1) void closeRequested(const Id<DocumentModel>& arg_1)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, closeRequested, arg_1)

          void ready() E_SIGNAL(SCORE_LIB_BASE_EXPORT, ready) void sizeChanged(
              QSize arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, sizeChanged, arg_1)

              public
      : void on_fileNameChanged(DocumentView* d, const QString& newName);
  W_SLOT(on_fileNameChanged);

  QWidget* centralDocumentWidget{};
  QSplitter* rightSplitter{};
  FixedTabWidget* leftTabs{};
  //QTabWidget* rightTabs{};
  FixedTabWidget* bottomTabs{};
  QTabWidget* centralTabs{};
  QWidget* transportBar{};
private:
  bool event(QEvent* event) override;
  void changeEvent(QEvent*) override;
  void resizeEvent(QResizeEvent*) override;

  std::map<QWidget*, DocumentView*> m_documents;
  std::vector<QPair<PanelDelegate*, QWidget*>> m_leftPanels;
  std::vector<QPair<PanelDelegate*, QWidget*>> m_rightPanels;

  Presenter* m_presenter{};
  QLabel* m_status{};
};
class FixedTabWidget : public QWidget
{
  W_OBJECT(FixedTabWidget)
public:
  FixedTabWidget() noexcept;

  QActionGroup* actionGroup() const noexcept;
  QToolBar* toolbar() const noexcept;

  QSize sizeHint() const override;
  void setTab(int index);
  std::pair<int, QAction*> addTab(QWidget* widg, const score::PanelStatus& v);

  QBrush brush;
  void paintEvent(QPaintEvent* ev) override;
  void actionTriggered(bool b) W_SIGNAL(actionTriggered, b)

private:
  score::MarginLess<QVBoxLayout> m_layout;
  QToolBar* m_buttons{};
  QStackedWidget m_stack;
  QActionGroup* m_actGrp{};
};
}
