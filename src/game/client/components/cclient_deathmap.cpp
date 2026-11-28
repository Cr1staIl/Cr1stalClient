/* Cr1stal Client: a map of the places where you die, one per map. Based on DDNet, see licence.txt. */
#include "cclient_deathmap.h"

#include "cclient_neon.h"

#include <base/io.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/client.h>
#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/map.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/collision.h>
#include <game/localization.h>
#include <game/mapitems.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

static const float TILE = 32.0f;
static const float MAX_TILE_PX = 48.0f;
// the map is drawn on a screen of its own, this high (the width follows the window)
static const float SCREEN_H = 400.0f;

int64_t CCDeathMap::Key(int TileX, int TileY)
{
	return ((int64_t)TileY << 32) | (uint32_t)TileX;
}

void CCDeathMap::OnConsoleInit()
{
	Console()->Register("cc_deathmap", "", CFGFLAG_CLIENT, ConToggle, this, "Open or close the map with the places where you died");
	Console()->Register("cc_deathmap_clear", "", CFGFLAG_CLIENT, ConClear, this, "Forget the deaths on this map");
}

void CCDeathMap::OnInit()
{
}

void CCDeathMap::ConToggle(IConsole::IResult *pResult, void *pUserData)
{
	CCDeathMap *pSelf = static_cast<CCDeathMap *>(pUserData);
	if(pSelf->m_Open)
		pSelf->SetOpen(false);
	else if(pSelf->Client()->State() == IClient::STATE_ONLINE && !pSelf->GameClient()->m_Menus.IsActive() && !pSelf->GameClient()->m_GameConsole.IsActive())
		pSelf->SetOpen(true);
}

void CCDeathMap::ConClear(IConsole::IResult *pResult, void *pUserData)
{
	static_cast<CCDeathMap *>(pUserData)->Clear();
}

