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

/*
  Expected features (v0.1):
    * [doable] set and control loop duration
    * [doable] add an empty TCO on start of each track (if not exist)
    * [doable] set MIDI input to only current selected track
    * [doable] open first TCO of choosen track on piano-roll (show piano-roll if hidden)
    * [doable] use MIDI Program Change (PC) messages to select track on piano-roll
    * use MIDI Control Cahnge (CC) messages to:
        - up/down track on piano-roll
        - start/stop play and record
    * control launch-Q (determine when a track will start playing/recording)
    * support custom MIDI mappings for above controls
    * save/load profiles (use loadSettings/saveSettings)
*/


#include "looper.h"

#include <iostream>

#include <QMdiSubWindow>
#include <QVBoxLayout>

#include "ConfigManager.h"
#include "embed.h"
#include "MidiClient.h"
#include "Engine.h"
#include "GuiApplication.h"
#include "InstrumentTrack.h"
#include "PianoRoll.h"
#include "plugin_export.h"
#include "Song.h"
#include "TimeLineWidget.h"
#include "ToolButton.h"
#include "TrackContentObject.h"


extern "C"
{
    Plugin::Descriptor PLUGIN_EXPORT looper_plugin_descriptor =
    {
        STRINGIFY(PLUGIN_NAME),
        "Looper Tool",
        QT_TRANSLATE_NOOP(
            "pluginBrowser",
            "Tool to help with live looping"),
        "Oscar Acena <oscaracena/at/gmail/dot/com>",
        0x0100,
        Plugin::Tool,
            new PluginPixmapLoader("logo"),
            nullptr,
            nullptr
    };

    PLUGIN_EXPORT Plugin * lmms_plugin_main(Model *parent, void *data)
    {
        return new LooperTool;
    }
}


LooperTool::LooperTool() :
	ToolPlugin(&looper_plugin_descriptor, nullptr)
{
}


QString LooperTool::nodeName() const
{
	return looper_plugin_descriptor.name;
}


LooperView::LooperView(ToolPlugin *tool) :
	ToolPluginView(tool),
    m_enabled(false, nullptr, tr("Enable/Disable Looper Tool"))
{
    // Widget is initially hidden
    auto parent = parentWidget();
    // parent->hide(); // FIXME: remove on production

    // Set some size related properties
    parent->resize(300, 200);
    parent->setMinimumWidth(300);
	parent->setMinimumHeight(200);

    // Remove maximize button
    Qt::WindowFlags flags = parent->windowFlags();
	flags &= ~Qt::WindowMaximizeButtonHint;
	parent->setWindowFlags(flags);

    // Add a GroupBox to enable/disable this component
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
	m_groupBox = new GroupBox(tr("Loop Controller:"));
    mainLayout->addWidget(m_groupBox);

    QVBoxLayout *gBoxLayout = new QVBoxLayout();
	gBoxLayout->setContentsMargins(3, 15, 3, 3);
	m_groupBox->setLayout(gBoxLayout);

    // when using with non-raw-clients, show list of input MIDI ports
    // FIXME: add support for raw-clients
	ToolButton *midiInputsBtn = new ToolButton(m_groupBox);
	midiInputsBtn->setIcon(embed::getIconPixmap("piano"));
    midiInputsBtn->setText(tr("MIDI-devices to receive events from"));
    midiInputsBtn->setGeometry(15, 24, 32, 32);
    midiInputsBtn->setPopupMode(QToolButton::InstantPopup);

	if (!Engine::audioEngine()->midiClient()->isRaw())
	{
		m_readablePorts = new MidiPortMenu(MidiPort::Input);
		midiInputsBtn->setMenu(m_readablePorts);
	}
    else
    {
        qWarning("Looper: sorry, no support for raw clients!");
    }

    // initialize model for enable/disable LED
    connect(&m_enabled, SIGNAL(dataChanged()),
		this, SLOT(onEnableChanged()));
    m_groupBox->setModel(&m_enabled);
}


LooperView::~LooperView()
{
    if (m_lcontrol) { ::delete m_lcontrol; }
    if (m_readablePorts) { ::delete m_readablePorts; }
}


