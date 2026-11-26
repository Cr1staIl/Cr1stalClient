/* Cr1stal Client: cursor trail, keystrokes, chat bubbles and physics balls. Based on DDNet, see licence.txt. */
#include "cclient_visuals.h"

#include "cclient_style.h"

#include <base/math.h>
#include <base/str.h>

#include <engine/image.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>
#include <game/collision.h>
#include <game/localization.h>

#include <algorithm>
#include <cmath>

void CCVisuals::OnStateChange(int NewState, int OldState)
{
	m_Trail.clear();
	for(SBubble &Bubble : m_aBubbles)
		Bubble = SBubble();
	m_vBalls.clear();
}

void CCVisuals::OnShutdown()
{
	m_vBalls.clear();
}

void CCVisuals::OnChatMessage(int ClientId, const char *pLine)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS || !g_Config.m_CcChatBubbles)
		return;
	// one line, not too long
	char aText[96];
	str_truncate(aText, sizeof(aText), pLine, 60);
	if(str_length(pLine) > 60)
		str_append(aText, "…");
	m_aBubbles[ClientId].m_Text = aText;
	m_aBubbles[ClientId].m_Age = 0.0f;
}

void CCVisuals::RenderCursorTrail(float Dt)
{
	for(auto &Point : m_Trail)
		Point.m_Age += Dt;
	if(!g_Config.m_CcCursorTrail || GameClient()->m_Snap.m_SpecInfo.m_Active || GameClient()->m_Menus.IsActive())
	{
		m_Trail.clear();
		return;
	}
	const vec2 Pos = GameClient()->m_Controls.m_aTargetPos[g_Config.m_ClDummy];
	if(m_Trail.empty() || distance(m_Trail.back().m_Pos, Pos) > 2.0f)
		m_Trail.push_back({Pos, 0.0f});
	const float Life = 0.012f * g_Config.m_CcCursorTrailLength;
	while(!m_Trail.empty() && m_Trail.front().m_Age > Life)
		m_Trail.pop_front();
	while((int)m_Trail.size() > 200)
		m_Trail.pop_front();
	if(m_Trail.size() < 2)
		return;

	Graphics()->TextureClear();
	Graphics()->BlendAdditive();
	Graphics()->QuadsBegin();
	const ColorRGBA Color = CCStyle::ACCENT_TEXT;
	for(size_t i = 1; i < m_Trail.size(); i++)
	{
		const vec2 A = m_Trail[i - 1].m_Pos, B = m_Trail[i].m_Pos;
		const float Fade = 1.0f - std::min(1.0f, m_Trail[i].m_Age / Life);
		const float Len = distance(A, B);
		if(Len <= 0.0f)
			continue;
		const vec2 Dir = (B - A) / Len;
		const vec2 Out = vec2(Dir.y, -Dir.x) * (4.0f * Fade + 0.5f);
		Graphics()->SetColor(Color.WithAlpha(0.6f * Fade * Fade));
		const IGraphics::CFreeformItem Item(A - Out, A + Out, B - Out, B + Out);
		Graphics()->QuadsDrawFreeform(&Item, 1);
	}
	Graphics()->QuadsEnd();
	Graphics()->BlendNormal();
}

void CCVisuals::RenderBubbles(float Dt)
{
	const float Time = (float)g_Config.m_CcChatBubbleTime;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		SBubble &Bubble = m_aBubbles[i];
		if(Bubble.m_Age < 0.0f)
			continue;
		Bubble.m_Age += Dt;
		if(Bubble.m_Age > Time || !g_Config.m_CcChatBubbles)
		{
			Bubble.m_Age = -1.0f;
			continue;
		}
		if(!GameClient()->m_Snap.m_aCharacters[i].m_Active)
			continue;
		// fade and rise in, fade out at the end
		const float In = std::min(1.0f, Bubble.m_Age / 0.2f);
		const float Out = std::min(1.0f, (Time - Bubble.m_Age) / 0.4f);
		const float Alpha = std::min(In, Out) * (GameClient()->IsOtherTeam(i) ? g_Config.m_ClShowOthersAlpha / 100.0f : 1.0f);
		const float FontSize = 13.0f;
		const float TextW = TextRender()->TextWidth(FontSize, Bubble.m_Text.c_str());
		const float W = TextW + 16.0f, H = FontSize + 10.0f;
		const vec2 Tee = GameClient()->m_aClients[i].m_RenderPos;
		const float Above = g_Config.m_ClNamePlates ? 92.0f : 60.0f;
		const float X = Tee.x - W / 2.0f, Y = Tee.y - Above - H + (1.0f - In) * 8.0f;
		Graphics()->DrawRect(X, Y, W, H, ColorRGBA(0.04f, 0.035f, 0.045f, 0.78f * Alpha), IGraphics::CORNER_ALL, H / 2.0f);
		// small tail towards the tee
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0.04f, 0.035f, 0.045f, 0.78f * Alpha);
		const IGraphics::CFreeformItem Tail(Tee.x - 5.0f, Y + H, Tee.x + 5.0f, Y + H, Tee.x, Y + H + 6.0f, Tee.x, Y + H + 6.0f);
		Graphics()->QuadsDrawFreeform(&Tail, 1);
		Graphics()->QuadsEnd();
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, Alpha);
		TextRender()->Text(X + 8.0f, Y + 4.0f, FontSize, Bubble.m_Text.c_str(), -1.0f);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
}

