/* Cr1stal Client: visual effects (jelly tees, weapon glow, hook effects, weather, freeze looks). Based on DDNet, see licence.txt. */
#include "cclient_effects.h"

#include <base/math.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <generated/protocol.h>

#include <game/client/gameclient.h>
#include <game/gamecore.h>

#include <algorithm>
#include <cmath>

static float Hash01(int a, int b)
{
	unsigned h = (unsigned)a * 374761393u + (unsigned)b * 668265263u;
	h = (h ^ (h >> 13)) * 1274126177u;
	h ^= h >> 16;
	return (h & 0xffffff) / (float)0xffffff;
}

void CCEffects::OnStateChange(int NewState, int OldState)
{
	for(SPlayer &Player : m_aPlayers)
		Player = SPlayer();
	m_vRings.clear();
	m_vSlashes.clear();
	m_vBolts.clear();
}

bool CCEffects::IsFrozen(int ClientId) const
{
	const auto &Data = GameClient()->m_aClients[ClientId];
	const int Tick = Client()->GameTick(g_Config.m_ClDummy);
	return Data.m_DeepFrozen || Data.m_LiveFrozen || (Data.m_FreezeEnd != 0 && Data.m_FreezeEnd > Tick);
}

vec2 CCEffects::JellyScale(int ClientId) const
{
	if(!g_Config.m_CcJelly || ClientId < 0 || ClientId >= MAX_CLIENTS)
		return vec2(1.0f, 1.0f);
	const bool Local = ClientId == GameClient()->m_aLocalIds[0] || (Client()->DummyConnected() && ClientId == GameClient()->m_aLocalIds[1]);
	if(!Local && !g_Config.m_CcJellyOthers)
		return vec2(1.0f, 1.0f);
	const float j = std::clamp(m_aPlayers[ClientId].m_Jelly, -0.3f, 0.3f);
	return vec2(1.0f + j, 1.0f - j);
}

ColorRGBA CCEffects::GlowColor(float Offset) const
{
	const ColorRGBA Base = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_CcGlowColor));
	switch(g_Config.m_CcGlowMode)
	{
	case 1: // prism: the colors of the rainbow running along
		return color_cast<ColorRGBA>(ColorHSLA(std::fmod(m_Time * 0.25f + Offset, 1.0f), 1.0f, 0.6f, 1.0f));
	case 2: // pulse
		return Base.WithAlpha(0.55f + 0.45f * std::sin(m_Time * 6.0f));
	case 3: // crystal: icy white and cyan with a little sparkle
		return ColorRGBA(0.75f, 0.95f, 1.0f, 0.8f + 0.2f * std::sin(m_Time * 11.0f + Offset * 30.0f));
	default:
		return Base;
	}
}

void CCEffects::Line(vec2 A, vec2 B, float Width, ColorRGBA Color) const
{
	const float Len = distance(A, B);
	if(Len <= 0.0f)
		return;
	const vec2 Dir = (B - A) / Len;
	const vec2 Out = vec2(Dir.y, -Dir.x) * (Width / 2.0f);
	Graphics()->SetColor(Color);
	const IGraphics::CFreeformItem Item(A - Out, A + Out, B - Out, B + Out);
	Graphics()->QuadsDrawFreeform(&Item, 1);
}

void CCEffects::LaserGlow(vec2 From, vec2 To, int LaserType, float Fade) const
{
	const bool On = (LaserType == LASERTYPE_RIFLE && g_Config.m_CcGlowLaser) || (LaserType == LASERTYPE_SHOTGUN && g_Config.m_CcGlowShotgun);
	if(!On || Fade <= 0.0f)
		return;
	const float Power = g_Config.m_CcGlowPower / 100.0f;
	Graphics()->TextureClear();
	Graphics()->BlendAdditive();
	Graphics()->QuadsBegin();
	const ColorRGBA Color = GlowColor(From.x * 0.0005f);
	Line(From, To, 34.0f * Fade, Color.WithMultipliedAlpha(0.10f * Power));
	Line(From, To, 20.0f * Fade, Color.WithMultipliedAlpha(0.20f * Power));
	Line(From, To, 11.0f * Fade, Color.WithMultipliedAlpha(0.35f * Power));
	Graphics()->QuadsEnd();
	Graphics()->BlendNormal();
}

