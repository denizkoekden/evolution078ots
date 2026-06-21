//////////////////////////////////////////////////////////////////////
// OTAdmin - OpenTibia
//////////////////////////////////////////////////////////////////////
//
// Portable front-end hook. The networking/command core (commands.cpp,
// networkmessage.cpp) reports progress and errors through addConsoleMessage().
// The original code routed this straight into a Win32 list box; the modern,
// cross-platform build implements it in the active front-end (the wxWidgets
// GUI in gui_wx.cpp) so the core stays free of any windowing API.
//////////////////////////////////////////////////////////////////////

#ifndef __OTADMIN_FRONTEND_H__
#define __OTADMIN_FRONTEND_H__

#include <string>

void addConsoleMessage(const std::string& message);

#endif
