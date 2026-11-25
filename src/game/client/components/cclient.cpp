/* Cr1stal Client (C-Client): save wheel and map markers. Based on DDNet, see licence.txt. */
#include "cclient.h"

#include <base/io.h>
#include <base/log.h>
#include <base/process.h>
#include <base/str.h>

#include <engine/gfx/image_loader.h>
#include <engine/graphics.h>
#include <engine/http.h>
#include <engine/image.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/shared/json.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <generated/protocol.h>

#include <game/client/cclient_version.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/collision.h>
#include <game/localization.h>
#include <game/mapitems.h>

#include <algorithm>
#include <cmath>

static const char *const s_apSaveFiles[CCClient::NUM_SAVES] = {
	"cclient/save_hook_up.png",
	"cclient/save_hook_right.png",
	"cclient/save_hammer.png",
	"cclient/save_stand_under.png",
	"cclient/save_hook_down.png",
	"cclient/save_hook_left.png",
	"cclient/save_wait.png",
	"cclient/save_thanks.png",
};

static const char *const s_apAuraFiles[CCClient::NUM_AURAS] = {
	"cclient/aura_cross.png",
	"cclient/aura_cross_ring.png",
	"cclient/aura_holy.png",
	"cclient/aura_gothic.png",
};

// family name as stored in the font file, and the name shown in the settings
static const char *const s_aapFonts[CCClient::NUM_FONTS][2] = {
	{"", "DejaVu Sans"},
	{"Montserrat", "Montserrat"},
	{"Rubik", "Rubik"},
	{"Comfortaa", "Comfortaa"},
	{"Exo 2", "Exo 2"},
	{"Russo One", "Russo One"},
	{"Press Start 2P", "Press Start 2P"},
	{"Unbounded", "Unbounded"},
	{"Play", "Play"},
	{"Jura", "Jura"},
	{"Lobster", "Lobster"},
};

const char *CCClient::FontFamily(int Font)
{
	return s_aapFonts[std::clamp(Font, 0, NUM_FONTS - 1)][0];
}

const char *CCClient::FontDisplayName(int Font)
{
	return s_aapFonts[std::clamp(Font, 0, NUM_FONTS - 1)][1];
}

void CCClient::ApplyFont()
{
	if(m_AppliedFont == g_Config.m_CcFont)
		return;
	const bool FirstTime = m_AppliedFont == -1;
	m_AppliedFont = g_Config.m_CcFont;
	TextRender()->SetCustomDefaultFont(FontFamily(g_Config.m_CcFont));
	// cached texts everywhere have to be laid out again with the new font
	// (not during start-up, nothing is cached yet and not every component is ready)
	if(!FirstTime)
		GameClient()->OnWindowResize();
}

void CCClient::RenderAura(vec2 Pos)
{
	const int Aura = std::clamp(g_Config.m_CcAura, 1, NUM_AURAS) - 1;
	const float Size = (float)g_Config.m_CcAuraSize;
	const float Time = Client()->GlobalTime();
	const float Pulse = 0.85f + 0.15f * std::sin(Time * 2.0f);
	Graphics()->BlendAdditive();
	Graphics()->TextureSet(m_aAuraTextures[Aura]);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, g_Config.m_CcAuraAlpha / 100.0f * Pulse);
	Graphics()->QuadsSetRotation(Time * g_Config.m_CcAuraSpin / 100.0f);
	IGraphics::CQuadItem Quad(Pos.x, Pos.y, Size, Size);
	Graphics()->QuadsDraw(&Quad, 1);
	Graphics()->QuadsSetRotation(0.0f);
	Graphics()->QuadsEnd();
	Graphics()->BlendNormal();
}

CCClient::CCClient()
{
	OnReset();
}

const char *CCClient::SaveName(int Save)
{
	switch(Save)
	{
	case SAVE_HOOK_UP: return Localize("Hook me up");
	case SAVE_HOOK_RIGHT: return Localize("Hook me to the right");
	case SAVE_HAMMER: return Localize("Hammer me");
	case SAVE_STAND_UNDER: return Localize("Stand under me");
	case SAVE_HOOK_DOWN: return Localize("Pull me down");
	case SAVE_HOOK_LEFT: return Localize("Hook me to the left");
	case SAVE_WAIT: return Localize("Wait, don't touch me");
	case SAVE_THANKS: return Localize("Thanks!");
	default: return "";
	}
}

void CCClient::ConPing(IConsole::IResult *pResult, void *pUserData)
{
	static_cast<CCClient *>(pUserData)->SendPing();
}

void CCClient::OnConsoleInit()
{
	Console()->Register("cc_ping", "", CFGFLAG_CLIENT, ConPing, this, "C-Client: put a marker for your team where your cursor is");
}

void CCClient::OnInit()
{
	for(int i = 0; i < NUM_SAVES; i++)
		m_aSaveTextures[i] = Graphics()->LoadTexture(s_apSaveFiles[i], IStorage::TYPE_ALL);
	for(int i = 0; i < NUM_AURAS; i++)
		m_aAuraTextures[i] = Graphics()->LoadTexture(s_apAuraFiles[i], IStorage::TYPE_ALL);
	m_IconTexture = Graphics()->LoadTexture("cclient/icon.png", IStorage::TYPE_ALL);
	ApplyFont();
	m_Media.Start();

	if(g_Config.m_CcUpdateCheck)
		CheckForUpdate();

	// give the new features keys once, without touching keys the player already uses
	if(!g_Config.m_CcDefaultBindsDone)
	{
		Console()->ExecuteLine("bind mouse3 cc_ping", IConsole::CLIENT_ID_UNSPECIFIED);
		g_Config.m_CcDefaultBindsDone = 1;
	}
}

void CCClient::OnReset()
{
	m_WheelActive = false;
	m_SelectorMouse = vec2(0.0f, 0.0f);
	m_SelectedSave = -1;
	for(auto &Marker : m_aMarkers)
		Marker.m_Time = -100.0f;
	m_NextMarker = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		m_aSaveRequest[i] = -1;
		m_aSaveRequestTime[i] = -100.0f;
	}
}