void CCEffects::ProjectileGlow(vec2 Pos, int Weapon, float Alpha) const
{
	if(Weapon != WEAPON_GRENADE || !g_Config.m_CcGlowRocket)
		return;
	const float Power = g_Config.m_CcGlowPower / 100.0f;
	const ColorRGBA Color = GlowColor(Pos.x * 0.001f);
	Graphics()->TextureClear();
	Graphics()->BlendAdditive();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(Color.WithMultipliedAlpha(0.12f * Power * Alpha));
	Graphics()->DrawCircle(Pos.x, Pos.y, 30.0f, 20);
	Graphics()->SetColor(Color.WithMultipliedAlpha(0.25f * Power * Alpha));
	Graphics()->DrawCircle(Pos.x, Pos.y, 17.0f, 16);
	Graphics()->QuadsEnd();
	Graphics()->BlendNormal();
}

void CCEffects::SpawnBolt(vec2 Pos)
{
	SBolt Bolt;
	Bolt.m_Age = 0.0f;
	Bolt.m_Hit = Pos;
	const float Height = 420.0f;
	vec2 Top = Pos + vec2(random_float(-60.0f, 60.0f), -Height);
	constexpr int Segments = 12;
	for(int i = 0; i <= Segments; i++)
	{
		const float t = i / (float)Segments;
		vec2 p = mix(Top, Pos, t);
		if(i != 0 && i != Segments)
			p.x += random_float(-22.0f, 22.0f) * (1.0f - t * 0.5f);
		Bolt.m_vPoints.push_back(p);
	}
	for(int b = 0; b < 3; b++)
	{
		const int Start = 2 + rand() % (Segments - 5);
		std::vector<vec2> vBranch;
		vec2 p = Bolt.m_vPoints[Start];
		const float Side = (rand() % 2) ? 1.0f : -1.0f;
		vBranch.push_back(p);
		for(int i = 0; i < 4; i++)
		{
			p += vec2(Side * random_float(10.0f, 24.0f), random_float(12.0f, 26.0f));
			vBranch.push_back(p);
		}
		Bolt.m_vBranches.push_back(vBranch);
	}
	m_vBolts.push_back(Bolt);
}

void CCEffects::UpdatePlayers(float Dt)
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		SPlayer &Player = m_aPlayers[i];
		if(!GameClient()->m_Snap.m_aCharacters[i].m_Active)
		{
			Player = SPlayer();
			continue;
		}
		const auto &Data = GameClient()->m_aClients[i];
		const vec2 Pos = Data.m_RenderPos;
		const bool Frozen = IsFrozen(i);
		const bool AirJumped = Data.m_RenderCur.m_Jumped & 2;
		if(!Player.m_Init)
		{
			Player.m_Init = true;
			Player.m_LastPos = Pos;
			Player.m_AirJumped = AirJumped;
			Player.m_AttackTick = Data.m_RenderCur.m_AttackTick;
			Player.m_Frozen = Frozen;
			continue;
		}

		// jelly: a spring that gets a push when the speed changes (landing, hooking, jumping)
		if(Dt > 0.0f)
		{
			if(distance(Pos, Player.m_LastPos) > 200.0f) // teleport or respawn
			{
				Player.m_Vel = vec2(0.0f, 0.0f);
				Player.m_Jelly = Player.m_JellyVel = 0.0f;
			}
			else
			{
				const vec2 Vel = (Pos - Player.m_LastPos) / Dt;
				const vec2 Change = Vel - Player.m_Vel;
				Player.m_Vel = Vel;
				const float Strength = g_Config.m_CcJellyStrength / 50.0f;
				Player.m_JellyVel += (-Change.y + absolute(Change.x) * 0.35f) * 0.004f * Strength;
				Player.m_JellyVel += (-180.0f * Player.m_Jelly - 11.0f * Player.m_JellyVel) * Dt;
				Player.m_Jelly += Player.m_JellyVel * Dt;
				Player.m_Jelly = std::clamp(Player.m_Jelly, -0.4f, 0.4f);
			}
		}
		Player.m_LastPos = Pos;

		const bool Visible = true;
		if(g_Config.m_CcAirJumpRing && AirJumped && !Player.m_AirJumped && Visible)
			m_vRings.push_back({Pos + vec2(0.0f, 16.0f), 0.0f});
		Player.m_AirJumped = AirJumped;

		if(Data.m_RenderCur.m_AttackTick != Player.m_AttackTick)
		{
			Player.m_AttackTick = Data.m_RenderCur.m_AttackTick;
			if(g_Config.m_CcHammerSlash && Data.m_RenderCur.m_Weapon == WEAPON_HAMMER)
			{
				const float Angle = Data.m_RenderCur.m_Angle / 256.0f;
				m_vSlashes.push_back({Pos, Angle, std::cos(Angle) >= 0.0f ? 1.0f : -1.0f, 0.0f});
			}
		}

		if(g_Config.m_CcFreezeLightning && Frozen && !Player.m_Frozen)
			SpawnBolt(Pos);
		Player.m_Frozen = Frozen;
	}
}

