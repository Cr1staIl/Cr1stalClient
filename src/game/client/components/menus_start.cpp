/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "menus_start.h"

#include <engine/client/updater.h>
#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <generated/client_data.h>

#include <game/client/components/cclient.h>
#include <game/client/components/cclient_style.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/localization.h>
#include <game/version.h>

#include <algorithm>
#include <cmath>
#include <string>

#if defined(CONF_PLATFORM_ANDROID)
#include <android/android_main.h>
#endif

void CMenusStart::ResetTitle()
{
	for(STextContainerIndex &Container : m_aTitleContainers)
		TextRender()->DeleteTextContainer(Container);
	m_NumTitleChars = 0;
}

void CMenusStart::RenderNeonTitle(float CenterX, float Y)
{
	static const char *const s_pTitle = "Cr\xe2\x99\xb1stal Client";
	constexpr int CrossCodepoint = 0x2671;
	constexpr float FontSize = 58.0f;
	constexpr float CrossSize = FontSize * 1.45f; // the cross is drawn bigger than the letters
	constexpr float LetterSpacing = 1.0f;
	constexpr int Bands = 9; // the color changes smoothly from top to bottom in this many steps
	const float Time = Client()->GlobalTime();

	// letters are laid out once and reused every frame
	bool Valid = m_NumTitleChars > 0;
	for(int i = 0; i < m_NumTitleChars && Valid; i++)
		Valid = m_aTitleSpace[i] || m_aTitleContainers[i].Valid();
	if(!Valid)
	{
		ResetTitle();
		TextRender()->SetFontPreset(EFontPreset::TITLE_FONT);
		const char *pCur = s_pTitle;
		while(*pCur && m_NumTitleChars < MAX_TITLE_CHARS)
		{
			const char *pStart = pCur;
			const int Code = str_utf8_decode(&pCur);
			if(Code <= 0)
				break;
			char aText[8];
			str_copy(aText, pStart, std::min<int>(sizeof(aText), pCur - pStart + 1));
			const int Index = m_NumTitleChars++;
			m_aTitleSizes[Index] = Code == CrossCodepoint ? CrossSize : FontSize;
			m_aTitleWidths[Index] = TextRender()->TextWidth(m_aTitleSizes[Index], aText) + LetterSpacing;
			m_aTitleSpace[Index] = Code == ' ';
			if(!m_aTitleSpace[Index])
			{
				CTextCursor Cursor;
				Cursor.SetPosition(vec2(0.0f, 0.0f));
				Cursor.m_FontSize = m_aTitleSizes[Index];
				TextRender()->CreateTextContainer(m_aTitleContainers[Index], &Cursor, aText);
			}
		}
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
	}

	float TotalWidth = 0.0f;
	for(int i = 0; i < m_NumTitleChars; i++)
		TotalWidth += m_aTitleWidths[i];

	const ColorRGBA Top(1.0f, 0.97f, 0.98f, 1.0f);
	const ColorRGBA Bottom(0.93f, 0.1f, 0.28f, 1.0f);
	const ColorRGBA NoOutline(0.0f, 0.0f, 0.0f, 0.0f);

	float X = CenterX - TotalWidth / 2.0f;
	for(int Index = 0; Index < m_NumTitleChars; Index++)
	{
		const float Size = m_aTitleSizes[Index];
		if(!m_aTitleSpace[Index] && m_aTitleContainers[Index].Valid())
		{
			// every letter floats gently up/down and sways a little, each with its own rhythm
			const float BobY = std::sin(Time * 1.3f + Index * 0.55f) * 3.0f;
			const float SwayX = std::sin(Time * 0.7f + Index * 1.1f) * 1.2f;
			const float CharX = X + SwayX;
			const float CharY = Y - (Size - FontSize) * 0.55f + BobY;

			// thin crimson neon around the letter, breathing slowly
			CCStyle::NeonGlow([&](vec2 Offset, ColorRGBA Color) { TextRender()->RenderTextContainer(m_aTitleContainers[Index], Color, NoOutline, CharX + Offset.x, CharY + Offset.y); },
				ColorRGBA(1.0f, 0.12f, 0.3f, 1.0f), CCStyle::NeonPulse(Time, Index * 0.35f), 1.5f);

			// smooth white -> red gradient from top to bottom, no outline, so no hard edges
			const float InkTop = CharY + Size * 0.18f;
			const float InkHeight = Size * 0.78f;
			for(int Band = 0; Band < Bands; Band++)
			{
				CUIRect Clip;
				Clip.x = CharX - Size * 0.2f;
				Clip.w = m_aTitleWidths[Index] + Size * 0.4f;
				Clip.y = Band == 0 ? CharY - Size : InkTop + InkHeight * Band / Bands;
				Clip.h = Band == Bands - 1 ? Size * 3.0f : InkTop + InkHeight * (Band + 1) / Bands - Clip.y;
				const float T = (Band + 0.5f) / Bands;
				const float Ease = T * T * (3.0f - 2.0f * T);
				const ColorRGBA Color(Top.r + (Bottom.r - Top.r) * Ease, Top.g + (Bottom.g - Top.g) * Ease, Top.b + (Bottom.b - Top.b) * Ease, 1.0f);
				Ui()->ClipEnable(&Clip);
				TextRender()->RenderTextContainer(m_aTitleContainers[Index], Color, NoOutline, CharX, CharY);
				Ui()->ClipDisable();
			}
		}
		X += m_aTitleWidths[Index];
	}
}

