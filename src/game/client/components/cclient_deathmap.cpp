/* Cr1stal Client: a map of the places where you die, one per map. Based on DDNet, see licence.txt. */
#include "cclient_deathmap.h"

#include <base/io.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/client.h>
#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/map.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>
#include <game/localization.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

static const float TILE = 32.0f;

int64_t CCDeathMap::Key(int TileX, int TileY)
{
	return ((int64_t)TileY << 32) | (uint32_t)TileX;
}

void CCDeathMap::OnConsoleInit()
{
	Console()->Register("+cc_deathmap", "", CFGFLAG_CLIENT, ConHold, this, "Show the places where you died while the key is held");
	Console()->Register("cc_deathmap_clear", "", CFGFLAG_CLIENT, ConClear, this, "Forget the deaths on this map");
}

void CCDeathMap::ConHold(IConsole::IResult *pResult, void *pUserData)
{
	static_cast<CCDeathMap *>(pUserData)->m_Hold = pResult->GetInteger(0) != 0;
}

void CCDeathMap::ConClear(IConsole::IResult *pResult, void *pUserData)
{
	static_cast<CCDeathMap *>(pUserData)->Clear();
}

bool CCDeathMap::Visible() const
{
	return (g_Config.m_CcDeathMap || m_Hold) && Client()->State() == IClient::STATE_ONLINE;
}

void CCDeathMap::FilePath(char *pBuf, int Size) const
{
	// the name of the map as a file name
	char aName[64];
	str_copy(aName, m_aMap);
	str_sanitize_filename(aName);
	str_format(pBuf, Size, "deaths/%s.txt", aName);
}

void CCDeathMap::Load()
{
	m_Cells.clear();
	m_Total = 0;
	m_MaxCount = 1;
	if(m_aMap[0] == '\0')
		return;
	char aPath[IO_MAX_PATH_LENGTH];
	FilePath(aPath, sizeof(aPath));
	char *pFile = Storage()->ReadFileStr(aPath, IStorage::TYPE_SAVE);
	if(!pFile)
		return;
	// one line for every tile: x <space> y <space> deaths
	char *pLine = pFile;
	while(pLine && *pLine)
	{
		char *pEnd = (char *)str_find(pLine, "\n");
		if(pEnd)
			*pEnd = '\0';
		char *pSecond = (char *)str_find(pLine, " ");
		char *pThird = pSecond ? (char *)str_find(pSecond + 1, " ") : nullptr;
		if(pSecond && pThird)
		{
			*pSecond = '\0';
			*pThird = '\0';
			const int Count = str_toint(pThird + 1);
			if(Count > 0)
			{
				m_Cells[Key(str_toint(pLine), str_toint(pSecond + 1))] += Count;
				m_Total += Count;
			}
		}
		pLine = pEnd ? pEnd + 1 : nullptr;
	}
	free(pFile);
	for(const auto &Cell : m_Cells)
		m_MaxCount = std::max(m_MaxCount, Cell.second);
}

