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


// Expected features (v0.1):
// * set MIDI input to only current selected track
// * use MIDI Program Change (PC) messages to select track on piano-roll
// * use MIDI Control Cahnge (CC) messages to:
//    - up/down track on piano-roll
//    - start/stop play and record
// * control launch-Q (determine when a track will start playing/recording)
// * support custom MIDI mappings for above controls
// * save/load profiles


#ifndef LOOPER_TOOL_H
#define LOOPER_TOOL_H

#include <QObject>

#include "ToolPlugin.h"
#include "ToolPluginView.h"


class LooperView : public ToolPluginView
{
public:
	LooperView(ToolPlugin * _tool);
};


class LooperTool : public ToolPlugin
{
public:
    LooperTool();

    virtual PluginView * instantiateView(QWidget *)
	{
		return new LooperView(this);
	}

	virtual QString nodeName() const;

	virtual void saveSettings(QDomDocument& doc, QDomElement& element)
	{
		Q_UNUSED(doc)
		Q_UNUSED(element)
	}

	virtual void loadSettings(const QDomElement& element)
	{
		Q_UNUSED(element)
	}
};


#endif