void CCClient::OnRelease()
{
	m_WheelActive = false;
	m_SelectedSave = -1;
}

int CCClient::LocalTeam() const
{
	const int LocalId = GameClient()->m_Snap.m_LocalClientId;
	if(LocalId < 0)
		return -1;
	return GameClient()->m_Teams.Team(LocalId);
}

bool CCClient::CanSend()
{
	// do not spam the chat
	const float Now = Client()->GlobalTime();
	if(Now - m_LastSendTime < 1.2f)
		return false;
	m_LastSendTime = Now;
	return true;
}

void CCClient::SendPing()
{
	if(Client()->State() != IClient::STATE_ONLINE || GameClient()->m_Snap.m_LocalClientId < 0)
		return;
	const vec2 Pos = GameClient()->m_Controls.m_aTargetPos[g_Config.m_ClDummy];
	const int Team = LocalTeam();
	if(Team <= TEAM_FLOCK || Team >= TEAM_SUPER)
	{
		// not in a team: only you see the marker, nothing is sent to everyone
		AddMarker(Pos, GameClient()->m_Snap.m_LocalClientId);
		return;
	}
	if(!CanSend())
		return;
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s #ccp:%d:%d", Localize("Over here!"), round_to_int(Pos.x), round_to_int(Pos.y));
	GameClient()->m_Chat.SendChat(1, aBuf);
}

void CCClient::SendSave(int Save)
{
	if(Save < 0 || Save >= NUM_SAVES || Client()->State() != IClient::STATE_ONLINE)
		return;
	const int LocalId = GameClient()->m_Snap.m_LocalClientId;
	const int Team = LocalTeam();
	if(Team <= TEAM_FLOCK || Team >= TEAM_SUPER)
	{
		ShowSave(LocalId, Save);
		return;
	}
	if(!CanSend())
		return;
	char aBuf[160];
	str_format(aBuf, sizeof(aBuf), "%s: %s #ccs:%d", Localize("Save me"), SaveName(Save), Save);
	GameClient()->m_Chat.SendChat(1, aBuf);
}

void CCClient::AddMarker(vec2 Pos, int ClientId)
{
	SMarker &Marker = m_aMarkers[m_NextMarker];
	Marker.m_Pos = Pos;
	Marker.m_ClientId = ClientId;
	Marker.m_Time = Client()->GlobalTime();
	m_NextMarker = (m_NextMarker + 1) % MAX_MARKERS;
}

void CCClient::ShowSave(int ClientId, int Save)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;
	m_aSaveRequest[ClientId] = Save;
	m_aSaveRequestTime[ClientId] = Client()->GlobalTime();
}

void CCClient::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType != NETMSGTYPE_SV_CHAT)
		return;
	const CNetMsg_Sv_Chat *pMsg = static_cast<const CNetMsg_Sv_Chat *>(pRawMsg);
	if(pMsg->m_ClientId < 0 || pMsg->m_ClientId >= MAX_CLIENTS || !pMsg->m_pMessage)
		return;

	// only from your own team
	const int LocalTeamId = LocalTeam();
	if(pMsg->m_ClientId != GameClient()->m_Snap.m_LocalClientId && (LocalTeamId < 0 || GameClient()->m_Teams.Team(pMsg->m_ClientId) != LocalTeamId))
		return;

	if(const char *pTag = str_find(pMsg->m_pMessage, "#ccp:"))
	{
		int X = 0, Y = 0;
		if(sscanf(pTag, "#ccp:%d:%d", &X, &Y) == 2)
			AddMarker(vec2(X, Y), pMsg->m_ClientId);
	}
	if(const char *pTag = str_find(pMsg->m_pMessage, "#ccs:"))
	{
		int Save = -1;
		if(sscanf(pTag, "#ccs:%d", &Save) == 1 && Save >= 0 && Save < NUM_SAVES)
			ShowSave(pMsg->m_ClientId, Save);
	}
}

bool CCClient::OnCursorMove(float x, float y, IInput::ECursorType CursorType)
{
	if(!m_WheelActive)
		return false;
	Ui()->ConvertMouseMove(&x, &y, CursorType);
	m_SelectorMouse += vec2(x, y);
	return true;
}

bool CCClient::OnInput(const IInput::CEvent &Event)
{
	if(!m_WheelActive)
	{
		// Shift (emote wheel) + Q or E switches to the save wheel
		if((Event.m_Flags & IInput::FLAG_PRESS) && (Event.m_Key == KEY_Q || Event.m_Key == KEY_E) && GameClient()->m_Emoticon.IsActive())
		{
			GameClient()->m_Emoticon.OnRelease();
			m_WheelActive = true;
			m_SelectorMouse = vec2(0.0f, 0.0f);
			m_SelectedSave = -1;
			return true;
		}
		return false;
	}

	if((Event.m_Flags & IInput::FLAG_PRESS) && Event.m_Key == KEY_ESCAPE)
	{
		OnRelease();
		return true;
	}
	// releasing shift sends the chosen save
	if((Event.m_Flags & IInput::FLAG_RELEASE) && (Event.m_Key == KEY_LSHIFT || Event.m_Key == KEY_RSHIFT))
	{
		SendSave(m_SelectedSave);
		OnRelease();
		return false; // the bind system still needs to see shift being released
	}
	if(Event.m_Key == KEY_Q || Event.m_Key == KEY_E)
		return true; // don't trigger the normal Q / E binds while the wheel is open
	return false;
}

void CCClient::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	RenderWorld();

	if(m_WheelActive)
	{
		if(GameClient()->m_Chat.IsActive() || GameClient()->m_Menus.IsActive() || GameClient()->m_Scoreboard.IsActive() || !GameClient()->m_Snap.m_pLocalCharacter)
			OnRelease();
		else
			RenderWheel();
	}
}

