/* Cr1stal Client: gameplay helpers (gores mode, fast actions, finish prediction, edge info, spectating). Based on DDNet, see licence.txt. */
#include "cclient_gameplay.h"

#include "cclient_style.h"

#include <base/str.h>
#include <base/time.h>

#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <generated/protocol.h>

#include <game/client/gameclient.h>
#include <game/collision.h>
#include <game/gamecore.h>
#include <game/localization.h>
#include <game/mapitems.h>

#include <algorithm>
#include <deque>

void CCGameplay::ConFastAction(IConsole::IResult *pResult, void *pUserData)
{
	CCGameplay *pSelf = static_cast<CCGameplay *>(pUserData);
	const char *apActions[] = {g_Config.m_CcFastAction1, g_Config.m_CcFastAction2, g_Config.m_CcFastAction3, g_Config.m_CcFastAction4, g_Config.m_CcFastAction5, g_Config.m_CcFastAction6};
	const int Index = pResult->GetInteger(0) - 1;
	if(Index < 0 || Index >= (int)std::size(apActions) || !apActions[Index][0])
		return;
	pSelf->Console()->ExecuteLine(apActions[Index], IConsole::CLIENT_ID_UNSPECIFIED);
}

void CCGameplay::OnConsoleInit()
{
	Console()->Register("cc_fast_action", "i[1-6]", CFGFLAG_CLIENT, ConFastAction, this, "C-Client: run one of your six fast actions");
}

void CCGameplay::OnStateChange(int NewState, int OldState)
{
	m_HadPausedPos = false;
	m_MovedWarning = 0.0f;
	m_BestProgress = 0.0f;
}

// ---------------------------------------------------------------- finish prediction

static bool IsPassable(const CCollision *pCollision, int Index)
{
	const int Tile = pCollision->GetTileIndex(Index);
	return Tile != TILE_SOLID && Tile != TILE_NOHOOK && Tile != TILE_DEATH;
}

void CCGameplay::BuildFinishDistance()
{
	const CCollision *pCollision = Collision();
	m_MapWidth = pCollision->GetWidth();
	m_MapHeight = pCollision->GetHeight();
	m_vFinishDistance.assign((size_t)std::max(0, m_MapWidth * m_MapHeight), -1);
	m_StartDistance = 0;
	if(m_MapWidth <= 0 || m_MapHeight <= 0)
		return;

	// walk from all finish tiles outwards through everything a tee can be in
	std::deque<int> Queue;
	for(int i = 0; i < m_MapWidth * m_MapHeight; i++)
	{
		if(pCollision->GetTileIndex(i) == TILE_FINISH || pCollision->GetFrontTileIndex(i) == TILE_FINISH)
		{
			m_vFinishDistance[i] = 0;
			Queue.push_back(i);
		}
	}
	while(!Queue.empty())
	{
		const int Index = Queue.front();
		Queue.pop_front();
		const int x = Index % m_MapWidth, y = Index / m_MapWidth;
		const int aNeighbours[4][2] = {{x - 1, y}, {x + 1, y}, {x, y - 1}, {x, y + 1}};
		for(const auto &Neighbour : aNeighbours)
		{
			if(Neighbour[0] < 0 || Neighbour[1] < 0 || Neighbour[0] >= m_MapWidth || Neighbour[1] >= m_MapHeight)
				continue;
			const int Next = Neighbour[1] * m_MapWidth + Neighbour[0];
			if(m_vFinishDistance[Next] >= 0 || !IsPassable(pCollision, Next))
				continue;
			m_vFinishDistance[Next] = m_vFinishDistance[Index] + 1;
			Queue.push_back(Next);
		}
	}
	for(int i = 0; i < m_MapWidth * m_MapHeight; i++)
		if((pCollision->GetTileIndex(i) == TILE_START || pCollision->GetFrontTileIndex(i) == TILE_START) && m_vFinishDistance[i] > m_StartDistance)
			m_StartDistance = m_vFinishDistance[i];
}

void CCGameplay::OnMapLoad()
{
	m_vFinishDistance.clear();
	m_BestProgress = 0.0f;
	if(g_Config.m_CcFinishPrediction)
		BuildFinishDistance();
}

