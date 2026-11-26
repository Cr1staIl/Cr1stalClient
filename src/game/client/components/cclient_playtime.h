/* Cr1stal Client: counts how long you play every map, for the profile page. Based on DDNet, see licence.txt. */
#ifndef GAME_CLIENT_COMPONENTS_CCLIENT_PLAYTIME_H
#define GAME_CLIENT_COMPONENTS_CCLIENT_PLAYTIME_H

#include <game/client/component.h>

#include <cstdint>
#include <string>
#include <vector>

// Counts the time you are in the game (not spectating) on a
// server, per map. The category (Easy, Main, ...) comes from the server name, like
// the KoG servers are named.
class CCPlaytime : public CComponent
{
public:
	enum
	{
		CAT_EASY = 0,
		CAT_MAIN,
		CAT_HARD,
		CAT_INSANE,
		CAT_EXTREME,
		CAT_MODS,
		CAT_JET,
		CAT_SOLO,
		CAT_OTHER,
		NUM_CATEGORIES,
	};

	struct SMap
	{
		std::string m_Name;
		int m_Category = CAT_OTHER;
		int64_t m_Seconds = 0;
	};

	// a map you finished (first finish counts for points, the best time is kept)
	struct SFinish
	{
		std::string m_Map;
		int m_Category = CAT_OTHER;
		int m_BestMs = 0;
		int64_t m_When = 0;
	};

	int Sizeof() const override { return sizeof(*this); }
	void OnMessage(int MsgType, void *pRawMsg) override;
	void OnInit() override;
	void OnUpdate() override;
	void OnShutdown() override;

	int64_t Total() const;
	int64_t CategorySeconds(int Category) const;
	// the maps you played the longest, longest first
	std::vector<const SMap *> TopMaps(int Num) const;
	int NumMaps() const { return (int)m_vMaps.size(); }
	// unix time when counting started
	int64_t Since() const { return m_Since; }

	const std::vector<SFinish> &Finishes() const { return m_vFinishes; }
	int NumFinished() const { return (int)m_vFinishes.size(); }
	// points like on KoG: every map you finished gives points by its difficulty
	int Points() const;
	static int CategoryPoints(int Category);
	// seconds per category as "a,b,c,..." (for sharing)
	void CategoryString(char *pBuf, int BufSize) const;

	static const char *CategoryName(int Category);
	static int CategoryFromServerName(const char *pServerName);
	// 1d 17h, 3h 3m, 58m, 12s
	static void FormatTime(int64_t Seconds, char *pBuf, int BufSize);

private:
	std::vector<SMap> m_vMaps;
	std::vector<SFinish> m_vFinishes;
	void AddFinish(int TimeMs);
	int64_t m_Since = 0;
	int64_t m_LastTick = 0;
	double m_Pending = 0.0;
	int64_t m_LastSave = 0;
	bool m_Dirty = false;

	SMap *FindOrAdd(const char *pName, int Category);
	void Load();
	void Save();
};

#endif
