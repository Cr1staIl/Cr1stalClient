/* Cr1stal Client: visual effects (jelly tees, weapon glow, hook effects, weather, freeze looks). Based on DDNet, see licence.txt. */
#ifndef GAME_CLIENT_COMPONENTS_CCLIENT_EFFECTS_H
#define GAME_CLIENT_COMPONENTS_CCLIENT_EFFECTS_H

#include <base/color.h>
#include <base/vmath.h>

#include <engine/shared/protocol.h>

#include <game/client/component.h>

#include <vector>

class CCEffects : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	void OnStateChange(int NewState, int OldState) override;

	// squash and stretch of the tee body, (1, 1) when off
	vec2 JellyScale(int ClientId) const;
	// called by the items while they draw lasers and projectiles
	void LaserGlow(vec2 From, vec2 To, int LaserType, float Fade) const;
	void ProjectileGlow(vec2 Pos, int Weapon, float Alpha) const;

private:
	struct SPlayer
	{
		bool m_Init = false;
		vec2 m_LastPos = vec2(0.0f, 0.0f);
		vec2 m_Vel = vec2(0.0f, 0.0f);
		float m_Jelly = 0.0f;
		float m_JellyVel = 0.0f;
		bool m_AirJumped = false;
		int m_AttackTick = 0;
		bool m_Frozen = false;
	};
	SPlayer m_aPlayers[MAX_CLIENTS];

	struct SRing
	{
		vec2 m_Pos;
		float m_Age;
	};
	struct SSlash
	{
		vec2 m_Pos;
		float m_Angle;
		float m_Dir;
		float m_Age;
	};
	struct SBolt
	{
		std::vector<vec2> m_vPoints;
		std::vector<std::vector<vec2>> m_vBranches;
		vec2 m_Hit;
		float m_Age;
	};
	std::vector<SRing> m_vRings;
	std::vector<SSlash> m_vSlashes;
	std::vector<SBolt> m_vBolts;

	struct SWeatherParticle
	{
		vec2 m_Pos;
		float m_Speed;
		float m_Size;
		float m_Phase;
	};
	std::vector<SWeatherParticle> m_vWeather;
	int m_WeatherType = -1;

	float m_Time = 0.0f;

	ColorRGBA GlowColor(float Offset) const;
	bool IsFrozen(int ClientId) const;
	void UpdatePlayers(float Dt);
	void RenderHookEffects();
	void RenderFrost();
	void RenderEvents(float Dt);
	void RenderWeather(float Dt, vec2 Center, vec2 Half);
	void SpawnBolt(vec2 Pos);
	void Line(vec2 A, vec2 B, float Width, ColorRGBA Color) const;
};

#endif