void CCVisuals::RenderBalls(float Dt)
{
	if(!g_Config.m_CcPhysBalls)
	{
		m_vBalls.clear();
		return;
	}
	const int LocalId = GameClient()->m_Snap.m_LocalClientId;
	if(LocalId < 0 || !GameClient()->m_Snap.m_aCharacters[LocalId].m_Active)
		return;
	const vec2 Local = GameClient()->m_aClients[LocalId].m_RenderPos;
	const int Wanted = g_Config.m_CcPhysBallsCount;
	const float Radius = 10.0f;
	// new balls fall from above you; balls far away come back
	while((int)m_vBalls.size() < Wanted)
		m_vBalls.push_back({Local + vec2(random_float(-60.0f, 60.0f), -80.0f - random_float(0.0f, 60.0f)), vec2(random_float(-100.0f, 100.0f), 0.0f), 0.0f, 0.0f});
	if((int)m_vBalls.size() > Wanted)
		m_vBalls.resize(Wanted);

	const int Steps = 4;
	const float h = Dt / Steps;
	for(int Step = 0; Step < Steps; Step++)
	{
		for(SBall &Ball : m_vBalls)
		{
			if(distance(Ball.m_Pos, Local) > 1500.0f)
			{
				Ball.m_Pos = Local + vec2(0.0f, -100.0f);
				Ball.m_Vel = vec2(0.0f, 0.0f);
			}
			Ball.m_Vel.y += 1400.0f * h;
			Ball.m_Vel *= std::pow(0.6f, h); // air
			// tees push the balls
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(!GameClient()->m_Snap.m_aCharacters[i].m_Active)
					continue;
				const vec2 Tee = GameClient()->m_aClients[i].m_RenderPos;
				const float Dist = distance(Ball.m_Pos, Tee);
				const float Min = 28.0f / 2.0f + Radius + 2.0f;
				if(Dist < Min && Dist > 0.001f)
				{
					const vec2 Normal = (Ball.m_Pos - Tee) / Dist;
					Ball.m_Pos = Tee + Normal * Min;
					const vec2 TeeVel = vec2(GameClient()->m_aClients[i].m_Predicted.m_Vel.x, GameClient()->m_aClients[i].m_Predicted.m_Vel.y) * 50.0f;
					Ball.m_Vel = Ball.m_Vel - Normal * dot(Ball.m_Vel - TeeVel, Normal) * 1.6f;
				}
			}
			// balls push each other
			for(SBall &Other : m_vBalls)
			{
				if(&Other == &Ball)
					continue;
				const float Dist = distance(Ball.m_Pos, Other.m_Pos);
				if(Dist < Radius * 2.0f && Dist > 0.001f)
				{
					const vec2 Normal = (Ball.m_Pos - Other.m_Pos) / Dist;
					const float Push = (Radius * 2.0f - Dist) / 2.0f;
					Ball.m_Pos += Normal * Push;
					Other.m_Pos -= Normal * Push;
					const float Rel = dot(Ball.m_Vel - Other.m_Vel, Normal);
					if(Rel < 0.0f)
					{
						Ball.m_Vel -= Normal * Rel * 0.9f;
						Other.m_Vel += Normal * Rel * 0.9f;
					}
				}
			}
			// the map: move on each axis and bounce on solid tiles
			vec2 Next = Ball.m_Pos + vec2(Ball.m_Vel.x * h, 0.0f);
			if(Collision()->CheckPoint(Next + vec2(Ball.m_Vel.x > 0.0f ? Radius : -Radius, 0.0f)))
			{
				Ball.m_Vel.x *= -0.6f;
				Next.x = Ball.m_Pos.x;
			}
			Ball.m_Pos.x = Next.x;
			Next = Ball.m_Pos + vec2(0.0f, Ball.m_Vel.y * h);
			if(Collision()->CheckPoint(Next + vec2(0.0f, Ball.m_Vel.y > 0.0f ? Radius : -Radius)))
			{
				Ball.m_Vel.y *= -0.55f;
				Ball.m_Vel.x *= 0.98f;
				Next.y = Ball.m_Pos.y;
			}
			Ball.m_Pos.y = Next.y;
			Ball.m_Spin = Ball.m_Vel.x / Radius;
			Ball.m_Angle += Ball.m_Spin * h;
		}
	}

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	for(const SBall &Ball : m_vBalls)
	{
		Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.35f);
		Graphics()->DrawCircle(Ball.m_Pos.x, Ball.m_Pos.y, Radius + 1.5f, 20);
		Graphics()->SetColor(CCStyle::ACCENT.WithAlpha(1.0f));
		Graphics()->DrawCircle(Ball.m_Pos.x, Ball.m_Pos.y, Radius, 20);
		// a stripe that turns with the ball, and a shine
		const vec2 Dir = direction(Ball.m_Angle);
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.85f);
		const vec2 Side = vec2(Dir.y, -Dir.x) * 1.6f;
		const IGraphics::CFreeformItem Stripe(Ball.m_Pos - Dir * Radius * 0.9f - Side, Ball.m_Pos - Dir * Radius * 0.9f + Side, Ball.m_Pos + Dir * Radius * 0.9f - Side, Ball.m_Pos + Dir * Radius * 0.9f + Side);
		Graphics()->QuadsDrawFreeform(&Stripe, 1);
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.35f);
		Graphics()->DrawCircle(Ball.m_Pos.x - Radius * 0.35f, Ball.m_Pos.y - Radius * 0.35f, Radius * 0.3f, 10);
	}
	Graphics()->QuadsEnd();
}

