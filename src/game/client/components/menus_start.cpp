/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "menus_start.h"

#include <algorithm>

#include <engine/client/updater.h>
#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <generated/client_data.h>

// bestclient
#include <game/client/bc_menu_badges.h>
#include <game/client/components/bestclient/version.h>
// bestclient
#include <game/client/cclient_version.h>
#include <game/client/components/cclient_neon.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/localization.h>
#include <game/version.h>

#if defined(CONF_PLATFORM_ANDROID)
#include <android/android_main.h>
#endif

void CMenusStart::RenderStartMenu(CUIRect MainView)
{
	if(CCNeon::On())
		RenderStartMenuNeon(MainView);
	else
		RenderStartMenuClassic(MainView);
}

void CMenusStart::RenderStartMenuClassic(CUIRect MainView)
{
	GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_START);

	// Cr1stal Client: small update button in the top right corner
	{
		CCUpdate &Update = GameClient()->m_CUpdate;
		const CCUpdate::EState State = Update.State();
		if(State == CCUpdate::EState::AVAILABLE || State == CCUpdate::EState::FAILED ||
			State == CCUpdate::EState::DOWNLOADING || State == CCUpdate::EState::INSTALLING)
		{
			CUIRect UpdateButton;
			UpdateButton.w = 170.0f;
			UpdateButton.h = 20.0f;
			UpdateButton.x = MainView.x + MainView.w - UpdateButton.w - 12.0f;
			UpdateButton.y = MainView.y + 10.0f;
			char aLabel[128];
			if(State == CCUpdate::EState::DOWNLOADING || State == CCUpdate::EState::INSTALLING)
			{
				Ui()->RenderProgressBar(UpdateButton, Update.Progress());
				str_format(aLabel, sizeof(aLabel), "%s %d%%", Localize("Downloading update"), round_to_int(Update.Progress() * 100.0f));
				Ui()->DoLabel(&UpdateButton, aLabel, 10.0f, TEXTALIGN_MC);
			}
			else
			{
				if(State == CCUpdate::EState::FAILED)
					str_copy(aLabel, Localize("Update failed, try again"));
				else
					str_format(aLabel, sizeof(aLabel), "%s (#%d)", Localize("Update Cr1stal Client"), Update.LatestBuild());
				static CButtonContainer s_UpdateButton;
				if(GameClient()->m_Menus.DoButton_Menu(&s_UpdateButton, aLabel, 0, &UpdateButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.2f, 0.6f, 0.3f, 0.35f)))
					Update.Start();
			}
		}
	}

	const float Rounding = 10.0f;
	const float VMargin = MainView.w / 2 - 190.0f;
	const float ExtMenuBottomOffset = 40.0f;

	CUIRect Button;
	int NewPage = -1;

	const auto SetIconMode = [&]() {
		TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
	};
	const auto ResetIconMode = [&]() {
		TextRender()->SetRenderFlags(0);
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
	};

	CUIRect ExtMenu;
	MainView.VSplitLeft(30.0f, nullptr, &ExtMenu);
	ExtMenu.VSplitLeft(100.0f, &ExtMenu, nullptr);
	ExtMenu.HSplitBottom(ExtMenuBottomOffset, &ExtMenu, nullptr);

	ExtMenu.HSplitBottom(20.0f, &ExtMenu, &Button);
	static CButtonContainer s_StatsButton;
	if(GameClient()->m_Menus.DoButton_Menu(&s_StatsButton, Localize("Statistics"), 0, &Button, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f))) // Cr1stal Client
		NewPage = CMenus::PAGE_CC_STATS;

	constexpr float LogoW = 360.0f;
	constexpr float LogoH = 103.0f;
	constexpr float LogoGap = 30.0f;
	constexpr float ButtonH = 40.0f;
	constexpr float ButtonGap = 5.0f;
	constexpr int MenuButtonCount = 5;
	const float ButtonsH = MenuButtonCount * ButtonH + (MenuButtonCount - 1) * ButtonGap;
	const float TotalBlockH = LogoH + LogoGap + ButtonsH;

	CUIRect Menu;
	MainView.VMargin(VMargin, &Menu);

	const float BlockStartY = MainView.y + (MainView.h - TotalBlockH) / 2.0f - 50.0f;

	{
		const float LogoX = MainView.x + MainView.w / 2.0f - LogoW / 2.0f;
		const IGraphics::CTextureHandle &LogoTexture = GameClient()->m_Menus.BcLogoTexture();
		Graphics()->TextureSet(LogoTexture.IsValid() && !LogoTexture.IsNullTexture() ? LogoTexture : g_pData->m_aImages[IMAGE_BANNER].m_Id);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1, 1, 1, 1);
		IGraphics::CQuadItem LogoQuad(LogoX, BlockStartY, LogoW, LogoH);
		Graphics()->QuadsDrawTL(&LogoQuad, 1);
		Graphics()->QuadsEnd();
	}

	const auto ScaleButtonRect = [](const CUIRect &Base, float Scale) -> CUIRect {
		CUIRect Out = Base;
		Out.w *= Scale;
		Out.h *= Scale;
		Out.x = Base.x + (Base.w - Out.w) * 0.5f;
		Out.y = Base.y + (Base.h - Out.h) * 0.5f;
		return Out;
	};

	CUIRect aMenuButtons[MenuButtonCount];
	{
		float Y = BlockStartY + LogoH + LogoGap;
		for(int i = 0; i < MenuButtonCount; ++i)
		{
			aMenuButtons[i] = {Menu.x, Y, Menu.w, ButtonH};
			Y += ButtonH + ButtonGap;
		}
	}

	static float s_aMenuButtonScale[MenuButtonCount] = {};
	static bool s_MenuButtonScaleInit = false;
	if(!s_MenuButtonScaleInit)
	{
		for(int i = 0; i < MenuButtonCount; ++i)
			s_aMenuButtonScale[i] = 1.0f;
		s_MenuButtonScaleInit = true;
	}

	{
		const bool AnimEnabled = g_Config.m_BcMainMenuAnimation != 0;
		int HoveredIndex = -1;
		if(AnimEnabled)
		{
			for(int i = 0; i < MenuButtonCount; ++i)
			{
				const CUIRect Scaled = ScaleButtonRect(aMenuButtons[i], s_aMenuButtonScale[i]);
				if(Ui()->MouseHovered(&Scaled))
				{
					HoveredIndex = i;
					break;
				}
			}
		}
		const bool AnyHovered = HoveredIndex != -1;
		const float HoverScale = 1.08f;
		const float OtherScale = 0.94f;
		const float Speed = (float)g_Config.m_BcMainMenuAnimationSpeed;
		const float Blend = AnimEnabled ? std::clamp(Client()->RenderFrameTime() * Speed, 0.0f, 1.0f) : 1.0f;
		for(int i = 0; i < MenuButtonCount; ++i)
		{
			const float Target = (AnimEnabled && AnyHovered) ? (i == HoveredIndex ? HoverScale : OtherScale) : 1.0f;
			s_aMenuButtonScale[i] += (Target - s_aMenuButtonScale[i]) * Blend;
		}
	}

	{
		CUIRect ScaledButton = ScaleButtonRect(aMenuButtons[0], s_aMenuButtonScale[0]);
		static CButtonContainer s_PlayButton;
		if((GameClient()->m_Menus.DoButton_MenuEx(&s_PlayButton, Localize("Play", "Start menu"), 0, &ScaledButton, BUTTONFLAG_LEFT, g_Config.m_ClShowStartMenuImages ? "play_game" : nullptr, IGraphics::CORNER_ALL, Rounding, 0.5f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), true) || Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER) || CheckHotKey(KEY_P)))
			NewPage = g_Config.m_UiPage >= CMenus::PAGE_INTERNET && g_Config.m_UiPage <= CMenus::PAGE_FAVORITE_COMMUNITY_5 ? g_Config.m_UiPage : CMenus::PAGE_INTERNET;
	}

	{
		CUIRect ScaledButton = ScaleButtonRect(aMenuButtons[1], s_aMenuButtonScale[1]);
		static CButtonContainer s_DemoButton;
		if((GameClient()->m_Menus.DoButton_MenuEx(&s_DemoButton, Localize("Demos"), 0, &ScaledButton, BUTTONFLAG_LEFT, g_Config.m_ClShowStartMenuImages ? "demos" : nullptr, IGraphics::CORNER_ALL, Rounding, 0.5f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), true) || CheckHotKey(KEY_D)))
			NewPage = CMenus::PAGE_DEMOS;
	}

	{
		CUIRect ScaledButton = ScaleButtonRect(aMenuButtons[2], s_aMenuButtonScale[2]);
		static CButtonContainer s_MapEditorButton;
		if((GameClient()->m_Menus.DoButton_MenuEx(&s_MapEditorButton, Localize("Editor"), 0, &ScaledButton, BUTTONFLAG_LEFT, g_Config.m_ClShowStartMenuImages ? "editor" : nullptr, IGraphics::CORNER_ALL, Rounding, 0.5f, GameClient()->Editor()->HasUnsavedData() ? ColorRGBA(0.0f, 1.0f, 0.0f, 0.25f) : ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), true) || CheckHotKey(KEY_E)))
		{
			g_Config.m_ClEditor = 1;
			Input()->MouseModeRelative();
		}
	}

	{
		CUIRect ScaledButton = ScaleButtonRect(aMenuButtons[3], s_aMenuButtonScale[3]);
		static CButtonContainer s_LocalServerButton;
		const bool LocalServerRunning = GameClient()->m_LocalServer.IsServerRunning();
		if((GameClient()->m_Menus.DoButton_MenuEx(&s_LocalServerButton, LocalServerRunning ? Localize("Stop server") : Localize("Run server"), 0, &ScaledButton, BUTTONFLAG_LEFT, g_Config.m_ClShowStartMenuImages ? "local_server" : nullptr, IGraphics::CORNER_ALL, Rounding, 0.5f, LocalServerRunning ? ColorRGBA(0.0f, 1.0f, 0.0f, 0.25f) : ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), true) || (CheckHotKey(KEY_R) && Input()->KeyPress(KEY_R))))
		{
			if(LocalServerRunning)
				GameClient()->m_LocalServer.KillServer();
			else
				GameClient()->m_LocalServer.RunServer({});
		}
	}

	const CUIRect SettingsButton = aMenuButtons[4];
	{
		CUIRect ScaledButton = ScaleButtonRect(aMenuButtons[4], s_aMenuButtonScale[4]);
		static CButtonContainer s_SettingsButton;
		if(GameClient()->m_Menus.DoButton_MenuEx(&s_SettingsButton, Localize("Settings"), 0, &ScaledButton, BUTTONFLAG_LEFT, g_Config.m_ClShowStartMenuImages ? "settings" : nullptr, IGraphics::CORNER_ALL, Rounding, 0.5f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), true) || CheckHotKey(KEY_S))
			NewPage = CMenus::PAGE_SETTINGS;
	}

