//////////////////////////////////////////////////////////////////////
// OTAdmin - OpenTibia  —  cross-platform front-end (wxWidgets)
//////////////////////////////////////////////////////////////////////
//
// This replaces the original Win32-only front-end (main.cpp + gui.cpp +
// stdafx.cpp + the .rc resources). It reproduces the original tool 1:1 in
// function — the same menu tree, the same input prompts and the same set of
// 27 admin commands — but renders through wxWidgets so it builds and runs
// natively on Windows, Linux and macOS. wxWidgets uses the platform's native
// widgets, so on Windows it looks essentially like the original.
//
// The networking / protocol / encryption core (commands.cpp, networkmessage.cpp,
// rsa.cpp) is reused unchanged; it talks back to this front-end only through
// addConsoleMessage() (see frontend.h).
//////////////////////////////////////////////////////////////////////

// definitions.h pulls in winsock2 before <windows.h> on Windows, so it must be
// included before the wxWidgets headers to avoid a winsock1/winsock2 clash.
#include "definitions.h"
#include "commands.h"
#include "frontend.h"

#include <wx/wx.h>
#include <wx/listbox.h>
#include <wx/textctrl.h>
#include <wx/textdlg.h>
#include <wx/statbmp.h>
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/image.h>

#include <string>

//////////////////////////////////////////////////////////////////////
// Globals the command/network core links against (were defined in main.cpp).
//////////////////////////////////////////////////////////////////////
long   next_command_delay = 0;
SOCKET g_socket    = SOCKET_ERROR;
bool   g_connected = false;

//////////////////////////////////////////////////////////////////////
// Menu / control identifiers
//////////////////////////////////////////////////////////////////////
enum
{
	ID_SET_SERVER = wxID_HIGHEST + 1,
	ID_CONNECT,
	ID_DISCONNECT,
	ID_BROADCAST,
	ID_KICK, ID_PREMIUM, ID_ADDON,
	ID_BAN_PLAYER, ID_BAN_ACCOUNT, ID_BAN_IP,
	ID_RELOAD_ACTIONS, ID_RELOAD_COMMANDS, ID_RELOAD_CONFIG, ID_RELOAD_MONSTERS,
	ID_RELOAD_MOVEMENTS, ID_RELOAD_RAIDS, ID_RELOAD_SPELLS, ID_RELOAD_TALKACTIONS,
	ID_RELOAD_WEAPONS, ID_RELOAD_EVERYTHING,
	ID_WT_NONPVP, ID_WT_PVP, ID_WT_PVP_ENF,
	ID_EXECUTE_RAID, ID_SERVER_SAVE, ID_CLEAN_MAP, ID_PLAYER_LIMIT,
	ID_CLEAR_SCREEN,
	ID_PLAYERLIST
};

//////////////////////////////////////////////////////////////////////
// Main window
//////////////////////////////////////////////////////////////////////
class MainFrame : public wxFrame
{
public:
	MainFrame();

	void appendConsole(const std::string& message);

private:
	// Replacement for the original CInputBox modal: a caption + a prompt,
	// returning the entered text. Mirrors the original behaviour where an
	// empty entry or a cancel simply does nothing.
	bool prompt(const wxString& caption, const wxString& message, std::string& out);

	void OnMenu(wxCommandEvent& event);
	void OnAbout(wxCommandEvent& event);
	void OnExit(wxCommandEvent& event);
	void OnClear(wxCommandEvent& event);
	void OnPlayerSelect(wxCommandEvent& event);

	wxListBox*    m_status   = nullptr;
	wxListBox*    m_players  = nullptr;
	wxTextCtrl*   m_nameField= nullptr;
	wxTextCtrl*   m_console  = nullptr;
};

//////////////////////////////////////////////////////////////////////
// addConsoleMessage hook (frontend.h) — routes the core's output to the GUI.
// Command handlers run on the GUI thread, so direct widget access is safe.
//////////////////////////////////////////////////////////////////////
static MainFrame* g_frame = nullptr;

void addConsoleMessage(const std::string& message)
{
	if(g_frame)
		g_frame->appendConsole(message);
}

// The command core takes non-const char*; hand it a writable view of the
// std::string (contiguous since C++11). The strings outlive every call here.
static char* cstr(std::string& s)
{
	return s.empty() ? const_cast<char*>("") : &s[0];
}

//////////////////////////////////////////////////////////////////////

