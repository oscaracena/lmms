/*
 * MidiConnectionDialog.h - dialog for helping with non-controller
 *                            connections
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


#ifndef LOOPER_TOOL_MCD_H
#define LOOPER_TOOL_MCD_H

#include <QDialog>

#include "MidiEventProcessor.h"
#include "MidiPort.h"


using MidiPortPtr = QSharedPointer<MidiPort>;


class AutodetectMidiControl : public MidiEventProcessor
{
public:
    AutodetectMidiControl(MidiPort *port, int initCh=0, int initKey=0);
	virtual ~AutodetectMidiControl();

	virtual void processInEvent(
		const MidiEvent &ev,
		const TimePos &time,
		f_cnt_t offset=0) override;

	virtual void processOutEvent(
		const MidiEvent &ev,
		const TimePos &time,
		f_cnt_t offset=0) override {};

	IntModel m_channel;
	IntModel m_key;

private:
	friend class MidiConnectionDialog;

	MidiPortPtr m_midiPort;
};


using AutodetectMidiControlPtr = QSharedPointer<AutodetectMidiControl>;


class MidiConnectionDialog : public QDialog
{
	Q_OBJECT
public:
	MidiConnectionDialog(QWidget *parent, MidiPort *port, int currentCh=0, int currentKey=0);
	virtual ~MidiConnectionDialog();
	QPair<int16_t, int16_t> getSelection();

private:
	AutodetectMidiControlPtr m_midiControl;
};


#endif