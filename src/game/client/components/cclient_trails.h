/* Cr1stal Client: trails behind tees. Ported from TClient (https://github.com/sjrc6/TaterClient-ddnet). Based on DDNet, see licence.txt. */
#ifndef GAME_CLIENT_COMPONENTS_CCLIENT_TRAILS_H
#define GAME_CLIENT_COMPONENTS_CCLIENT_TRAILS_H

#include <base/color.h>

#include <engine/shared/protocol.h>

#include <game/client/component.h>

class CCTrailPart
{
public:
	vec2 m_Pos = vec2(0.0f, 0.0f);
	vec2 m_UnmovedPos = vec2(0.0f, 0.0f);
	ColorRGBA m_Col;
	float m_Width = 0.0f;
	vec2 m_Normal = vec2(0.0f, 0.0f);
	vec2 m_Top = vec2(0.0f, 0.0f);
	vec2 m_Bot = vec2(0.0f, 0.0f);
	bool m_Flip = false;
	float m_Progress = 1.0f;
	int m_Tick = -1;

	bool operator==(const CCTrailPart &Other) const
	{
		return m_Pos == Other.m_Pos;
	}
};

class CCTrails : public CComponent
{
public:
	CCTrails() = default;
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	void OnReset() override;

	enum COLORMODES
	{
		COLORMODE_SOLID = 1,
		COLORMODE_TEE,
		COLORMODE_RAINBOW,
		COLORMODE_SPEED,
	};

private:
	class CInfo
	{
	public:
		vec2 m_Pos;
		int m_Tick;
	};
	CInfo m_History[MAX_CLIENTS][200];
	bool m_HistoryValid[MAX_CLIENTS] = {};

	void ClearAllHistory();
	void ClearHistory(int ClientId);
	bool ShouldPredictPlayer(int ClientId);
};

#endif
