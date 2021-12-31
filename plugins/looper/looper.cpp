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
    * add an empty TCO on start of each track (if not exist)
    * set MIDI input to only current selected track
    * use MIDI Program Change (PC) messages to select track on piano-roll
    * use MIDI Control Cahnge (CC) messages to:
        - up/down track on piano-roll
        - start/stop play and record
    * control launch-Q (determine when a track will start playing/recording)
    * support custom MIDI mappings for above controls
    * save/load profiles
*/


#include "looper.h"

#include <iostream>

#include <QMdiSubWindow>
#include <QVBoxLayout>

#include "embed.h"
#include "Engine.h"
#include "plugin_export.h"
#include "Song.h"
#include "TimeLineWidget.h"


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
	ToolPluginView(tool)
{
    // Widget is initially hidden
    auto parent = parentWidget();
    // parent->hide(); // FIXME: remove on production

    // Set some size related properties
    parent->resize(350, 300);
    parent->setMinimumWidth(350);
	parent->setMinimumHeight(300);

    // Remove maximize button
    Qt::WindowFlags flags = parent->windowFlags();
	flags &= ~Qt::WindowMaximizeButtonHint;
	parent->setWindowFlags(flags);

    // Add a GroupBox to enable/disable this component
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
	m_groupBox = new GroupBox(tr("Loop Controller:"));
    mainLayout->addWidget(m_groupBox);

    QVBoxLayout *gBoxLayout = new QVBoxLayout();
	gBoxLayout->setContentsMargins(5, 16, 5, 5);
	m_groupBox->setLayout(gBoxLayout);

    // FIXME: just for TESTING
	QPushButton* btn = new QPushButton;
	btn->setText(tr("test"));
	gBoxLayout->addWidget(btn);
	connect(btn, SIGNAL(clicked()), this, SLOT(onTestClicked()));
}

void LooperView::onTestClicked() {

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

    // --- [WIP] set MIDI input to only current selected track  -------------------
    auto song = Engine::getSong();
    for(auto t: song->tracks()) {
        std::cout << t->numOfTCOs() << std::endl;
    }

}