/*
  This file is part of the Ofi Labs X2 project.

  Copyright (C) 2011 Ariya Hidayat <ariya.hidayat@gmail.com>
  Copyright (C) 2010 Ariya Hidayat <ariya.hidayat@gmail.com>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the <organization> nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef OFILABS_JSEDIT
#define OFILABS_JSEDIT

#include <QColor>
#include <wobjectdefs.h>
#include <QPlainTextEdit>
#include <QScopedPointer>
#include <score_lib_base_export.h>

class JSEditPrivate;

class SCORE_LIB_BASE_EXPORT JSEdit final : public QPlainTextEdit
{
  W_OBJECT(JSEdit)





public:
  typedef enum
  {
    Background,
    Normal,
    Comment,
    Number,
    String,
    Operator,
    Identifier,
    Keyword,
    BuiltIn,
    Sidebar,
    LineNumber,
    Cursor,
    Marker,
    BracketMatch,
    BracketError,
    FoldIndicator
  } ColorComponent;

  JSEdit(QWidget* parent = 0);
  ~JSEdit();

  void setColor(ColorComponent component, const QColor& color);

  QStringList keywords() const;
  void setKeywords(const QStringList& keywords);

  bool isBracketsMatchingEnabled() const;
  bool isCodeFoldingEnabled() const;
  bool isLineNumbersVisible() const;
  bool isTextWrapEnabled() const;

  bool isFoldable(int line) const;
  bool isFolded(int line) const;

  void setError(int line);
  void clearError();

public:
  void editingFinished(QString arg_1) W_SIGNAL(editingFinished, arg_1);
  void focused() W_SIGNAL(focused);

public:
  void updateSidebar(); W_SLOT(updateSidebar);
  void
  mark(const QString& str, Qt::CaseSensitivity sens = Qt::CaseInsensitive); W_SLOT(mark);
  void setBracketsMatchingEnabled(bool enable); W_SLOT(setBracketsMatchingEnabled);
  void setCodeFoldingEnabled(bool enable); W_SLOT(setCodeFoldingEnabled);
  void setLineNumbersVisible(bool visible); W_SLOT(setLineNumbersVisible);
  void setTextWrapEnabled(bool enable); W_SLOT(setTextWrapEnabled);

  void fold(int line); W_SLOT(fold);
  void unfold(int line); W_SLOT(unfold);
  void toggleFold(int line); W_SLOT(toggleFold);

protected:
  void resizeEvent(QResizeEvent* e) override;
  void wheelEvent(QWheelEvent* e) override;
  void keyPressEvent(QKeyEvent* e) override;
  void focusOutEvent(QFocusEvent* event) override
  {
    editingFinished(this->toPlainText());
    event->ignore();
  }

private:
  void updateCursor(); W_SLOT(updateCursor);
  void do_updateSidebar(const QRect& rect, int d); W_SLOT(do_updateSidebar);

private:
  void focusInEvent(QFocusEvent* event) override;
  QScopedPointer<JSEditPrivate> d_ptr;
  Q_DECLARE_PRIVATE(JSEdit)
  Q_DISABLE_COPY(JSEdit)

W_PROPERTY(bool, textWrapEnabled READ isTextWrapEnabled WRITE setTextWrapEnabled)

W_PROPERTY(bool, lineNumbersVisible READ isLineNumbersVisible WRITE setLineNumbersVisible)

W_PROPERTY(bool, codeFoldingEnabled READ isCodeFoldingEnabled WRITE setCodeFoldingEnabled)

W_PROPERTY(bool, bracketsMatchingEnabled READ isBracketsMatchingEnabled WRITE setBracketsMatchingEnabled)
};

W_REGISTER_ARGTYPE(Qt::CaseSensitivity)
#endif // OFILABS_JSEDIT
