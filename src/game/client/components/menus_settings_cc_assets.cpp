/* Cr1stal Client: skins, sound packs and menu backgrounds on the textures page. Based on DDNet, see licence.txt. */
#include "cclient_style.h"
#include "menus.h"

#include <base/io.h>
#include <base/str.h>

#include <engine/graphics.h>
#include <engine/image.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <generated/client_data.h>

#include <game/client/gameclient.h>
#include <game/client/ui_listbox.h>
#include <game/localization.h>

#include <algorithm>

enum
{
	CC_TAB_SKINS = 0,
	CC_TAB_SOUNDS,
	CC_TAB_BACKGROUNDS,
};

static const char *const SOUNDS_FOLDER = "assets/sounds";
static const char *const BACKGROUNDS_FOLDER = "assets/backgrounds";

// open a folder in the save directory, making it first
static void OpenFolder(IStorage *pStorage, IClient *pClient, const char *pFolder)
{
	pStorage->CreateFolder("assets", IStorage::TYPE_SAVE);
	pStorage->CreateFolder(pFolder, IStorage::TYPE_SAVE);
	char aPath[IO_MAX_PATH_LENGTH];
	pStorage->GetCompletePath(IStorage::TYPE_SAVE, pFolder, aPath, sizeof(aPath));
	pClient->ViewFile(aPath);
}

// a list of every game sound file, so a pack can be made without guessing the names
static void WriteSoundNames(IStorage *pStorage)
{
	pStorage->CreateFolder("assets", IStorage::TYPE_SAVE);
	pStorage->CreateFolder(SOUNDS_FOLDER, IStorage::TYPE_SAVE);
	IOHANDLE File = pStorage->OpenFile("assets/sounds/sound_names.txt", IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
		return;
	const char *pHead =
		"Cr1stal Client sound packs\n"
		"Make a folder here (its name is the name of the pack) and put sounds in it\n"
		"named like below. .wv and .opus work. Sounds you leave out stay normal.\n\n";
	io_write(File, pHead, str_length(pHead));
	for(int s = 0; s < g_pData->m_NumSounds; s++)
	{
		for(int i = 0; i < g_pData->m_aSounds[s].m_NumSounds; i++)
		{
			const char *pName = g_pData->m_aSounds[s].m_aSounds[i].m_pFilename;
			if(str_startswith(pName, "audio/"))
				pName += 6;
			io_write(File, pName, str_length(pName));
			io_write_newline(File);
		}
	}
	io_close(File);
}

static int SoundPackScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	auto *pList = static_cast<std::vector<std::string> *>(pUser);
	if(!IsDir || pName[0] == '.' || str_comp(pName, "default") == 0)
		return 0;
	if(std::find(pList->begin(), pList->end(), pName) == pList->end())
		pList->emplace_back(pName);
	return 0;
}

static int BackgroundScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	auto *pList = static_cast<std::vector<std::string> *>(pUser);
	if(IsDir || !str_endswith(pName, ".png"))
		return 0;
	char aName[64];
	str_truncate(aName, sizeof(aName), pName, str_length(pName) - 4);
	if(std::find(pList->begin(), pList->end(), aName) == pList->end())
		pList->emplace_back(aName);
	return 0;
}

void CMenus::ClearCcBackgrounds()
{
	for(SCcBackground &Background : m_vCcBackgrounds)
		Graphics()->UnloadTexture(&Background.m_Texture);
	m_vCcBackgrounds.clear();
	m_CcBackgroundsScanned = false;
}

