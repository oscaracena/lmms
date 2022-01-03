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
    * [OK] set and control loop duration
    * [OK] add an empty TCO on start of each track (if not exist)
    * [OK] open first TCO of choosen track on piano-roll (show piano-roll if hidden)
    * [OK] MIDI PC: view current track (first TCO) on piano-roll
    * [OK] MIDI PC: set MIDI input to only current track
    * [OK] MIDI CC pad1: start/stop playing (on song editor)
    * [OK] MIDI CC pad2: start/stop record (on piano-roll)
    * [OK] MIDI CC pad3: enable/disable (mute) current track
    * [OK] MIDI CC pad4: toggle 'solo' for current track
    * [OK] MIDI CC pad5: enable (unmute) all tracks
    * [OK] support custom MIDI mappings for CC controls
    * save/load profiles (use loadSettings/saveSettings)

  Future features (v0.2):
    * control launch-Q (determine when a track will mute/unmute itself)
    * add dynamic automations for tracks
    * wait start recording until first note comes
    * support for TCO colors (no controlled, random, fixed)
*/


#include "looper.h"
#include "MidiConnectionDialog.h"

#include <iostream>

#include <QMdiSubWindow>
#include <QVBoxLayout>

#include "ConfigManager.h"
#include "embed.h"
#include "Engine.h"
#include "GuiApplication.h"
#include "InstrumentTrack.h"
#include "LcdSpinBox.h"
#include "MainWindow.h"
#include "MidiClient.h"
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
	ToolPlugin(&looper_plugin_descriptor)
{
}


LooperTool::~LooperTool()
{
}


QString LooperTool::nodeName() const
{
	return looper_plugin_descriptor.name;
}


void LooperTool::saveSettings(QDomDocument &doc, QDomElement &element)
{
    std::cout << "save settings\n";
}


void LooperTool::loadSettings(const QDomElement &element)
{
    std::cout << "load settings\n";
}


// -- LopperView class ------------------------------------------------------------------


LooperView::LooperView(ToolPlugin *tool) :
	ToolPluginView(tool),
    m_enabled(false),
    m_loopLength(4, 1, 256)
{
    // widget is initially hidden
    auto parent = parentWidget();
    parent->hide();

    // set some size related properties
    parent->resize(300, 190);
    parent->setMaximumSize(parent->width(), parent->height());
    parent->setMinimumSize(parent->width(), parent->height());

    // remove maximize button
    auto flags = parent->windowFlags();
	flags &= ~Qt::WindowMaximizeButtonHint;
	parent->setWindowFlags(flags);

    // add a GroupBox to enable/disable this component
    auto mainLayout = new QVBoxLayout(this);
	auto m_groupBox = new GroupBox(tr("Loop Controller:"));
    mainLayout->addWidget(m_groupBox);

    auto grid = new QGridLayout(m_groupBox);
    grid->setContentsMargins(5, 20, 5, 5);
    grid->setSpacing(10);
    grid->setColumnStretch(1, 1);

    // when using with non-raw-clients, show list of input MIDI ports
    // FIXME: add support for raw-clients
	auto midiInputsBtn = new ToolButton(m_groupBox);
	midiInputsBtn->setIcon(embed::getIconPixmap("piano"));
    midiInputsBtn->setToolTip(tr("MIDI-devices to receive events from"));
    midiInputsBtn->setPopupMode(QToolButton::InstantPopup);
    grid->addWidget(midiInputsBtn, 0, 0, Qt::AlignLeft);

	if (!Engine::audioEngine()->midiClient()->isRaw())
	{
		m_readablePorts = new MidiPortMenu(MidiPort::Input);
		midiInputsBtn->setMenu(m_readablePorts);
	}
    else { qWarning("Looper: sorry, no support for raw clients!"); }

    // input to set loop length
    auto loopLength = new LcdSpinBox(3, m_groupBox, tr("Middle key"));
	loopLength->setLabel(tr("LENGTH"));
	loopLength->setToolTip(tr("Select the loop length (in bars)"));
	loopLength->setModel(&m_loopLength);
    connect(&m_loopLength, SIGNAL(dataChanged()),
        this, SLOT(onLoopLengthChanged()));
    grid->addWidget(loopLength, 0, 1, Qt::AlignLeft);

    // initialize model for enable/disable LED
    connect(&m_enabled, SIGNAL(dataChanged()),
		this, SLOT(onEnableChanged()));
    m_groupBox->setModel(&m_enabled);

    // show buttons to change mappings
    auto buttonTab = new TabWidget(tr("Button Mappgings:"), m_groupBox);
    auto buttons = new QHBoxLayout(buttonTab);
    grid->addWidget(buttonTab, 1, 0, 1, 2);

    auto playBtn = new QPushButton();
    playBtn->setProperty("action", "play");;
	playBtn->setIcon(embed::getIconPixmap("play"));
    playBtn->setStyleSheet("padding: 3px");
    playBtn->setToolTip(tr("Play/Stop button mapping"));
    connect(playBtn, SIGNAL(clicked()), this, SLOT(onMappingBtnClicked()));
    buttons->addWidget(playBtn);

    auto recordBtn = new QPushButton();
    recordBtn->setProperty("action", "record");;
	recordBtn->setIcon(embed::getIconPixmap("record_accompany"));
    recordBtn->setToolTip(tr("Start/Stop Recording button mapping"));
    recordBtn->setStyleSheet("padding: 3px");
    connect(recordBtn, SIGNAL(clicked()), this, SLOT(onMappingBtnClicked()));
    buttons->addWidget(recordBtn);

    auto muteBtn = new QPushButton();
    muteBtn->setProperty("action", "muteCurrent");;
	muteBtn->setIcon(PLUGIN_NAME::getIconPixmap("mute_current"));
    muteBtn->setToolTip(tr("Toggle Mute Current Track button mapping"));
    muteBtn->setStyleSheet("padding: 3px");
    connect(muteBtn, SIGNAL(clicked()), this, SLOT(onMappingBtnClicked()));
    buttons->addWidget(muteBtn);

    auto unmuteAllBtn = new QPushButton();
    unmuteAllBtn->setProperty("action", "unmuteAll");;
	unmuteAllBtn->setIcon(PLUGIN_NAME::getIconPixmap("unmute_all"));
    unmuteAllBtn->setToolTip(tr("Unmute All Tracks button mapping"));
    unmuteAllBtn->setStyleSheet("padding: 3px");
    connect(unmuteAllBtn, SIGNAL(clicked()), this, SLOT(onMappingBtnClicked()));
    buttons->addWidget(unmuteAllBtn);

    auto soloBtn = new QPushButton();
    soloBtn->setProperty("action", "solo");;
	soloBtn->setIcon(PLUGIN_NAME::getIconPixmap("solo"));
    soloBtn->setToolTip(tr("Solo Track button mapping"));
    soloBtn->setStyleSheet("padding: 3px");
    connect(soloBtn, SIGNAL(clicked()), this, SLOT(onMappingBtnClicked()));
    buttons->addWidget(soloBtn);

    buttons->addStretch(1);
}


