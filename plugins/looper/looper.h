/*
 * looper.h - handy tool to help with live looping functions
 *
 * Copyright (c) 2022 Oscar Acena <oscaracena/at/gmail/dot/com>
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 */


#ifndef LOOPER_TOOL_H
#define LOOPER_TOOL_H

#define LOOPER_TOOL_VERSION "0.2"

#include <QObject>
#include <QSharedPointer>

#include "GroupBox.h"
#include "MidiEventProcessor.h"
#include "MidiPort.h"
#include "MidiPortMenu.h"
#include "ToolPlugin.h"
#include "ToolPluginView.h"
#include "Track.h"


using MidiPortPtr = QSharedPointer<MidiPort>;
using KeyBind = QPair<int16_t, int16_t>;


class LooperCtrl : public QObject, public MidiEventProcessor
{
	Q_OBJECT
public:
	LooperCtrl();
	virtual ~LooperCtrl();

	virtual void processInEvent(
		const MidiEvent &ev,
		const TimePos &time,
		f_cnt_t offset=0) override;

	virtual void processOutEvent(
		const MidiEvent &ev,
		const TimePos &time,
		f_cnt_t offset=0) override {};

signals:
	void trackChanged(int newTrackId);

private slots:
	void onLoopRestart();

private:
	friend class LooperView;

	enum PendingAction
	{
		// these actions are 'preemtivable'
		NoAction,
		StartRecord,
		ToggleMuteTrack,
		ToggleSoloTrack,
		UnMuteAllTracks,

		// these actions are NOT 'preemtivable'
		ProtectedAction,
		StopRecord,
		NumOfPendingActions,
	};

	void togglePlay();
	void toggleRecord();
	void toggleMuteTrack();
	int getInstrumentTrackAt(int position);
	void setMidiOnTrack(int trackId = -1);
	void setPendingAction(PendingAction action, bool preempt = false);
	void setColor(QColor c);

	void saveSettings(QDomDocument &doc, QDomElement &element);
	void loadSettings(const QDomElement &element);

	const QColor m_colNormal = "#3465A4";
	const QColor m_colRecording = "#A40000";
	const QColor m_colQueuedAction = "#CE5C00";

	MidiPortPtr m_midiPort;
	KeyBind m_play = {-1, -1};
	KeyBind m_record = {-1, -1};
	KeyBind m_muteCurrent = {-1, -1};
	KeyBind m_unmuteAll = {-1, -1};
	KeyBind m_solo = {-1, -1};
	KeyBind m_clearNotes = {-1, -1};

	BoolModel m_useColors = true;
	PendingAction m_pendingAction = NoAction;
};


class LooperView : public ToolPluginView
{
  	Q_OBJECT
public:
	LooperView(ToolPlugin *tool);
	virtual ~LooperView();

public slots:
	void onEnableChanged();
	void onLoopLengthChanged();
	void onTrackChanged(int newTrackId);
	void onMappingBtnClicked();
	void onSavePresetClicked();
	void onLoadPresetClicked();
	void onProjectLoad();
	void onTrackAdded(Track* track);

private:
	void enableLoop();
	void openTrackOnPianoRoll(int trackId=-1);
	bool loadPreset(QString path);

	void saveSettings(QDomDocument &doc, QDomElement &element);
	void loadSettings(const QDomElement &element);

	LooperCtrl *m_lcontrol = nullptr;
	MidiPortMenu *m_readablePorts = nullptr;
	BoolModel m_enabled = false;
	IntModel m_loopLength;
};


class LooperTool : public ToolPlugin
{
public:
    LooperTool();
	virtual ~LooperTool();

    virtual PluginView* instantiateView(QWidget *)
	{
		return new LooperView(this);
	}

	virtual QString nodeName() const;

	virtual void saveSettings(QDomDocument &doc, QDomElement &element) override;
	virtual void loadSettings(const QDomElement &element) override;
};


#endif