void CCVisuals::RenderKeystrokes(float Dt)
{
	if(!g_Config.m_CcKeystrokes || GameClient()->m_Snap.m_SpecInfo.m_Active)
		return;
	const CNetObj_PlayerInput &Input = GameClient()->m_Controls.m_aInputData[g_Config.m_ClDummy];
	const bool aDown[5] = {Input.m_Direction < 0, Input.m_Jump != 0, Input.m_Direction > 0, (Input.m_Fire & 1) != 0, Input.m_Hook != 0};
	for(int i = 0; i < 5; i++)
		m_aKeyAnim[i] = std::clamp(m_aKeyAnim[i] + (aDown[i] ? 1.0f : -1.0f) * Dt / 0.06f, 0.0f, 1.0f);

	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	Graphics()->MapScreenToSize(Width, Height);
	const float Key = 14.0f, Gap = 2.0f;
	const float BoxW = Key * 3.0f + Gap * 2.0f;
	const float X0 = std::clamp(Width * g_Config.m_CcKeystrokesX / 100.0f, 0.0f, Width - BoxW);
	const float Y0 = std::clamp(Height * g_Config.m_CcKeystrokesY / 100.0f, 0.0f, Height - Key * 2.0f - Gap);
	struct SKey
	{
		float m_X, m_Y, m_W;
		const char *m_pLabel;
	};
	const char *pFire = Localize("Fire");
	const char *pHook = Localize("Hook");
	const SKey aKeys[5] = {
		{X0, Y0 + Key + Gap, Key, "<"},
		{X0 + Key + Gap, Y0, Key, "^"},
		{X0 + (Key + Gap) * 2.0f, Y0 + Key + Gap, Key, ">"},
		{X0, Y0 + (Key + Gap) * 2.0f, (BoxW - Gap) / 2.0f, pFire},
		{X0 + (BoxW + Gap) / 2.0f, Y0 + (Key + Gap) * 2.0f, (BoxW - Gap) / 2.0f, pHook},
	};
	for(int i = 0; i < 5; i++)
	{
		const SKey &K = aKeys[i];
		const float a = m_aKeyAnim[i];
		const ColorRGBA Bg = CCStyle::Mix(ColorRGBA(0.0f, 0.0f, 0.0f, 0.4f), CCStyle::ACCENT.WithAlpha(0.85f), a);
		Graphics()->DrawRect(K.m_X, K.m_Y, K.m_W, Key, Bg, IGraphics::CORNER_ALL, 3.0f);
		const float FontSize = i < 3 ? 7.0f : 5.0f;
		const float TextW = TextRender()->TextWidth(FontSize, K.m_pLabel);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.6f + 0.4f * a);
		TextRender()->Text(K.m_X + (K.m_W - TextW) / 2.0f, K.m_Y + (Key - FontSize) / 2.0f - 0.5f, FontSize, K.m_pLabel, -1.0f);
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

void CCVisuals::RenderShapes(float Dt, vec2 Center, vec2 Half)
{
	if(!g_Config.m_Cc3dParticles)
	{
		m_vShapes.clear();
		return;
	}
	const vec2 Area = Half + vec2(60.0f, 60.0f);
	const int Wanted = g_Config.m_Cc3dParticlesCount;
	while((int)m_vShapes.size() < Wanted)
	{
		SShape Shape;
		Shape.m_Pos = Center + vec2(random_float(-Area.x, Area.x), random_float(-Area.y, Area.y));
		Shape.m_Drift = vec2(random_float(-15.0f, 15.0f), random_float(-25.0f, -5.0f));
		Shape.m_Size = random_float(5.0f, 13.0f);
		Shape.m_Rot = vec3(random_float(0.0f, 6.28f), random_float(0.0f, 6.28f), random_float(0.0f, 6.28f));
		Shape.m_Spin = vec3(random_float(-1.2f, 1.2f), random_float(-1.2f, 1.2f), random_float(-0.6f, 0.6f));
		m_vShapes.push_back(Shape);
	}
	if((int)m_vShapes.size() > Wanted)
		m_vShapes.resize(Wanted);

	// a cube: 8 corners and 12 edges, turned in 3D and seen from the front
	static const vec3 s_aCorners[8] = {
		vec3(-1, -1, -1), vec3(1, -1, -1), vec3(1, 1, -1), vec3(-1, 1, -1),
		vec3(-1, -1, 1), vec3(1, -1, 1), vec3(1, 1, 1), vec3(-1, 1, 1)};
	static const int s_aEdges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

	Graphics()->TextureClear();
	Graphics()->BlendAdditive();
	Graphics()->QuadsBegin();
	const ColorRGBA Color = CCStyle::ACCENT_TEXT;
	for(SShape &Shape : m_vShapes)
	{
		Shape.m_Pos += Shape.m_Drift * Dt;
		Shape.m_Rot += Shape.m_Spin * Dt;
		vec2 Rel = Shape.m_Pos - Center;
		if(Rel.x < -Area.x)
			Rel.x += Area.x * 2.0f;
		if(Rel.x > Area.x)
			Rel.x -= Area.x * 2.0f;
		if(Rel.y < -Area.y)
			Rel.y += Area.y * 2.0f;
		if(Rel.y > Area.y)
			Rel.y -= Area.y * 2.0f;
		Shape.m_Pos = Center + Rel;

		const float cx = std::cos(Shape.m_Rot.x), sx = std::sin(Shape.m_Rot.x);
		const float cy = std::cos(Shape.m_Rot.y), sy = std::sin(Shape.m_Rot.y);
		const float cz = std::cos(Shape.m_Rot.z), sz = std::sin(Shape.m_Rot.z);
		vec2 aPoints[8];
		float aDepth[8];
		for(int c = 0; c < 8; c++)
		{
			vec3 p = s_aCorners[c];
			p = vec3(p.x, p.y * cx - p.z * sx, p.y * sx + p.z * cx);
			p = vec3(p.x * cy + p.z * sy, p.y, -p.x * sy + p.z * cy);
			p = vec3(p.x * cz - p.y * sz, p.x * sz + p.y * cz, p.z);
			const float Persp = 1.0f / (1.0f + p.z * 0.18f);
			aPoints[c] = Shape.m_Pos + vec2(p.x, p.y) * Shape.m_Size * Persp;
			aDepth[c] = p.z;
		}
		for(const auto &Edge : s_aEdges)
		{
			const vec2 A = aPoints[Edge[0]], B = aPoints[Edge[1]];
			const float Near = 0.5f - (aDepth[Edge[0]] + aDepth[Edge[1]]) * 0.15f;
			const float Len = distance(A, B);
			if(Len <= 0.0f)
				continue;
			const vec2 Dir = (B - A) / Len;
			const vec2 Out = vec2(Dir.y, -Dir.x) * 0.7f;
			Graphics()->SetColor(Color.WithAlpha(std::clamp(Near, 0.15f, 0.85f) * 0.55f));
			const IGraphics::CFreeformItem Item(A - Out, A + Out, B - Out, B + Out);
			Graphics()->QuadsDrawFreeform(&Item, 1);
		}
	}
	Graphics()->QuadsEnd();
	Graphics()->BlendNormal();
}

void CCVisuals::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	const float Dt = std::clamp(Client()->RenderFrameTime(), 0.0f, 0.05f);

	const vec2 Center = GameClient()->m_Camera.m_Center;
	const CScreenRect ScreenRect = Graphics()->MapScreenToWorld(Center.x, Center.y, 100.0f, 100.0f, 100.0f, 0, 0, Graphics()->ScreenAspect(), GameClient()->m_Camera.m_Zoom);
	Graphics()->MapScreen(ScreenRect);
	RenderShapes(Dt, (ScreenRect.m_TopLeft + ScreenRect.m_BottomRight) / 2.0f, (ScreenRect.m_BottomRight - ScreenRect.m_TopLeft) / 2.0f);
	RenderBalls(Dt);
	RenderCursorTrail(Dt);
	RenderBubbles(Dt);
	RenderKeystrokes(Dt);
	// leave the world mapping for the components after this one
	Graphics()->MapScreen(ScreenRect);
}

