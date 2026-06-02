#pragma once

enum NoteCounterStyle
{
	UMP,
	MIDITrail
};

enum NoteCounterAlignment
{
	TopLeft,
	TopRight
};

static const NoteCounterStyle DEFAULT_NOTE_COUNTER_STYLE = NoteCounterStyle::UMP;
static const NoteCounterAlignment DEFAULT_NOTE_COUNTER_ALIGNMENT = NoteCounterAlignment::TopRight;
static const int DEFAULT_NOTE_COUNTER_WIDTH = 200;