#if defined(CONF_AUTOUPDATE)
	{
		char aUpdateBuf[128] = "";
		const IUpdater::EUpdaterState State = Updater()->GetCurrentState();
		const bool NeedUpdate = Updater()->GetLatestVersionString()[0] != '\0';
		const bool ShowDownloadButton = State == IUpdater::VERSION_AVAILABLE;
		const bool ShowRetryButton = NeedUpdate && State == IUpdater::FAIL;
		const bool ShowRestartButton = State == IUpdater::NEED_RESTART;
		const bool ShowUpdateProgress = State == IUpdater::DOWNLOADING;

		if(ShowDownloadButton || ShowRetryButton || ShowRestartButton || ShowUpdateProgress)
		{
			CUIRect UpdateRow = SettingsButton;
			UpdateRow.y += SettingsButton.h + ButtonGap;
			UpdateRow.h = 22.0f;

			// bestclient
			CUIRect UpdateLabel, UpdateButton, ChangelogButton{};
			const bool ShowChangelogButton = ShowDownloadButton || ShowRetryButton;
			if(ShowChangelogButton)
			{
				const float ButtonFont = 14.0f;
				const float DownloadW = std::max(90.0f, TextRender()->TextWidth(ButtonFont, Localize("Download")) + 16.0f);
				const float ChangelogW = std::max(90.0f, TextRender()->TextWidth(ButtonFont, BcLocalize("Changelog")) + 16.0f);
				UpdateRow.VSplitRight(DownloadW, &UpdateRow, &UpdateButton);
				UpdateRow.VSplitRight(6.0f, &UpdateRow, nullptr);
				UpdateRow.VSplitRight(ChangelogW, &UpdateLabel, &ChangelogButton);
			}
			else
			{
				UpdateRow.VSplitRight(120.0f, &UpdateLabel, &UpdateButton);
			}
			// bestclient
			UpdateLabel.VSplitRight(10.0f, &UpdateLabel, nullptr);

			if(ShowDownloadButton)
			{
				str_format(aUpdateBuf, sizeof(aUpdateBuf), Localize("Update %s is available!"), Updater()->GetLatestVersionString());
				TextRender()->TextColor(1.0f, 0.4f, 0.4f, 1.0f);
			}
			else if(ShowUpdateProgress)
			{
				if(State == IUpdater::GETTING_MANIFEST)
					str_copy(aUpdateBuf, Localize("Preparing update..."));
				else
					str_format(aUpdateBuf, sizeof(aUpdateBuf), Localize("Downloading %d%%"), Updater()->GetCurrentPercent());
			}
			else if(ShowRetryButton)
			{
				str_copy(aUpdateBuf, Localize("Update failed"));
				TextRender()->TextColor(1.0f, 0.4f, 0.4f, 1.0f);
			}
			else if(ShowRestartButton)
			{
				str_copy(aUpdateBuf, Localize("Update downloaded"));
				TextRender()->TextColor(0.7f, 1.0f, 0.7f, 1.0f);
			}

			Ui()->DoLabel(&UpdateLabel, aUpdateBuf, 14.0f, TEXTALIGN_ML);
			TextRender()->TextColor(TextRender()->DefaultTextColor());

			if(ShowDownloadButton || ShowRetryButton)
			{
				static CButtonContainer s_MenuUpdateDownload;
				if(GameClient()->m_Menus.DoButton_Menu(&s_MenuUpdateDownload, Localize("Download"), 0, &UpdateButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
					Updater()->InitiateUpdate();
			}
			else if(ShowRestartButton)
			{
				static CButtonContainer s_MenuUpdateRestart;
#if defined(CONF_PLATFORM_ANDROID)
				const char *pRestartButtonLabel = Localize("Install");
#else
				const char *pRestartButtonLabel = Localize("Restart");
#endif
				if(GameClient()->m_Menus.DoButton_Menu(&s_MenuUpdateRestart, pRestartButtonLabel, 0, &UpdateButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
					Updater()->ApplyUpdateAndRestart();
			}
			else
			{
				Ui()->RenderProgressBar(UpdateButton, Updater()->GetCurrentPercent() / 100.0f);
			}
		}
	}
#endif

	{
		CUIRect QuitArea;
		MainView.VMargin(VMargin, &QuitArea);
		QuitArea.HSplitBottom(25.0f, &QuitArea, nullptr);
		QuitArea.HSplitBottom(40.0f, &QuitArea, &Button);
		CUIRect QuitButton = Button;
		QuitButton.w = QuitButton.h;
		QuitButton.x += (Button.w - QuitButton.w) / 2.0f;
		static CButtonContainer s_QuitButton;
		bool UsedEscape = false;
		SetIconMode();
		if(GameClient()->m_Menus.DoButton_Menu(&s_QuitButton, FontIcon::POWER_OFF, 0, &QuitButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, Rounding, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)) || (UsedEscape = Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE)) || CheckHotKey(KEY_Q))
		{
			ResetIconMode();
			if(g_Config.m_BcConfirmQuit || UsedEscape || GameClient()->Editor()->HasUnsavedData() || (GameClient()->CurrentRaceTime() / 60 >= g_Config.m_ClConfirmQuitTime && g_Config.m_ClConfirmQuitTime >= 0))
				GameClient()->m_Menus.ShowQuitPopup();
			else
				Client()->Quit();
		}
		ResetIconMode();
	}

	{
		CUIRect CurVersion, ConsoleButton;
		MainView.HSplitBottom(74.0f, nullptr, &CurVersion);
		CurVersion.VSplitRight(40.0f, &CurVersion, nullptr);
		CurVersion.HSplitTop(20.0f, &ConsoleButton, &CurVersion);
		CurVersion.HSplitTop(5.0f, nullptr, &CurVersion);
		ConsoleButton.VSplitRight(40.0f, nullptr, &ConsoleButton);

		CUIRect VersionLine1, VersionLine2, VersionLine3;
		CurVersion.HSplitTop(16.0f, &VersionLine1, &CurVersion);
		CurVersion.HSplitTop(2.0f, nullptr, &CurVersion);
		CurVersion.HSplitTop(16.0f, &VersionLine2, &CurVersion);
		CurVersion.HSplitTop(2.0f, nullptr, &CurVersion);
		CurVersion.HSplitTop(16.0f, &VersionLine3, &CurVersion);

		char aDDNetBuf[64];
		char aCrystalBuf[64];
		str_format(aDDNetBuf, sizeof(aDDNetBuf), "DDNet %s", GAME_RELEASE_VERSION);
		str_format(aCrystalBuf, sizeof(aCrystalBuf), "Cr\xe2\x99\xb1stal Client %d", (int)CCLIENT_BUILD);
		(void)VersionLine1;
		Ui()->DoLabel(&VersionLine2, aDDNetBuf, 14.0f, TEXTALIGN_MR);
		Ui()->DoLabel(&VersionLine3, aCrystalBuf, 14.0f, TEXTALIGN_MR);

		static CButtonContainer s_ConsoleButton;
		SetIconMode();
		if(GameClient()->m_Menus.DoButton_Menu(&s_ConsoleButton, FontIcon::TERMINAL, 0, &ConsoleButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.1f)))
			GameClient()->m_GameConsole.Toggle(CGameConsole::CONSOLETYPE_LOCAL);
		ResetIconMode();
	}

	if(NewPage != -1)
	{
		GameClient()->m_Menus.SetShowStart(false);
		GameClient()->m_Menus.SetMenuPage(NewPage);
	}
}