bool CMenus::RenderCcMenuBackgroundImage()
{
	if(g_Config.m_CcMenuBackground[0] == '\0')
		return false;
	if(str_comp(m_aCcMenuBgLoaded, g_Config.m_CcMenuBackground) != 0)
	{
		Graphics()->UnloadTexture(&m_CcMenuBgTexture);
		str_copy(m_aCcMenuBgLoaded, g_Config.m_CcMenuBackground);
		char aPath[IO_MAX_PATH_LENGTH];
		str_format(aPath, sizeof(aPath), "%s/%s.png", BACKGROUNDS_FOLDER, g_Config.m_CcMenuBackground);
		CImageInfo Image;
		m_CcMenuBgAspect = 16.0f / 9.0f;
		if(Graphics()->LoadPng(Image, aPath, IStorage::TYPE_ALL))
		{
			m_CcMenuBgAspect = Image.m_Width / (float)std::max<size_t>(1, Image.m_Height);
			m_CcMenuBgTexture = Graphics()->LoadTextureRawMove(Image, 0, aPath);
		}
	}
	if(!m_CcMenuBgTexture.IsValid() || m_CcMenuBgTexture.IsNullTexture())
		return false;

	const float ScreenHeight = 300.0f;
	const float ScreenWidth = ScreenHeight * Graphics()->ScreenAspect();
	Graphics()->MapScreenToSize(ScreenWidth, ScreenHeight);

	// cover the screen and keep the picture's shape
	const float ImageAspect = std::max(0.01f, m_CcMenuBgAspect);
	float DrawW = ScreenWidth, DrawH = ScreenWidth / ImageAspect;
	if(DrawH < ScreenHeight)
	{
		DrawH = ScreenHeight;
		DrawW = ScreenHeight * ImageAspect;
	}
	Graphics()->WrapClamp();
	Graphics()->TextureSet(m_CcMenuBgTexture);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	const IGraphics::CQuadItem Picture((ScreenWidth - DrawW) / 2.0f, (ScreenHeight - DrawH) / 2.0f, DrawW, DrawH);
	Graphics()->QuadsDrawTL(&Picture, 1);
	Graphics()->QuadsEnd();
	Graphics()->WrapNormal();

	// a little darker so the menus stay readable
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.3f);
	const IGraphics::CQuadItem Shade(0, 0, ScreenWidth, ScreenHeight);
	Graphics()->QuadsDrawTL(&Shade, 1);
	Graphics()->QuadsEnd();
	return true;
}