void CCEffects::RenderHookEffects()
{
	if(!g_Config.m_CcHookLasso && !g_Config.m_CcHookBox && !g_Config.m_CcHookHold)
		return;
	for(int Dummy = 0; Dummy < NUM_DUMMIES; Dummy++)
	{
		const int LocalId = GameClient()->m_aLocalIds[Dummy];
		if(LocalId < 0 || (Dummy == 1 && !Client()->DummyConnected()) || !GameClient()->m_Snap.m_aCharacters[LocalId].m_Active)
			continue;
		const auto &Data = GameClient()->m_aClients[LocalId];
		const CCharacterCore &Core = Data.m_Predicted;
		if(Core.m_HookState != HOOK_GRABBED)
			continue;
		const int Hooked = Core.HookedPlayer();
		const vec2 From = Data.m_RenderPos;
		const vec2 To = Hooked >= 0 && Hooked < MAX_CLIENTS ? GameClient()->m_aClients[Hooked].m_RenderPos : Core.m_HookPos;
		const ColorRGBA Accent(0.95f, 0.18f, 0.33f, 1.0f);

		Graphics()->TextureClear();
		Graphics()->BlendAdditive();
		Graphics()->QuadsBegin();
		// pulses running along the chain while you hold
		if(g_Config.m_CcHookHold)
		{
			const float Len = distance(From, To);
			for(int p = 0; p < 3; p++)
			{
				const float t = std::fmod(m_Time * 1.6f + p / 3.0f, 1.0f);
				const vec2 Pos = mix(From, To, t);
				Graphics()->SetColor(Accent.WithAlpha(0.35f * std::sin(t * pi)));
				Graphics()->DrawCircle(Pos.x, Pos.y, 5.0f + Len * 0.004f, 12);
			}
		}
		if(Hooked >= 0 && Hooked < MAX_CLIENTS)
		{
			// a spinning dashed ring around the tee you hold
			if(g_Config.m_CcHookLasso)
			{
				constexpr int Dashes = 12;
				const float Radius = 34.0f;
				for(int d = 0; d < Dashes; d++)
				{
					const float A0 = m_Time * 2.5f + d * 2.0f * pi / Dashes;
					const float A1 = A0 + pi / Dashes;
					Line(To + direction(A0) * Radius, To + direction(A1) * Radius, 2.5f, Accent.WithAlpha(0.8f));
				}
			}
			// corner brackets
			if(g_Config.m_CcHookBox)
			{
				const float S = 30.0f + 2.0f * std::sin(m_Time * 6.0f);
				const float L = 10.0f;
				const ColorRGBA Color = ColorRGBA(1.0f, 1.0f, 1.0f, 0.85f);
				for(int c = 0; c < 4; c++)
				{
					const float sx = (c & 1) ? 1.0f : -1.0f;
					const float sy = (c & 2) ? 1.0f : -1.0f;
					const vec2 Corner = To + vec2(sx * S, sy * S);
					Line(Corner, Corner - vec2(sx * L, 0.0f), 2.5f, Color);
					Line(Corner, Corner - vec2(0.0f, sy * L), 2.5f, Color);
				}
			}
		}
		Graphics()->QuadsEnd();
		Graphics()->BlendNormal();
	}
}

