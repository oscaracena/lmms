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
    * [OK] add an empty Clip on start of each track (if not exist)
    * [OK] open first Clip of choosen track on piano-roll (show piano-roll if hidden)
    * [OK] MIDI PC: view current track (first Clip) on piano-roll
    * [OK] MIDI PC: set MIDI input to only current track
    * [OK] MIDI CC pad1: start/stop playing (on song editor)
    * [OK] MIDI CC pad2: start/stop record (on piano-roll)
    * [OK] MIDI CC pad3: enable/disable (mute) current track
    * [OK] MIDI CC pad4: toggle 'solo' for current track
    * [OK] MIDI CC pad5: enable (unmute) all tracks
    * [OK] support custom MIDI mappings for CC controls
    * [OK] save/load presets (load 'default' on init)

  Expected features (v0.2):
    * [OK] record for one single loop iteration (keep playing afterwards)
    * [OK] MIDI CC pad6: clear all Clip notes of current track (press twice to confirm)
    * [OK] start/stop recording within quantization (at start of next loop iter)
    * [OK] change between play and record seamlessly (no stop playing)
    * mute/unmute within quantization (at start of next loop iter)
    * wait start recording until first note comes
    * support for Clip colors (no controlled, random, fixed)
    *  - green: normal, red: recording, orange: queued for action (play/record)
    * set individual loop size for each track
    * on end recording clip, clone it to cover the largest one

  Expected features (v0.3):
    * add dynamic automations for tracks
    * support for multiple Clips per track (move Clip outside the loop, and create new)
    * handle "Sample" tracks
*/


#include "looper.h"
#include "MidiConnectionDialog.h"

#include <QMdiSubWindow>
#include <QMessageBox>
#include <QVBoxLayout>

#include "ConfigManager.h"
#include "DataFile.h"
#include "embed.h"
#include "Engine.h"
#include "FileDialog.h"
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
#include "Clip.h"


extern "C"
{
    Plugin::Descriptor PLUGIN_EXPORT looper_plugin_descriptor =
    {
        STRINGIFY(PLUGIN_NAME),
        "Looper Tool",
        QT_TRANSLATE_NOOP(
            "pluginBrowser",
            "A tool to help with live looping"),
        "Oscar Acena <oscaracena/at/gmail/dot/com>",
        0x0100,
        Plugin::Tool,
            new PluginPixmapLoader("logo"),
            nullptr,
            nullptr
    };

    PLUGIN_EXPORT Plugin *lmms_plugin_main(Model *parent, void *data)
    {
        return new LooperTool;
    }
}


LooperTool::LooperTool() :
	ToolPlugin(&looper_plugin_descriptor, nullptr)
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
    Q_UNUSED(doc);
    Q_UNUSED(element);
}