void CCClient::RenderWorld()
{
	const float Now = Client()->GlobalTime();
	bool AnyMarker = false;
	for(const auto &Marker : m_aMarkers)
		AnyMarker |= Now - Marker.m_Time < MARKER_LIFETIME;
	bool AnySave = false;
	for(int i = 0; i < MAX_CLIENTS; i++)
		AnySave |= m_aSaveRequest[i] >= 0 && Now - m_aSaveRequestTime[i] < SAVE_LIFETIME;
	if(!AnyMarker && !AnySave)
		return;

	const vec2 Center = GameClient()->m_Camera.m_Center;
	const CScreenRect ScreenRect = Graphics()->MapScreenToWorld(Center.x, Center.y, 100.0f, 100.0f, 100.0f, 0, 0, Graphics()->ScreenAspect(), GameClient()->m_Camera.m_Zoom);
	Graphics()->MapScreen(ScreenRect);

	// markers: pulsing ring and a diamond, with the name of who placed it
	for(const auto &Marker : m_aMarkers)
	{
		const float Age = Now - Marker.m_Time;
		if(Age >= MARKER_LIFETIME)
			continue;
		const float Fade = std::min(1.0f, (MARKER_LIFETIME - Age) / 1.5f);
		const float Pulse = std::fmod(Age * 1.5f, 1.0f);
		const vec2 Pos = Marker.m_Pos;

		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0.0f, 0.95f, 1.0f, 0.35f * (1.0f - Pulse) * Fade);
		Graphics()->DrawCircle(Pos.x, Pos.y, 20.0f + 50.0f * Pulse, 32);
		Graphics()->SetColor(1.0f, 0.2f, 0.85f, 0.85f * Fade);
		Graphics()->QuadsSetRotation(pi / 4.0f);
		IGraphics::CQuadItem Diamond(Pos.x, Pos.y, 22.0f, 22.0f);
		Graphics()->QuadsDraw(&Diamond, 1);
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.9f * Fade);
		IGraphics::CQuadItem Inner(Pos.x, Pos.y, 9.0f, 9.0f);
		Graphics()->QuadsDraw(&Inner, 1);
		Graphics()->QuadsSetRotation(0.0f);
		Graphics()->QuadsEnd();

		if(Marker.m_ClientId >= 0 && Marker.m_ClientId < MAX_CLIENTS)
		{
			const char *pName = GameClient()->m_aClients[Marker.m_ClientId].m_aName;
			const float FontSize = 18.0f;
			TextRender()->TextColor(0.85f, 0.95f, 1.0f, Fade);
			TextRender()->Text(Pos.x - TextRender()->TextWidth(FontSize, pName) / 2.0f, Pos.y - 45.0f, FontSize, pName);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}
	}

	// save requests: a picture above the tee that asked for it
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		const int Save = m_aSaveRequest[i];
		if(Save < 0 || Now - m_aSaveRequestTime[i] >= SAVE_LIFETIME)
			continue;
		if(!GameClient()->m_Snap.m_aCharacters[i].m_Active)
			continue;
		const float Fade = std::min(1.0f, (SAVE_LIFETIME - (Now - m_aSaveRequestTime[i])) / 1.0f);
		const float Bob = std::sin(Now * 5.0f) * 3.0f;
		const vec2 Pos = GameClient()->m_aClients[i].m_RenderPos + vec2(0.0f, -95.0f + Bob);
		const float Size = 64.0f;

		Graphics()->TextureSet(m_aSaveTextures[Save]);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, Fade);
		IGraphics::CQuadItem Icon(Pos.x, Pos.y, Size, Size);
		Graphics()->QuadsDraw(&Icon, 1);
		Graphics()->QuadsEnd();

		const char *pText = SaveName(Save);
		const float FontSize = 14.0f;
		TextRender()->TextColor(1.0f, 0.85f, 1.0f, Fade);
		TextRender()->Text(Pos.x - TextRender()->TextWidth(FontSize, pText) / 2.0f, Pos.y - Size / 2.0f - FontSize - 4.0f, FontSize, pText);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
}

void CCClient::RenderWheel()
{
	const CUIRect Screen = *Ui()->Screen();
	const vec2 ScreenCenter = Screen.Center();
	Ui()->MapScreen();

	if(length(m_SelectorMouse) > 170.0f)
		m_SelectorMouse = normalize(m_SelectorMouse) * 170.0f;
	m_SelectedSave = -1;
	if(length(m_SelectorMouse) > 50.0f)
		m_SelectedSave = (int)(std::fmod(angle(m_SelectorMouse) + 2 * pi + pi / (float)NUM_SAVES, 2 * pi) / (2 * pi) * (float)NUM_SAVES) % NUM_SAVES;

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.06f, 0.01f, 0.14f, 0.75f);
	Graphics()->DrawCircle(ScreenCenter.x, ScreenCenter.y, 200.0f, 64);
	Graphics()->SetColor(0.6f, 0.3f, 1.0f, 0.25f);
	Graphics()->DrawCircle(ScreenCenter.x, ScreenCenter.y, 60.0f, 48);
	Graphics()->QuadsEnd();

	for(int Save = 0; Save < NUM_SAVES; Save++)
	{
		float Angle = 2 * pi * Save / (float)NUM_SAVES;
		if(Angle > pi)
			Angle -= 2 * pi;
		const vec2 Pos = ScreenCenter + direction(Angle) * 140.0f;
		const bool Selected = m_SelectedSave == Save;
		const float Size = Selected ? 86.0f : 62.0f;

		if(Selected)
		{
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(0.0f, 0.95f, 1.0f, 0.25f);
			Graphics()->DrawCircle(Pos.x, Pos.y, Size * 0.62f, 32);
			Graphics()->QuadsEnd();
		}

		Graphics()->TextureSet(m_aSaveTextures[Save]);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, Selected ? 1.0f : 0.8f);
		IGraphics::CQuadItem Icon(Pos.x, Pos.y, Size, Size);
		Graphics()->QuadsDraw(&Icon, 1);
		Graphics()->QuadsEnd();
	}

	// name of the selected save in the middle
	const char *pTitle = m_SelectedSave >= 0 ? SaveName(m_SelectedSave) : Localize("How to save you?");
	const float FontSize = 12.0f;
	CUIRect Label;
	Label.x = ScreenCenter.x - 58.0f;
	Label.y = ScreenCenter.y - 20.0f;
	Label.w = 116.0f;
	Label.h = 40.0f;
	SLabelProperties Props;
	Props.m_MaxWidth = Label.w;
	Props.SetColor(ColorRGBA(0.85f, 0.95f, 1.0f, 1.0f));
	Ui()->DoLabel(&Label, pTitle, FontSize, TEXTALIGN_MC, Props);

	RenderTools()->RenderCursor(ScreenCenter + m_SelectorMouse, 24.0f);
}