// Cr1stal Client: the main menu of the new design
void CMenusStart::RenderStartMenuNeon(CUIRect MainView)
{
	GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_START);
	CMenus &Menus = GameClient()->m_Menus;

	const auto SetIconMode = [&]() {
		TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
	};
	const auto ResetIconMode = [&]() {
		TextRender()->SetRenderFlags(0);
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
	};
	const auto SetLabelColor = [&](ColorRGBA Color) {
		TextRender()->TextColor(Color);
	};
	const auto ResetLabelColor = [&]() {
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	};

	int NewPage = -1;

	const float Left = MainView.x + 14.0f;
	const float Right = MainView.x + MainView.w - 14.0f;
	const float Top = MainView.y + 8.0f;
	const float ColumnW = 190.0f;

	// logo with its own neon glow (the glow is part of the picture)
	constexpr float LogoW = 214.0f;
	constexpr float LogoH = LogoW * 879.0f / 3072.0f;
	{
		const IGraphics::CTextureHandle &LogoTexture = Menus.BcLogoTexture();
		Graphics()->TextureSet(LogoTexture.IsValid() && !LogoTexture.IsNullTexture() ? LogoTexture : g_pData->m_aImages[IMAGE_BANNER].m_Id);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1, 1, 1, 1);
		IGraphics::CQuadItem LogoQuad(Left - 6.0f, Top + 6.0f, LogoW, LogoH);
		Graphics()->QuadsDrawTL(&LogoQuad, 1);
		Graphics()->QuadsEnd();
	}

	// the main buttons
	struct SEntry
	{
		const char *m_pLabel;
		const char *m_pIcon;
		int m_Key;
		bool m_Primary;
		bool m_Lit;
	};
	const bool LocalServerRunning = GameClient()->m_LocalServer.IsServerRunning();
	const SEntry aEntries[] = {
		{Localize("Play", "Start menu"), FontIcon::PLAY, KEY_P, true, false},
		{Localize("Demos"), FontIcon::CLAPPERBOARD, KEY_D, false, false},
		{Localize("Editor"), FontIcon::PEN_TO_SQUARE, KEY_E, false, GameClient()->Editor()->HasUnsavedData()},
		{Localize("Statistics"), FontIcon::FLAG_CHECKERED, KEY_UNKNOWN, false, false},
		{LocalServerRunning ? Localize("Stop server") : Localize("Run server"), FontIcon::NETWORK_WIRED, KEY_R, false, LocalServerRunning},
		{Localize("Settings"), FontIcon::GEAR, KEY_S, false, false},
	};
	constexpr int NumEntries = sizeof(aEntries) / sizeof(aEntries[0]);
	static CButtonContainer s_aButtons[NumEntries];

	const float ButtonH = 22.0f;
	const float ButtonGap = 4.0f;
	float Y = Top + 6.0f + LogoH + 12.0f;
	const float ButtonsTop = Y;
	for(int i = 0; i < NumEntries; ++i)
	{
		const SEntry &Entry = aEntries[i];
		CUIRect Row;
		Row.x = Left;
		Row.y = Y;
		Row.w = ColumnW;
		Row.h = ButtonH;
		Y += ButtonH + ButtonGap;

		const bool Hot = Ui()->HotItem() == &s_aButtons[i];
		const bool Active = Ui()->CheckActiveItem(&s_aButtons[i]);
		if(Entry.m_Primary)
			CCNeon::DrawPrimary(Row, Hot, Active, IGraphics::CORNER_ALL, 5.0f);
		else if(Entry.m_Lit)
			CCNeon::DrawSelected(Row, 5.0f);
		else
			CCNeon::DrawSecondary(Row, Hot, Active, IGraphics::CORNER_ALL, 5.0f);

		CUIRect IconRect, TextRect;
		Row.VSplitLeft(ButtonH + 2.0f, &IconRect, &TextRect);
		SetIconMode();
		SetLabelColor(Entry.m_Primary ? CCNeon::TEXT : (Entry.m_Lit || Hot ? CCNeon::ACCENT_TEXT : CCNeon::TEXT_SUB));
		Ui()->DoLabel(&IconRect, Entry.m_pIcon, ButtonH * 0.46f, TEXTALIGN_MC);
		ResetIconMode();
		SetLabelColor(CCNeon::TEXT);
		Ui()->DoLabel(&TextRect, Entry.m_pLabel, ButtonH * 0.52f, TEXTALIGN_ML);
		ResetLabelColor();

		bool Pressed = Ui()->DoButtonLogic(&s_aButtons[i], 0, &Row, BUTTONFLAG_LEFT) != 0;
		if(i == 0)
			Pressed = Pressed || Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER) || CheckHotKey(KEY_P);
		else if(Entry.m_Key == KEY_R)
			Pressed = Pressed || (CheckHotKey(KEY_R) && Input()->KeyPress(KEY_R));
		else if(Entry.m_Key != KEY_UNKNOWN)
			Pressed = Pressed || CheckHotKey(Entry.m_Key);
		if(!Pressed)
			continue;

		switch(i)
		{
		case 0:
			NewPage = g_Config.m_UiPage >= CMenus::PAGE_INTERNET && g_Config.m_UiPage <= CMenus::PAGE_FAVORITE_COMMUNITY_5 ? g_Config.m_UiPage : CMenus::PAGE_INTERNET;
			break;
		case 1:
			NewPage = CMenus::PAGE_DEMOS;
			break;
		case 2:
			g_Config.m_ClEditor = 1;
			Input()->MouseModeRelative();
			break;
		case 3:
			NewPage = CMenus::PAGE_CC_STATS;
			break;
		case 4:
			if(LocalServerRunning)
				GameClient()->m_LocalServer.KillServer();
			else
				GameClient()->m_LocalServer.RunServer({});
			break;
		default:
			NewPage = CMenus::PAGE_SETTINGS;
			break;
		}
	}

	// cards on the right: last server and your profile
	const float CardX = Left + ColumnW + 30.0f;
	const float CardW = Right - CardX;
	float CardY = ButtonsTop;
	const float BottomLimit = MainView.y + MainView.h - 34.0f;

	const auto DrawCardTitle = [&](const CUIRect &Card, const char *pTitle) {
		CUIRect Title;
		Card.Margin(8.0f, &Title);
		Title.h = 12.0f;
		SetLabelColor(CCNeon::ACCENT_TEXT);
		Ui()->DoLabel(&Title, pTitle, 10.0f, TEXTALIGN_ML);
		ResetLabelColor();
	};

	const bool HasLastServer = g_Config.m_UiServerAddress[0] != '\0' && str_comp(g_Config.m_UiServerAddress, "localhost:8303") != 0;
	if(HasLastServer)
	{
		CUIRect Card;
		Card.x = CardX;
		Card.y = CardY;
		Card.w = CardW;
		Card.h = 62.0f;
		CCNeon::DrawFramed(Card, CCNeon::CARD, CCNeon::LINE, CCNeon::Scaled(8.0f, Card));
		DrawCardTitle(Card, Localize("Quick join"));

		CUIRect Body, AddressRect, ButtonRect;
		Card.Margin(8.0f, &Body);
		Body.HSplitTop(14.0f, nullptr, &Body);
		Body.HSplitTop(14.0f, &AddressRect, &Body);
		Ui()->DoLabel(&AddressRect, g_Config.m_UiServerAddress, 13.0f, TEXTALIGN_ML);
		Body.HSplitBottom(18.0f, nullptr, &ButtonRect);
		static CButtonContainer s_JoinButton;
		const bool Hot = Ui()->HotItem() == &s_JoinButton;
		const bool Active = Ui()->CheckActiveItem(&s_JoinButton);
		CCNeon::DrawPrimary(ButtonRect, Hot, Active, IGraphics::CORNER_ALL, 5.0f);
		SetLabelColor(CCNeon::TEXT);
		Ui()->DoLabel(&ButtonRect, Localize("Connect"), 12.0f, TEXTALIGN_MC);
		ResetLabelColor();
		if(Ui()->DoButtonLogic(&s_JoinButton, 0, &ButtonRect, BUTTONFLAG_LEFT))
			Client()->Connect(g_Config.m_UiServerAddress);
		CardY += Card.h + 8.0f;
	}

	{
		CCPlaytime &Playtime = GameClient()->m_CPlaytime;
		CUIRect Card;
		Card.x = CardX;
		Card.y = CardY;
		Card.w = CardW;
		Card.h = std::min(78.0f, std::max(0.0f, BottomLimit - CardY));
		if(Card.h >= 60.0f)
		{
			static CButtonContainer s_ProfileCard;
			const bool Hot = Ui()->HotItem() == &s_ProfileCard;
			CCNeon::DrawFramed(Card, Hot ? CCNeon::ELEVATED : CCNeon::CARD, Hot ? ColorRGBA(1.0f, 1.0f, 1.0f, 0.14f) : CCNeon::LINE, CCNeon::Scaled(8.0f, Card));
			DrawCardTitle(Card, Localize("Profile"));

			CUIRect Body;
			Card.Margin(8.0f, &Body);
			Body.HSplitTop(16.0f, nullptr, &Body);

			const int64_t Seconds = Playtime.Total();
			char aTime[64];
			str_format(aTime, sizeof(aTime), Localize("%d h %d min"), (int)(Seconds / 3600), (int)((Seconds / 60) % 60));
			char aPoints[32];
			str_format(aPoints, sizeof(aPoints), "%d", Playtime.Points());
			char aMaps[32];
			str_format(aMaps, sizeof(aMaps), "%d", Playtime.NumFinished());

			const char *apLabels[3] = {Localize("Play time"), Localize("Points"), Localize("Maps finished")};
			const char *apValues[3] = {aTime, aPoints, aMaps};
			for(int i = 0; i < 3; ++i)
			{
				CUIRect Line, Label, Value;
				Body.HSplitTop(14.0f, &Line, &Body);
				Line.VSplitRight(Line.w * 0.45f, &Label, &Value);
				SetLabelColor(CCNeon::TEXT_SUB);
				Ui()->DoLabel(&Label, apLabels[i], 11.0f, TEXTALIGN_ML);
				SetLabelColor(CCNeon::TEXT);
				Ui()->DoLabel(&Value, apValues[i], 11.0f, TEXTALIGN_MR);
				ResetLabelColor();
			}
			if(Ui()->DoButtonLogic(&s_ProfileCard, 0, &Card, BUTTONFLAG_LEFT))
				NewPage = CMenus::PAGE_CC_STATS;
		}
	}

	// update: one pill in the top right corner
	{
		CCUpdate &Update = GameClient()->m_CUpdate;
		const CCUpdate::EState State = Update.State();
		if(State == CCUpdate::EState::AVAILABLE || State == CCUpdate::EState::FAILED ||
			State == CCUpdate::EState::DOWNLOADING || State == CCUpdate::EState::INSTALLING)
		{
			CUIRect Pill;
			Pill.w = 128.0f;
			Pill.h = 20.0f;
			Pill.x = Right - Pill.w;
			Pill.y = Top + 4.0f;
			char aLabel[128];
			static CButtonContainer s_UpdatePill;
			if(State == CCUpdate::EState::DOWNLOADING || State == CCUpdate::EState::INSTALLING)
			{
				CCNeon::DrawFramed(Pill, CCNeon::TRACK, CCNeon::LINE, Pill.h / 2.0f);
				CUIRect Fill = Pill;
				Fill.w = std::max(Pill.h, Pill.w * Update.Progress());
				CCNeon::Glow(Fill, Fill.h / 2.0f, CCNeon::NEON, 0.8f, IGraphics::CORNER_ALL, 1.0f, 3);
				CCNeon::DrawFramed(Fill, CCNeon::ACCENT, CCNeon::PRIMARY_EDGE, Fill.h / 2.0f);
				str_format(aLabel, sizeof(aLabel), "%s %d%%", Localize("Downloading update"), round_to_int(Update.Progress() * 100.0f));
				SetLabelColor(CCNeon::TEXT);
				Ui()->DoLabel(&Pill, aLabel, 10.0f, TEXTALIGN_MC);
				ResetLabelColor();
			}
			else
			{
				const bool Hot = Ui()->HotItem() == &s_UpdatePill;
				CCNeon::DrawPrimary(Pill, Hot, Ui()->CheckActiveItem(&s_UpdatePill), IGraphics::CORNER_ALL, 10.0f);
				if(State == CCUpdate::EState::FAILED)
					str_copy(aLabel, Localize("Update failed, try again"));
				else
					str_format(aLabel, sizeof(aLabel), "%s #%d", Localize("Update"), Update.LatestBuild());
				SetLabelColor(CCNeon::TEXT);
				Ui()->DoLabel(&Pill, aLabel, 11.0f, TEXTALIGN_MC);
				ResetLabelColor();
				if(Ui()->DoButtonLogic(&s_UpdatePill, 0, &Pill, BUTTONFLAG_LEFT))
					Update.Start();
			}
		}
	}

	// bottom row: quit and console on the left, versions on the right
	{
		const float Size = 22.0f;
		CUIRect QuitButton, ConsoleButton;
		QuitButton.x = Left;
		QuitButton.y = MainView.y + MainView.h - Size - 8.0f;
		QuitButton.w = Size;
		QuitButton.h = Size;
		ConsoleButton = QuitButton;
		ConsoleButton.x += Size + 6.0f;

		static CButtonContainer s_QuitButton;
		static CButtonContainer s_ConsoleButton;
		bool UsedEscape = false;
		const bool QuitHot = Ui()->HotItem() == &s_QuitButton;
		if(QuitHot)
			CCNeon::DrawFramed(QuitButton, ColorRGBA(0.75f, 0.1f, 0.2f, 0.55f), CCNeon::LINE, Size / 2.0f);
		else
			CCNeon::DrawSecondary(QuitButton, false, Ui()->CheckActiveItem(&s_QuitButton), IGraphics::CORNER_ALL, Size / 2.0f);
		SetIconMode();
		SetLabelColor(QuitHot ? CCNeon::TEXT : CCNeon::TEXT_SUB);
		Ui()->DoLabel(&QuitButton, FontIcon::POWER_OFF, Size * 0.46f, TEXTALIGN_MC);
		ResetLabelColor();
		ResetIconMode();
		if(Ui()->DoButtonLogic(&s_QuitButton, 0, &QuitButton, BUTTONFLAG_LEFT) || (UsedEscape = Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE)) || CheckHotKey(KEY_Q))
		{
			if(g_Config.m_BcConfirmQuit || UsedEscape || GameClient()->Editor()->HasUnsavedData() || (GameClient()->CurrentRaceTime() / 60 >= g_Config.m_ClConfirmQuitTime && g_Config.m_ClConfirmQuitTime >= 0))
				Menus.ShowQuitPopup();
			else
				Client()->Quit();
		}

		CCNeon::DrawSecondary(ConsoleButton, Ui()->HotItem() == &s_ConsoleButton, Ui()->CheckActiveItem(&s_ConsoleButton), IGraphics::CORNER_ALL, Size / 2.0f);
		SetIconMode();
		SetLabelColor(Ui()->HotItem() == &s_ConsoleButton ? CCNeon::TEXT : CCNeon::TEXT_SUB);
		Ui()->DoLabel(&ConsoleButton, FontIcon::TERMINAL, Size * 0.46f, TEXTALIGN_MC);
		ResetLabelColor();
		ResetIconMode();
		if(Ui()->DoButtonLogic(&s_ConsoleButton, 0, &ConsoleButton, BUTTONFLAG_LEFT))
			GameClient()->m_GameConsole.Toggle(CGameConsole::CONSOLETYPE_LOCAL);

		CUIRect Versions, Line1, Line2;
		Versions.w = 150.0f;
		Versions.h = 24.0f;
		Versions.x = Right - Versions.w;
		Versions.y = MainView.y + MainView.h - Versions.h - 8.0f;
		Versions.HSplitTop(12.0f, &Line1, &Line2);
		char aDDNetBuf[64];
		char aCrystalBuf[64];
		str_format(aDDNetBuf, sizeof(aDDNetBuf), "DDNet %s", GAME_RELEASE_VERSION);
		str_format(aCrystalBuf, sizeof(aCrystalBuf), "Cr\xe2\x99\xb1stal Client %d", (int)CCLIENT_BUILD);
		SetLabelColor(CCNeon::TEXT_DIM);
		Ui()->DoLabel(&Line1, aDDNetBuf, 10.0f, TEXTALIGN_MR);
		Ui()->DoLabel(&Line2, aCrystalBuf, 10.0f, TEXTALIGN_MR);
		ResetLabelColor();
	}

	if(NewPage != -1)
	{
		Menus.SetShowStart(false);
		Menus.SetMenuPage(NewPage);
	}
}

bool CMenusStart::CheckHotKey(int Key) const
{
	return !Input()->ShiftIsPressed() && !Input()->ModifierIsPressed() && !Input()->AltIsPressed() &&
	       Input()->KeyPress(Key) &&
	       !GameClient()->m_GameConsole.IsActive();
}