void CMenusStart::RenderStartMenu(CUIRect MainView)
{
	GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_START);

	// render the neon "Cr♱stal Client" title
	RenderNeonTitle(MainView.w / 2, 70.0f);

	// Cr1stal Client in-game update button
	{
		CCClient &CClient = GameClient()->m_CClient;
		const CCClient::EUpdateState State = CClient.UpdateState();
		if(State == CCClient::EUpdateState::AVAILABLE || State == CCClient::EUpdateState::FAILED ||
			State == CCClient::EUpdateState::DOWNLOADING || State == CCClient::EUpdateState::INSTALLING)
		{
			// small and quiet in the top right corner
			CUIRect UpdateButton;
			UpdateButton.w = 150.0f;
			UpdateButton.h = 18.0f;
			UpdateButton.x = MainView.x + MainView.w - UpdateButton.w - 12.0f;
			UpdateButton.y = MainView.y + 10.0f;
			char aLabel[128];
			if(State == CCClient::EUpdateState::DOWNLOADING || State == CCClient::EUpdateState::INSTALLING)
			{
				Ui()->RenderProgressBar(UpdateButton, CClient.UpdateProgress());
				str_format(aLabel, sizeof(aLabel), "%s %d%%", Localize("Downloading update"), round_to_int(CClient.UpdateProgress() * 100.0f));
				Ui()->DoLabel(&UpdateButton, aLabel, 9.0f, TEXTALIGN_MC);
			}
			else
			{
				if(State == CCClient::EUpdateState::FAILED)
					str_copy(aLabel, Localize("Update failed, try again"));
				else
					str_format(aLabel, sizeof(aLabel), "%s (#%d)", Localize("Update Cr1stal Client"), CClient.LatestBuild());
				static CButtonContainer s_UpdateButton;
				TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 0.7f));
				const bool Pressed = GameClient()->m_Menus.DoButton_Menu(&s_UpdateButton, aLabel, 0, &UpdateButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 9.0f, 0.35f, ColorRGBA(0.55f, 0.55f, 0.58f, 0.25f));
				TextRender()->TextColor(TextRender()->DefaultTextColor());
				if(Pressed)
					CClient.StartUpdate();
			}
		}
	}

	const float VMargin = MainView.w / 2 - 190.0f;

	CUIRect Button;
	int NewPage = -1;

	// small links in the bottom left corner
	{
		CUIRect Links;
		MainView.VSplitLeft(30.0f, nullptr, &Links);
		Links.VSplitLeft(110.0f, &Links, nullptr);
		Links.HSplitBottom(25.0f, &Links, nullptr);
		static CButtonContainer s_DiscordButton, s_LearnButton, s_TutorialButton, s_WebsiteButton;
		struct
		{
			CButtonContainer *m_pButton;
			const char *m_pText;
		} aLinks[] = {
			{&s_DiscordButton, Localize("Discord")},
			{&s_LearnButton, Localize("Learn")},
			{&s_TutorialButton, Localize("Tutorial")},
			{&s_WebsiteButton, Localize("Website")},
		};
		for(int i = 0; i < (int)std::size(aLinks); i++)
		{
			Links.HSplitBottom(18.0f, &Links, &Button);
			Links.HSplitBottom(3.0f, &Links, nullptr);
			const float HoverA = Ui()->HoverAnim(aLinks[i].m_pButton);
			Button.VSplitLeft(4.0f * HoverA, nullptr, &Button); // slides a little to the right
			SLabelProperties Props;
			Props.SetColor(ColorRGBA(0.62f + 0.38f * HoverA, 0.6f - 0.4f * HoverA, 0.65f - 0.35f * HoverA, 0.75f + 0.25f * HoverA));
			Ui()->DoLabel(&Button, aLinks[i].m_pText, 11.0f, TEXTALIGN_ML, Props);
			if(Ui()->DoButtonLogic(aLinks[i].m_pButton, 0, &Button, BUTTONFLAG_LEFT))
			{
				if(i == 0)
					Client()->ViewLink(Localize("https://ddnet.org/discord"));
				else if(i == 1)
					Client()->ViewLink(Localize("https://wiki.ddnet.org/"));
				else if(i == 2)
					GameClient()->m_Menus.JoinTutorial();
				else
					Client()->ViewLink("https://ddnet.org/");
			}
		}
	}

	// big play button under the title
	{
		static CButtonContainer s_PlayButton;
		const float HoverA = Ui()->HoverAnim(&s_PlayButton);
		const float Grow = 4.0f * HoverA;
		CUIRect Play;
		Play.w = 220.0f + Grow * 2.0f;
		Play.h = 42.0f + Grow;
		Play.x = MainView.w / 2.0f - Play.w / 2.0f;
		Play.y = 190.0f - Grow / 2.0f;
		// bright red so it stands out from the dark top half, a bit lighter on hover
		const float L = 0.1f * HoverA;
		Play.Draw4(ColorRGBA(1.0f, 0.24f + L, 0.38f + L, 1.0f), ColorRGBA(0.9f + L, 0.08f + L, 0.24f + L, 1.0f), ColorRGBA(0.86f + L, 0.08f + L, 0.24f + L, 1.0f), ColorRGBA(0.68f + L, 0.03f + L, 0.16f + L, 1.0f), IGraphics::CORNER_ALL, Play.h / 2.0f);
		SLabelProperties Props;
		TextRender()->TextOutlineColor(0.0f, 0.0f, 0.0f, 0.0f); // no dark edge around the glow
		CCStyle::NeonGlow([&](vec2 Offset, ColorRGBA Color) {
			CUIRect Shifted = Play;
			Shifted.x += Offset.x;
			Shifted.y += Offset.y;
			SLabelProperties GlowProps;
			GlowProps.SetColor(Color);
			Ui()->DoLabel(&Shifted, Localize("Play", "Start menu"), 17.0f + HoverA, TEXTALIGN_MC, GlowProps);
		},
			ColorRGBA(1.0f, 0.85f, 0.9f, 1.0f), CCStyle::NeonPulse(Client()->GlobalTime(), 1.0f) * (0.6f + 0.4f * HoverA), 1.2f);
		TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
		Props.SetColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
		Ui()->DoLabel(&Play, Localize("Play", "Start menu"), 17.0f + HoverA, TEXTALIGN_MC, Props);
		if(Ui()->DoButtonLogic(&s_PlayButton, 0, &Play, BUTTONFLAG_LEFT) || Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER) || CheckHotKey(KEY_P))
			NewPage = g_Config.m_UiPage >= CMenus::PAGE_INTERNET && g_Config.m_UiPage <= CMenus::PAGE_FAVORITE_COMMUNITY_5 ? g_Config.m_UiPage : CMenus::PAGE_INTERNET;
	}

	// dock at the bottom
	{
		const bool LocalServerRunning = GameClient()->m_LocalServer.IsServerRunning();
		enum
		{
			DOCK_PLAY = 0,
			DOCK_DEMOS,
			DOCK_EDITOR,
			DOCK_SERVER,
			DOCK_PROFILE,
			DOCK_NEWS,
			DOCK_SETTINGS,
			DOCK_QUIT,
			NUM_DOCK,
		};
		const char *apIcons[NUM_DOCK] = {FontIcon::PLAY, FontIcon::FILM, FontIcon::PEN_TO_SQUARE, LocalServerRunning ? FontIcon::STOP : FontIcon::NETWORK_WIRED, FontIcon::USER, FontIcon::NEWSPAPER, FontIcon::GEAR, FontIcon::POWER_OFF};
		const char *apLabels[NUM_DOCK] = {Localize("Play", "Start menu"), Localize("Demos"), Localize("Editor"), LocalServerRunning ? Localize("Stop server") : Localize("Run server"), Localize("Profile"), Localize("News"), Localize("Settings"), Localize("Quit")};
		const bool aHighlighted[NUM_DOCK] = {false, false, GameClient()->Editor()->HasUnsavedData(), LocalServerRunning, false, g_Config.m_UiUnreadNews != 0, false, false};
		const int Clicked = RenderDock(MainView, apIcons, apLabels, aHighlighted, NUM_DOCK);

		bool UsedEscape = false;
		if(Clicked == DOCK_DEMOS || CheckHotKey(KEY_D))
			NewPage = CMenus::PAGE_DEMOS;
		if(Clicked == DOCK_PLAY)
			NewPage = g_Config.m_UiPage >= CMenus::PAGE_INTERNET && g_Config.m_UiPage <= CMenus::PAGE_FAVORITE_COMMUNITY_5 ? g_Config.m_UiPage : CMenus::PAGE_INTERNET;
		if(Clicked == DOCK_EDITOR || CheckHotKey(KEY_E))
		{
			g_Config.m_ClEditor = 1;
			Input()->MouseModeRelative();
		}
		if(Clicked == DOCK_SERVER || CheckHotKey(KEY_R))
		{
			if(LocalServerRunning)
				GameClient()->m_LocalServer.KillServer();
			else
				GameClient()->m_LocalServer.RunServer({});
		}
		if(LocalServerRunning && CheckHotKey(KEY_J))
			GameClient()->m_LocalServer.Connect();
		if(Clicked == DOCK_PROFILE)
		{
			g_Config.m_UiSettingsPage = CMenus::SETTINGS_PLAYER;
			NewPage = CMenus::PAGE_SETTINGS;
		}
		if(Clicked == DOCK_NEWS || CheckHotKey(KEY_N))
			NewPage = CMenus::PAGE_NEWS;
		if(Clicked == DOCK_SETTINGS || CheckHotKey(KEY_S))
			NewPage = CMenus::PAGE_SETTINGS;
		if(Clicked == DOCK_QUIT || (UsedEscape = Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE)) || CheckHotKey(KEY_Q))
		{
			if(UsedEscape || GameClient()->Editor()->HasUnsavedData() || (GameClient()->CurrentRaceTime() / 60 >= g_Config.m_ClConfirmQuitTime && g_Config.m_ClConfirmQuitTime >= 0))
				GameClient()->m_Menus.ShowQuitPopup();
			else
				Client()->Quit();
		}
	}

	// render version
	CUIRect CurVersion, ConsoleButton;
	MainView.HSplitBottom(45.0f, nullptr, &CurVersion);
	CurVersion.VSplitRight(40.0f, &CurVersion, nullptr);
	CurVersion.HSplitTop(20.0f, &ConsoleButton, &CurVersion);
	CurVersion.HSplitTop(5.0f, nullptr, &CurVersion);
	ConsoleButton.VSplitRight(40.0f, nullptr, &ConsoleButton);
	// version in the corner, with a quiet credit to DDNet
	SLabelProperties VersionLabelProps;
	VersionLabelProps.SetColor(ColorRGBA(0.8f, 0.8f, 1.0f, 0.35f));
	Ui()->DoLabel(&CurVersion, "based on DDNet " GAME_RELEASE_VERSION, 10.0f, TEXTALIGN_MR, VersionLabelProps);

	static CButtonContainer s_ConsoleButton;
	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
	if(GameClient()->m_Menus.DoButton_Menu(&s_ConsoleButton, FontIcon::TERMINAL, 0, &ConsoleButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.1f)))
	{
		GameClient()->m_GameConsole.Toggle(CGameConsole::CONSOLETYPE_LOCAL);
	}
	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);

	CUIRect VersionUpdate;
	MainView.HSplitBottom(20.0f, nullptr, &VersionUpdate);
	VersionUpdate.VMargin(VMargin, &VersionUpdate);