void CCEffects::RenderFrost()
{
	if(!g_Config.m_CcFreezeFrost)
		return;
	Graphics()->TextureClear();
	Graphics()->BlendAdditive();
	Graphics()->QuadsBegin();
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!GameClient()->m_Snap.m_aCharacters[i].m_Active || !IsFrozen(i))
			continue;
		const vec2 Pos = GameClient()->m_aClients[i].m_RenderPos;
		const float Alpha = GameClient()->IsOtherTeam(i) ? g_Config.m_ClShowOthersAlpha / 100.0f : 1.0f;
		// cold light around the tee
		Graphics()->SetColor(0.45f, 0.8f, 1.0f, 0.10f * Alpha);
		Graphics()->DrawCircle(Pos.x, Pos.y, 40.0f, 24);
		Graphics()->SetColor(0.6f, 0.9f, 1.0f, 0.10f * Alpha);
		Graphics()->DrawCircle(Pos.x, Pos.y, 27.0f, 20);
		// ice shards slowly turning around it
		constexpr int Shards = 6;
		for(int s = 0; s < Shards; s++)
		{
			const float A = m_Time * 0.6f + s * 2.0f * pi / Shards + i;
			const vec2 Dir = direction(A);
			const vec2 Side = vec2(Dir.y, -Dir.x);
			const float R = 31.0f + 2.0f * std::sin(m_Time * 2.0f + s);
			const vec2 C = Pos + Dir * R;
			const float Len = 7.0f + 2.0f * Hash01(i, s);
			Graphics()->SetColor(0.8f, 0.97f, 1.0f, 0.75f * Alpha);
			const IGraphics::CFreeformItem Shard(C - Dir * Len, C + Side * 2.2f, C - Side * 2.2f, C + Dir * Len);
			Graphics()->QuadsDrawFreeform(&Shard, 1);
		}
	}
	Graphics()->QuadsEnd();
	Graphics()->BlendNormal();
}

