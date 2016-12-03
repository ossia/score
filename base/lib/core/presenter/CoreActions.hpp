#pragma once
#include <iscore/actions/Action.hpp>

ISCORE_DECLARE_ACTION(Undo, "&Undo", Common, QKeySequence::Undo)
ISCORE_DECLARE_ACTION(Redo, "&Redo", Common, QKeySequence::Redo)

ISCORE_DECLARE_ACTION(New, "&New", Common, QKeySequence::New)

ISCORE_DECLARE_ACTION(Load, "&Load", Common, QKeySequence::Open)
ISCORE_DECLARE_ACTION(Save, "&Save", Common, QKeySequence::Save)
ISCORE_DECLARE_ACTION(SaveAs, "&Save as...", Common, QKeySequence::SaveAs)

ISCORE_DECLARE_ACTION(Close, "&Close", Common, QKeySequence::Close)
ISCORE_DECLARE_ACTION(Quit, "&Quit", Common, QKeySequence::Quit)

ISCORE_DECLARE_ACTION(
    OpenSettings, "&Settings", Common, QKeySequence::Preferences)

ISCORE_DECLARE_ACTION(
    RestoreLayout, "&Restore Layout", Common, QKeySequence::UnknownKey)
ISCORE_DECLARE_ACTION(About, "&About", Common, QKeySequence::UnknownKey)