#if defined(CONF_AUTOUPDATE)
	CUIRect UpdateButton;
	VersionUpdate.VSplitRight(100.0f, &VersionUpdate, &UpdateButton);
	VersionUpdate.VSplitRight(10.0f, &VersionUpdate, nullptr);

	char aBuf[128];
	const IUpdater::EUpdaterState State = Updater()->GetCurrentState();
	const bool NeedUpdate = str_comp(Client()->LatestVersion(), "0");

	if(State == IUpdater::CLEAN && NeedUpdate)
	{
		static CButtonContainer s_VersionUpdate;
		if(GameClient()->m_Menus.DoButton_Menu(&s_VersionUpdate, Localize("Update now"), 0, &UpdateButton, BUTTONFLAG_LEFT, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.07f)))
		{
			Updater()->InitiateUpdate();
		}
	}
	else if(State == IUpdater::NEED_RESTART)
	{
		static CButtonContainer s_VersionUpdate;
		if(GameClient()->m_Menus.DoButton_Menu(&s_VersionUpdate, Localize("Restart"), 0, &UpdateButton, BUTTONFLAG_LEFT, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.07f)))
		{
			Client()->Restart();
		}
	}
	else if(State >= IUpdater::GETTING_MANIFEST && State < IUpdater::NEED_RESTART)
	{
		Ui()->RenderProgressBar(UpdateButton, Updater()->GetCurrentPercent() / 100.0f);
	}

	if(State == IUpdater::CLEAN && NeedUpdate)
	{
		str_format(aBuf, sizeof(aBuf), Localize("DDNet %s is out!"), Client()->LatestVersion());
		TextRender()->TextColor(1.0f, 0.4f, 0.4f, 1.0f);
	}
	else if(State == IUpdater::CLEAN)
	{
		aBuf[0] = '\0';
	}
	else if(State >= IUpdater::GETTING_MANIFEST && State < IUpdater::NEED_RESTART)
	{
		char aCurrentFile[64];
		Updater()->GetCurrentFile(aCurrentFile, sizeof(aCurrentFile));
		str_format(aBuf, sizeof(aBuf), Localize("Downloading %s:"), aCurrentFile);
	}
	else if(State == IUpdater::FAIL)
	{
		str_copy(aBuf, Localize("Update failed! Check log…"));
		TextRender()->TextColor(1.0f, 0.4f, 0.4f, 1.0f);
	}
	else if(State == IUpdater::NEED_RESTART)
	{
		str_copy(aBuf, Localize("DDNet Client updated!"));
		TextRender()->TextColor(1.0f, 0.4f, 0.4f, 1.0f);
	}
	Ui()->DoLabel(&VersionUpdate, aBuf, 14.0f, TEXTALIGN_ML);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
