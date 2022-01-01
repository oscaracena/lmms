/*
 * looper.cpp - handy tool to help with live looping functions
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

#include <memory>

#include <QObject>

#include "GroupBox.h"
#include "MidiEventProcessor.h"
#include "MidiPort.h"
#include "MidiPortMenu.h"
#include "ToolPlugin.h"
#include "ToolPluginView.h"


class LooperCtrl : public QObject, public MidiEventProcessor
{
	Q_OBJECT
public:
	LooperCtrl();
	~LooperCtrl();

	virtual void processInEvent(
		const MidiEvent &ev,
		const TimePos &time,
		f_cnt_t offset=0) override;

	virtual void processOutEvent(
		const MidiEvent &ev,
		const TimePos &time,
		f_cnt_t offset=0) override {};

private:
	friend class LooperView;

	void setMidiOnTrack(int trackId);

	MidiPort m_midiPort;
};


class LooperView : public ToolPluginView
{
  	Q_OBJECT
public:
	LooperView(ToolPlugin *tool);
	~LooperView();

private slots:
	void onEnableChanged();

private:
    GroupBox *m_groupBox;

	LooperCtrl *m_lcontrol = nullptr;
	MidiPortMenu *m_readablePorts = nullptr;
	BoolModel m_enabled;
};


class LooperTool : public ToolPlugin
{
public:
    LooperTool();

    virtual PluginView* instantiateView(QWidget *)
	{
		return new LooperView(this);
	}

	virtual QString nodeName() const;

	virtual void saveSettings(QDomDocument &doc, QDomElement &element)
	{
		Q_UNUSED(doc)
		Q_UNUSED(element)
	}

	virtual void loadSettings(const QDomElement &element)
	{
		Q_UNUSED(element)
	}
};


#endif