void CMenus::RenderCcAssetsTab(CUIRect MainView, int Tab)
{
	// header card: what goes where, and the buttons
	CUIRect Header, Line, Buttons, Button;
	MainView.HSplitTop(10.0f, nullptr, &MainView);
	MainView.HSplitTop(78.0f, &Header, &MainView);
	CCStyle::DrawCard(Header, 10.0f);
	Header.Margin(10.0f, &Header);

	const char *pTitle = "";
	const char *pText1 = "";
	const char *pText2 = "";
	if(Tab == CC_TAB_SKINS)
	{
		pTitle = Localize("Skins");
		pText1 = Localize("Put .png skins into the skins folder.");
		pText2 = Localize("They appear in the skin list of the Tee settings right after a reload.");
	}
	else if(Tab == CC_TAB_SOUNDS)
	{
		pTitle = Localize("Sounds");
		pText1 = Localize("Make a folder for your pack in the sounds folder and put .wv or .opus sounds in it.");
		pText2 = Localize("The names are in sound_names.txt there. Sounds you leave out stay normal.");
	}
	else
	{
		pTitle = Localize("Backgrounds");
		pText1 = Localize("Put .png pictures into the backgrounds folder.");
		pText2 = Localize("The chosen picture is shown behind the menus.");
	}

	Header.HSplitTop(18.0f, &Line, &Header);
	TextRender()->TextColor(CCStyle::ACCENT_TEXT);
	Ui()->DoLabel(&Line, CCStyle::Upper(pTitle).c_str(), 13.0f, TEXTALIGN_ML);
	TextRender()->TextColor(CCStyle::TEXT_DIM);
	Header.HSplitTop(14.0f, &Line, &Header);
	Ui()->DoLabel(&Line, pText1, 11.0f, TEXTALIGN_ML);
	Header.HSplitTop(14.0f, &Line, &Header);
	Ui()->DoLabel(&Line, pText2, 11.0f, TEXTALIGN_ML);
	TextRender()->TextColor(TextRender()->DefaultTextColor());

	MainView.HSplitTop(8.0f, nullptr, &MainView);
	MainView.HSplitTop(24.0f, &Buttons, &MainView);
	MainView.HSplitTop(8.0f, nullptr, &MainView);

	static CButtonContainer s_OpenButton, s_ReloadButton, s_ExtraButton;
	Buttons.VSplitLeft(170.0f, &Button, &Buttons);
	if(DoButton_Menu(&s_OpenButton, Localize("Open folder"), 0, &Button, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 8.0f, 0.0f, CCStyle::ACCENT))
	{
		if(Tab == CC_TAB_SKINS)
		{
			Storage()->CreateFolder("skins", IStorage::TYPE_SAVE);
			char aPath[IO_MAX_PATH_LENGTH];
			Storage()->GetCompletePath(IStorage::TYPE_SAVE, "skins", aPath, sizeof(aPath));
			Client()->ViewFile(aPath);
		}
		else if(Tab == CC_TAB_SOUNDS)
		{
			WriteSoundNames(Storage());
			OpenFolder(Storage(), Client(), SOUNDS_FOLDER);
		}
		else
		{
			OpenFolder(Storage(), Client(), BACKGROUNDS_FOLDER);
		}
	}
	Buttons.VSplitLeft(8.0f, nullptr, &Buttons);
	Buttons.VSplitLeft(130.0f, &Button, &Buttons);
	if(DoButton_Menu(&s_ReloadButton, Localize("Reload"), 0, &Button, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 8.0f) || Input()->KeyPress(KEY_F5))
	{
		if(Tab == CC_TAB_SKINS)
			GameClient()->RefreshSkins(CSkinDescriptor::FLAG_SIX);
		else if(Tab == CC_TAB_SOUNDS)
		{
			m_vCcSoundPacks.clear();
			m_CcSoundPacksScanned = false;
			GameClient()->m_Sounds.ReloadSoundPack();
		}
		else
		{
			ClearCcBackgrounds();
			m_aCcMenuBgLoaded[0] = '\0'; // load the chosen picture again too
		}
	}
	if(Tab == CC_TAB_SKINS)
	{
		Buttons.VSplitLeft(8.0f, nullptr, &Buttons);
		Buttons.VSplitLeft(150.0f, &Button, &Buttons);
		if(DoButton_Menu(&s_ExtraButton, Localize("Choose skin"), 0, &Button, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 8.0f))
			g_Config.m_UiSettingsPage = SETTINGS_TEE;
		return;
	}
	if(Tab == CC_TAB_SOUNDS)
	{
		Buttons.VSplitLeft(8.0f, nullptr, &Buttons);
		Buttons.VSplitLeft(130.0f, &Button, &Buttons);
		if(DoButton_Menu(&s_ExtraButton, Localize("Test sound"), 0, &Button, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 8.0f))
			GameClient()->m_Sounds.Play(CSounds::CHN_GUI, SOUND_HOOK_ATTACH_GROUND, 1.0f);
	}

	// the list of packs / pictures
	if(Tab == CC_TAB_SOUNDS)
	{
		if(!m_CcSoundPacksScanned)
		{
			m_vCcSoundPacks.clear();
			Storage()->ListDirectory(IStorage::TYPE_ALL, SOUNDS_FOLDER, SoundPackScan, &m_vCcSoundPacks);
			std::sort(m_vCcSoundPacks.begin(), m_vCcSoundPacks.end());
			m_vCcSoundPacks.insert(m_vCcSoundPacks.begin(), "default");
			m_CcSoundPacksScanned = true;
		}
		int OldSelected = -1;
		for(size_t i = 0; i < m_vCcSoundPacks.size(); i++)
			if(str_comp(m_vCcSoundPacks[i].c_str(), g_Config.m_CcAssetSounds) == 0)
				OldSelected = i;
		static CListBox s_ListBox;
		s_ListBox.DoStart(26.0f, m_vCcSoundPacks.size(), 1, 3, OldSelected, &MainView);
		for(size_t i = 0; i < m_vCcSoundPacks.size(); i++)
		{
			const CListboxItem Item = s_ListBox.DoNextItem(&m_vCcSoundPacks[i], (int)i == OldSelected);
			if(!Item.m_Visible)
				continue;
			CUIRect Label;
			Item.m_Rect.VMargin(10.0f, &Label);
			const char *pName = i == 0 ? Localize("Default") : m_vCcSoundPacks[i].c_str();
			Ui()->DoLabel(&Label, pName, 13.0f, TEXTALIGN_ML);
		}
		const int NewSelected = s_ListBox.DoEnd();
		if(NewSelected != OldSelected && NewSelected >= 0 && NewSelected < (int)m_vCcSoundPacks.size())
		{
			str_copy(g_Config.m_CcAssetSounds, m_vCcSoundPacks[NewSelected].c_str());
			GameClient()->m_Sounds.ReloadSoundPack();
			GameClient()->m_Sounds.Play(CSounds::CHN_GUI, SOUND_HOOK_ATTACH_GROUND, 1.0f);
		}
		return;
	}

	// backgrounds
	if(!m_CcBackgroundsScanned)
	{
		ClearCcBackgrounds();
		std::vector<std::string> vNames;
		Storage()->ListDirectory(IStorage::TYPE_ALL, BACKGROUNDS_FOLDER, BackgroundScan, &vNames);
		std::sort(vNames.begin(), vNames.end());
		SCcBackground Neon;
		Neon.m_aName[0] = '\0';
		m_vCcBackgrounds.push_back(Neon);
		for(const std::string &Name : vNames)
		{
			SCcBackground Background;
			str_copy(Background.m_aName, Name.c_str());
			char aPath[IO_MAX_PATH_LENGTH];
			str_format(aPath, sizeof(aPath), "%s/%s.png", BACKGROUNDS_FOLDER, Name.c_str());
			Background.m_Texture = Graphics()->LoadTexture(aPath, IStorage::TYPE_ALL);
			m_vCcBackgrounds.push_back(Background);
		}
		m_CcBackgroundsScanned = true;
	}
	int OldSelected = -1;
	for(size_t i = 0; i < m_vCcBackgrounds.size(); i++)
		if(str_comp(m_vCcBackgrounds[i].m_aName, g_Config.m_CcMenuBackground) == 0)
			OldSelected = i;

	constexpr float ItemWidth = 180.0f;
	constexpr float PreviewHeight = 100.0f;
	static CListBox s_ListBox;
	s_ListBox.DoStart(PreviewHeight + 32.0f, m_vCcBackgrounds.size(), std::max(1, (int)(MainView.w / ItemWidth)), 1, OldSelected, &MainView, false);
	for(size_t i = 0; i < m_vCcBackgrounds.size(); i++)
	{
		const SCcBackground &Background = m_vCcBackgrounds[i];
		const CListboxItem Item = s_ListBox.DoNextItem(&Background, (int)i == OldSelected);
		if(!Item.m_Visible)
			continue;
		CUIRect Rect, Label, Preview;
		Item.m_Rect.Margin(6.0f, &Rect);
		Rect.HSplitBottom(18.0f, &Preview, &Label);
		Ui()->DoLabel(&Label, Background.m_aName[0] ? Background.m_aName : Localize("Neon"), 11.0f, TEXTALIGN_MC);
		if(Background.m_aName[0] == '\0')
		{
			// the neon one: a small dark card with a red glow at the bottom
			Preview.Draw(ColorRGBA(0.03f, 0.027f, 0.031f, 1.0f), IGraphics::CORNER_ALL, 6.0f);
			CUIRect Glow;
			Preview.HSplitBottom(Preview.h * 0.45f, nullptr, &Glow);
			Glow.Draw(CCStyle::ACCENT_DEEP.WithAlpha(0.45f), IGraphics::CORNER_B, 6.0f);
		}
		else if(Background.m_Texture.IsValid() && !Background.m_Texture.IsNullTexture())
		{
			Graphics()->WrapClamp();
			Graphics()->TextureSet(Background.m_Texture);
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
			const IGraphics::CQuadItem Quad(Preview.x, Preview.y, Preview.w, Preview.h);
			Graphics()->QuadsDrawTL(&Quad, 1);
			Graphics()->QuadsEnd();
			Graphics()->WrapNormal();
		}
	}
	const int NewSelected = s_ListBox.DoEnd();
	if(NewSelected != OldSelected && NewSelected >= 0 && NewSelected < (int)m_vCcBackgrounds.size())
		str_copy(g_Config.m_CcMenuBackground, m_vCcBackgrounds[NewSelected].m_aName);
}