int CCGameplay::TileDistance(vec2 Pos) const
{
	const int x = (int)(Pos.x / 32.0f), y = (int)(Pos.y / 32.0f);
	if(x < 0 || y < 0 || x >= m_MapWidth || y >= m_MapHeight || m_vFinishDistance.empty())
		return -1;
	// the tee may stand right in a wall corner, look around a little
	int Best = m_vFinishDistance[y * m_MapWidth + x];
	for(int dy = -1; dy <= 1 && Best < 0; dy++)
		for(int dx = -1; dx <= 1; dx++)
		{
			const int nx = x + dx, ny = y + dy;
			if(nx < 0 || ny < 0 || nx >= m_MapWidth || ny >= m_MapHeight)
				continue;
			const int d = m_vFinishDistance[ny * m_MapWidth + nx];
			if(d >= 0 && (Best < 0 || d < Best))
				Best = d;
		}
	return Best;
}

void CCGameplay::RenderFinishPrediction(float Width, float Height)
{
	if(!g_Config.m_CcFinishPrediction)
		return;
	if(m_vFinishDistance.empty())
		BuildFinishDistance();
	const int LocalId = GameClient()->m_Snap.m_LocalClientId;
	if(LocalId < 0 || !GameClient()->m_Snap.m_aCharacters[LocalId].m_Active || m_StartDistance <= 0)
		return;
	const int Dist = TileDistance(GameClient()->m_LocalCharacterPos);
	if(Dist < 0)
		return;
	const float Progress = std::clamp(1.0f - Dist / (float)m_StartDistance, 0.0f, 1.0f);
	m_BestProgress = std::max(m_BestProgress, Progress);

	// time: the race time so far, divided by how far you are
	char aTime[32] = "";
	const CNetObj_GameInfo *pInfo = GameClient()->m_Snap.m_pGameInfoObj;
	if(pInfo && (pInfo->m_GameStateFlags & GAMESTATEFLAG_RACETIME) && Progress > 0.03f)
	{
		const float RaceTime = (Client()->GameTick(g_Config.m_ClDummy) + pInfo->m_WarmupTimer) / (float)Client()->GameTickSpeed();
		if(RaceTime > 0.0f)
		{
			const float Total = RaceTime / Progress;
			str_time((int64_t)(Total * 100.0f), ETimeFormat::HOURS_CENTISECS, aTime, sizeof(aTime));
		}
	}
	char aBuf[64];
	if(aTime[0])
		str_format(aBuf, sizeof(aBuf), "%d%%  ~%s", (int)(Progress * 100.0f), aTime);
	else
		str_format(aBuf, sizeof(aBuf), "%d%%", (int)(Progress * 100.0f));

	const float BarW = 70.0f, BarH = 3.0f;
	const float X = (Width - BarW) / 2.0f, Y = 22.0f;
	Graphics()->DrawRect(X, Y, BarW, BarH, ColorRGBA(0.0f, 0.0f, 0.0f, 0.4f), IGraphics::CORNER_ALL, BarH / 2.0f);
	Graphics()->DrawRect(X, Y, BarW * Progress, BarH, CCStyle::ACCENT.WithAlpha(0.9f), IGraphics::CORNER_ALL, BarH / 2.0f);
	const float FontSize = 5.5f;
	const float TextW = TextRender()->TextWidth(FontSize, aBuf);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.85f);
	TextRender()->Text((Width - TextW) / 2.0f, Y + BarH + 1.5f, FontSize, aBuf, -1.0f);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

// ---------------------------------------------------------------- edge info