void CCEffects::RenderEvents(float Dt)
{
	for(auto &Ring : m_vRings)
		Ring.m_Age += Dt;
	for(auto &Slash : m_vSlashes)
		Slash.m_Age += Dt;
	for(auto &Bolt : m_vBolts)
		Bolt.m_Age += Dt;
	m_vRings.erase(std::remove_if(m_vRings.begin(), m_vRings.end(), [](const SRing &Ring) { return Ring.m_Age > 0.4f; }), m_vRings.end());
	m_vSlashes.erase(std::remove_if(m_vSlashes.begin(), m_vSlashes.end(), [](const SSlash &Slash) { return Slash.m_Age > 0.22f; }), m_vSlashes.end());
	m_vBolts.erase(std::remove_if(m_vBolts.begin(), m_vBolts.end(), [](const SBolt &Bolt) { return Bolt.m_Age > 0.45f; }), m_vBolts.end());
	if(m_vRings.empty() && m_vSlashes.empty() && m_vBolts.empty())
		return;

	Graphics()->TextureClear();
	Graphics()->BlendAdditive();
	Graphics()->QuadsBegin();

	// air jump: a flat ring that opens under the feet
	for(const auto &Ring : m_vRings)
	{
		const float t = Ring.m_Age / 0.4f;
		const float Radius = 8.0f + 30.0f * (1.0f - (1.0f - t) * (1.0f - t));
		constexpr int Segments = 28;
		for(int s = 0; s < Segments; s++)
		{
			const float A0 = s * 2.0f * pi / Segments, A1 = (s + 1) * 2.0f * pi / Segments;
			const vec2 P0 = Ring.m_Pos + vec2(std::cos(A0) * Radius, std::sin(A0) * Radius * 0.35f);
			const vec2 P1 = Ring.m_Pos + vec2(std::cos(A1) * Radius, std::sin(A1) * Radius * 0.35f);
			Line(P0, P1, 3.0f * (1.0f - t) + 0.5f, ColorRGBA(0.75f, 0.9f, 1.0f, 0.8f * (1.0f - t)));
		}
	}

	// hammer: a glowing arc in front of the tee
	for(const auto &Slash : m_vSlashes)
	{
		const float t = Slash.m_Age / 0.22f;
		constexpr int Segments = 14;
		const float Radius = 42.0f;
		const float Sweep = 1.9f * std::min(1.0f, t * 2.5f);
		const float Start = Slash.m_Angle - 0.95f * Slash.m_Dir;
		for(int s = 0; s < Segments; s++)
		{
			const float k0 = s / (float)Segments, k1 = (s + 1) / (float)Segments;
			const float A0 = Start + Sweep * k0 * Slash.m_Dir, A1 = Start + Sweep * k1 * Slash.m_Dir;
			const float W = 7.0f * std::sin(k0 * pi) * (1.0f - t);
			Line(Slash.m_Pos + direction(A0) * Radius, Slash.m_Pos + direction(A1) * Radius, W + 0.5f, ColorRGBA(0.95f, 0.18f, 0.33f, 0.9f * (1.0f - t)));
		}
	}

	// freeze lightning
	const ColorRGBA BoltColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_CcFreezeLightningColor));
	for(const auto &Bolt : m_vBolts)
	{
		const float t = Bolt.m_Age / 0.45f;
		const float A = (1.0f - t) * (t < 0.1f ? 1.0f : 0.6f + 0.4f * std::sin(Bolt.m_Age * 60.0f));
		for(size_t p = 1; p < Bolt.m_vPoints.size(); p++)
		{
			Line(Bolt.m_vPoints[p - 1], Bolt.m_vPoints[p], 10.0f, BoltColor.WithAlpha(0.18f * A));
			Line(Bolt.m_vPoints[p - 1], Bolt.m_vPoints[p], 3.5f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.9f * A));
		}
		for(const auto &vBranch : Bolt.m_vBranches)
			for(size_t p = 1; p < vBranch.size(); p++)
				Line(vBranch[p - 1], vBranch[p], 2.0f, BoltColor.WithAlpha(0.7f * A));
		Graphics()->SetColor(BoltColor.WithAlpha(0.35f * A));
		Graphics()->DrawCircle(Bolt.m_Hit.x, Bolt.m_Hit.y, 20.0f + 40.0f * t, 24);
	}

	Graphics()->QuadsEnd();
	Graphics()->BlendNormal();
}