void LooperTool::loadSettings(const QDomElement &element)
{
    Q_UNUSED(element);
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

    // initialize model for enable/disable tool
    connect(&m_enabled, SIGNAL(dataChanged()), this, SLOT(onEnableChanged()));
    m_groupBox->setModel(&m_enabled);

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
    connect(&m_loopLength, SIGNAL(dataChanged()), this, SLOT(onLoopLengthChanged()));
    grid->addWidget(loopLength, 0, 1, Qt::AlignLeft);

    grid->setColumnStretch(2, 1);

    // save/load settings as presets
	auto savePresetBtn = new QPushButton(embed::getIconPixmap("project_save" ), "");
    savePresetBtn->setToolTip(tr("Save current Looper settings to a preset file"));
    savePresetBtn->setStyleSheet("padding: 3px");
	connect(savePresetBtn, SIGNAL(clicked()), this, SLOT(onSavePresetClicked()));
	grid->addWidget(savePresetBtn, 0, 3, Qt::AlignRight);

	auto loadPresetBtn = new QPushButton(embed::getIconPixmap("project_open" ), "");
    loadPresetBtn->setStyleSheet("padding: 3px");
    loadPresetBtn->setToolTip(tr("Load Looper settings from a preset file"));
	connect(loadPresetBtn, SIGNAL(clicked()), this, SLOT(onLoadPresetClicked()));
	grid->addWidget(loadPresetBtn, 0, 4, Qt::AlignRight);

    // show buttons to change mappings
    auto buttonTab = new TabWidget(tr("Button Mappgings:"), m_groupBox);
    auto buttons = new QHBoxLayout(buttonTab);
    grid->addWidget(buttonTab, 1, 0, 1, 5);

    auto playBtn = new QPushButton(embed::getIconPixmap("play"), "");
    playBtn->setProperty("action", "play");;
    playBtn->setStyleSheet("padding: 3px");
    playBtn->setToolTip(tr("Play/Stop button mapping"));
    connect(playBtn, SIGNAL(clicked()), this, SLOT(onMappingBtnClicked()));
    buttons->addWidget(playBtn);

    auto recordBtn = new QPushButton(embed::getIconPixmap("record_accompany"), "");
    recordBtn->setProperty("action", "record");;
    recordBtn->setToolTip(tr("Start/Stop Recording button mapping"));
    recordBtn->setStyleSheet("padding: 3px");
    connect(recordBtn, SIGNAL(clicked()), this, SLOT(onMappingBtnClicked()));
    buttons->addWidget(recordBtn);

    auto muteBtn = new QPushButton(PLUGIN_NAME::getIconPixmap("mute_current"), "");
    muteBtn->setProperty("action", "muteCurrent");;
    muteBtn->setToolTip(tr("Toggle Mute Current Track button mapping"));
    muteBtn->setStyleSheet("padding: 3px");
    connect(muteBtn, SIGNAL(clicked()), this, SLOT(onMappingBtnClicked()));
    buttons->addWidget(muteBtn);

    auto unmuteAllBtn = new QPushButton(PLUGIN_NAME::getIconPixmap("unmute_all"), "");
    unmuteAllBtn->setProperty("action", "unmuteAll");;
    unmuteAllBtn->setToolTip(tr("Unmute All Tracks button mapping"));
    unmuteAllBtn->setStyleSheet("padding: 3px");
    connect(unmuteAllBtn, SIGNAL(clicked()), this, SLOT(onMappingBtnClicked()));
    buttons->addWidget(unmuteAllBtn);

    auto soloBtn = new QPushButton(PLUGIN_NAME::getIconPixmap("solo"), "");
    soloBtn->setProperty("action", "solo");;
    soloBtn->setToolTip(tr("Solo Track button mapping"));
    soloBtn->setStyleSheet("padding: 3px");
    connect(soloBtn, SIGNAL(clicked()), this, SLOT(onMappingBtnClicked()));
    buttons->addWidget(soloBtn);

    auto clearBtn = new QPushButton(embed::getIconPixmap("edit_erase"), "");
    clearBtn->setProperty("action", "clearNotes");
    clearBtn->setToolTip(tr("Clear Track button mapping"));
    clearBtn->setStyleSheet("padding: 3px");
    connect(clearBtn, SIGNAL(clicked()), this, SLOT(onMappingBtnClicked()));
    buttons->addWidget(clearBtn);

    // add space on button holder to align content to the left
    buttons->addStretch(1);

    // create the looper controller
    m_lcontrol = ::new LooperCtrl();
    connect(m_lcontrol, SIGNAL(trackChanged(int)), this, SLOT(onTrackChanged(int)));

    // load default preset, if exists
    auto defaultPreset = ConfigManager::inst()->userPresetsDir() + "Looper/default.xpf";
    if (QFile(defaultPreset).exists())
    {
        if (!loadPreset(defaultPreset))
        {
            qWarning("Looper: could not load default preset! (invalid file)");
        }
    }

    // connect a callback to handle actions on loop restart
    auto song = Engine::getSong();
    connect(song, SIGNAL(updateSampleTracks()), this, SLOT(onLoopRestart()));
}


LooperView::~LooperView()
{
    if (m_lcontrol)
    {
        ::delete m_lcontrol;
        m_lcontrol = nullptr;
    }
}