// ---------------------------------------------------------------- updater

static const char *const s_pUpdateBaseUrl = "https://github.com/Cr1staIl/Cr1stalClient/releases/download/test-latest/";
static const char *const s_pUpdateFile = "cclient_update/Cr1stalClient-Setup.exe";

int CCClient::CurrentBuild()
{
	return CCLIENT_BUILD;
}

bool CCClient::CanSelfUpdate() const
{
#if defined(CONF_FAMILY_WINDOWS)
	return CurrentBuild() > 0; // only builds made by GitHub know their number
#else
	return false;
#endif
}

void CCClient::CheckForUpdate()
{
	if(!CanSelfUpdate() || m_UpdateState == EUpdateState::CHECKING || m_UpdateState == EUpdateState::DOWNLOADING || m_UpdateState == EUpdateState::INSTALLING)
		return;
	char aUrl[256];
	str_format(aUrl, sizeof(aUrl), "%sversion.json", s_pUpdateBaseUrl);
	m_pVersionRequest = HttpGet(aUrl);
	m_pVersionRequest->Timeout(CTimeout{10000, 0, 500, 10});
	m_pVersionRequest->MaxResponseSize(16 * 1024);
	m_pVersionRequest->LogProgress(HTTPLOG::FAILURE);
	Http()->Run(m_pVersionRequest);
	m_UpdateState = EUpdateState::CHECKING;
}

void CCClient::StartUpdate()
{
	if(m_UpdateState != EUpdateState::AVAILABLE && m_UpdateState != EUpdateState::FAILED)
		return;
	Storage()->CreateFolder("cclient_update", IStorage::TYPE_SAVE);
	char aUrl[256];
	str_format(aUrl, sizeof(aUrl), "%sCr1stalClient-Setup.exe", s_pUpdateBaseUrl);
	m_pSetupRequest = HttpGetFile(aUrl, Storage(), s_pUpdateFile, IStorage::TYPE_SAVE);
	m_pSetupRequest->Timeout(CTimeout{10000, 0, 500, 30});
	m_pSetupRequest->MaxResponseSize(300 * 1024 * 1024);
	Http()->Run(m_pSetupRequest);
	m_UpdateState = EUpdateState::DOWNLOADING;
}

float CCClient::UpdateProgress() const
{
	if(m_UpdateState == EUpdateState::DOWNLOADING && m_pSetupRequest)
		return m_pSetupRequest->Progress() / 100.0f;
	return m_UpdateState == EUpdateState::INSTALLING ? 1.0f : 0.0f;
}

void CCClient::OnUpdate()
{
	ApplyFont();
	UpdateMedia();

	if(m_UpdateState == EUpdateState::CHECKING && m_pVersionRequest && m_pVersionRequest->Done())
	{
		m_UpdateState = EUpdateState::FAILED;
		if(m_pVersionRequest->State() == EHttpState::DONE)
		{
			json_value *pJson = m_pVersionRequest->ResultJson();
			if(pJson)
			{
				const json_value *pBuild = json_object_get(pJson, "build");
				if(pBuild && pBuild->type == json_integer)
				{
					m_LatestBuild = json_int_get(pBuild);
					m_UpdateState = m_LatestBuild > CurrentBuild() ? EUpdateState::AVAILABLE : EUpdateState::UP_TO_DATE;
				}
				json_value_free(pJson);
			}
		}
		if(m_UpdateState == EUpdateState::FAILED)
			m_UpdateState = EUpdateState::NONE; // no button when the check simply did not work
		m_pVersionRequest = nullptr;
	}

	if(m_UpdateState == EUpdateState::DOWNLOADING && m_pSetupRequest && m_pSetupRequest->Done())
	{
		if(m_pSetupRequest->State() == EHttpState::DONE)
			FinishUpdate();
		else
			m_UpdateState = EUpdateState::FAILED;
		m_pSetupRequest = nullptr;
	}
}

void CCClient::FinishUpdate()
{
	char aPath[IO_MAX_PATH_LENGTH];
	Storage()->GetCompletePath(IStorage::TYPE_SAVE, s_pUpdateFile, aPath, sizeof(aPath));
	// the installer closes the game, installs over it and starts it again
	const char *apArgs[] = {"/SILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/CLOSEAPPLICATIONS"};
	const PROCESS Process = process_execute(aPath, EShellExecuteWindowState::FOREGROUND, apArgs, std::size(apArgs));
	if(Process == INVALID_PROCESS)
	{
		log_error("cclient", "could not start the installer '%s'", aPath);
		m_UpdateState = EUpdateState::FAILED;
		return;
	}
	m_UpdateState = EUpdateState::INSTALLING;
	Client()->Quit();
}

void CCClient::OnShutdown()
{
	m_Media.Stop();
	if(m_pVersionRequest)
		m_pVersionRequest->Abort();
	if(m_pSetupRequest && m_UpdateState == EUpdateState::DOWNLOADING)
		m_pSetupRequest->Abort();
}

// ---------------------------------------------------------------- tile outlines

enum
{
	OUTLINE_NONE = 0,
	OUTLINE_SOLID,
	OUTLINE_FREEZE,
	OUTLINE_UNFREEZE,
	OUTLINE_DEATH,
	NUM_OUTLINE_KINDS,
};