void CCGameBackground::OnShutdown()
{
	Graphics()->UnloadTexture(&m_Texture);
	m_aLoaded[0] = '\0';
}

void CCGameBackground::OnRender()
{
	if(g_Config.m_CcGameBackground[0] == '\0' || (Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK))
		return;
	if(str_comp(m_aLoaded, g_Config.m_CcGameBackground) != 0)
	{
		Graphics()->UnloadTexture(&m_Texture);
		str_copy(m_aLoaded, g_Config.m_CcGameBackground);
		char aPath[IO_MAX_PATH_LENGTH];
		str_format(aPath, sizeof(aPath), "assets/backgrounds/%s.png", g_Config.m_CcGameBackground);
		CImageInfo Image;
		if(Graphics()->LoadPng(Image, aPath, IStorage::TYPE_ALL))
		{
			m_Aspect = Image.m_Width / (float)std::max<size_t>(1, Image.m_Height);
			m_Texture = Graphics()->LoadTextureRawMove(Image, 0, aPath);
		}
	}
	if(!m_Texture.IsValid() || m_Texture.IsNullTexture())
		return;

	// fixed to the screen, a little movement with the camera
	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	Graphics()->MapScreenToSize(Width, Height);
	float DrawW = Width * 1.08f, DrawH = DrawW / m_Aspect;
	if(DrawH < Height * 1.08f)
	{
		DrawH = Height * 1.08f;
		DrawW = DrawH * m_Aspect;
	}
	const vec2 Camera = GameClient()->m_Camera.m_Center;
	const float Px = std::sin(Camera.x * 0.0004f) * (DrawW - Width) * 0.45f;
	const float Py = std::sin(Camera.y * 0.0004f) * (DrawH - Height) * 0.45f;
	Graphics()->WrapClamp();
	Graphics()->TextureSet(m_Texture);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, g_Config.m_CcGameBackgroundAlpha / 100.0f);
	const IGraphics::CQuadItem Quad((Width - DrawW) / 2.0f + Px, (Height - DrawH) / 2.0f + Py, DrawW, DrawH);
	Graphics()->QuadsDrawTL(&Quad, 1);
	Graphics()->QuadsEnd();
	Graphics()->WrapNormal();

	// back to the world for the map layers after this
	const CScreenRect ScreenRect = Graphics()->MapScreenToWorld(Camera.x, Camera.y, 100.0f, 100.0f, 100.0f, 0, 0, Graphics()->ScreenAspect(), GameClient()->m_Camera.m_Zoom);
	Graphics()->MapScreen(ScreenRect);
}