void LooperView::onEnableChanged() {
    if (m_enabled.value())
    {
        if (!m_lcontrol)
        {
            m_lcontrol = ::new LooperCtrl();
        }
        m_lcontrol->m_midiPort.setMode(MidiPort::Input);
        if (m_readablePorts)
        {
            m_readablePorts->setModel(&(m_lcontrol->m_midiPort));
        }
    }
    else
    {
        m_lcontrol->m_midiPort.setMode(MidiPort::Disabled);
    }
}

    // [OK] use MIDI Program Change (PC) messages to select track on piano-roll
    // class EventProcessor: public MidiEventProcessor {
    // public:
	//     virtual void processInEvent(const MidiEvent &ev, const TimePos &time, f_cnt_t offset=0) override
    //     {
    //         if (ev.type() == MidiProgramChange)
    //         {
    //             std::cout << "Program Change - channel: " << int(ev.channel() + 1)
    //                       << ", key: " << int(ev.key() + 1) << std::endl;
    //         }
    //         else
    //         {
    //             std::cout << "midi event (not handled)" << std::endl;
    //         }
    //     }
    // 	virtual void processOutEvent( const MidiEvent &ev, const TimePos &time, f_cnt_t offset=0) override
	//     {
    // 	}
    // };
    // // IMPORTANT! disconnect this port when looper is disabled (it will be system wide visible)
    // // NOTE: 'new' is used here just for testing (to avoid GC), try to avoid it (or use smart pointers)
    // // FIXME: listen for new devices, and also connect to them
    // auto ep = new EventProcessor();
    // auto midiPort = new MidiPort(
    //     tr("looper-controller"), Engine::audioEngine()->midiClient(), ep, nullptr, MidiPort::Input);
    // midiPort->setName(QString("Looper Tool"));
    // auto inputs = midiPort->readablePorts();
	// for (auto it = inputs.constBegin(); it != inputs.constEnd(); ++it)
	// {
	// 	midiPort->subscribeReadablePort(it.key(), true);
	// }


    // [OK] open first TCO of choosen track on piano-roll (show piano-roll if hidden)
    // auto trackId = 1;
    // auto track = Engine::getSong()->tracks().at(trackId);
    // if (!track || track->type() != Track::InstrumentTrack) {
    //     qWarning("Warning: invalid selected track");
	// 	return;
    // }
    // // FIXME: get TCO at pos 0 (not first TCO)
    // auto pattern = dynamic_cast<Pattern*>(track->getTCO(0));
    // getGUI()->pianoRoll()->setCurrentPattern(pattern);
	// getGUI()->pianoRoll()->parentWidget()->show();
	// getGUI()->pianoRoll()->show();
	// getGUI()->pianoRoll()->setFocus();


    // [OK] add an empty TCO on start of each track (if not exist)
    // auto tracks = Engine::getSong()->tracks();
    // for (auto t : tracks) {
    //     if (t->type() != Track::InstrumentTrack) {
    //         continue;
    //     }
    //     bool exist = false;
    //     auto tcos = t->getTCOs();
    //     for (auto tco : tcos) {
    //         std::cout << tco->startPosition() << std::endl;
    //         if (tco->startPosition() == 0) {
    //             exist = true;
    //             break;
    //         }
    //     }
    //     if (!exist) {
    //         // FIXME: add steps to match loop length
    //         t->createTCO(0);
    //     }
    // }


    // --- [OK] activate loop points ----------------------------------------------
    // auto song = Engine::getSong();
    // auto timeline = song->getPlayPos(song->Mode_PlaySong).m_timeLine;
    // QDomDocument doc;
    // auto config = doc.createElement("config");
    // FIXME: get a copy of current settings, and modify only the needed ones
    // config.setAttribute("lp0pos", 0);
	// config.setAttribute("lp1pos", 4 * DefaultTicksPerBar);
	// config.setAttribute("lpstate", TimeLineWidget::LoopPointsEnabled);
    // config.setAttribute("stopbehaviour", timeline->behaviourAtStop());
    // timeline->loadSettings(config);