static int OutlineKind(const CCollision *pCollision, int Index)
{
	const int Game = pCollision->GetTileIndex(Index);
	const int Front = pCollision->GetFrontTileIndex(Index);
	if(Game == TILE_SOLID || Game == TILE_NOHOOK)
		return OUTLINE_SOLID;
	if(Game == TILE_DEATH || Front == TILE_DEATH)
		return OUTLINE_DEATH;
	auto IsFreeze = [](int Tile) { return Tile == TILE_FREEZE || Tile == TILE_DFREEZE || Tile == TILE_LFREEZE; };
	auto IsUnfreeze = [](int Tile) { return Tile == TILE_UNFREEZE || Tile == TILE_DUNFREEZE || Tile == TILE_LUNFREEZE; };
	if(IsFreeze(Game) || IsFreeze(Front))
		return OUTLINE_FREEZE;
	if(IsUnfreeze(Game) || IsUnfreeze(Front))
		return OUTLINE_UNFREEZE;
	return OUTLINE_NONE;
}

void CCOutlines::OnRender()
{
	if(!g_Config.m_CcOutline)
		return;
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	const CCollision *pCollision = Collision();
	const int Width = pCollision->GetWidth();
	const int Height = pCollision->GetHeight();
	if(Width <= 0 || Height <= 0)
		return;

	const bool aEnabled[NUM_OUTLINE_KINDS] = {false, (bool)g_Config.m_CcOutlineSolid, (bool)g_Config.m_CcOutlineFreeze, (bool)g_Config.m_CcOutlineUnfreeze, (bool)g_Config.m_CcOutlineDeath};
	const ColorRGBA aColors[NUM_OUTLINE_KINDS] = {
		ColorRGBA(0, 0, 0, 0),
		color_cast<ColorRGBA>(ColorHSLA(g_Config.m_CcOutlineColorSolid, true)),
		color_cast<ColorRGBA>(ColorHSLA(g_Config.m_CcOutlineColorFreeze, true)),
		color_cast<ColorRGBA>(ColorHSLA(g_Config.m_CcOutlineColorUnfreeze, true)),
		color_cast<ColorRGBA>(ColorHSLA(g_Config.m_CcOutlineColorDeath, true)),
	};

	const CScreenRect OldScreen = Graphics()->GetScreen();
	const vec2 Center = GameClient()->m_Camera.m_Center;
	const CScreenRect ScreenRect = Graphics()->MapScreenToWorld(Center.x, Center.y, 100.0f, 100.0f, 100.0f, 0, 0, Graphics()->ScreenAspect(), GameClient()->m_Camera.m_Zoom);
	Graphics()->MapScreen(ScreenRect);

	const int StartX = std::clamp((int)std::floor(ScreenRect.m_TopLeft.x / 32.0f) - 1, 0, Width - 1);
	const int StartY = std::clamp((int)std::floor(ScreenRect.m_TopLeft.y / 32.0f) - 1, 0, Height - 1);
	const int EndX = std::clamp((int)std::ceil(ScreenRect.m_BottomRight.x / 32.0f) + 1, 0, Width - 1);
	const int EndY = std::clamp((int)std::ceil(ScreenRect.m_BottomRight.y / 32.0f) + 1, 0, Height - 1);
	const float W = (float)g_Config.m_CcOutlineWidth;

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	for(int Kind = OUTLINE_SOLID; Kind < NUM_OUTLINE_KINDS; Kind++)
	{
		if(!aEnabled[Kind])
			continue;
		Graphics()->SetColor(aColors[Kind]);
		IGraphics::CQuadItem aQuads[64];
		int NumQuads = 0;
		auto Push = [&](float X, float Y, float QW, float QH) {
			aQuads[NumQuads++] = IGraphics::CQuadItem(X, Y, QW, QH);
			if(NumQuads == (int)std::size(aQuads))
			{
				Graphics()->QuadsDrawTL(aQuads, NumQuads);
				NumQuads = 0;
			}
		};
		for(int y = StartY; y <= EndY; y++)
		{
			for(int x = StartX; x <= EndX; x++)
			{
				if(OutlineKind(pCollision, y * Width + x) != Kind)
					continue;
				const float X = x * 32.0f, Y = y * 32.0f;
				// draw the edges that border a different kind of tile, on the inside of this tile
				if(x == 0 || OutlineKind(pCollision, y * Width + x - 1) != Kind)
					Push(X, Y, W, 32.0f);
				if(x == Width - 1 || OutlineKind(pCollision, y * Width + x + 1) != Kind)
					Push(X + 32.0f - W, Y, W, 32.0f);
				if(y == 0 || OutlineKind(pCollision, (y - 1) * Width + x) != Kind)
					Push(X, Y, 32.0f, W);
				if(y == Height - 1 || OutlineKind(pCollision, (y + 1) * Width + x) != Kind)
					Push(X, Y + 32.0f - W, 32.0f, W);
			}
		}
		if(NumQuads > 0)
			Graphics()->QuadsDrawTL(aQuads, NumQuads);
	}
	Graphics()->QuadsEnd();

	Graphics()->MapScreen(OldScreen);
}

// ---------------------------------------------------------------- music island

void CCClient::UpdateMedia()
{
	if(!g_Config.m_CcMusicIsland)
		return;
	const float Now = Client()->GlobalTime();
	if(Now < m_NextMediaPoll)
		return;
	m_NextMediaPoll = Now + 0.5f;
	m_MediaInfo = m_Media.Get();

	if(m_MediaInfo.m_Version != m_CoverVersion)
	{
		m_CoverVersion = m_MediaInfo.m_Version;
		if(m_CoverTexture.IsValid())
			Graphics()->UnloadTexture(&m_CoverTexture);
		if(!m_MediaInfo.m_vCover.empty() && m_MediaInfo.m_CoverWidth > 0 &&
			m_MediaInfo.m_vCover.size() >= (size_t)m_MediaInfo.m_CoverWidth * m_MediaInfo.m_CoverHeight * 4)
		{
			CImageInfo Image;
			Image.m_Width = m_MediaInfo.m_CoverWidth;
			Image.m_Height = m_MediaInfo.m_CoverHeight;
			Image.m_Format = CImageInfo::FORMAT_RGBA;
			Image.m_pData = m_MediaInfo.m_vCover.data();
			m_CoverTexture = Graphics()->LoadTextureRaw(Image, 0, "cclient_cover");
		}
	}
}

