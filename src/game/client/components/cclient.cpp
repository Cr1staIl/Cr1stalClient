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
	Graphics()->SetColor(0.05f, 0.045f, 0.055f, 0.82f);
	Graphics()->DrawCircle(ScreenCenter.x, ScreenCenter.y, 200.0f, 64);
	Graphics()->SetColor(0.86f, 0.07f, 0.23f, 0.22f);
	Graphics()->DrawCircle(ScreenCenter.x, ScreenCenter.y, 60.0f, 48);
	Graphics()->QuadsEnd();

	for(int Save = 0; Save < NUM_SAVES; Save++)
	{
		float Angle = 2 * pi * Save / (float)NUM_SAVES;
		if(Angle > pi)
			Angle -= 2 * pi;
		const vec2 Pos = ScreenCenter + direction(Angle) * 140.0f;
		const bool Selected = m_SelectedSave == Save;
		// the chosen picture grows smoothly, the old one shrinks back
		m_aWheelAnim[Save] += ((Selected ? 1.0f : 0.0f) - m_aWheelAnim[Save]) * std::min(1.0f, Client()->RenderFrameTime() * 14.0f);
		const float Anim = m_aWheelAnim[Save];
		const float Size = 62.0f + 24.0f * Anim;

		if(Anim > 0.01f)
		{
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(0.86f, 0.07f, 0.23f, 0.3f * Anim);
			Graphics()->DrawCircle(Pos.x, Pos.y, Size * 0.62f, 32);
			Graphics()->QuadsEnd();
		}

		Graphics()->TextureSet(m_aSaveTextures[Save]);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.8f + 0.2f * Anim);
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
	Props.SetColor(ColorRGBA(1.0f, 0.88f, 0.9f, 1.0f));
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

void CCClient::UpdateOptimize()
{
	// fps over the last second
	m_FpsFrames++;
	m_FpsTime += Client()->RenderFrameTime();
	if(m_FpsTime >= 1.0f)
	{
		m_Fps = round_to_int(m_FpsFrames / m_FpsTime);
		m_FpsFrames = 0;
		m_FpsTime = 0.0f;
	}

	if(m_LastOptimize == -1)
	{
		m_LastOptimize = g_Config.m_CcOptimize;
		m_OptimizeStrong = str_find(g_Config.m_CcOptimizeBackup, "cl_showquads") != nullptr;
		return;
	}
	if(m_LastOptimize == g_Config.m_CcOptimize)
		return;
	m_LastOptimize = g_Config.m_CcOptimize;

	if(!g_Config.m_CcOptimize)
	{
		// put back exactly what was there before
		if(g_Config.m_CcOptimizeBackup[0] != '\0')
			Console()->ExecuteLine(g_Config.m_CcOptimizeBackup, IConsole::CLIENT_ID_UNSPECIFIED);
		g_Config.m_CcOptimizeBackup[0] = '\0';
		m_OptimizeStrong = false;
		return;
	}

	// remember the old value of every setting that gets changed, then change it
	char aBackup[sizeof(g_Config.m_CcOptimizeBackup)] = "";
	auto Set = [&](const char *pName, int *pValue, int NewValue) {
		if(*pValue == NewValue)
			return;
		char aPart[64];
		str_format(aPart, sizeof(aPart), "%s%s %d", aBackup[0] ? "; " : "", pName, *pValue);
		str_append(aBackup, aPart, sizeof(aBackup));
		*pValue = NewValue;
	};
	Set("gfx_high_detail", &g_Config.m_GfxHighDetail, 0);
	Set("gfx_backgroundrender", &g_Config.m_GfxBackgroundRender, 0);
	Set("cl_refresh_rate_inactive", &g_Config.m_ClRefreshRateInactive, 30);
	Set("cl_unpredicted_shadow", &g_Config.m_ClUnpredictedShadow, 0);
	Set("cc_trail", &g_Config.m_CcTrail, 0);
	Set("cc_aura", &g_Config.m_CcAura, 0);
	// a weaker PC (under 144 fps right now) also hides the decoration quads of the map
	m_OptimizeStrong = m_Fps > 0 && m_Fps < 144;
	if(m_OptimizeStrong)
		Set("cl_showquads", &g_Config.m_ClShowQuads, 0);
	str_copy(g_Config.m_CcOptimizeBackup, aBackup);
}

