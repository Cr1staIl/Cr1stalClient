#include "background.h"

#include <base/io.h>
#include <base/str.h>

#include <engine/image.h>
#include <engine/map.h>
#include <engine/shared/config.h>

#include <algorithm>
#include <cmath>

#include <engine/storage.h>

#include <game/client/components/mapimages.h>
#include <game/client/components/maplayers.h>
// bestclient
#include <game/client/components/bestclient/menu_media_background.h>
// bestclient
#include <game/client/gameclient.h>
#include <game/layers.h>
#include <game/localization.h>

CBackground::CBackground(ERenderType MapType, bool OnlineOnly) :
	CMapLayers(MapType, OnlineOnly)
{
	m_pLayers = new CLayers;
	m_pBackgroundLayers = m_pLayers;
	m_pImages = new CMapImages;
	m_pBackgroundImages = m_pImages;
	m_Loaded = false;
	m_aMapName[0] = '\0';
}

void CBackground::RenderCcPicture()
{
	if(g_Config.m_CcGameBackground[0] == '\0' || (Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK))
		return;
	if(str_comp(m_aCcPictureLoaded, g_Config.m_CcGameBackground) != 0)
	{
		Graphics()->UnloadTexture(&m_CcPicture);
		str_copy(m_aCcPictureLoaded, g_Config.m_CcGameBackground);
		char aPath[IO_MAX_PATH_LENGTH];
		str_format(aPath, sizeof(aPath), "assets/backgrounds/%s.png", g_Config.m_CcGameBackground);
		CImageInfo Image;
		if(Graphics()->LoadPng(Image, aPath, IStorage::TYPE_ALL))
		{
			m_CcPictureAspect = Image.m_Width / (float)std::max<size_t>(1, Image.m_Height);
			m_CcPicture = Graphics()->LoadTextureRawMove(Image, 0, aPath);
		}
	}
	if(!m_CcPicture.IsValid() || m_CcPicture.IsNullTexture())
		return;

	// fixed to the screen, a little movement with the camera
	const CScreenRect PreviousScreen = Graphics()->GetScreen();
	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	Graphics()->MapScreenToSize(Width, Height);
	const float Aspect = std::max(0.01f, m_CcPictureAspect);
	float DrawW = Width * 1.08f, DrawH = DrawW / Aspect;
	if(DrawH < Height * 1.08f)
	{
		DrawH = Height * 1.08f;
		DrawW = DrawH * Aspect;
	}
	const vec2 Camera = GetCurCamera()->m_Center;
	const float Px = std::sin(Camera.x * 0.0004f) * (DrawW - Width) * 0.45f;
	const float Py = std::sin(Camera.y * 0.0004f) * (DrawH - Height) * 0.45f;
	Graphics()->WrapClamp();
	Graphics()->TextureSet(m_CcPicture);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, g_Config.m_CcGameBackgroundAlpha / 100.0f);
	const IGraphics::CQuadItem Quad((Width - DrawW) / 2.0f + Px, (Height - DrawH) / 2.0f + Py, DrawW, DrawH);
	Graphics()->QuadsDrawTL(&Quad, 1);
	Graphics()->QuadsEnd();
	Graphics()->WrapNormal();
	Graphics()->MapScreen(PreviousScreen);
}

CBackground::~CBackground()
{
	delete m_pBackgroundLayers;
	delete m_pBackgroundImages;
}

void CBackground::OnInit()
{
	m_pBackgroundMap = CreateMap();
	m_pMap = m_pBackgroundMap.get();

	m_pImages->OnInterfacesInit(GameClient());
	if(g_Config.m_ClBackgroundEntities[0] != '\0' && str_comp(g_Config.m_ClBackgroundEntities, CURRENT_MAP))
		LoadBackground();
}

void CBackground::LoadBackground()
{
	if(m_Loaded && m_pMap == m_pBackgroundMap.get())
		m_pMap->Unload();

	m_Loaded = false;
	m_pMap = m_pBackgroundMap.get();
	m_pLayers = m_pBackgroundLayers;
	m_pImages = m_pBackgroundImages;

	str_copy(m_aMapName, g_Config.m_ClBackgroundEntities);
	if(g_Config.m_ClBackgroundEntities[0] != '\0')
	{
		bool NeedImageLoading = false;

		char aBuf[IO_MAX_PATH_LENGTH];
		str_format(aBuf, sizeof(aBuf), "maps/%s%s", g_Config.m_ClBackgroundEntities, str_endswith(g_Config.m_ClBackgroundEntities, ".map") ? "" : ".map");
		if(str_comp(g_Config.m_ClBackgroundEntities, CURRENT_MAP) == 0)
		{
			m_pMap = GameClient()->Map();
			if(m_pMap->IsLoaded())
			{
				m_pLayers = GameClient()->Layers();
				m_pImages = &GameClient()->m_MapImages;
				m_Loaded = true;
			}
		}
		else if(m_pMap->Load(g_Config.m_ClBackgroundEntities, Storage(), aBuf, IStorage::TYPE_ALL))
		{
			m_pLayers->Init(m_pMap, true, true);
			NeedImageLoading = true;
			m_Loaded = true;
		}

		if(m_Loaded)
		{
			if(NeedImageLoading)
			{
				m_pImages->LoadBackground(m_pLayers, m_pMap);
			}
			CMapLayers::OnMapLoad();
		}
	}
}

void CBackground::OnMapLoad()
{
	if(str_comp(g_Config.m_ClBackgroundEntities, CURRENT_MAP) == 0 || str_comp(g_Config.m_ClBackgroundEntities, m_aMapName))
	{
		LoadBackground();
	}
}

void CBackground::OnRender()
{
	RenderCcPicture();

	// bestclient
	if(Client()->State() == IClient::STATE_ONLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		CMenuMediaBackground &MediaBackground = GameClient()->m_BestClient.MenuMediaBackground();
		MediaBackground.SyncFromConfig(g_Config.m_BcGameMediaBackground, g_Config.m_BcMenuMediaBackgroundPath);
		MediaBackground.Update();
		if(g_Config.m_BcGameMediaBackground && g_Config.m_ClOverlayEntities > 0)
		{
			float ViewWidth = 0.0f;
			float ViewHeight = 0.0f;
			Graphics()->CalcScreenParams(Graphics()->ScreenAspect(), GetCurCamera()->m_Zoom, &ViewWidth, &ViewHeight);

			CMenuMediaBackground::SRenderContext RenderContext;
			RenderContext.m_CameraCenterX = GetCurCamera()->m_Center.x;
			RenderContext.m_CameraCenterY = GetCurCamera()->m_Center.y;
			RenderContext.m_ViewWidth = ViewWidth;
			RenderContext.m_ViewHeight = ViewHeight;
			RenderContext.m_WorldOffset = (float)g_Config.m_BcGameMediaBackgroundOffset / 100.0f;
			if(GameClient()->Layers() != nullptr && GameClient()->Layers()->GameLayer() != nullptr)
			{
				RenderContext.m_MapWidth = GameClient()->Layers()->GameLayer()->m_Width * 32.0f;
				RenderContext.m_MapHeight = GameClient()->Layers()->GameLayer()->m_Height * 32.0f;
			}

			MediaBackground.Render(ViewWidth, ViewHeight, &RenderContext);
		}
	}
	// bestclient

	if(g_Config.m_ClOverlayEntities != 100)
		return;

	if(!m_Loaded)
		return;

	CMapLayers::OnRender();
}