void CCClient::RenderMusicIsland(float ScreenWidth)
{
	if(!g_Config.m_CcMusicIsland)
		return;

	const float Delta = std::clamp(Client()->RenderFrameTime(), 0.0f, 0.1f);
	const float Time = Client()->GlobalTime();
	const float Scale = g_Config.m_CcMusicIslandSize / 100.0f;
	const bool Music = m_MediaInfo.m_Valid;

	// map timer
	char aTime[32] = "";
	bool TimeWarning = false;
	const bool HasTime = g_Config.m_ClShowhudTimer && GameClient()->m_Hud.GameTimerText(aTime, sizeof(aTime), &TimeWarning);

	// 0 = compact (icon + time), 1 = music; blends smoothly between the two
	m_IslandVisible += ((Music ? 1.0f : 0.0f) - m_IslandVisible) * std::min(1.0f, Delta * 6.0f);
	const float MusicBlend = m_IslandVisible;

	const float Height = 16.0f * Scale;
	const float Pad = 2.4f * Scale;
	const float IconSize = Height - Pad * 2.0f;
	const float TimeSize = 6.5f * Scale;
	const float TitleSize = 5.2f * Scale;
	const float ArtistSize = 4.3f * Scale;
	const float TimeWidth = HasTime ? TextRender()->TextWidth(TimeSize, aTime) : 0.0f;
	const bool ShowText = g_Config.m_CcMusicIslandText;
	const bool ShowCover = g_Config.m_CcMusicIslandCover;

	// width of both layouts
	const float CompactWidth = Pad * 2.0f + IconSize + (HasTime ? Pad * 1.6f + TimeWidth : 0.0f) + Pad;
	float MusicTextWidth = 0.0f;
	if(Music && ShowText)
		MusicTextWidth = std::min(95.0f * Scale, std::max(TextRender()->TextWidth(TitleSize, m_MediaInfo.m_Title.c_str()), TextRender()->TextWidth(ArtistSize, m_MediaInfo.m_Artist.c_str())));
	const float BarsWidth = 8.0f * Scale;
	const float MusicWidth = Pad * 2.0f + IconSize + Pad * 1.6f + (ShowText ? MusicTextWidth + Pad * 1.6f : 0.0f) + BarsWidth + (HasTime ? Pad * 2.0f + TimeWidth : 0.0f) + Pad;
	const float TargetWidth = CompactWidth + (MusicWidth - CompactWidth) * MusicBlend;
	if(m_IslandWidth <= 0.0f)
		m_IslandWidth = TargetWidth;
	m_IslandWidth += (TargetWidth - m_IslandWidth) * std::min(1.0f, Delta * 9.0f);

	const float Width = m_IslandWidth;
	const float X = ScreenWidth / 2.0f - Width / 2.0f;
	const float Y = (float)g_Config.m_CcMusicIslandY;

	// pill with a thin neon edge
	CUIRect Pill;
	Pill.x = X;
	Pill.y = Y;
	Pill.w = Width;
	Pill.h = Height;
	CUIRect Edge = Pill;
	Edge.x -= 0.35f;
	Edge.y -= 0.35f;
	Edge.w += 0.7f;
	Edge.h += 0.7f;
	Edge.Draw(ColorRGBA(0.55f, 0.3f, 1.0f, 0.35f), IGraphics::CORNER_ALL, Edge.h / 2.0f);
	Pill.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.88f), IGraphics::CORNER_ALL, Height / 2.0f);

	float CursorX = X + Pad * 1.4f;
	const float CenterY = Y + Height / 2.0f;

	// left picture: cover when music plays, client icon otherwise
	const bool UseCover = Music && ShowCover && m_CoverTexture.IsValid() && MusicBlend > 0.5f;
	Graphics()->TextureSet(UseCover ? m_CoverTexture : m_IconTexture);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	IGraphics::CQuadItem Picture(CursorX, CenterY - IconSize / 2.0f, IconSize, IconSize);
	Graphics()->QuadsDrawTL(&Picture, 1);
	Graphics()->QuadsEnd();
	CursorX += IconSize + Pad * 1.6f;

	// time on the right side
	if(HasTime)
	{
		const float TimeX = X + Width - Pad * 1.6f - TimeWidth;
		TextRender()->TextColor(TimeWarning ? ColorRGBA(1.0f, 0.35f, 0.4f, 1.0f) : ColorRGBA(0.92f, 0.95f, 1.0f, 1.0f));
		TextRender()->Text(TimeX, CenterY - TimeSize * 0.62f, TimeSize, aTime);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	// music: title, artist and a small equalizer, fading in while the island grows
	const float ContentAlpha = std::clamp((MusicBlend - 0.35f) / 0.65f, 0.0f, 1.0f);
	if(Music && ContentAlpha > 0.01f)
	{
		if(ShowText)
		{
			CTextCursor Cursor;
			Cursor.SetPosition(vec2(CursorX, CenterY - TitleSize - 0.2f * Scale));
			Cursor.m_FontSize = TitleSize;
			Cursor.m_LineWidth = MusicTextWidth;
			Cursor.m_MaxLines = 1;
			Cursor.m_Flags = TEXTFLAG_RENDER | TEXTFLAG_ELLIPSIS_AT_END;
			TextRender()->TextColor(1.0f, 1.0f, 1.0f, ContentAlpha);
			TextRender()->TextEx(&Cursor, m_MediaInfo.m_Title.c_str());

			CTextCursor ArtistCursor;
			ArtistCursor.SetPosition(vec2(CursorX, CenterY + 0.4f * Scale));
			ArtistCursor.m_FontSize = ArtistSize;
			ArtistCursor.m_LineWidth = MusicTextWidth;
			ArtistCursor.m_MaxLines = 1;
			ArtistCursor.m_Flags = TEXTFLAG_RENDER | TEXTFLAG_ELLIPSIS_AT_END;
			TextRender()->TextColor(0.7f, 0.75f, 0.9f, ContentAlpha);
			TextRender()->TextEx(&ArtistCursor, m_MediaInfo.m_Artist.c_str());
			TextRender()->TextColor(TextRender()->DefaultTextColor());
			CursorX += MusicTextWidth + Pad * 1.6f;
		}

		const float BarW = 1.1f * Scale;
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		for(int i = 0; i < 4; i++)
		{
			const float Level = m_MediaInfo.m_Playing ? 0.3f + 0.7f * std::fabs(std::sin(Time * (4.5f + i * 1.7f) + i * 1.3f)) : 0.18f;
			const float BarH = (Height - Pad * 3.2f) * Level;
			Graphics()->SetColor(i % 2 == 0 ? ColorRGBA(0.0f, 0.95f, 1.0f, ContentAlpha) : ColorRGBA(1.0f, 0.25f, 0.85f, ContentAlpha));
			IGraphics::CQuadItem Bar(CursorX + i * BarW * 1.9f, CenterY - BarH / 2.0f, BarW, BarH);
			Graphics()->QuadsDrawTL(&Bar, 1);
		}
		Graphics()->QuadsEnd();
	}
}