void CCClient::OnUpdate()
{
	ApplyFont();
	UpdateMedia();
	UpdateOptimize();

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
	const float TitleSize = 5.6f * Scale;
	const float TimeWidth = HasTime ? TextRender()->TextWidth(TimeSize, aTime) : 0.0f;
	const bool ShowText = g_Config.m_CcMusicIslandText;
	const bool ShowCover = g_Config.m_CcMusicIslandCover;

	// a new song starts its title from the beginning
	if(Music && m_IslandTitle != m_MediaInfo.m_Title)
	{
		m_IslandTitle = m_MediaInfo.m_Title;
		m_IslandTitleStart = Time;
	}

	// the title gets a box of limited width, a longer one slides from side to side inside it
	const float TitleWidth = Music && ShowText ? TextRender()->TextWidth(TitleSize, m_MediaInfo.m_Title.c_str()) : 0.0f;
	const float TitleBoxWidth = std::min(TitleWidth, 62.0f * Scale);

	const float CompactWidth = Pad * 2.0f + IconSize + (HasTime ? Pad * 1.6f + TimeWidth : 0.0f) + Pad;
	const float MusicWidth = Pad * 2.0f + IconSize + (ShowText && TitleBoxWidth > 0.0f ? Pad * 1.8f + TitleBoxWidth : 0.0f) + (HasTime ? Pad * 2.2f + TimeWidth : 0.0f) + Pad;
	const float TargetWidth = CompactWidth + (MusicWidth - CompactWidth) * MusicBlend;
	if(m_IslandWidth <= 0.0f)
		m_IslandWidth = TargetWidth;
	m_IslandWidth += (TargetWidth - m_IslandWidth) * std::min(1.0f, Delta * 9.0f);

	const float Width = m_IslandWidth;
	const float X = ScreenWidth / 2.0f - Width / 2.0f;
	const float Y = (float)g_Config.m_CcMusicIslandY;

	// black pill with a barely visible edge, like the iPhone island
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
	Edge.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.08f), IGraphics::CORNER_ALL, Edge.h / 2.0f);
	Pill.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.9f), IGraphics::CORNER_ALL, Height / 2.0f);

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
	CursorX += IconSize + Pad * 1.8f;

	// time on the right side
	if(HasTime)
	{
		const float TimeX = X + Width - Pad * 1.6f - TimeWidth;
		TextRender()->TextColor(TimeWarning ? ColorRGBA(1.0f, 0.35f, 0.4f, 1.0f) : ColorRGBA(0.92f, 0.95f, 1.0f, 1.0f));
		TextRender()->Text(TimeX, CenterY - TimeSize * 0.62f, TimeSize, aTime);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	// song title, fading in while the island grows
	const float ContentAlpha = std::clamp((MusicBlend - 0.35f) / 0.65f, 0.0f, 1.0f);
	if(Music && ShowText && TitleBoxWidth > 0.0f && ContentAlpha > 0.01f)
	{
		float Offset = 0.0f;
		const float Overflow = TitleWidth - TitleBoxWidth;
		if(Overflow > 0.5f)
		{
			// wait, slide to the end, wait, slide back
			const float Pause = 1.6f;
			const float Speed = 9.0f * Scale;
			const float MoveTime = Overflow / Speed;
			const float Cycle = 2.0f * (Pause + MoveTime);
			const float t = std::fmod(Time - m_IslandTitleStart, Cycle);
			if(t < Pause)
				Offset = 0.0f;
			else if(t < Pause + MoveTime)
				Offset = (t - Pause) * Speed;
			else if(t < 2.0f * Pause + MoveTime)
				Offset = Overflow;
			else
				Offset = Overflow - (t - 2.0f * Pause - MoveTime) * Speed;
		}

		// only draw inside the title box
		const CScreenRect Screen = Graphics()->GetScreen();
		const float ScaleX = Graphics()->ScreenWidth() / (Screen.m_BottomRight.x - Screen.m_TopLeft.x);
		const float ScaleY = Graphics()->ScreenHeight() / (Screen.m_BottomRight.y - Screen.m_TopLeft.y);
		const float BoxTop = Y + Pad * 0.5f;
		const float BoxHeight = Height - Pad;
		Graphics()->ClipEnable(
			(int)std::round((CursorX - Screen.m_TopLeft.x) * ScaleX),
			(int)std::round((BoxTop - Screen.m_TopLeft.y) * ScaleY),
			(int)std::round(TitleBoxWidth * ScaleX),
			(int)std::round(BoxHeight * ScaleY));
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, ContentAlpha);
		TextRender()->Text(CursorX - Offset, CenterY - TitleSize * 0.62f, TitleSize, m_MediaInfo.m_Title.c_str());
		TextRender()->TextColor(TextRender()->DefaultTextColor());
		Graphics()->ClipDisable();
	}
}