void CCGameplay::RenderEdgeInfo(float Width, float Height)
{
	if(!g_Config.m_CcEdgeInfo)
		return;
	const int LocalId = GameClient()->m_Snap.m_LocalClientId;
	if(LocalId < 0 || !GameClient()->m_Snap.m_aCharacters[LocalId].m_Active)
		return;
	const CCollision *pCollision = Collision();
	const vec2 Pos = GameClient()->m_LocalCharacterPos;
	const int MapW = pCollision->GetWidth(), MapH = pCollision->GetHeight();
	const int TileX = (int)(Pos.x / 32.0f), TileY = (int)(Pos.y / 32.0f);
	auto IsFreeze = [&](int x, int y) {
		const int Index = y * MapW + x;
		const int Game = pCollision->GetTileIndex(Index), Front = pCollision->GetFrontTileIndex(Index);
		return Game == TILE_FREEZE || Game == TILE_DFREEZE || Front == TILE_FREEZE || Front == TILE_DFREEZE || Game == TILE_DEATH || Front == TILE_DEATH;
	};
	// walk in one direction until freeze (a number), a wall (safe) or too far
	auto Scan = [&](int dx, int dy, int Max) {
		for(int i = 1; i <= Max; i++)
		{
			const int x = TileX + dx * i, y = TileY + dy * i;
			if(x < 0 || y < 0 || x >= MapW || y >= MapH)
				return -1;
			if(IsFreeze(x, y))
				return i;
			if(pCollision->GetTileIndex(y * MapW + x) == TILE_SOLID || pCollision->GetTileIndex(y * MapW + x) == TILE_NOHOOK)
				return -1;
		}
		return -1;
	};
	const int aDist[3] = {Scan(-1, 0, 25), Scan(0, 1, 40), Scan(1, 0, 25)};
	const char *apArrows[3] = {"<", "v", ">"};
	const float BoxW = 18.0f, BoxH = 11.0f, Gap = 2.0f;
	const float X0 = (Width - (BoxW * 3.0f + Gap * 2.0f)) / 2.0f;
	const float Y0 = Height * 0.62f;
	for(int i = 0; i < 3; i++)
	{
		const float X = X0 + i * (BoxW + Gap);
		ColorRGBA Color(0.2f, 0.75f, 0.4f, 0.55f); // safe
		char aBuf[16];
		if(aDist[i] < 0)
			str_format(aBuf, sizeof(aBuf), "%s -", apArrows[i]);
		else
		{
			str_format(aBuf, sizeof(aBuf), "%s %d", apArrows[i], aDist[i]);
			const float Danger = std::clamp(1.0f - (aDist[i] - 1) / 6.0f, 0.0f, 1.0f);
			Color = CCStyle::Mix(ColorRGBA(0.85f, 0.7f, 0.2f, 0.55f), ColorRGBA(0.45f, 0.75f, 1.0f, 0.7f), Danger);
		}
		Graphics()->DrawRect(X, Y0, BoxW, BoxH, Color, IGraphics::CORNER_ALL, 3.0f);
		const float FontSize = 5.5f;
		const float TextW = TextRender()->TextWidth(FontSize, aBuf);
		TextRender()->Text(X + (BoxW - TextW) / 2.0f, Y0 + (BoxH - FontSize) / 2.0f - 0.3f, FontSize, aBuf, -1.0f);
	}
}

// ---------------------------------------------------------------- moved while paused

void CCGameplay::RenderMovedWarning(float Width, float Height)
{
	if(m_MovedWarning <= 0.0f)
		return;
	const float Alpha = std::min(1.0f, m_MovedWarning / 0.4f);
	const char *pText = Localize("Your tee was moved in the game!");
	const float FontSize = 9.0f;
	const float TextW = TextRender()->TextWidth(FontSize, pText);
	const float W = TextW + 20.0f, H = FontSize + 9.0f;
	Graphics()->DrawRect((Width - W) / 2.0f, 50.0f, W, H, CCStyle::ACCENT.WithAlpha(0.8f * Alpha), IGraphics::CORNER_ALL, H / 2.0f);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, Alpha);
	TextRender()->Text((Width - TextW) / 2.0f, 50.0f + 4.0f, FontSize, pText, -1.0f);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

// ---------------------------------------------------------------- update, input, render

void CCGameplay::OnUpdate()
{
	// process priority follows the setting
	if((bool)g_Config.m_CcHighPriority != m_PriorityApplied)
	{
		CcSetHighProcessPriority(g_Config.m_CcHighPriority);
		m_PriorityApplied = g_Config.m_CcHighPriority;
	}

	if(Client()->State() != IClient::STATE_ONLINE)
		return;

	// gores mode: when the hammer is out, go back to the gun
	m_GoresHeavyWeapon = false;
	if(g_Config.m_CcGoresMode && GameClient()->m_Snap.m_pLocalCharacter)
	{
		const int Weapon = GameClient()->m_Snap.m_pLocalCharacter->m_Weapon;
		m_GoresHeavyWeapon = Weapon == WEAPON_GRENADE || Weapon == WEAPON_LASER || Weapon == WEAPON_SHOTGUN;
		if(!(g_Config.m_CcGoresModeWeapons && m_GoresHeavyWeapon) && Weapon == WEAPON_HAMMER && !(GameClient()->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Fire & 1))
			GameClient()->m_Controls.m_aInputData[g_Config.m_ClDummy].m_WantedWeapon = WEAPON_GUN + 1;
	}

	// someone moved your tee while you are paused or spectating
	const int LocalId = GameClient()->m_Snap.m_LocalClientId;
	m_MovedWarning = std::max(0.0f, m_MovedWarning - Client()->RenderFrameTime());
	if(g_Config.m_CcSpecMovedNotify && LocalId >= 0)
	{
		const auto &Data = GameClient()->m_aClients[LocalId];
		const bool Paused = Data.m_Paused || Data.m_Spec;
		if(Paused && GameClient()->m_Snap.m_aCharacters[LocalId].m_Active)
		{
			const vec2 Pos = vec2(GameClient()->m_Snap.m_aCharacters[LocalId].m_Cur.m_X, GameClient()->m_Snap.m_aCharacters[LocalId].m_Cur.m_Y);
			if(m_HadPausedPos && distance(Pos, m_LastPausedPos) > 6.0f)
			{
				if(m_MovedWarning <= 0.0f)
					GameClient()->m_Sounds.Play(CSounds::CHN_GUI, SOUND_HOOK_ATTACH_PLAYER, 1.0f);
				m_MovedWarning = 3.0f;
			}
			m_LastPausedPos = Pos;
			m_HadPausedPos = true;
		}
		else
			m_HadPausedPos = false;
	}
}