void LooperView::onEnableChanged()
{
    if (m_enabled.value())
    {
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
    if (m_enabled.value()) { enableLoop(); }
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
    else if (action == "clearNotes")  { def = m_lcontrol->m_clearNotes; }

    MidiConnectionDialog dialog(
        getGUI()->mainWindow(), m_lcontrol->m_midiPort.get(),
        def.first + 1, def.second + 1);
    if (dialog.exec() != QDialog::Accepted) { return; }

    auto selected = dialog.getSelection();

    if (action == "play")             { m_lcontrol->m_play = selected; }
    else if (action == "record")      { m_lcontrol->m_record = selected; }
    else if (action == "muteCurrent") { m_lcontrol->m_muteCurrent = selected; }
    else if (action == "unmuteAll")   { m_lcontrol->m_unmuteAll = selected; }
    else if (action == "solo")        { m_lcontrol->m_solo = selected; }
    else if (action == "clearNotes")  { m_lcontrol->m_clearNotes = selected; }
}


void LooperView::onSavePresetClicked()
{
    FileDialog saveDialog(this, tr("Save preset"), "", tr("XML preset file (*.xpf)"));

	auto presetRoot = ConfigManager::inst()->userPresetsDir();
    auto presetDir = presetRoot + QString("Looper");

    // this will create all needed dirs, and if they exist, will do nothing
	QDir().mkpath(presetDir);

	saveDialog.setAcceptMode(FileDialog::AcceptSave);
	saveDialog.setDirectory(presetDir);
    saveDialog.selectFile("default");
	saveDialog.setFileMode(FileDialog::AnyFile);
	saveDialog.setDefaultSuffix("xpf");

	if (saveDialog.exec() == QDialog::Accepted &&
	    !saveDialog.selectedFiles().isEmpty() &&
	    !saveDialog.selectedFiles().first().isEmpty())
	{
        // FIXME: there is no ToolPluginSettings type (add it!)
	    DataFile dataFile(DataFile::UnknownType);
	    QDomElement &content(dataFile.content());
        content.setTagName("toolplugin");

        saveSettings(dataFile, content);

        auto filename = saveDialog.selectedFiles()[0];
    	dataFile.writeFile(filename);
    }
}


void LooperView::onLoadPresetClicked()
{
    FileDialog loadDialog(this, tr("Load preset"), "", tr("XML preset file (*.xpf)"));
	loadDialog.setAcceptMode(FileDialog::AcceptOpen);
	loadDialog.setFileMode(FileDialog::ExistingFile);
	loadDialog.setDefaultSuffix("xpf");

    auto presetRoot = ConfigManager::inst()->userPresetsDir();
    auto presetDir = presetRoot + QString("Looper");
    if (QDir(presetDir).exists()) { loadDialog.setDirectory(presetDir); }
    else { loadDialog.setDirectory(presetRoot); }

	if (loadDialog.exec() == QDialog::Accepted &&
	    !loadDialog.selectedFiles().isEmpty() &&
	    !loadDialog.selectedFiles().first().isEmpty())
	{
        auto filename = loadDialog.selectedFiles()[0];
        if (!loadPreset(filename))
        {
            QMessageBox::warning(this,
		        tr("Load preset failed"),
		        tr("Sorry, this is not a valid Looper preset."));
        }
    }
}


bool LooperView::loadPreset(QString path)
{
    DataFile dataFile(path);
    if (dataFile.head().isNull())
    {
        return false;
    }

    auto nodes = dataFile.elementsByTagName("toolplugin");
    if (nodes.size() < 1)
    {
        return false;
    }

    auto document = nodes.at(0).toElement();
    if (!document.hasAttribute("name") || document.attribute("name") != "Looper")
    {
        return false;
    }

    loadSettings(document);
    return true;
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
    // open first Clip of choosen track on piano-roll (show piano-roll if hidden)
    auto tracks = Engine::getSong()->tracks();

    // if track is not given, search for the first instrument track (if any)
    if (trackId == -1)
    {
        trackId = m_lcontrol->getInstrumentTrackAt(0);
        if (trackId == -1) { return; }
    }
    Track *track = tracks.at(trackId);

    // get Clip at pos 0 (not first Clip!)
    MidiClip *midiclip = nullptr;
    auto clips = track->getClips();
    for (int i=0; i<clips.size(); i++)
    {
        auto clip = clips.at(i);
        if (clip->startPosition() == 0)
        {
            midiclip = dynamic_cast<MidiClip*>(clip);
        }
    }

    // if not midiclip, there is no Clip at pos 0, create it
    if (!midiclip)
    {
        auto clip = track->createClip(0);
        clip->setName(QString("looper-track-%1").arg(trackId));
        midiclip = dynamic_cast<MidiClip*>(clip);
    }

    getGUI()->pianoRoll()->setCurrentMidiClip(midiclip);
    getGUI()->pianoRoll()->parentWidget()->show();
	getGUI()->pianoRoll()->show();
}


void LooperView::saveSettings(QDomDocument &doc, QDomElement &element)
{
    element.setAttribute("name", "Looper");
    element.setAttribute("version", LOOPER_TOOL_VERSION);

    // FIXME: enable this line when saving project only
    element.setAttribute("visible", parentWidget()->isVisible());
    element.setAttribute("x", parentWidget()->pos().x());
    element.setAttribute("y", parentWidget()->pos().y());

    m_loopLength.saveSettings(doc, element, "loop-length");
    m_enabled.saveSettings(doc, element, "enable");
    m_lcontrol->saveSettings(doc, element);
}


void LooperView::loadSettings(const QDomElement &element)
{
    m_loopLength.loadSettings(element, "loop-length");
    m_lcontrol->loadSettings(element);
    m_enabled.loadSettings(element, "enable");

    // move window to saved position
    auto x = element.attribute("x", "0").toInt();
    auto y = element.attribute("y", "0").toInt();
    parentWidget()->move(x, y);

    if (element.attribute("visible", "0").toInt())
    {
        parentWidget()->show();
    }
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

    // connect a callback to handle actions on loop restart
    auto song = Engine::getSong();
    connect(song, SIGNAL(updateSampleTracks()), this, SLOT(onLoopRestart()));

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

        // play action
        if (ev.channel() == m_play.first && ev.key() == m_play.second)
        {
            togglePlay();
        }

        // record action
        else if (ev.channel() == m_record.first && ev.key() == m_record.second)
        {
            toggleRecord();
        }

        // mute current track action
        else if (ev.channel() == m_muteCurrent.first && ev.key() == m_muteCurrent.second)
        {
            if (m_pendingAction < ProtectedAction) {
                m_pendingAction = ToggleMuteTrack;
            }
        }

        // unmute all tracks
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

        // set solo on current track
        else if (ev.channel() == m_solo.first && ev.key() == m_solo.second)
        {
            auto clip = pianoRoll->currentMidiClip();
            if (!clip) { return; }
            auto track = clip->getTrack();
            track->setSolo(!track->isSolo());
        }

        // clear all notes of current track
        else if (ev.channel() == m_clearNotes.first && ev.key() == m_clearNotes.second)
        {
            auto clip = pianoRoll->currentMidiClip();
            if (!clip) { return; }
            ((MidiClip*)clip)->clearNotes();
        }
    }
}