LooperView::~LooperView()
{
}


void LooperView::onEnableChanged()
{
    if (m_enabled.value())
    {
        if (!m_lcontrol)
        {
            m_lcontrol = ::new LooperCtrl();
            connect(m_lcontrol, SIGNAL(trackChanged(int)),
                this, SLOT(onTrackChanged(int)));
        }

        m_lcontrol->m_midiPort->setMode(MidiPort::Input);
        if (m_readablePorts)
        {
            m_readablePorts->setModel(m_lcontrol->m_midiPort.get());
        }

        enableLoop();

        auto trackId = m_lcontrol->getInstrumentTrackAt(0);
        if (trackId != -1)
        {
            openTrackOnPianoRoll(trackId);
            m_lcontrol->setMidiOnTrack(trackId);
        }
    }
    else
    {
        m_lcontrol->m_midiPort->setMode(MidiPort::Disabled);
    }
}


void LooperView::onLoopLengthChanged()
{
    if (!m_enabled.value()) { return; }
    enableLoop();
}


void LooperView::onTrackChanged(int newTrackId)
{
    openTrackOnPianoRoll(newTrackId);
}


void LooperView::onMappingBtnClicked()
{
    // if looper not enabled, do nothing
    if (!m_enabled.value() || !m_lcontrol) { return; }

    auto action = sender()->property("action").toString().toStdString();
    KeyBind def = {-1, -1};
    if (action == "play")             { def = m_lcontrol->m_play; }
    else if (action == "record")      { def = m_lcontrol->m_record; }
    else if (action == "muteCurrent") { def = m_lcontrol->m_muteCurrent; }
    else if (action == "unmuteAll")   { def = m_lcontrol->m_unmuteAll; }
    else if (action == "solo")        { def = m_lcontrol->m_solo; }

    MidiConnectionDialog dialog(
        getGUI()->mainWindow(), m_lcontrol->m_midiPort.get(),
        def.first + 1, def.second + 1);
    if (dialog.exec() != QDialog::Accepted) { return; }

    auto selected = dialog.getSelection();

    if (action == "play") { m_lcontrol->m_play = selected; }
    else if (action == "record") { m_lcontrol->m_record = selected; }
    else if (action == "muteCurrent") { m_lcontrol->m_muteCurrent = selected; }
    else if (action == "unmuteAll") { m_lcontrol->m_unmuteAll = selected; }
    else if (action == "solo") { m_lcontrol->m_solo = selected; }
}


void LooperView::enableLoop()
{
    // activate loop points
    auto song = Engine::getSong();
    auto timeline = song->getPlayPos(song->Mode_PlaySong).m_timeLine;

    // get current settings, and change only what we want
    QDomDocument doc;
    auto config = doc.createElement("config");
    timeline->saveSettings(doc, config);
    config.setAttribute("lp0pos", 0);
	config.setAttribute("lp1pos", m_loopLength.value() * DefaultTicksPerBar);
	config.setAttribute("lpstate", TimeLineWidget::LoopPointsEnabled);
    timeline->loadSettings(config);
}