#elif defined(CONF_INFORM_UPDATE)
	if(str_comp(Client()->LatestVersion(), "0") != 0)
	{
		CUIRect DownloadButton;
		VersionUpdate.VSplitRight(100.0f, &VersionUpdate, &DownloadButton);
		VersionUpdate.VSplitRight(10.0f, &VersionUpdate, nullptr);

		static CButtonContainer s_DownloadButton;
		if(GameClient()->m_Menus.DoButton_Menu(&s_DownloadButton, Localize("Download"), 0, &DownloadButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.07f)))
		{
			Client()->ViewLink("https://ddnet.org/downloads/");
		}

		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), Localize("DDNet %s is out!"), Client()->LatestVersion());
		SLabelProperties UpdateLabelProps;
		UpdateLabelProps.SetColor(ColorRGBA(1.0f, 0.4f, 0.4f, 1.0f));
		Ui()->DoLabel(&VersionUpdate, aBuf, 14.0f, TEXTALIGN_ML, UpdateLabelProps);
	}
#endif

	if(NewPage != -1)
	{
		GameClient()->m_Menus.SetShowStart(false);
		GameClient()->m_Menus.SetMenuPage(NewPage);
	}
}

int CMenusStart::RenderDock(const CUIRect &MainView, const char *const *ppIcons, const char *const *ppLabels, const bool *pHighlighted, int Num)
{
	static CButtonContainer s_aButtons[16];
	static char s_aLabelKeys[16]; // own addresses for the label fade
	static char s_DockKey;
	Num = std::min(Num, (int)std::size(s_aButtons));

	const float BaseSize = 30.0f;
	const float Gap = 7.0f;
	const float Pad = 7.0f;
	const float MaxGrow = 0.5f; // the icon under the mouse becomes 50% bigger
	const float Reach = 52.0f; // how far the growing spreads to the neighbours
	const float BottomY = MainView.y + MainView.h - 22.0f;

	// the dock without growing, to know where the mouse is relative to the icons
	const float BaseWidth = Num * BaseSize + (Num - 1) * Gap;
	const float BaseX = MainView.x + MainView.w / 2.0f - BaseWidth / 2.0f;
	CUIRect Panel;
	Panel.x = BaseX - Pad;
	Panel.y = BottomY - BaseSize - Pad;
	Panel.w = BaseWidth + Pad * 2.0f;
	Panel.h = BaseSize + Pad * 2.0f;
	// the grown icons stick out above the panel, they still count as the dock
	CUIRect HoverZone = Panel;
	HoverZone.y -= BaseSize * MaxGrow + 6.0f;
	HoverZone.h += BaseSize * MaxGrow + 6.0f;
	HoverZone.x -= BaseSize * MaxGrow;
	HoverZone.w += BaseSize * MaxGrow * 2.0f;
	const float DockA = Ui()->StateAnim(&s_DockKey, Ui()->MouseInside(&HoverZone) && !Ui()->IsPopupOpen(), 10.0f);
	const float MouseX = Ui()->MouseX();

	float aScale[16];
	float Total = 0.0f;
	for(int i = 0; i < Num; i++)
	{
		const float Center = BaseX + i * (BaseSize + Gap) + BaseSize / 2.0f;
		const float Dist = (MouseX - Center) / Reach;
		aScale[i] = 1.0f + MaxGrow * DockA * std::exp(-Dist * Dist);
		Total += BaseSize * aScale[i];
	}
	Total += (Num - 1) * Gap;

	// panel grows with the icons, like on a Mac
	Panel.w = Total + Pad * 2.0f;
	Panel.x = MainView.x + MainView.w / 2.0f - Panel.w / 2.0f;
	CCStyle::DrawCard(Panel, 12.0f, ColorRGBA(0.07f, 0.065f, 0.075f, 0.92f));

	int Clicked = -1;
	float X = Panel.x + Pad;
	for(int i = 0; i < Num; i++)
	{
		const float Size = BaseSize * aScale[i];
		CUIRect Icon;
		Icon.x = X;
		Icon.y = BottomY - Size;
		Icon.w = Size;
		Icon.h = Size;
		X += Size + Gap;

		const float HoverA = Ui()->HoverAnim(&s_aButtons[i]);
		CCStyle::DrawCard(Icon, 7.0f + 2.0f * (aScale[i] - 1.0f), ColorRGBA(0.1f, 0.095f, 0.105f, 1.0f), HoverA);
		if(pHighlighted[i])
		{
			// small dot under an icon that wants attention
			CUIRect Dot;
			Dot.w = Dot.h = 3.0f;
			Dot.x = Icon.x + Icon.w / 2.0f - 1.5f;
			Dot.y = Icon.y + Icon.h + 2.5f;
			Dot.Draw(CCStyle::ACCENT, IGraphics::CORNER_ALL, 1.5f);
		}

		TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
		SLabelProperties IconProps;
		IconProps.SetColor(CCStyle::Mix(ColorRGBA(0.85f, 0.12f, 0.3f, 0.85f), ColorRGBA(1.0f, 0.38f, 0.5f, 1.0f), HoverA));
		TextRender()->TextOutlineColor(0.0f, 0.0f, 0.0f, 0.0f); // no dark edge around the glow
		CCStyle::NeonGlow([&](vec2 Offset, ColorRGBA Color) {
			CUIRect Shifted = Icon;
			Shifted.x += Offset.x;
			Shifted.y += Offset.y;
			SLabelProperties GlowProps;
			GlowProps.SetColor(Color);
			Ui()->DoLabel(&Shifted, ppIcons[i], Size * 0.42f, TEXTALIGN_MC, GlowProps);
		},
			ColorRGBA(1.0f, 0.15f, 0.32f, 1.0f), (0.45f + 0.55f * HoverA) * CCStyle::NeonPulse(Client()->GlobalTime(), i * 0.7f), 1.0f);
		TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
		Ui()->DoLabel(&Icon, ppIcons[i], Size * 0.42f, TEXTALIGN_MC, IconProps);
		TextRender()->SetRenderFlags(0);
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);

		// label above the icon, fades in and rises a little
		const float LabelA = Ui()->StateAnim(&s_aLabelKeys[i], Ui()->HotItem() == &s_aButtons[i], 14.0f);
		if(LabelA > 0.01f)
		{
			const float FontSize = 8.5f;
			const std::string Text = CCStyle::Upper(ppLabels[i]);
			CUIRect Label;
			Label.w = TextRender()->TextWidth(FontSize, Text.c_str()) + 12.0f;
			Label.h = 14.0f;
			Label.x = Icon.x + Icon.w / 2.0f - Label.w / 2.0f;
			Label.y = Icon.y - Label.h - 5.0f - 4.0f * LabelA;
			Label.Draw(ColorRGBA(0.12f, 0.03f, 0.06f, 0.95f * LabelA), IGraphics::CORNER_ALL, 4.0f);
			TextRender()->TextOutlineColor(0.0f, 0.0f, 0.0f, 0.0f); // no dark edge around the glow
			CCStyle::NeonGlow([&](vec2 Offset, ColorRGBA Color) {
				CUIRect Shifted = Label;
				Shifted.x += Offset.x;
				Shifted.y += Offset.y;
				SLabelProperties GlowProps;
				GlowProps.SetColor(Color);
				Ui()->DoLabel(&Shifted, Text.c_str(), FontSize, TEXTALIGN_MC, GlowProps);
			},
				CCStyle::ACCENT_TEXT, LabelA * CCStyle::NeonPulse(Client()->GlobalTime(), i * 0.5f), 0.9f);
			TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
			SLabelProperties Props;
			Props.SetColor(CCStyle::ACCENT_TEXT.WithAlpha(LabelA));
			Ui()->DoLabel(&Label, Text.c_str(), FontSize, TEXTALIGN_MC, Props);
		}

		if(Ui()->DoButtonLogic(&s_aButtons[i], 0, &Icon, BUTTONFLAG_LEFT))
			Clicked = i;
	}
	return Clicked;
}

bool CMenusStart::CheckHotKey(int Key) const
{
	return !Input()->ShiftIsPressed() && !Input()->ModifierIsPressed() && !Input()->AltIsPressed() && // no modifier
	       Input()->KeyPress(Key) &&
	       !GameClient()->m_GameConsole.IsActive();
}