void LooperCtrl::onLoopRestart()
{
    qInfo("loop restart, action: %d", m_pendingAction);
    auto pianoRoll = getGUI()->pianoRoll();

    switch (m_pendingAction)
    {
    case StartRecord:
        qInfo(" - action: start record, set stop record action");
        pianoRoll->recordAccompany();
        m_pendingAction = StopRecord;
        break;

    case StopRecord:
        qInfo(" - action: stop record, set no action");
        pianoRoll->stopRecording();
        m_pendingAction = NoAction;
        break;

    case ToggleMuteTrack:
        qInfo(" - action: toggle mute, set no action");
        toggleMuteTrack();
        m_pendingAction = NoAction;
        break;

    default:
        break;
    }
}

void LooperCtrl::toggleMuteTrack()
{
    auto pianoRoll = getGUI()->pianoRoll();
    auto clip = pianoRoll->currentMidiClip();
    if (!clip) { return; }
    auto track = clip->getTrack();
    track->setMuted(!track->isMuted());
}


// intended behaviour for play and record buttons:
//
// ------\ State   recording       playing        idle
// Action \-----   ----------------------------------------------------
// - play btn      stop-record     stop           play
// - record btn    stop-record     start-record   start play & record

void LooperCtrl::togglePlay()
{
    qInfo("toggle play:");

    auto song = Engine::getSong();
    auto pianoRoll = getGUI()->pianoRoll();

    if (pianoRoll->isRecording())
    {
        qInfo(" - is recording, toggle record");
        toggleRecord();
    }
    else if (song->isPlaying())
    {
        qInfo(" - is playing, stop play, set no action");
        song->stop();
        m_pendingAction = NoAction;
    }
    else
    {
        qInfo(" - is idle, start play");
        song->playSong();
    }
}