// ---------------------------------------------------------------- skin builder

ColorRGBA CCClient::SkinBuilderColor(int Index)
{
	static const ColorRGBA s_aColors[SB_COLORS] = {
		ColorRGBA(0.0f, 0.94f, 1.0f), // cyan
		ColorRGBA(0.6f, 0.32f, 1.0f), // violet
		ColorRGBA(1.0f, 0.2f, 0.82f), // magenta
		ColorRGBA(1.0f, 0.32f, 0.32f), // red
		ColorRGBA(1.0f, 0.62f, 0.16f), // orange
		ColorRGBA(1.0f, 0.9f, 0.28f), // yellow
		ColorRGBA(0.6f, 1.0f, 0.24f), // lime
		ColorRGBA(0.16f, 0.86f, 0.48f), // green
		ColorRGBA(0.24f, 0.48f, 1.0f), // blue
		ColorRGBA(1.0f, 0.6f, 0.8f), // pink
		ColorRGBA(0.95f, 0.95f, 0.98f), // white
		ColorRGBA(0.3f, 0.28f, 0.36f), // graphite
	};
	return s_aColors[std::clamp(Index, 0, SB_COLORS - 1)];
}

void CCClient::SkinBuilderLoadParts()
{
	if(m_SbPartsLoaded)
		return;
	m_SbPartsLoaded = true;
	m_SbPartsOk = true;
	char aPath[128];
	auto Load = [&](CImageInfo &Image, const char *pKind, int Index, size_t Width, size_t Height) {
		str_format(aPath, sizeof(aPath), "cclient/skinparts/%s_%d.png", pKind, Index);
		if(!Graphics()->LoadPng(Image, aPath, IStorage::TYPE_ALL) || Image.m_Format != CImageInfo::FORMAT_RGBA || Image.m_Width != Width || Image.m_Height != Height)
		{
			log_error("cclient", "skin builder part missing or wrong: %s", aPath);
			m_SbPartsOk = false;
		}
	};
	for(int i = 0; i < SB_PARTS; i++)
	{
		Load(m_aSbBody[i], "body", i, 96, 96);
		Load(m_aSbEars[i], "ears", i, 96, 96);
		Load(m_aSbFeet[i], "feet", i, 64, 32);
		Load(m_aSbEyes[i], "eyes", i, 192, 32);
	}
}

