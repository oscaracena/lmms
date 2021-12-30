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


#include "looper.h"

#include "embed.h"
#include "plugin_export.h"


extern "C"
{

Plugin::Descriptor PLUGIN_EXPORT looper_plugin_descriptor =
{
    STRINGIFY(PLUGIN_NAME),
    "Looper Tool",
    QT_TRANSLATE_NOOP("pluginBrowser",
                        "Tool to help with live looping"),
    "Oscar Acena <oscaracena/at/gmail/dot/com>",
    0x0100,
    Plugin::Tool,
        new PluginPixmapLoader("logo"),
        nullptr,
        nullptr
};


PLUGIN_EXPORT Plugin * lmms_plugin_main(Model * _parent, void * _data)
{
    return new LooperTool;
}

} // end of extern "C"


LooperTool::LooperTool() :
	ToolPlugin(&looper_plugin_descriptor, nullptr)
{
}


QString LooperTool::nodeName() const
{
	return looper_plugin_descriptor.name;
}