/* Cr1stal Client: markers your team can see ("hook here"). Based on DDNet, see licence.txt. */
#ifndef GAME_CLIENT_COMPONENTS_CCLIENT_MARKERS_H
#define GAME_CLIENT_COMPONENTS_CCLIENT_MARKERS_H

#include <base/types.h>
#include <base/vmath.h>

#include <engine/console.h>
#include <engine/shared/protocol.h>

#include <game/client/component.h>

#include <cstdint>
#include <memory>
#include <set>
#include <vector>

class IHttpRequest;

// You press a key and a marker appears where your cursor is. Teammates who use Cr1stal Client see it in the
// world and as an arrow at the edge of the screen. It goes through the same database as the presence notes:
// every player has one note per team, the others read the notes of their team about once a second.
class CCMarkers : public CComponent
{
public:
	enum
	{
		KIND_HERE = 0,
		KIND_HOOK,
		KIND_DANGER,
		KIND_GO,
		NUM_KINDS,
	};

	int Sizeof() const override { return sizeof(*this); }
	void OnConsoleInit() override;
	void OnUpdate() override;
	void OnRender() override;
	void OnReset() override;
	void OnStateChange(int NewState, int OldState) override;
	void OnShutdown() override;

	// puts a marker where the cursor is, or takes yours away when it is already there
	void Place(int Kind);
	void ClearOwn();

private:
	struct SMarker
	{
		int m_Id = 0;
		vec2 m_Pos = vec2(0.0f, 0.0f);
		int m_Kind = KIND_HERE;
		char m_aName[MAX_NAME_LENGTH] = "";
		int64_t m_Born = 0;
		bool m_Own = false;
	};

	std::vector<SMarker> m_vMarkers;
	std::set<int> m_Seen;
	bool m_Baseline = false;
	char m_aScope[64] = "";
	char m_aOwnKey[9] = "";
	int64_t m_NextPoll = 0;
	int64_t m_NextTeamCheck = 0;
	int64_t m_OwnRemoteUntil = 0;
	char m_aOwnPath[96] = "";
	bool m_HasCrystalTeammate = false;
	bool m_HintShown = false;
	std::shared_ptr<IHttpRequest> m_pPoll;

	// where the markers of your team are written, "" when you are not in a team or share nothing
	bool ScopePath(char *pBuf, int Size) const;
	void RemoveOwnRemote();
	void Poll(const char *pScope);
	void ReadPoll();

	static void ConMarker(IConsole::IResult *pResult, void *pUserData);
	static void ConMarkerClear(IConsole::IResult *pResult, void *pUserData);
};

#endif
