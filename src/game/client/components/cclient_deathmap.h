/* Cr1stal Client: a map of the places where you die, one per map. Based on DDNet, see licence.txt. */
#ifndef GAME_CLIENT_COMPONENTS_CCLIENT_DEATHMAP_H
#define GAME_CLIENT_COMPONENTS_CCLIENT_DEATHMAP_H

#include <base/vmath.h>

#include <engine/console.h>
#include <engine/graphics.h>

#include <game/client/component.h>

#include <cstdint>
#include <unordered_map>

// Remembers the tile where you died on every map (not the deaths you made with kill) and shows them on a picture
// of the whole map. The picture opens with the cc_deathmap key (G when nothing else is on it), the wheel zooms,
// the left button moves the map.
class CCDeathMap : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnConsoleInit() override;
	void OnInit() override;
	void OnMapLoad() override;
	void OnReset() override;
	void OnUpdate() override;
	void OnRender() override;
	void OnShutdown() override;
	bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;
	bool OnInput(const IInput::CEvent &Event) override;

	bool IsOpen() const { return m_Open; }
	// deaths on this map, all time and since you came
	int Total() const { return m_Total; }
	int Session() const { return m_Session; }
	void Clear();

private:
	std::unordered_map<int64_t, int> m_Cells;
	int m_Total = 0;
	int m_Session = 0;
	int m_MaxCount = 1;
	bool m_Dirty = false;
	bool m_KeyChecked = false;
	int64_t m_LastSave = 0;
	char m_aMap[64] = "";

	// what the tee did on the last update
	bool m_WasAlive = false;
	vec2 m_LastPos = vec2(0.0f, 0.0f);

	// the picture of the map, one pixel (or a few) for every tile
	IGraphics::CTextureHandle m_MapTexture;
	bool m_TextureTried = false;
	int m_MapW = 0;
	int m_MapH = 0;

	// the view
	bool m_Open = false;
	float m_Anim = 0.0f;
	vec2 m_Mouse = vec2(0.0f, 0.0f);
	vec2 m_ViewCenter = vec2(0.0f, 0.0f); // in tiles
	float m_TilePx = 1.0f; // screen units for one tile
	bool m_Dragging = false;
	int64_t m_ClearArmedUntil = 0;
	bool m_WantFit = false;
	bool m_WantMe = false;

	// the buttons of the last frame, to know where a click lands
	enum
	{
		BTN_FIT,
		BTN_ME,
		BTN_RECORD,
		BTN_CLEAR,
		BTN_CLOSE,
		NUM_BTNS,
	};
	struct SButton
	{
		float m_X = 0.0f, m_Y = 0.0f, m_W = 0.0f, m_H = 0.0f;
		bool Hit(vec2 Pos) const { return m_W > 0.0f && Pos.x >= m_X && Pos.x < m_X + m_W && Pos.y >= m_Y && Pos.y < m_Y + m_H; }
	};
	SButton m_aButtons[NUM_BTNS];
	SButton m_MapArea;

	static int64_t Key(int TileX, int TileY);
	void Record(vec2 Pos);
	void FilePath(char *pBuf, int Size) const;
	void Load();
	void Save();

	void BuildTexture();
	void FreeTexture();
	void SetOpen(bool Open);
	void FitView();
	void ZoomAt(vec2 Pos, float Factor);
	void PressButton(int Button);

	static void ConToggle(IConsole::IResult *pResult, void *pUserData);
	static void ConClear(IConsole::IResult *pResult, void *pUserData);
};

#endif
