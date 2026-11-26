/* Cr1stal Client: gameplay helpers (gores mode, fast actions, finish prediction, edge info, spectating). Based on DDNet, see licence.txt. */
#ifndef GAME_CLIENT_COMPONENTS_CCLIENT_GAMEPLAY_H
#define GAME_CLIENT_COMPONENTS_CCLIENT_GAMEPLAY_H

#include <base/vmath.h>

#include <engine/console.h>

#include <game/client/component.h>

#include <vector>

class CCGameplay : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnConsoleInit() override;
	void OnMapLoad() override;
	void OnUpdate() override;
	void OnRender() override;
	bool OnInput(const IInput::CEvent &Event) override;
	void OnStateChange(int NewState, int OldState) override;

	// gores mode: fire swings the hammer, the gun comes back after
	void OnFireInput(bool Pressed);

private:
	// distance (in tiles, walking through air and freeze) from every tile to the nearest finish
	std::vector<int> m_vFinishDistance;
	int m_StartDistance = 0;
	int m_MapWidth = 0;
	int m_MapHeight = 0;
	float m_BestProgress = 0.0f;

	bool m_GoresHeavyWeapon = false;
	vec2 m_LastPausedPos = vec2(0.0f, 0.0f);
	bool m_HadPausedPos = false;
	float m_MovedWarning = 0.0f;
	bool m_PriorityApplied = false;

	static void ConFastAction(IConsole::IResult *pResult, void *pUserData);
	void BuildFinishDistance();
	int TileDistance(vec2 Pos) const;
	void RenderFinishPrediction(float Width, float Height);
	void RenderEdgeInfo(float Width, float Height);
	void RenderMovedWarning(float Width, float Height);
};

// sets the priority of the game process (Windows only, does nothing elsewhere)
void CcSetHighProcessPriority(bool High);

#endif