MainFrame::MainFrame()
	: wxFrame(nullptr, wxID_ANY, "Evolutions Remote-Control 0.7.7",
	          wxDefaultPosition, wxDefaultSize,
	          wxDEFAULT_FRAME_STYLE & ~(wxRESIZE_BORDER | wxMAXIMIZE_BOX))
{
	const wxString exeDir =
		wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath();

#if defined(_WIN32)
	SetIcon(wxIcon(exeDir + wxFILE_SEP_PATH + "otserv.ico", wxBITMAP_TYPE_ICO));
#endif

	// ---- Menu bar (mirrors the original otserv.rc menu tree 1:1) ----------
	wxMenuBar* bar = new wxMenuBar();

	wxMenu* client = new wxMenu();
	client->Append(ID_SET_SERVER,  "&Set Server");
	client->Append(ID_CONNECT,     "&Connect Server");
	client->Append(ID_DISCONNECT,  "&Disconnect Server");
	client->AppendSeparator();
	client->Append(wxID_EXIT,      "E&xit");
	bar->Append(client, "&Client");

	wxMenu* action = new wxMenu();
	action->Append(ID_BROADCAST, "&Broadcast Message");

	wxMenu* playerActions = new wxMenu();
	playerActions->Append(ID_KICK,    "&Kick Player");
	playerActions->Append(ID_PREMIUM, "&Premium Account");
	playerActions->Append(ID_ADDON,   "&Outfit Addon");
	wxMenu* ban = new wxMenu();
	ban->Append(ID_BAN_PLAYER,  "&Ban Player");
	ban->Append(ID_BAN_ACCOUNT, "&Ban Account");
	ban->Append(ID_BAN_IP,      "&Ban IP-Address");
	playerActions->AppendSubMenu(ban, "&Ban Player");
	action->AppendSubMenu(playerActions, "&Player Actions");

	wxMenu* reload = new wxMenu();
	reload->Append(ID_RELOAD_ACTIONS,     "&Actions");
	reload->Append(ID_RELOAD_COMMANDS,    "&Commands");
	reload->Append(ID_RELOAD_CONFIG,      "&Config");
	reload->Append(ID_RELOAD_MONSTERS,    "&Monsters");
	reload->Append(ID_RELOAD_MOVEMENTS,   "&Movements");
	reload->Append(ID_RELOAD_RAIDS,       "&Raids");
	reload->Append(ID_RELOAD_SPELLS,      "&Spells");
	reload->Append(ID_RELOAD_TALKACTIONS, "&Talkactions");
	reload->Append(ID_RELOAD_WEAPONS,     "&Weapons");
	reload->AppendSeparator();
	reload->Append(ID_RELOAD_EVERYTHING,  "&Reload All");
	action->AppendSubMenu(reload, "&Reload File");

	wxMenu* world = new wxMenu();
	world->Append(ID_WT_NONPVP,  "&Non-PvP");
	world->Append(ID_WT_PVP,     "&PvP");
	world->Append(ID_WT_PVP_ENF, "&PvP-Enforced");
	action->AppendSubMenu(world, "&Change World Type");

	wxMenu* others = new wxMenu();
	others->Append(ID_EXECUTE_RAID, "&Execute Raid");
	others->Append(ID_SERVER_SAVE,  "&Force Server Save");
	others->Append(ID_CLEAN_MAP,    "&Clean Map");
	others->Append(ID_PLAYER_LIMIT, "&Set Max Players");
	action->AppendSubMenu(others, "&Others");

	bar->Append(action, "&Action");

	wxMenu* about = new wxMenu();
	about->Append(wxID_ABOUT, "&About");
	bar->Append(about, "&About");

	SetMenuBar(bar);

	// ---- Layout -----------------------------------------------------------
	wxPanel*     panel = new wxPanel(this);
	wxBoxSizer*  root  = new wxBoxSizer(wxVERTICAL);

	// Row 1: banner (logo.bmp, loaded next to the executable) + status list.
	wxBoxSizer* row1 = new wxBoxSizer(wxHORIZONTAL);
	wxBitmap banner;
	if(banner.LoadFile(exeDir + wxFILE_SEP_PATH + "logo.bmp", wxBITMAP_TYPE_BMP))
		row1->Add(new wxStaticBitmap(panel, wxID_ANY, banner), 0, wxALL, 5);

	wxStaticBoxSizer* statusBox = new wxStaticBoxSizer(wxVERTICAL, panel, "Status Messages");
	m_status = new wxListBox(panel, wxID_ANY, wxDefaultPosition, wxSize(220, 130));
	m_status->Append("Status Messages:");
	statusBox->Add(m_status, 1, wxEXPAND | wxALL, 3);
	row1->Add(statusBox, 1, wxEXPAND | wxALL, 5);
	root->Add(row1, 0, wxEXPAND);

	// Row 2: target player name field + online player list.
	wxBoxSizer* row2 = new wxBoxSizer(wxHORIZONTAL);
	wxStaticBoxSizer* nameBox = new wxStaticBoxSizer(wxVERTICAL, panel, "Player");
	m_nameField = new wxTextCtrl(panel, wxID_ANY, "", wxDefaultPosition, wxSize(180, -1));
	nameBox->Add(m_nameField, 0, wxEXPAND | wxALL, 3);
	row2->Add(nameBox, 0, wxALL, 5);

	wxStaticBoxSizer* playersBox = new wxStaticBoxSizer(wxVERTICAL, panel, "Players Online");
	m_players = new wxListBox(panel, ID_PLAYERLIST, wxDefaultPosition, wxSize(180, 150),
	                          0, nullptr, wxLB_SINGLE | wxLB_SORT);
	playersBox->Add(m_players, 1, wxEXPAND | wxALL, 3);
	row2->Add(playersBox, 1, wxEXPAND | wxALL, 5);
	root->Add(row2, 0, wxEXPAND);

	// Row 3: console log + clear button.
	wxStaticBoxSizer* consoleBox = new wxStaticBoxSizer(wxVERTICAL, panel, "Console");
	m_console = new wxTextCtrl(panel, wxID_ANY, "", wxDefaultPosition, wxSize(580, 220),
	                           wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
	consoleBox->Add(m_console, 1, wxEXPAND | wxALL, 3);
	consoleBox->Add(new wxButton(panel, ID_CLEAR_SCREEN, "Clear Screen"),
	                0, wxALIGN_RIGHT | wxALL, 3);
	root->Add(consoleBox, 1, wxEXPAND | wxALL, 5);

	panel->SetSizer(root);
	root->SetSizeHints(this);   // size the frame to its content and fix it there

	// ---- Events -----------------------------------------------------------
	Bind(wxEVT_MENU, &MainFrame::OnExit,  this, wxID_EXIT);
	Bind(wxEVT_MENU, &MainFrame::OnAbout, this, wxID_ABOUT);
	// All command menu items go through one dispatcher (mirrors the WndProc switch).
	for(int id = ID_SET_SERVER; id <= ID_PLAYER_LIMIT; ++id)
		Bind(wxEVT_MENU, &MainFrame::OnMenu, this, id);
	Bind(wxEVT_BUTTON,  &MainFrame::OnClear,        this, ID_CLEAR_SCREEN);
	Bind(wxEVT_LISTBOX, &MainFrame::OnPlayerSelect, this, ID_PLAYERLIST);

	g_frame = this;
}

void MainFrame::appendConsole(const std::string& message)
{
	m_console->AppendText(wxString::FromUTF8(message.c_str()));
	m_console->AppendText("\n");
}

bool MainFrame::prompt(const wxString& caption, const wxString& message, std::string& out)
{
	wxTextEntryDialog dlg(this, message, caption);
	if(dlg.ShowModal() != wxID_OK)
		return false;
	const wxString value = dlg.GetValue();
	if(value.empty())
		return false;
	out = value.ToStdString();
	return true;
}

void MainFrame::OnExit(wxCommandEvent&)
{
	Close(true);
}

void MainFrame::OnClear(wxCommandEvent&)
{
	m_console->Clear();
}

void MainFrame::OnPlayerSelect(wxCommandEvent& event)
{
	// Selecting a name in the online list copies it into the target field,
	// exactly like the original LBN_SELCHANGE handler.
	m_nameField->SetValue(event.GetString());
}

void MainFrame::OnAbout(wxCommandEvent&)
{
	wxMessageBox("Evolutions Remote-Control\nVersion 0.7.7\nCopyright 2007: Xidaozu and Junkfood",
	             "About", wxOK | wxICON_INFORMATION, this);
}

void MainFrame::OnMenu(wxCommandEvent& event)
{
	std::string p;
	switch(event.GetId())
	{
		case ID_SET_SERVER:
			if(prompt("Set Server",
			          "Please enter the IP-Address and Server Port you want to get a "
			          "connection with when you connect.\nExample: \"localhost 7171\"", p))
				if(setServer(cstr(p)) == 1)
					addConsoleMessage("Successfully set server.");
			break;

		case ID_CONNECT:
			if(prompt("Connect Server", "Enter the security password to connect to the server.", p))
				if(cmdConnect(cstr(p)) == 1)
					addConsoleMessage("Connected to server.");
			break;

		case ID_DISCONNECT:    cmdDisconnect(nullptr); break;

		case ID_BROADCAST:
			if(prompt("Broadcast", "Please enter the message you want to broadcast.", p))
				commandBroadcast(cstr(p));
			break;

		case ID_KICK:
			if(prompt("Kick Player", "Please enter the name of the player that want to kick.", p))
				commandKickPlayer(cstr(p));
			break;

		case ID_PREMIUM:
			if(prompt("Premium Account", "Please enter the name of the player that want to give 7 days of premium.", p))
				commandPremiumPlayer(cstr(p));
			break;

		case ID_ADDON:
			if(prompt("Outfit Addons", "Please enter the name of the player that want to give both addons.", p))
				commandAddonPlayer(cstr(p));
			break;

		case ID_BAN_PLAYER:
			if(prompt("Ban Player", "Please enter the name of the player that want to ban for 7 days.", p))
				commandBanPlayer(cstr(p));
			break;

		case ID_BAN_ACCOUNT:
			if(prompt("Ban Account by Playername", "Please enter the name of the player of the account you want to ban for 7 days.", p))
				commandBanAccount(cstr(p));
			break;

		case ID_BAN_IP:
			if(prompt("Ban IP-Address by Playername", "Please enter the name of the player of the IP-Address you want to ban for 7 days.", p))
				commandBanIP(cstr(p));
			break;

		// The original "Reload > Actions" menu item had no matching server
		// command and fell through to DefWindowProc, i.e. it did nothing.
		case ID_RELOAD_ACTIONS: break;

		case ID_RELOAD_COMMANDS:    commandReloadCommands();    break;
		case ID_RELOAD_CONFIG:      commandReloadConfig(); addConsoleMessage("Reloaded config."); break;
		case ID_RELOAD_MONSTERS:    commandReloadMonsters();    break;
		case ID_RELOAD_MOVEMENTS:   commandReloadMovements();   break;
		case ID_RELOAD_RAIDS:       commandReloadRaids();       break;
		case ID_RELOAD_SPELLS:      commandReloadSpells();      break;
		case ID_RELOAD_TALKACTIONS: commandReloadTalkactions(); break;
		case ID_RELOAD_WEAPONS:     commandReloadWeapons(); addConsoleMessage("Reloaded weapons."); break;
		case ID_RELOAD_EVERYTHING:  commandReloadEverything();  break;

		case ID_WT_NONPVP:  commandSetWorldType(CMD_SET_NON_PVP); break;
		case ID_WT_PVP:     commandSetWorldType(CMD_SET_PVP);     break;
		case ID_WT_PVP_ENF: commandSetWorldType(CMD_SET_PVP_ENF); break;

		case ID_EXECUTE_RAID:
			if(prompt("Execute Raid", "Please enter the name of the raid that you want to execute.", p))
				commandExecuteRaid(cstr(p));
			break;

		case ID_SERVER_SAVE: commandServerSave(); break;
		case ID_CLEAN_MAP:   commandCleanMap();   break;

		case ID_PLAYER_LIMIT:
			if(prompt("Change Player Limit", "Please enter the amount of the new player limit.", p))
				commandSetPlayerLimit(cstr(p));
			break;
	}
}

//////////////////////////////////////////////////////////////////////
// Application entry point
//////////////////////////////////////////////////////////////////////
class RemoteControlApp : public wxApp
{
public:
	bool OnInit() override
	{
#if defined(_WIN32)
		WSADATA wsd;
		if(WSAStartup(MAKEWORD(2, 2), &wsd) != 0)
			return false;
#endif
		wxInitAllImageHandlers();
		(new MainFrame())->Show(true);
		return true;
	}

	int OnExit() override
	{
		g_frame = nullptr;
#if defined(_WIN32)
		WSACleanup();
#endif
		return 0;
	}
};

wxIMPLEMENT_APP(RemoteControlApp);