bool CCClient::SkinBuilderCompose()
{
	SkinBuilderLoadParts();
	if(!m_SbPartsOk)
		return false;

	constexpr int W = 256, H = 128;
	m_vSbComposed.assign((size_t)W * H * 4, 0);
	uint8_t *pOut = m_vSbComposed.data();

	const ColorRGBA BodyColor = SkinBuilderColor(g_Config.m_CcSbBodyColor);
	const ColorRGBA FeetColor = SkinBuilderColor(g_Config.m_CcSbFeetColor);

	// copy a part into the skin, gray pixels are tinted with the colour, coloured pixels stay as they are
	auto Blit = [&](const CImageInfo &Src, int SrcX, int SrcY, int SrcW, int SrcH, int DstX, int DstY, const ColorRGBA *pTint) {
		for(int y = 0; y < SrcH; y++)
		{
			for(int x = 0; x < SrcW; x++)
			{
				const uint8_t *pS = Src.m_pData + ((size_t)(SrcY + y) * Src.m_Width + (SrcX + x)) * 4;
				const float Alpha = pS[3] / 255.0f;
				if(Alpha <= 0.0f)
					continue;
				float R = pS[0] / 255.0f, G = pS[1] / 255.0f, B = pS[2] / 255.0f;
				const float Sat = std::max({R, G, B}) - std::min({R, G, B});
				if(pTint && Sat < 0.06f)
				{
					R *= pTint->r;
					G *= pTint->g;
					B *= pTint->b;
				}
				uint8_t *pD = pOut + ((size_t)(DstY + y) * W + (DstX + x)) * 4;
				const float DstA = pD[3] / 255.0f;
				const float OutA = Alpha + DstA * (1.0f - Alpha);
				auto Mix = [&](float Src, uint8_t Dst) { return (uint8_t)std::clamp((Src * Alpha + Dst / 255.0f * DstA * (1.0f - Alpha)) / std::max(OutA, 0.0001f) * 255.0f, 0.0f, 255.0f); };
				pD[0] = Mix(R, pD[0]);
				pD[1] = Mix(G, pD[1]);
				pD[2] = Mix(B, pD[2]);
				pD[3] = (uint8_t)std::clamp(OutA * 255.0f, 0.0f, 255.0f);
			}
		}
	};
	// black outline: the silhouette of a cell, grown by a few pixels
	auto Outline = [&](int SrcX, int SrcY, int CellW, int CellH, int DstX, int DstY, int Grow) {
		for(int y = 0; y < CellH; y++)
		{
			for(int x = 0; x < CellW; x++)
			{
				int MaxA = 0;
				for(int dy = -Grow; dy <= Grow && MaxA < 255; dy++)
				{
					for(int dx = -Grow; dx <= Grow; dx++)
					{
						if(dx * dx + dy * dy > Grow * Grow)
							continue;
						const int sx = x + dx, sy = y + dy;
						if(sx < 0 || sy < 0 || sx >= CellW || sy >= CellH)
							continue;
						MaxA = std::max<int>(MaxA, pOut[((size_t)(SrcY + sy) * W + (SrcX + sx)) * 4 + 3]);
					}
				}
				uint8_t *pD = pOut + ((size_t)(DstY + y) * W + (DstX + x)) * 4;
				pD[0] = pD[1] = pD[2] = 0;
				pD[3] = (uint8_t)MaxA;
			}
		}
	};

	const int Body = std::clamp(g_Config.m_CcSbBody, 0, SB_PARTS - 1);
	const int Ears = std::clamp(g_Config.m_CcSbEars, 0, SB_PARTS - 1);
	const int Feet = std::clamp(g_Config.m_CcSbFeet, 0, SB_PARTS - 1);
	const int Eyes = std::clamp(g_Config.m_CcSbEyes, 0, SB_PARTS - 1);

	// body (0,0) with ears on top, and its outline (96,0)
	Blit(m_aSbBody[Body], 0, 0, 96, 96, 0, 0, &BodyColor);
	Blit(m_aSbEars[Ears], 0, 0, 96, 96, 0, 0, &BodyColor);
	Outline(0, 0, 96, 96, 96, 0, 3);
	// hand (192,0): a small ball in the body colour, outline (224,0)
	for(int y = 0; y < 32; y++)
	{
		for(int x = 0; x < 32; x++)
		{
			const float dx = (x + 0.5f - 16.0f) / 10.0f, dy = (y + 0.5f - 16.0f) / 10.0f;
			const float d = dx * dx + dy * dy;
			if(d > 1.0f)
				continue;
			const float Light = std::clamp(0.85f + 0.2f * (1.0f - ((dx + 0.4f) * (dx + 0.4f) + (dy + 0.5f) * (dy + 0.5f))) - 0.15f * d, 0.3f, 1.0f);
			uint8_t *pD = pOut + ((size_t)y * W + (192 + x)) * 4;
			pD[0] = (uint8_t)(BodyColor.r * Light * 255.0f);
			pD[1] = (uint8_t)(BodyColor.g * Light * 255.0f);
			pD[2] = (uint8_t)(BodyColor.b * Light * 255.0f);
			pD[3] = 255;
		}
	}
	Outline(192, 0, 32, 32, 224, 0, 2);
	// foot (192,32), outline (192,64)
	Blit(m_aSbFeet[Feet], 0, 0, 64, 32, 192, 32, &FeetColor);
	Outline(192, 32, 64, 32, 192, 64, 2);
	// eyes: normal, angry, pain, happy, dead, surprise at (64..224, 96)
	for(int Emote = 0; Emote < 6; Emote++)
		Blit(m_aSbEyes[Eyes], Emote * 32, 0, 32, 32, 64 + Emote * 32, 96, nullptr);
	return true;
}

IGraphics::CTextureHandle CCClient::SkinBuilderPreview()
{
	const int Key = ((((g_Config.m_CcSbBody * 10 + g_Config.m_CcSbEars) * 10 + g_Config.m_CcSbFeet) * 10 + g_Config.m_CcSbEyes) * 12 + g_Config.m_CcSbBodyColor) * 12 + g_Config.m_CcSbFeetColor;
	if(Key != m_SbPreviewKey)
	{
		m_SbPreviewKey = Key;
		if(m_SbPreviewTexture.IsValid())
			Graphics()->UnloadTexture(&m_SbPreviewTexture);
		if(SkinBuilderCompose())
		{
			CImageInfo Image;
			Image.m_Width = 256;
			Image.m_Height = 128;
			Image.m_Format = CImageInfo::FORMAT_RGBA;
			Image.m_pData = m_vSbComposed.data();
			m_SbPreviewTexture = Graphics()->LoadTextureRaw(Image, 0, "cclient_skin_builder");
		}
	}
	return m_SbPreviewTexture;
}

bool CCClient::SkinBuilderSave()
{
	if(!SkinBuilderCompose())
		return false;

	// skin names: letters, digits, '_' and '-' only
	char aName[MAX_SKIN_LENGTH];
	int Length = 0;
	for(const char *p = g_Config.m_CcSbName; *p && Length < (int)sizeof(aName) - 1; p++)
	{
		const char c = *p;
		if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')
			aName[Length++] = c;
	}
	aName[Length] = '\0';
	if(Length == 0)
		str_copy(aName, "cr1stal_skin");

	Storage()->CreateFolder("skins", IStorage::TYPE_SAVE);
	char aPath[IO_MAX_PATH_LENGTH];
	str_format(aPath, sizeof(aPath), "skins/%s.png", aName);
	CImageInfo Image;
	Image.m_Width = 256;
	Image.m_Height = 128;
	Image.m_Format = CImageInfo::FORMAT_RGBA;
	Image.m_pData = m_vSbComposed.data();
	IOHANDLE File = Storage()->OpenFile(aPath, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!CImageLoader::SavePng(File, aPath, Image))
		return false;

	// reload the skin list so the new or changed skin is used, then wear it
	GameClient()->m_Skins.Refresh([]() {});
	char aCommand[64 + MAX_SKIN_LENGTH];
	str_format(aCommand, sizeof(aCommand), "player_skin \"%s\"; player_use_custom_color 0", aName);
	Console()->ExecuteLine(aCommand, IConsole::CLIENT_ID_UNSPECIFIED);
	return true;
}
