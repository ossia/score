#pragma once
#include <score/actions/Action.hpp>

SCORE_DECLARE_ACTION(Undo, "&Undo", Common, QKeySequence::Undo)
SCORE_DECLARE_ACTION(Redo, "&Redo", Common, QKeySequence::Redo)

SCORE_DECLARE_ACTION(New, "&New", Common, QKeySequence::New)

SCORE_DECLARE_ACTION(Load, "&Load", Common, QKeySequence::Open)
SCORE_DECLARE_ACTION(Save, "&Save", Common, QKeySequence::Save)
SCORE_DECLARE_ACTION(SaveAs, "&Save as...", Common, QKeySequence::SaveAs)

SCORE_DECLARE_ACTION(Close, "&Close", Common, QKeySequence::Close)
SCORE_DECLARE_ACTION(Quit, "&Quit", Common, QKeySequence::Quit)

SCORE_DECLARE_ACTION(
    OpenSettings,
    "&Settings",
    Common,
    QKeySequence::Preferences)
SCORE_DECLARE_ACTION(
    OpenProjectSettings,
    "&Project Settings",
    Common,
    QKeySequence::UnknownKey)

SCORE_DECLARE_ACTION(
    RestoreLayout,
    "&Restore Layout",
    Common,
    QKeySequence::UnknownKey)
SCORE_DECLARE_ACTION(About, "&About", Common, QKeySequence::UnknownKey)
SCORE_DECLARE_ACTION(Help, "&Help", Common, QKeySequence::UnknownKey)