void CCDeathMap::SetOpen(bool Open)
{
	if(m_Open == Open)
		return;
	m_Open = Open;
	m_Dragging = false;
	m_ClearArmedUntil = 0;
	if(Open)
	{
		m_Mouse = vec2(SCREEN_H * Graphics()->ScreenAspect() / 2.0f, SCREEN_H / 2.0f);
		// first the whole map, the view is made when the size of the screen is known
		m_WantFit = true;
		m_WantMe = false;
		if(!m_TextureTried)
			BuildTexture();
	}
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

// ---- the picture of the map ----

namespace
{
struct SPixel
{
	uint8_t m_R, m_G, m_B, m_A;
};

constexpr SPixel Px(float R, float G, float B, float A = 1.0f)
{
	return SPixel{(uint8_t)(R * 255.0f), (uint8_t)(G * 255.0f), (uint8_t)(B * 255.0f), (uint8_t)(A * 255.0f)};
}

// the colour of a tile of the game or the front layer, false for the ones that are not worth a pixel
bool IndexColor(int Index, SPixel *pOut)
{
	switch(Index)
	{
	case TILE_SOLID: *pOut = Px(0.66f, 0.72f, 0.84f); return true;
	case TILE_NOHOOK: *pOut = Px(0.43f, 0.40f, 0.62f); return true;
	case TILE_DEATH: *pOut = Px(1.00f, 0.18f, 0.30f); return true;
	case TILE_FREEZE: *pOut = Px(0.28f, 0.74f, 0.98f, 0.85f); return true;
	case TILE_DFREEZE: *pOut = Px(0.55f, 0.42f, 1.00f, 0.90f); return true;
	case TILE_UNFREEZE:
	case TILE_DUNFREEZE: *pOut = Px(0.40f, 0.95f, 0.65f, 0.55f); return true;
	case TILE_START: *pOut = Px(0.30f, 1.00f, 0.50f); return true;
	case TILE_FINISH: *pOut = Px(1.00f, 0.84f, 0.25f); return true;
	case TILE_THROUGH:
	case TILE_THROUGH_CUT: *pOut = Px(0.80f, 0.85f, 0.95f, 0.35f); return true;
	default: return false;
	}
}
} // namespace

void CCDeathMap::FreeTexture()
{
	if(m_MapTexture.IsValid())
		Graphics()->UnloadTexture(&m_MapTexture);
	m_MapTexture = IGraphics::CTextureHandle();
	m_TextureTried = false;
	m_MapW = 0;
	m_MapH = 0;
}

void CCDeathMap::BuildTexture()
{
	m_TextureTried = true;
	const CCollision *pCollision = GameClient()->Collision();
	const int W = pCollision->GetWidth();
	const int H = pCollision->GetHeight();
	const CTile *pGame = pCollision->GameLayer();
	if(W <= 0 || H <= 0 || W > 8192 || H > 8192 || !pGame)
		return;
	const CTile *pFront = pCollision->FrontLayer();
	const CTeleTile *pTele = pCollision->TeleLayer();
	const CSpeedupTile *pSpeedup = pCollision->SpeedupLayer();

	// a few pixels for a tile when the map is small, so it stays sharp
	const int Scale = std::clamp(2048 / std::max(W, H), 1, 4);

	CImageInfo Image;
	Image.m_Width = (size_t)W * Scale;
	Image.m_Height = (size_t)H * Scale;
	Image.m_Format = CImageInfo::FORMAT_RGBA;
	Image.AllocateFillZero();

	for(int y = 0; y < H; y++)
	{
		for(int x = 0; x < W; x++)
		{
			const int i = y * W + x;
			SPixel Pixel = Px(0.0f, 0.0f, 0.0f, 0.0f);
			const int Game = pGame[i].m_Index;
			const int Front = pFront ? pFront[i].m_Index : 0;
			// walls and spikes of the game layer win over what is in front of them
			bool Found = (Game == TILE_SOLID || Game == TILE_NOHOOK || Game == TILE_DEATH) && IndexColor(Game, &Pixel);
			if(!Found)
				Found = IndexColor(Front, &Pixel);
			if(!Found)
				Found = IndexColor(Game, &Pixel);
			if(!Found && pTele && pTele[i].m_Type != 0 && pTele[i].m_Number != 0)
			{
				Pixel = Px(0.75f, 0.42f, 1.00f, 0.75f);
				Found = true;
			}
			if(!Found && pSpeedup && pSpeedup[i].m_Force > 0)
			{
				Pixel = Px(1.00f, 0.66f, 0.25f, 0.70f);
				Found = true;
			}
			if(!Found)
				continue;
			for(int dy = 0; dy < Scale; dy++)
			{
				uint8_t *pRow = Image.m_pData + (((size_t)y * Scale + dy) * Image.m_Width + (size_t)x * Scale) * 4;
				for(int dx = 0; dx < Scale; dx++)
				{
					pRow[dx * 4 + 0] = Pixel.m_R;
					pRow[dx * 4 + 1] = Pixel.m_G;
					pRow[dx * 4 + 2] = Pixel.m_B;
					pRow[dx * 4 + 3] = Pixel.m_A;
				}
			}
		}
	}
	m_MapW = W;
	m_MapH = H;
	m_MapTexture = Graphics()->LoadTextureRawMove(Image, 0, "cr1stal death map");
}

// ---- map and tee ----

void CCDeathMap::OnMapLoad()
{
	if(m_Dirty)
		Save();
	SetOpen(false);
	FreeTexture();
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
	m_Open = false;
	m_Dragging = false;
}

void CCDeathMap::OnShutdown()
{
	if(m_Dirty)
		Save();
	FreeTexture();
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
	// the key G is given to the map once, and only when it is free. Not before the first update: the config
	// with the binds of the player is read after the components start
	if(!m_KeyChecked)
	{
		m_KeyChecked = true;
		if(!g_Config.m_CcDeathMapKeySet)
		{
			g_Config.m_CcDeathMapKeySet = 1;
			GameClient()->m_Binds.Bind(KEY_G, "cc_deathmap", true);
		}
	}

	const bool Online = Client()->State() == IClient::STATE_ONLINE;
	const auto &Snap = GameClient()->m_Snap;
	const bool Playing = Online && Snap.m_pLocalInfo && Snap.m_pLocalInfo->m_Team != TEAM_SPECTATORS && !Snap.m_SpecInfo.m_Active;
	const bool Alive = Playing && Snap.m_pLocalCharacter != nullptr;

	if(m_Open && (!Online || GameClient()->m_Menus.IsActive() || GameClient()->m_GameConsole.IsActive()))
		SetOpen(false);

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

// ---- view and input ----

void CCDeathMap::FitView()
{
	if(m_MapW <= 0 || m_MapH <= 0 || m_MapArea.m_W <= 0.0f)
		return;
	m_TilePx = std::min(m_MapArea.m_W / m_MapW, m_MapArea.m_H / m_MapH) * 0.97f;
	m_TilePx = std::clamp(m_TilePx, 0.02f, MAX_TILE_PX);
	m_ViewCenter = vec2(m_MapW / 2.0f, m_MapH / 2.0f);
}

void CCDeathMap::ZoomAt(vec2 Pos, float Factor)
{
	const vec2 AreaCenter(m_MapArea.m_X + m_MapArea.m_W / 2.0f, m_MapArea.m_Y + m_MapArea.m_H / 2.0f);
	const vec2 TileUnder = m_ViewCenter + (Pos - AreaCenter) / m_TilePx;
	const float MinPx = m_MapW > 0 ? std::min(m_MapArea.m_W / m_MapW, m_MapArea.m_H / m_MapH) * 0.5f : 0.02f;
	m_TilePx = std::clamp(m_TilePx * Factor, std::max(0.02f, MinPx), MAX_TILE_PX);
	m_ViewCenter = TileUnder - (Pos - AreaCenter) / m_TilePx;
}

void CCDeathMap::PressButton(int Button)
{
	switch(Button)
	{
	case BTN_FIT:
		m_WantFit = true;
		break;
	case BTN_ME:
		m_WantMe = true;
		break;
	case BTN_RECORD:
		g_Config.m_CcDeathMapRecord = !g_Config.m_CcDeathMapRecord;
		break;
	case BTN_CLEAR:
		// two clicks, so one slip does not wipe the map
		if(m_ClearArmedUntil > time_get())
		{
			Clear();
			m_ClearArmedUntil = 0;
		}
		else
		{
			m_ClearArmedUntil = time_get() + time_freq() * 3;
		}
		break;
	case BTN_CLOSE:
		SetOpen(false);
		break;
	}
}

bool CCDeathMap::OnCursorMove(float x, float y, IInput::ECursorType CursorType)
{
	if(!m_Open)
		return false;
	Ui()->ConvertMouseMove(&x, &y, CursorType);
	// pixels of the window to units of the screen
	const float ScreenW = SCREEN_H * Graphics()->ScreenAspect();
	const vec2 Delta = vec2(x, y) * (ScreenW / (float)Graphics()->WindowWidth());
	m_Mouse.x = std::clamp(m_Mouse.x + Delta.x, 0.0f, ScreenW - 1.0f);
	m_Mouse.y = std::clamp(m_Mouse.y + Delta.y, 0.0f, SCREEN_H - 1.0f);
	if(m_Dragging && m_TilePx > 0.0f)
		m_ViewCenter -= Delta / m_TilePx;
	return true;
}

bool CCDeathMap::OnInput(const IInput::CEvent &Event)
{
	// the release of a button always goes on to the game: it may have been pressed before the map was opened
	if(Event.m_Key == KEY_MOUSE_1 && (Event.m_Flags & IInput::FLAG_RELEASE))
		m_Dragging = false;
	if(!m_Open || !(Event.m_Flags & IInput::FLAG_PRESS))
		return false;

	if(Event.m_Key == KEY_ESCAPE)
	{
		SetOpen(false);
		return true;
	}
	if(Event.m_Key == KEY_MOUSE_WHEEL_UP || Event.m_Key == KEY_MOUSE_WHEEL_DOWN)
	{
		if(m_MapArea.Hit(m_Mouse))
			ZoomAt(m_Mouse, Event.m_Key == KEY_MOUSE_WHEEL_UP ? 1.2f : 1.0f / 1.2f);
		return true;
	}
	if(Event.m_Key == KEY_MOUSE_1)
	{
		for(int i = 0; i < NUM_BTNS; i++)
		{
			if(m_aButtons[i].Hit(m_Mouse))
			{
				PressButton(i);
				return true;
			}
		}
		if(m_MapArea.Hit(m_Mouse))
			m_Dragging = true;
		return true;
	}
	// no shooting and hooking while you look at the map
	return Event.m_Key == KEY_MOUSE_2;
}

// ---- drawing ----

void CCDeathMap::OnRender()
{
	const float Target = m_Open ? 1.0f : 0.0f;
	m_Anim += (Target - m_Anim) * std::min(1.0f, Client()->RenderFrameTime() * 14.0f);
	if(std::fabs(Target - m_Anim) < 0.01f)
		m_Anim = Target;
	if(m_Anim <= 0.0f || Client()->State() != IClient::STATE_ONLINE)
		return;

	const float A = m_Anim;
	const CScreenRect PreviousScreen = Graphics()->GetScreen();
	const float H = SCREEN_H;
	const float W = H * Graphics()->ScreenAspect();
	Graphics()->MapScreenToSize(W, H);

	CUIRect Full;
	Full.x = 0.0f;
	Full.y = 0.0f;
	Full.w = W;
	Full.h = H;
	Full.Draw(ColorRGBA(0.03f, 0.02f, 0.05f, 0.90f * A), IGraphics::CORNER_NONE, 0.0f);

	// ---- the bar on top ----
	const float Margin = 8.0f;
	CUIRect Bar;
	Bar.x = Margin;
	Bar.y = Margin;
	Bar.w = W - Margin * 2.0f;
	Bar.h = 24.0f;
	CCNeon::Glow(Bar, 6.0f, CCNeon::NEON, 0.6f * A);
	CCNeon::DrawFramed(Bar, ColorRGBA(CCNeon::PANEL.r, CCNeon::PANEL.g, CCNeon::PANEL.b, 0.97f * A), ColorRGBA(1.0f, 1.0f, 1.0f, 0.10f * A), 6.0f);

	const float BtnH = 16.0f;
	const float BtnY = Bar.y + (Bar.h - BtnH) / 2.0f;
	float Right = Bar.x + Bar.w - 6.0f;
	const bool ClearArmed = m_ClearArmedUntil > time_get();
	char aRecord[48];
	str_format(aRecord, sizeof(aRecord), "%s: %s", Localize("Record"), g_Config.m_CcDeathMapRecord ? Localize("on") : Localize("off"));
	const char *apLabels[NUM_BTNS] = {
		Localize("Whole map"),
		Localize("Where I am"),
		aRecord,
		ClearArmed ? Localize("Click again to clear") : Localize("Clear this map"),
		Localize("Close"),
	};
	for(int i = NUM_BTNS - 1; i >= 0; i--)
	{
		const float TextW = TextRender()->TextWidth(8.0f, apLabels[i]);
		SButton &Btn = m_aButtons[i];
		Btn.m_W = TextW + 14.0f;
		Btn.m_H = BtnH;
		Btn.m_X = Right - Btn.m_W;
		Btn.m_Y = BtnY;
		Right = Btn.m_X - 4.0f;

		const bool Hot = Btn.Hit(m_Mouse);
		const bool Danger = i == BTN_CLEAR && ClearArmed;
		const bool Primary = i == BTN_CLOSE;
		ColorRGBA Fill = Hot ? CCNeon::HOVER : CCNeon::ELEVATED;
		ColorRGBA Edge = ColorRGBA(1.0f, 1.0f, 1.0f, 0.08f);
		if(Danger || Primary)
		{
			Fill = Hot ? CCNeon::ACCENT_HI : CCNeon::ACCENT;
			Edge = CCNeon::PRIMARY_EDGE;
		}
		Fill.a *= A;
		Edge.a *= A;
		CUIRect Rect;
		Rect.x = Btn.m_X;
		Rect.y = Btn.m_Y;
		Rect.w = Btn.m_W;
		Rect.h = Btn.m_H;
		if(Hot)
			CCNeon::Glow(Rect, 4.0f, CCNeon::NEON, 0.5f * A);
		CCNeon::DrawFramed(Rect, Fill, Edge, 4.0f);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, A);
		TextRender()->Text(Btn.m_X + 7.0f, Btn.m_Y + (BtnH - 8.0f) / 2.0f - 0.3f, 8.0f, apLabels[i]);
	}

	// the title on the left
	{
		char aTitle[160];
		str_format(aTitle, sizeof(aTitle), "%s", m_aMap[0] != '\0' ? m_aMap : "-");
		TextRender()->TextColor(CCNeon::ACCENT_TEXT.r, CCNeon::ACCENT_TEXT.g, CCNeon::ACCENT_TEXT.b, A);
		TextRender()->Text(Bar.x + 10.0f, Bar.y + 5.0f, 10.0f, Localize("Death map"));
		const float TitleW = TextRender()->TextWidth(10.0f, Localize("Death map"));
		TextRender()->TextColor(CCNeon::TEXT.r, CCNeon::TEXT.g, CCNeon::TEXT.b, A);
		TextRender()->Text(Bar.x + 10.0f + TitleW + 8.0f, Bar.y + 5.0f, 10.0f, aTitle);
		char aCounts[96];
		str_format(aCounts, sizeof(aCounts), Localize("%d deaths, %d now"), m_Total, m_Session);
		const float TitleEnd = Bar.x + 10.0f + TitleW + 8.0f + TextRender()->TextWidth(10.0f, aTitle) + 12.0f;
		if(TitleEnd + TextRender()->TextWidth(8.0f, aCounts) < Right)
		{
			TextRender()->TextColor(CCNeon::TEXT_DIM.r, CCNeon::TEXT_DIM.g, CCNeon::TEXT_DIM.b, A);
			TextRender()->Text(TitleEnd, Bar.y + 6.5f, 8.0f, aCounts);
		}
	}

	// ---- the map ----
	CUIRect Area;
	Area.x = Margin;
	Area.y = Bar.y + Bar.h + 6.0f;
	Area.w = W - Margin * 2.0f;
	Area.h = H - Area.y - Margin - 12.0f;
	m_MapArea.m_X = Area.x;
	m_MapArea.m_Y = Area.y;
	m_MapArea.m_W = Area.w;
	m_MapArea.m_H = Area.h;
	CCNeon::DrawFramed(Area, ColorRGBA(CCNeon::INPUT.r, CCNeon::INPUT.g, CCNeon::INPUT.b, 0.97f * A), ColorRGBA(1.0f, 1.0f, 1.0f, 0.08f * A), 6.0f);

	if(m_MapW <= 0 || !m_MapTexture.IsValid())
	{
		TextRender()->TextColor(CCNeon::TEXT_DIM.r, CCNeon::TEXT_DIM.g, CCNeon::TEXT_DIM.b, A);
		const char *pText = Localize("The map has no picture");
		TextRender()->Text(Area.x + (Area.w - TextRender()->TextWidth(10.0f, pText)) / 2.0f, Area.y + Area.h / 2.0f - 5.0f, 10.0f, pText);
	}
	else
	{
		if(m_WantFit)
		{
			FitView();
			m_WantFit = false;
		}
		if(m_WantMe)
		{
			m_ViewCenter = m_LastPos / TILE;
			m_TilePx = std::clamp(std::max(m_TilePx, 7.0f), 0.02f, MAX_TILE_PX);
			m_WantMe = false;
		}

		const vec2 AreaCenter(Area.x + Area.w / 2.0f, Area.y + Area.h / 2.0f);
		auto ToScreen = [&](float TileX, float TileY) {
			return AreaCenter + (vec2(TileX, TileY) - m_ViewCenter) * m_TilePx;
		};

		CUIRect Clip = Area;
		Clip.Margin(1.0f, &Clip);
		{
			const float ToPx = (float)Graphics()->ScreenHeight() / H;
			Graphics()->ClipEnable((int)(Clip.x * ToPx), (int)(Clip.y * ToPx), (int)std::ceil(Clip.w * ToPx), (int)std::ceil(Clip.h * ToPx));
		}

		// the field of the map and its walls
		const vec2 TopLeft = ToScreen(0.0f, 0.0f);
		const vec2 BottomRight = ToScreen((float)m_MapW, (float)m_MapH);
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0.10f, 0.08f, 0.14f, A);
		{
			const IGraphics::CQuadItem Quad(TopLeft.x, TopLeft.y, BottomRight.x - TopLeft.x, BottomRight.y - TopLeft.y);
			Graphics()->QuadsDrawTL(&Quad, 1);
		}
		Graphics()->QuadsEnd();

		Graphics()->TextureSet(m_MapTexture);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, A);
		{
			const IGraphics::CQuadItem Quad(TopLeft.x, TopLeft.y, BottomRight.x - TopLeft.x, BottomRight.y - TopLeft.y);
			Graphics()->QuadsDrawTL(&Quad, 1);
		}
		Graphics()->QuadsEnd();

		// the deaths: a square on the tile, but never smaller than a dot you can see
		struct SShown
		{
			vec2 m_Pos;
			int m_Count;
		};
		std::vector<SShown> vShown;
		const float DotSize = std::max(m_TilePx, 3.2f);
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		for(const auto &Cell : m_Cells)
		{
			const int TileX = (int)(int32_t)(Cell.first & 0xFFFFFFFF);
			const int TileY = (int)(Cell.first >> 32);
			const vec2 Center = ToScreen(TileX + 0.5f, TileY + 0.5f);
			if(Center.x < Clip.x - DotSize || Center.x > Clip.x + Clip.w + DotSize || Center.y < Clip.y - DotSize || Center.y > Clip.y + Clip.h + DotSize)
				continue;
			// more deaths: from amber to the red of the client
			const float T = std::sqrt((float)Cell.second / (float)m_MaxCount);
			Graphics()->SetColor(1.0f, 0.72f - 0.56f * T, 0.20f + 0.15f * T, A * (0.55f + 0.45f * T));
			const IGraphics::CQuadItem Quad(Center.x - DotSize / 2.0f, Center.y - DotSize / 2.0f, DotSize, DotSize);
			Graphics()->QuadsDrawTL(&Quad, 1);
			if(Cell.second >= 2)
				vShown.push_back({Center, Cell.second});
		}
		Graphics()->QuadsEnd();

		// the number where it happened more than once, when the tiles are big enough to hold it
		if(m_TilePx >= 8.0f && vShown.size() <= 400)
		{
			TextRender()->TextColor(1.0f, 1.0f, 1.0f, A);
			const float Size = std::min(m_TilePx * 0.55f, 9.0f);
			for(const SShown &Shown : vShown)
			{
				char aCount[16];
				str_format(aCount, sizeof(aCount), "%d", Shown.m_Count);
				TextRender()->Text(Shown.m_Pos.x - TextRender()->TextWidth(Size, aCount) / 2.0f, Shown.m_Pos.y - Size / 2.0f, Size, aCount);
			}
		}

		// you: a pulsing ring around a bright dot
		{
			const vec2 Me = ToScreen(m_LastPos.x / TILE, m_LastPos.y / TILE);
			const float Pulse = 0.5f + 0.5f * std::sin((float)(time_get() % time_freq()) / (float)time_freq() * 6.2832f);
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(CCNeon::NEON.r, CCNeon::NEON.g, CCNeon::NEON.b, A * (0.18f + 0.18f * Pulse));
			Graphics()->DrawCircle(Me.x, Me.y, 6.0f + 3.0f * Pulse, 24);
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, A);
			Graphics()->DrawCircle(Me.x, Me.y, 2.2f, 16);
			Graphics()->QuadsEnd();
		}

		Graphics()->ClipDisable();

		// what is under the cursor
		if(Area.Inside(m_Mouse) && !m_Dragging)
		{
			const vec2 Tile = m_ViewCenter + (m_Mouse - AreaCenter) / m_TilePx;
			const auto It = m_Cells.find(Key((int)std::floor(Tile.x), (int)std::floor(Tile.y)));
			if(It != m_Cells.end())
			{
				char aTip[64];
				str_format(aTip, sizeof(aTip), Localize("%d deaths here"), It->second);
				const float TipW = TextRender()->TextWidth(8.0f, aTip) + 10.0f;
				CUIRect Tip;
				Tip.w = TipW;
				Tip.h = 14.0f;
				Tip.x = std::min(m_Mouse.x + 12.0f, W - TipW - 2.0f);
				Tip.y = std::min(m_Mouse.y + 14.0f, H - 16.0f);
				CCNeon::DrawFramed(Tip, ColorRGBA(CCNeon::ELEVATED.r, CCNeon::ELEVATED.g, CCNeon::ELEVATED.b, 0.98f * A), ColorRGBA(1.0f, 1.0f, 1.0f, 0.12f * A), 4.0f);
				TextRender()->TextColor(1.0f, 1.0f, 1.0f, A);
				TextRender()->Text(Tip.x + 5.0f, Tip.y + 3.0f, 8.0f, aTip);
			}
		}
	}

	// the hint at the bottom
	{
		const char *pHint = Localize("Wheel: zoom   Left button: move the map   Key or Esc: close");
		TextRender()->TextColor(CCNeon::TEXT_DIM.r, CCNeon::TEXT_DIM.g, CCNeon::TEXT_DIM.b, A);
		TextRender()->Text(Margin + 2.0f, H - Margin - 7.5f, 7.5f, pHint);
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());

	if(m_Open)
		RenderTools()->RenderCursor(m_Mouse, 16.0f);
	Graphics()->MapScreen(PreviousScreen);
}