void CCEffects::RenderWeather(float Dt, vec2 Center, vec2 Half)
{
	const int Type = g_Config.m_CcWeather;
	if(Type <= 0)
	{
		m_vWeather.clear();
		m_WeatherType = -1;
		return;
	}
	const vec2 Area = Half + vec2(80.0f, 80.0f);
	const int Wanted = std::clamp((int)(Half.x * Half.y * 4.0f / 9000.0f * g_Config.m_CcWeatherAmount / 100.0f), 20, 1500);
	if(Type != m_WeatherType)
	{
		m_vWeather.clear();
		m_WeatherType = Type;
	}
	while((int)m_vWeather.size() < Wanted)
	{
		SWeatherParticle Particle;
		Particle.m_Pos = Center + vec2(random_float(-Area.x, Area.x), random_float(-Area.y, Area.y));
		Particle.m_Speed = random_float(0.6f, 1.4f);
		Particle.m_Size = random_float(0.6f, 1.4f);
		Particle.m_Phase = random_float(0.0f, 2.0f * pi);
		m_vWeather.push_back(Particle);
	}
	if((int)m_vWeather.size() > Wanted)
		m_vWeather.resize(Wanted);

	// move, and put the ones that left the view back on the other side
	for(auto &Particle : m_vWeather)
	{
		vec2 Vel;
		if(Type == 1) // snow
			Vel = vec2(std::sin(m_Time * 1.3f + Particle.m_Phase) * 18.0f, 70.0f * Particle.m_Speed);
		else if(Type == 2) // rain
			Vel = vec2(110.0f, 1100.0f * Particle.m_Speed);
		else if(Type == 3) // fireflies
			Vel = vec2(std::sin(m_Time * 0.7f + Particle.m_Phase) * 28.0f, std::cos(m_Time * 0.9f + Particle.m_Phase * 1.7f) * 22.0f);
		else // dust
			Vel = vec2(12.0f + std::sin(m_Time * 0.4f + Particle.m_Phase) * 10.0f, std::cos(m_Time * 0.5f + Particle.m_Phase) * 8.0f);
		Particle.m_Pos += Vel * Dt;
		vec2 Rel = Particle.m_Pos - Center;
		if(Rel.x < -Area.x)
			Rel.x += Area.x * 2.0f;
		if(Rel.x > Area.x)
			Rel.x -= Area.x * 2.0f;
		if(Rel.y < -Area.y)
			Rel.y += Area.y * 2.0f;
		if(Rel.y > Area.y)
			Rel.y -= Area.y * 2.0f;
		Particle.m_Pos = Center + Rel;
	}

	Graphics()->TextureClear();
	if(Type == 3)
		Graphics()->BlendAdditive();
	Graphics()->QuadsBegin();
	for(const auto &Particle : m_vWeather)
	{
		const vec2 P = Particle.m_Pos;
		if(Type == 1)
		{
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.75f);
			Graphics()->DrawCircle(P.x, P.y, 2.2f * Particle.m_Size, 8);
		}
		else if(Type == 2)
		{
			Line(P, P + normalize(vec2(110.0f, 1100.0f)) * 16.0f * Particle.m_Size, 1.3f, ColorRGBA(0.65f, 0.75f, 0.9f, 0.45f));
		}
		else if(Type == 3)
		{
			const float Glow = 0.5f + 0.5f * std::sin(m_Time * 2.5f + Particle.m_Phase * 3.0f);
			Graphics()->SetColor(0.8f, 1.0f, 0.4f, 0.12f * Glow);
			Graphics()->DrawCircle(P.x, P.y, 9.0f * Particle.m_Size, 12);
			Graphics()->SetColor(0.9f, 1.0f, 0.6f, 0.8f * Glow);
			Graphics()->DrawCircle(P.x, P.y, 2.0f * Particle.m_Size, 8);
		}
		else
		{
			Graphics()->SetColor(0.85f, 0.8f, 0.7f, 0.35f);
			Graphics()->DrawCircle(P.x, P.y, 1.4f * Particle.m_Size, 6);
		}
	}
	Graphics()->QuadsEnd();
	Graphics()->BlendNormal();
}

void CCEffects::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	const float Dt = std::clamp(Client()->RenderFrameTime(), 0.0f, 0.05f);
	m_Time += Dt;

	UpdatePlayers(Dt);

	const vec2 Center = GameClient()->m_Camera.m_Center;
	const CScreenRect ScreenRect = Graphics()->MapScreenToWorld(Center.x, Center.y, 100.0f, 100.0f, 100.0f, 0, 0, Graphics()->ScreenAspect(), GameClient()->m_Camera.m_Zoom);
	Graphics()->MapScreen(ScreenRect);
	const vec2 Half = (ScreenRect.m_BottomRight - ScreenRect.m_TopLeft) / 2.0f;

	RenderFrost();
	RenderHookEffects();
	RenderEvents(Dt);
	RenderWeather(Dt, (ScreenRect.m_TopLeft + ScreenRect.m_BottomRight) / 2.0f, Half);
}