void CCDeathMap::Save()
{
	m_Dirty = false;
	m_LastSave = time_get();
	if(m_aMap[0] == '\0')
		return;
	Storage()->CreateFolder("deaths", IStorage::TYPE_SAVE);
	char aPath[IO_MAX_PATH_LENGTH];
	FilePath(aPath, sizeof(aPath));
	if(m_Cells.empty())
	{
		Storage()->RemoveFile(aPath, IStorage::TYPE_SAVE);
		return;
	}
	IOHANDLE File = Storage()->OpenFile(aPath, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
		return;
	char aLine[64];
	for(const auto &Cell : m_Cells)
	{
		str_format(aLine, sizeof(aLine), "%d %d %d\n", (int)(int32_t)(Cell.first & 0xFFFFFFFF), (int)(Cell.first >> 32), Cell.second);
		io_write(File, aLine, str_length(aLine));
	}
	io_close(File);
}

void CCDeathMap::Clear()
{
	m_Cells.clear();
	m_Total = 0;
	m_Session = 0;
	m_MaxCount = 1;
	m_Dirty = true;
	Save();
}

void CCDeathMap::OnMapLoad()
{
	if(m_Dirty)
		Save();
	m_aMap[0] = '\0';
	if(GameClient()->Map() && GameClient()->Map()->BaseName()[0] != '\0')
		str_copy(m_aMap, GameClient()->Map()->BaseName());
	m_Session = 0;
	m_WasAlive = false;
	Load();
}

void CCDeathMap::OnReset()
{
	m_WasAlive = false;
	m_Hold = false;
}

void CCDeathMap::OnShutdown()
{
	if(m_Dirty)
		Save();
}

void CCDeathMap::Record(vec2 Pos)
{
	const int TileX = (int)std::floor(Pos.x / TILE);
	const int TileY = (int)std::floor(Pos.y / TILE);
	int &Count = m_Cells[Key(TileX, TileY)];
	Count++;
	m_MaxCount = std::max(m_MaxCount, Count);
	m_Total++;
	m_Session++;
	m_Dirty = true;
}

void CCDeathMap::OnUpdate()
{
	const bool Online = Client()->State() == IClient::STATE_ONLINE;
	const auto &Snap = GameClient()->m_Snap;
	const bool Playing = Online && Snap.m_pLocalInfo && Snap.m_pLocalInfo->m_Team != TEAM_SPECTATORS && !Snap.m_SpecInfo.m_Active;
	const bool Alive = Playing && Snap.m_pLocalCharacter != nullptr;

	if(Alive)
	{
		m_LastPos = vec2((float)Snap.m_pLocalCharacter->m_X, (float)Snap.m_pLocalCharacter->m_Y);
	}
	else if(m_WasAlive && Playing && g_Config.m_CcDeathMapRecord)
	{
		// the tee is gone but you are still playing: you died. A death you asked for with kill is not a place to learn
		const bool ByKill = time_get() - GameClient()->m_LastKillRequest < time_freq() * 3 / 2;
		if(!ByKill)
			Record(m_LastPos);
	}
	m_WasAlive = Alive;

	if(m_Dirty && time_get() - m_LastSave > time_freq() * 30)
		Save();
}

void CCDeathMap::OnRender()
{
	if(!Visible() || m_Cells.empty())
		return;

	const CScreenRect Previous = Graphics()->GetScreen();
	const CScreenRect World = Graphics()->MapScreenToWorld(
		GameClient()->m_Camera.m_Center.x, GameClient()->m_Camera.m_Center.y,
		100.0f, 100.0f, 100.0f, 0, 0,
		Graphics()->ScreenAspect(), GameClient()->m_Camera.m_Zoom);
	Graphics()->MapScreen(World);

	const float MinX = World.m_TopLeft.x - TILE, MaxX = World.m_BottomRight.x + TILE;
	const float MinY = World.m_TopLeft.y - TILE, MaxY = World.m_BottomRight.y + TILE;
	const float Strength = std::clamp(g_Config.m_CcDeathMapAlpha, 10, 100) / 100.0f;

	struct SVisible
	{
		float m_X, m_Y;
		int m_Count;
	};
	std::vector<SVisible> vVisible;
	for(const auto &Cell : m_Cells)
	{
		const int TileX = (int)(int32_t)(Cell.first & 0xFFFFFFFF);
		const int TileY = (int)(Cell.first >> 32);
		const float X = TileX * TILE, Y = TileY * TILE;
		if(X < MinX || X > MaxX || Y < MinY || Y > MaxY)
			continue;
		vVisible.push_back({X, Y, Cell.second});
	}

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	for(const SVisible &Cell : vVisible)
	{
		// more deaths: from amber to the red of the client
		const float T = std::sqrt((float)Cell.m_Count / (float)m_MaxCount);
		const float R = 1.0f;
		const float G = 0.72f - 0.56f * T;
		const float B = 0.20f + 0.15f * T;
		Graphics()->SetColor(R, G, B, Strength * (0.22f + 0.58f * T));
		const IGraphics::CQuadItem Quad(Cell.m_X + 1.0f, Cell.m_Y + 1.0f, TILE - 2.0f, TILE - 2.0f);
		Graphics()->QuadsDrawTL(&Quad, 1);
	}
	Graphics()->QuadsEnd();

	// the number on the tiles where it happened more than once, when there are not too many in view
	if(vVisible.size() <= 300)
	{
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, std::min(1.0f, Strength + 0.2f));
		for(const SVisible &Cell : vVisible)
		{
			if(Cell.m_Count < 2)
				continue;
			char aCount[16];
			str_format(aCount, sizeof(aCount), "%d", Cell.m_Count);
			const float Size = 14.0f;
			const float Width = TextRender()->TextWidth(Size, aCount);
			TextRender()->Text(Cell.m_X + (TILE - Width) / 2.0f, Cell.m_Y + (TILE - Size) / 2.0f, Size, aCount);
		}
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	// the title on the screen
	Ui()->MapScreen();
	{
		char aText[128];
		str_format(aText, sizeof(aText), Localize("Death map: %d here, %d now"), m_Total, m_Session);
		const float Size = 9.0f;
		const float Width = TextRender()->TextWidth(Size, aText);
		const float X = (Ui()->Screen()->w - Width) / 2.0f;
		TextRender()->TextColor(1.0f, 0.49f, 0.62f, 0.95f);
		TextRender()->Text(X, 22.0f, Size, aText);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	Graphics()->MapScreen(Previous);
}