LooperCtrl::LooperCtrl() :
    m_midiPort(QString("looper-controller"), Engine::audioEngine()->midiClient(),
        this, nullptr, MidiPort::Input)
{
    qInfo("Looper: controller created");
    m_midiPort.setName(QString("Looper Tool"));

    // get system wide MIDI selected device (if any) and connect it
    // FIXME: add support for raw-client
    auto client = Engine::audioEngine()->midiClient();
    if (!client->isRaw())
    {
	    const QString &device = ConfigManager::inst()->value(
            "midi", "midiautoassign");
	    if (client->readablePorts().indexOf(device) >= 0)
	    {
		    m_midiPort.subscribeReadablePort(device, true);
	    }
    }
}


LooperCtrl::~LooperCtrl()
{
    qInfo("Looper: controller destroyed");
}


void LooperCtrl::processInEvent(
    const MidiEvent &ev, const TimePos &time, f_cnt_t offset)
{
    if (ev.type() == MidiProgramChange)
    {
        std::cout << "Program Change - channel: " << int(ev.channel() + 1)
                  << ", key: " << int(ev.key() + 1) << std::endl;
        setMidiOnTrack(ev.key());
    }
}

void LooperCtrl::setMidiOnTrack(int trackId)
{
    // set MIDI input to only current selected track
    auto tracks = Engine::getSong()->tracks(); 
    if (trackId >= tracks.size())
    {
        qWarning("Looper: missing track %d", trackId);
        return;
    }
    
    auto t = tracks.at(trackId);
    if (t->type() != Track::InstrumentTrack) 
    {
        qWarning("Looper: track %d is not an Instrument Track, ignored", trackId);
        return;
    }

    // enable MIDI input on given track
    auto track = static_cast<InstrumentTrack*>(t);
    auto port = track->midiPort();
    auto trInputs = port->readablePorts();  
    auto cfgInputs = m_midiPort.readablePorts();
    for (auto it = cfgInputs.constBegin(); it != cfgInputs.constEnd(); it++)  
    {
        if (trInputs.constFind(it.key()) != trInputs.constEnd()) {
            port->subscribeReadablePort(it.key(), it.value());
        }
    }
    emit port->readablePortsChanged();

    // remove MIDI input from other tracks
    for (int i = 0; i < tracks.size(); i++) 
    {
        if (i == trackId) { continue; }
        auto t = tracks.at(i);
        if (t->type() != Track::InstrumentTrack) { continue; }

        auto track = static_cast<InstrumentTrack*>(t);
        auto port = track->midiPort();
        auto inputs = port->readablePorts();  

        for (auto it = inputs.constBegin(); it != inputs.constEnd(); it++)  
        {        
            port->subscribeReadablePort(it.key(), false);        
        }
        emit port->readablePortsChanged();
    }

    // for (int idx = 0; idx < tracks.size(); ++idx) {
    //     std::cout << "track id: " << idx << std::endl;
    //     // handle only instrument tracks
    //     auto t = tracks.at(idx);
    //     if (t->type() != Track::InstrumentTrack) {
    //         continue;
    //     }
    //     auto track = static_cast<InstrumentTrack *>(t);
    //     auto port = track->midiPort();
    //     auto inputs = port->readablePorts();
    //     // if track is not selected one, disconnect all midi inputs
    //     if (idx != trackId) {
    //         auto in = inputs.constBegin();
    //         while (in != inputs.constEnd()) {
    //             if (in.value()) {
    //                 port->subscribeReadablePort(in.key(), false);
    //                 emit port->readablePortsChanged();
    //             }
    //             ++in;
    //         }
    //     }
    //     // otherwise, connect only selected input
    //     else {
    //         auto in = inputs.constBegin();
    //         while (in != inputs.constEnd()) {
    //             if (in.key() == midi_in) {
    //                 if (!in.value()) {
    //                     port->subscribeReadablePort(in.key(), true);
    //                     emit port->readablePortsChanged();
    //                 }
    //             }
    //             else {
    //                 if (in.value()) {
    //                     port->subscribeReadablePort(in.key(), false);
    //                     emit port->readablePortsChanged();
    //                 }
    //             }
    //             ++in;
    //         }
    //     }
    // }
}