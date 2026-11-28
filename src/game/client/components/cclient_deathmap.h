/* Cr1stal Client: a map of the places where you die, one per map. Based on DDNet, see licence.txt. */
#ifndef GAME_CLIENT_COMPONENTS_CCLIENT_DEATHMAP_H
#define GAME_CLIENT_COMPONENTS_CCLIENT_DEATHMAP_H

#include <base/vmath.h>

#include <engine/console.h>

#include <game/client/component.h>

#include <cstdint>
#include <unordered_map>

// Remembers the tile where you died on every map (not the deaths you made with kill) and can show them on the
// map as warm squares. Shown with the cc_death_map setting or while the +cc_deathmap key is held.
class CCDeathMap : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnConsoleInit() override;
	void OnMapLoad() override;
	void OnReset() override;
	void OnUpdate() override;
	void OnRender() override;
	void OnShutdown() override;

	bool Visible() const;
	// deaths on this map, all time and since you came
	int Total() const { return m_Total; }
	int Session() const { return m_Session; }
	void Clear();

private:
	std::unordered_map<int64_t, int> m_Cells;
	int m_Total = 0;
	int m_Session = 0;
	int m_MaxCount = 1;
	bool m_Hold = false;
	bool m_Dirty = false;
	int64_t m_LastSave = 0;
	char m_aMap[64] = "";

	// what the tee did on the last update
	bool m_WasAlive = false;
	vec2 m_LastPos = vec2(0.0f, 0.0f);

	static int64_t Key(int TileX, int TileY);
	void Record(vec2 Pos);
	void FilePath(char *pBuf, int Size) const;
	void Load();
	void Save();

	static void ConHold(IConsole::IResult *pResult, void *pUserData);
	static void ConClear(IConsole::IResult *pResult, void *pUserData);
};

#endif