void LooperCtrl::toggleRecord()
{
    // note: 'updateSampleTracks' is emitted by Song on enforceLoop lambda (if
    // loop is reset) and also by setPlayPos, which is used when left, right
    // or home keys are pressed
    qInfo("toggle record:");

    auto song = Engine::getSong();
    auto pianoRoll = getGUI()->pianoRoll();

    if (pianoRoll->isRecording())
    {
        qInfo(" - is recording, stop recording");
        pianoRoll->stopRecording();
    }
    else if (song->isPlaying())
    {
        if (pianoRoll->currentMidiClip() == nullptr)
        {
            qWarning("Looper: record required, but no clip selected!");
            return;
        }

        if (m_pendingAction < ProtectedAction)
        {
            // start recording on next loop reset
            qInfo(" - is playing, set start record");
            m_pendingAction = StartRecord;
        }
    }
    else
    {
        qInfo(" - is idle, record accompany, set action stop record");
        pianoRoll->recordAccompany();
        m_pendingAction = StopRecord;
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

        for (auto it = inputs.constBegin(); it != inputs.constEnd (); it++)
        {
            port->subscribeReadablePort(it.key(), false);
        }
        emit port->readablePortsChanged();
    }
}


void LooperCtrl::saveSettings(QDomDocument &doc, QDomElement &element)
{
    // save key bindings
    auto keybinds = doc.createElement("keybinds");
    element.appendChild(keybinds);

    QMap<QString, KeyBind> keys =
    {
        {"play", m_play},
        {"record", m_record},
        {"muteCurrent", m_muteCurrent},
        {"unmuteAll", m_unmuteAll},
        {"solo", m_solo},
        {"clearNotes", m_clearNotes}
    };

    auto it = keys.constBegin();
    while (it != keys.constEnd())
    {
        auto key = doc.createElement("key");
        key.setAttribute("name", it.key());
        key.setAttribute("channel", QString::number(it.value().first));
        key.setAttribute("control", QString::number(it.value().second));
        keybinds.appendChild(key);
        it++;
    }

    // save midi input list
    // FIXME: add support for raw clients
    if (!Engine::audioEngine()->midiClient()->isRaw())
    {
        auto midi = doc.createElement("midi");
        element.appendChild(midi);

        auto mports = m_midiPort->readablePorts();
        for (auto it = mports.constBegin(); it != mports.constEnd(); it++)
        {
            if (it.value()) {
                auto input = doc.createElement("input");
                input.setAttribute("name", it.key());
                input.setAttribute("enabled", "1");
                midi.appendChild(input);
            }
        }
    }
}


void LooperCtrl::loadSettings(const QDomElement &element)
{
    // load key bindings
    QMap<QString, KeyBind*> keys =
    {
        {"play", &m_play},
        {"record", &m_record},
        {"muteCurrent", &m_muteCurrent},
        {"unmuteAll", &m_unmuteAll},
        {"solo", &m_solo},
        {"clearNotes", &m_clearNotes}
    };

    auto keybinds = element.firstChildElement("keybinds");
	if (!keybinds.isNull())
    {
        auto binds = keybinds.childNodes();
        for (int i = 0; i < binds.size(); i++)
        {
            auto bind = binds.at(i).toElement();
            auto it = keys.find(bind.attribute("name"));
            if (it != keys.end())
            {
                it.value()->first = bind.attribute("channel", "-1").toInt();
                it.value()->second = bind.attribute("control", "-1").toInt();
            }
        }
    }

    // load midi input list
    // FIXME: add support for raw clients
    if (!Engine::audioEngine()->midiClient()->isRaw())
    {
        auto midi = element.firstChildElement("midi");
        if (!midi.isNull())
        {
            auto mports = m_midiPort->readablePorts();
            auto inputs = midi.childNodes();

            // get list of enabled inputs
            QList<QString> enabled;
            for (int i = 0; i < inputs.size(); i++)
            {
                enabled.append(inputs.at(i).toElement().attribute("name"));
            }

            // enable only those inputs that were defined in preset
            for (auto it = mports.constBegin(); it != mports.constEnd(); it++)
            {
                m_midiPort->subscribeReadablePort(it.key(), enabled.contains(it.key()));
            }
        }
    }
}