void CCGameplay::OnFireInput(bool Pressed)
{
	if(!g_Config.m_CcGoresMode || (g_Config.m_CcGoresModeWeapons && m_GoresHeavyWeapon))
		return;
	// press: switch to the hammer (previous weapon), the update brings the gun back
	CNetObj_PlayerInput &Input = GameClient()->m_Controls.m_aInputData[g_Config.m_ClDummy];
	if((Input.m_PrevWeapon & 1) != (int)Pressed)
		Input.m_PrevWeapon++;
	Input.m_PrevWeapon &= INPUT_STATE_MASK;
	Input.m_WantedWeapon = 0;
}

bool CCGameplay::OnInput(const IInput::CEvent &Event)
{
	// better spectate: in the free view click the tee in the middle to follow it, right click to look freely
	if(!g_Config.m_CcBetterSpectate || !(Event.m_Flags & IInput::FLAG_PRESS) || Client()->State() != IClient::STATE_ONLINE)
		return false;
	if(!GameClient()->m_Snap.m_SpecInfo.m_Active || GameClient()->m_Menus.IsActive() || GameClient()->m_Chat.IsActive() || GameClient()->m_Spectator.IsActive() || GameClient()->m_Scoreboard.IsActive())
		return false;
	if(Event.m_Key == KEY_MOUSE_2 && GameClient()->m_Snap.m_SpecInfo.m_SpectatorId != SPEC_FREEVIEW)
	{
		GameClient()->m_Spectator.Spectate(SPEC_FREEVIEW);
		return true;
	}
	if(Event.m_Key == KEY_MOUSE_1 && GameClient()->m_Snap.m_SpecInfo.m_SpectatorId == SPEC_FREEVIEW)
	{
		const vec2 Center = GameClient()->m_Camera.m_Center;
		int Best = -1;
		float BestDist = 160.0f;
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(!GameClient()->m_Snap.m_aCharacters[i].m_Active || i == GameClient()->m_Snap.m_LocalClientId)
				continue;
			const float Dist = distance(GameClient()->m_aClients[i].m_RenderPos, Center);
			if(Dist < BestDist)
			{
				BestDist = Dist;
				Best = i;
			}
		}
		if(Best >= 0)
		{
			GameClient()->m_Spectator.Spectate(Best);
			return true;
		}
	}
	return false;
}

void CCGameplay::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	if(GameClient()->m_Menus.IsActive() && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	Graphics()->MapScreenToSize(Width, Height);
	RenderFinishPrediction(Width, Height);
	RenderEdgeInfo(Width, Height);
	RenderMovedWarning(Width, Height);

	// show where "better spectate" would click
	if(g_Config.m_CcBetterSpectate && GameClient()->m_Snap.m_SpecInfo.m_Active && GameClient()->m_Snap.m_SpecInfo.m_SpectatorId == SPEC_FREEVIEW && !GameClient()->m_Spectator.IsActive())
	{
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(CCStyle::ACCENT_TEXT.WithAlpha(0.6f));
		const float c = Width / 2.0f, m = Height / 2.0f;
		const IGraphics::CQuadItem aCross[] = {IGraphics::CQuadItem(c - 4.0f, m - 0.4f, 8.0f, 0.8f), IGraphics::CQuadItem(c - 0.4f, m - 4.0f, 0.8f, 8.0f)};
		Graphics()->QuadsDrawTL(aCross, 2);
		Graphics()->QuadsEnd();
	}
}
