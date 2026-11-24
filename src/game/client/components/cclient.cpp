/* Cr1stal Client (C-Client): save wheel and map markers. Based on DDNet, see licence.txt. */
#include "cclient.h"

#include <base/str.h>

#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <generated/protocol.h>

#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/localization.h>

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
