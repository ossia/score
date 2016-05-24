#pragma once
#include <iscore/actions/Action.hpp>

ISCORE_DECLARE_ACTION(Undo, Common, QKeySequence::Undo)
ISCORE_DECLARE_ACTION(Redo, Common, QKeySequence::Redo)

ISCORE_DECLARE_ACTION(New, Common, QKeySequence::New)

ISCORE_DECLARE_ACTION(Load, Common, QKeySequence::Open)
ISCORE_DECLARE_ACTION(Save, Common, QKeySequence::Save)
ISCORE_DECLARE_ACTION(SaveAs, Common, QKeySequence::SaveAs)

ISCORE_DECLARE_ACTION(Close, Common, QKeySequence::Close)
ISCORE_DECLARE_ACTION(Quit, Common, QKeySequence::Quit)

ISCORE_DECLARE_ACTION(OpenSettings, Common, QKeySequence::Preferences)
ISCORE_DECLARE_ACTION(About, Common, QKeySequence::UnknownKey)
