/*
 * MidiConnectionDialog.cpp - dialog for helping with non-controller
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


#include "MidiConnectionDialog.h"

#include <QGridLayout>

#include "AudioEngine.h"
#include "embed.h"
#include "Engine.h"
#include "GroupBox.h"
#include "gui_templates.h"
#include "LcdSpinBox.h"
#include "MainWindow.h"
#include "MidiClient.h"
#include "TabWidget.h"


AutodetectMidiControl::AutodetectMidiControl(MidiPort *midiPort, int initCh, int initKey) :
    m_channel(initCh, 0, 16),
    m_key(initKey, 0, 128)
{
    if (!midiPort)
    {
        qWarning("Looper: invalid midi port given, it will not work.");
        return;
    }

    m_midiPort = MidiPortPtr::create(QString("tmp-port"), Engine::audioEngine()->midiClient(),
        this, nullptr, MidiPort::Input);

    // use the same MIDI input ports that looper has
    // FIXME: add support for raw-clients
    auto client = Engine::audioEngine()->midiClient();
    if (!client->isRaw())
    {
        auto subscribed = midiPort->readablePorts();
        for (auto it = subscribed.constBegin(); it != subscribed.constEnd(); it++)
        {
            auto readable = m_midiPort->readablePorts();
            if (readable.find(it.key()) != readable.constEnd())
	        {
		        m_midiPort->subscribeReadablePort(it.key(), it.value());
	        }
        }
    }
}


AutodetectMidiControl::~AutodetectMidiControl()
{
    m_midiPort->setMode(MidiPort::Disabled);
}


void AutodetectMidiControl::processInEvent(
    const MidiEvent &ev, const TimePos &time, f_cnt_t offset)
{
    if (ev.type() == MidiControlChange)
    {
        if (ev.velocity() == 0) { return; }

        // view ranges: channel = 1-16, note = 0-127
        m_key.setValue(ev.key() + 1);
        m_channel.setValue(ev.channel() + 1);
    }
}


MidiConnectionDialog::MidiConnectionDialog(
        QWidget *parent, MidiPort* midiPort, int currentCh, int currentKey) :
    QDialog(parent),
	m_midiControl(nullptr)
{
	setWindowIcon(embed::getIconPixmap("setup_audio"));
	setWindowTitle(tr("Midi Button Mapping"));
	setModal(true);

    auto grid = new QGridLayout(this);
    grid->setSpacing(10);

    auto label = new QLabel(tr("Press the MIDI button you want to use:"), this);
    label->setWordWrap(true);
    label->setFont(pointSize<8>(label->font()));
    grid->addWidget(label, 0, 0, 1, 3);

    auto midiTab = new TabWidget(tr("MIDI CONTROL"), this);
    auto tabLayout = new QHBoxLayout(midiTab);
    tabLayout->setContentsMargins(5, 20, 5, 15);
    grid->addWidget(midiTab, 1, 0, 1, 3);

	auto midiChannelSpinBox = new LcdSpinBox(2, this, tr("Input channel"));
	midiChannelSpinBox->addTextForValue(0, "--");
	midiChannelSpinBox->setLabel(tr("CHANNEL"));
    tabLayout->addWidget(midiChannelSpinBox, 0, Qt::AlignRight);

	auto midiControlSpinBox = new LcdSpinBox(3, this, tr("Input controller"));
	midiControlSpinBox->addTextForValue(0, "---");
	midiControlSpinBox->setLabel(tr("CONTROLLER"));
    tabLayout->addWidget(midiControlSpinBox, 0, Qt::AlignLeft);

    grid->setRowMinimumHeight(1, 50);

    auto okBtn = new QPushButton(embed::getIconPixmap("apply"), tr("OK"), this);
	connect(okBtn, SIGNAL(clicked()), this, SLOT(accept()));
    grid->addWidget(okBtn, 2, 2);

	auto cancelBtn = new QPushButton(embed::getIconPixmap("cancel"), tr("Cancel"), this);
	connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));
    grid->addWidget(cancelBtn, 2, 1);

    // midi auto controller
    m_midiControl = AutodetectMidiControlPtr(
        new AutodetectMidiControl(midiPort, currentCh, currentKey));
    midiChannelSpinBox->setModel(&m_midiControl->m_channel);
    midiControlSpinBox->setModel(&m_midiControl->m_key);

    layout()->setSizeConstraint(QLayout::SetFixedSize);
}


QPair<int16_t, int16_t> MidiConnectionDialog::getSelection()
{
    return
    {
        // view ranges: channel = 1-16, note = 0-127
        (int16_t) (m_midiControl->m_channel.value() - 1),
        (int16_t) (m_midiControl->m_key.value() - 1)
    };
}


MidiConnectionDialog::~MidiConnectionDialog()
{
}
