/* Cr1stal Client: cursor trail, keystrokes, chat bubbles, physics balls and the picture behind the map. Based on DDNet, see licence.txt. */
#ifndef GAME_CLIENT_COMPONENTS_CCLIENT_VISUALS_H
#define GAME_CLIENT_COMPONENTS_CCLIENT_VISUALS_H

#include <base/vmath.h>

#include <engine/graphics.h>
#include <engine/shared/protocol.h>

#include <game/client/component.h>

#include <deque>
#include <string>
#include <vector>

class CCVisuals : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	void OnStateChange(int NewState, int OldState) override;
	void OnShutdown() override;

	void OnChatMessage(int ClientId, const char *pLine);

private:
	struct STrailPoint
	{
		vec2 m_Pos;
		float m_Age;
	};
	std::deque<STrailPoint> m_Trail;

	struct SBubble
	{
		std::string m_Text;
		float m_Age = -1.0f;
	};
	SBubble m_aBubbles[MAX_CLIENTS];

	struct SBall
	{
		vec2 m_Pos;
		vec2 m_Vel;
		float m_Spin;
		float m_Angle;
	};
	std::vector<SBall> m_vBalls;

	float m_aKeyAnim[5] = {};

	void RenderCursorTrail(float Dt);
	void RenderBubbles(float Dt);
	void RenderBalls(float Dt);
	void RenderKeystrokes(float Dt);
};

#endif