void LooperView::openTrackOnPianoRoll(int trackId)
{
    // open first TCO of choosen track on piano-roll (show piano-roll if hidden)
    auto tracks = Engine::getSong()->tracks();

    // if track is not given, search for the first instrument track (if any)
    if (trackId == -1)
    {
        trackId = m_lcontrol->getInstrumentTrackAt(0);
        if (trackId == -1) { return; }
    }
    Track *track = tracks.at(trackId);

    // get TCO at pos 0 (not first TCO!)
    Pattern *pattern = nullptr;
    auto tcos = track->getTCOs();
    for (int i=0; i<tcos.size(); i++)
    {
        auto tco = tcos.at(i);
        if (tco->startPosition() == 0)
        {
            pattern = dynamic_cast<Pattern*>(tco);
        }
    }

    // if not pattern, there is no TCO at pos 0, create it
    if (!pattern)
    {
        auto tco = track->createTCO(0);
        tco->setName(QString("looper-track-%1").arg(trackId));
        pattern = dynamic_cast<Pattern*>(tco);
    }

    getGUI()->pianoRoll()->setCurrentPattern(pattern);
    getGUI()->pianoRoll()->parentWidget()->show();
	getGUI()->pianoRoll()->show();
	getGUI()->pianoRoll()->setFocus();
}


// -- LopperCtrl class ------------------------------------------------------------------


LooperCtrl::LooperCtrl()
{
    m_midiPort = MidiPortPtr::create(QString("looper-controller"), Engine::audioEngine()->midiClient(),
        this, nullptr, MidiPort::Input);

    m_midiPort->setName(QString("Looper Tool"));

    // get system wide MIDI selected device (if any) and connect it
    // FIXME: add support for raw-client
    auto client = Engine::audioEngine()->midiClient();
    if (!client->isRaw())
    {
	    const QString &device = ConfigManager::inst()->value(
            "midi", "midiautoassign");
	    if (client->readablePorts().indexOf(device) >= 0)
	    {
		    m_midiPort->subscribeReadablePort(device, true);
	    }
    }

    qInfo("Looper: controller created");
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
        int trackId = getInstrumentTrackAt(ev.key());
        if (trackId == -1)
        {
            qWarning("Looper: there is no Instrument Track number %d", ev.key());
            return;
        }
        setMidiOnTrack(trackId);
        emit trackChanged(trackId);
    }

    else if (ev.type() == MidiControlChange)
    {
        if (ev.velocity() == 0) { return; }
        auto pianoRoll = getGUI()->pianoRoll();

        if (ev.channel() == m_play.first && ev.key() == m_play.second)
        {
            auto song = Engine::getSong();
            if (pianoRoll->isRecording()) { pianoRoll->stop(); }
            else if (song->isPlaying()) { song->stop(); }
            else { song->playSong(); }
        }

        else if (ev.channel() == m_record.first && ev.key() == m_record.second)
        {
            if (pianoRoll->isRecording()) { pianoRoll->stop(); }
            else { pianoRoll->recordAccompany(); }
        }

        else if (ev.channel() == m_muteCurrent.first && ev.key() == m_muteCurrent.second)
        {
            auto tco = pianoRoll->currentPattern();
            if (!tco) { return; }
            auto track = tco->getTrack();
            track->setMuted(!track->isMuted());
        }

        else if (ev.channel() == m_unmuteAll.first && ev.key() == m_unmuteAll.second)
        {
            auto tracks = Engine::getSong()->tracks();
            for (int i = 0; i < tracks.size(); i++)
            {
                auto t = tracks.at(i);
                if (t->type() != Track::InstrumentTrack) { continue; }
                t->setMuted(false);
            }
        }

        else if (ev.channel() == m_solo.first && ev.key() == m_solo.second)
        {
            auto tco = pianoRoll->currentPattern();
            if (!tco) { return; }
            auto track = tco->getTrack();
            track->setSolo(!track->isSolo());
        }
    }
}


int LooperCtrl::getInstrumentTrackAt(int position)
{
    auto tracks = Engine::getSong()->tracks();
    for (int i = 0; i < tracks.size(); i++)
    {
        auto track = tracks.at(i);
        if (track->type() == Track::InstrumentTrack) {
            if (position-- == 0) { return i; }
        }
    }
    return -1;
}


// Note: before calling this method, ensure client is not raw
void LooperCtrl::setMidiOnTrack(int trackId)
{
    // if track is not given, search for the first instrument track (if any)
    if (trackId == -1)
    {
        trackId = getInstrumentTrackAt(0);
        if (trackId == -1) { return; }
    }

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
    auto cfgInputs = m_midiPort->readablePorts();
    for (auto it = cfgInputs.constBegin(); it != cfgInputs.constEnd(); it++)
    {
        if (trInputs.constFind(it.key()) != trInputs.constEnd())
        {
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
}
