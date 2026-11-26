#include "gameclient.h"

#include <base/str.h>

#include <engine/graphics.h>
#include <engine/image.h>
#include <engine/storage.h>

#include <generated/client_data.h>
#include <generated/client_data7.h>

CGameClient::CImageAsset CGameClient::LoadAssetFromPath(const char *pPath, bool AsDir, int AssetId, const char *pDirectory) const
{
	CImageAsset LoadedAsset;
	LoadedAsset.m_IsDefault = str_comp(pPath, "default") == 0;
	if(LoadedAsset.m_IsDefault)
	{
		str_copy(LoadedAsset.m_aPath, g_pData->m_aImages[AssetId].m_pFilename);
	}
	else if(AsDir)
	{
		str_format(LoadedAsset.m_aPath, sizeof(LoadedAsset.m_aPath), "assets/%s/%s/%s", pDirectory, pPath, g_pData->m_aImages[AssetId].m_pFilename);
	}
	else
	{
		str_format(LoadedAsset.m_aPath, sizeof(LoadedAsset.m_aPath), "assets/%s/%s.png", pDirectory, pPath);
	}

	Graphics()->LoadPng(LoadedAsset.m_ImageInfo, LoadedAsset.m_aPath, IStorage::TYPE_ALL);

	if(!LoadedAsset.m_IsDefault && LoadedAsset.IsLoaded())
	{
		CImageInfo ImgDefaultInfo;
		if(Graphics()->LoadPng(ImgDefaultInfo, g_pData->m_aImages[AssetId].m_pFilename, IStorage::TYPE_ALL))
			LoadedAsset.m_FallbackImageInfo = std::move(ImgDefaultInfo);
	}
	return LoadedAsset;
}

void CGameClient::UnloadGameSkin(CGameSkin &Skin)
{
	Graphics()->UnloadTexture(&Skin.m_SpriteHealthFull);
	Graphics()->UnloadTexture(&Skin.m_SpriteHealthEmpty);
	Graphics()->UnloadTexture(&Skin.m_SpriteArmorFull);
	Graphics()->UnloadTexture(&Skin.m_SpriteArmorEmpty);

	Graphics()->UnloadTexture(&Skin.m_SpriteWeaponHammerCursor);
	Graphics()->UnloadTexture(&Skin.m_SpriteWeaponGunCursor);
	Graphics()->UnloadTexture(&Skin.m_SpriteWeaponShotgunCursor);
	Graphics()->UnloadTexture(&Skin.m_SpriteWeaponGrenadeCursor);
	Graphics()->UnloadTexture(&Skin.m_SpriteWeaponNinjaCursor);
	Graphics()->UnloadTexture(&Skin.m_SpriteWeaponLaserCursor);

	std::fill(std::begin(Skin.m_aSpriteWeaponCursors), std::end(Skin.m_aSpriteWeaponCursors), IGraphics::CTextureHandle());

	Graphics()->UnloadTexture(&Skin.m_SpriteHookChain);
	Graphics()->UnloadTexture(&Skin.m_SpriteHookHead);
	Graphics()->UnloadTexture(&Skin.m_SpriteWeaponHammer);
	Graphics()->UnloadTexture(&Skin.m_SpriteWeaponGun);
	Graphics()->UnloadTexture(&Skin.m_SpriteWeaponShotgun);
	Graphics()->UnloadTexture(&Skin.m_SpriteWeaponGrenade);
	Graphics()->UnloadTexture(&Skin.m_SpriteWeaponNinja);
	Graphics()->UnloadTexture(&Skin.m_SpriteWeaponLaser);

	std::fill(std::begin(Skin.m_aSpriteWeapons), std::end(Skin.m_aSpriteWeapons), IGraphics::CTextureHandle());

	for(auto &SpriteParticle : Skin.m_aSpriteParticles)
	{
		Graphics()->UnloadTexture(&SpriteParticle);
	}

	for(auto &SpriteStar : Skin.m_aSpriteStars)
	{
		Graphics()->UnloadTexture(&SpriteStar);
	}

	Graphics()->UnloadTexture(&Skin.m_SpriteWeaponGunProjectile);
	Graphics()->UnloadTexture(&Skin.m_SpriteWeaponShotgunProjectile);
	Graphics()->UnloadTexture(&Skin.m_SpriteWeaponGrenadeProjectile);
	Graphics()->UnloadTexture(&Skin.m_SpriteWeaponHammerProjectile);
	Graphics()->UnloadTexture(&Skin.m_SpriteWeaponNinjaProjectile);
	Graphics()->UnloadTexture(&Skin.m_SpriteWeaponLaserProjectile);

	std::fill(std::begin(Skin.m_aSpriteWeaponProjectiles), std::end(Skin.m_aSpriteWeaponProjectiles), IGraphics::CTextureHandle());

	for(int i = 0; i < 3; ++i)
	{
		Graphics()->UnloadTexture(&Skin.m_aSpriteWeaponGunMuzzles[i]);
		Graphics()->UnloadTexture(&Skin.m_aSpriteWeaponShotgunMuzzles[i]);
		Graphics()->UnloadTexture(&Skin.m_aaSpriteWeaponNinjaMuzzles[i]);
	}
	for(auto &aSpriteWeaponsMuzzles : Skin.m_aaSpriteWeaponsMuzzles)
	{
		std::fill(std::begin(aSpriteWeaponsMuzzles), std::end(aSpriteWeaponsMuzzles), IGraphics::CTextureHandle());
	}

	Graphics()->UnloadTexture(&Skin.m_SpritePickupHealth);
	Graphics()->UnloadTexture(&Skin.m_SpritePickupFreeze);
	Graphics()->UnloadTexture(&Skin.m_SpritePickupArmor);
	Graphics()->UnloadTexture(&Skin.m_SpritePickupArmorShotgun);
	Graphics()->UnloadTexture(&Skin.m_SpritePickupArmorGrenade);
	Graphics()->UnloadTexture(&Skin.m_SpritePickupArmorLaser);
	Graphics()->UnloadTexture(&Skin.m_SpritePickupArmorNinja);
	Graphics()->UnloadTexture(&Skin.m_SpritePickupGrenade);
	Graphics()->UnloadTexture(&Skin.m_SpritePickupShotgun);
	Graphics()->UnloadTexture(&Skin.m_SpritePickupLaser);
	Graphics()->UnloadTexture(&Skin.m_SpritePickupNinja);
	Graphics()->UnloadTexture(&Skin.m_SpritePickupGun);
	Graphics()->UnloadTexture(&Skin.m_SpritePickupHammer);

	std::fill(std::begin(Skin.m_aSpritePickupWeapons), std::end(Skin.m_aSpritePickupWeapons), IGraphics::CTextureHandle());
	std::fill(std::begin(Skin.m_aSpritePickupWeaponArmor), std::end(Skin.m_aSpritePickupWeaponArmor), IGraphics::CTextureHandle());

	Graphics()->UnloadTexture(&Skin.m_SpriteFlagBlue);
	Graphics()->UnloadTexture(&Skin.m_SpriteFlagRed);

	if(Skin.IsSixup())
	{
		Graphics()->UnloadTexture(&Skin.m_SpriteNinjaBarFullLeft);
		Graphics()->UnloadTexture(&Skin.m_SpriteNinjaBarFull);
		Graphics()->UnloadTexture(&Skin.m_SpriteNinjaBarEmpty);
		Graphics()->UnloadTexture(&Skin.m_SpriteNinjaBarEmptyRight);
	}

	Skin.m_Loaded = false;
}

void CGameClient::FillGameSkin(CGameSkin &Skin, CImageInfo &ImgInfo, std::optional<CImageInfo> &FallbackImgInfo)
{
	Skin.m_SpriteHealthFull = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HEALTH_FULL]);
	Skin.m_SpriteHealthEmpty = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HEALTH_EMPTY]);
	Skin.m_SpriteArmorFull = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_ARMOR_FULL]);
	Skin.m_SpriteArmorEmpty = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_ARMOR_EMPTY]);

	Skin.m_SpriteWeaponHammerCursor = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_HAMMER_CURSOR]);
	Skin.m_SpriteWeaponGunCursor = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_GUN_CURSOR]);
	Skin.m_SpriteWeaponShotgunCursor = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_SHOTGUN_CURSOR]);
	Skin.m_SpriteWeaponGrenadeCursor = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_GRENADE_CURSOR]);
	Skin.m_SpriteWeaponNinjaCursor = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_NINJA_CURSOR]);
	Skin.m_SpriteWeaponLaserCursor = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_LASER_CURSOR]);

	Skin.m_aSpriteWeaponCursors[0] = Skin.m_SpriteWeaponHammerCursor;
	Skin.m_aSpriteWeaponCursors[1] = Skin.m_SpriteWeaponGunCursor;
	Skin.m_aSpriteWeaponCursors[2] = Skin.m_SpriteWeaponShotgunCursor;
	Skin.m_aSpriteWeaponCursors[3] = Skin.m_SpriteWeaponGrenadeCursor;
	Skin.m_aSpriteWeaponCursors[4] = Skin.m_SpriteWeaponLaserCursor;
	Skin.m_aSpriteWeaponCursors[5] = Skin.m_SpriteWeaponNinjaCursor;

	// weapons and hook
	Skin.m_SpriteHookChain = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HOOK_CHAIN]);
	Skin.m_SpriteHookHead = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HOOK_HEAD]);
	Skin.m_SpriteWeaponHammer = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_HAMMER_BODY]);
	Skin.m_SpriteWeaponGun = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_GUN_BODY]);
	Skin.m_SpriteWeaponShotgun = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_SHOTGUN_BODY]);
	Skin.m_SpriteWeaponGrenade = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_GRENADE_BODY]);
	Skin.m_SpriteWeaponNinja = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_NINJA_BODY]);
	Skin.m_SpriteWeaponLaser = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_LASER_BODY]);

	Skin.m_aSpriteWeapons[0] = Skin.m_SpriteWeaponHammer;
	Skin.m_aSpriteWeapons[1] = Skin.m_SpriteWeaponGun;
	Skin.m_aSpriteWeapons[2] = Skin.m_SpriteWeaponShotgun;
	Skin.m_aSpriteWeapons[3] = Skin.m_SpriteWeaponGrenade;
	Skin.m_aSpriteWeapons[4] = Skin.m_SpriteWeaponLaser;
	Skin.m_aSpriteWeapons[5] = Skin.m_SpriteWeaponNinja;

	// particles
	for(int i = 0; i < 9; ++i)
	{
		Skin.m_aSpriteParticles[i] = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_PART1 + i]);
	}

	// stars
	for(int i = 0; i < 3; ++i)
	{
		Skin.m_aSpriteStars[i] = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_STAR1 + i]);
	}

	// projectiles
	Skin.m_SpriteWeaponGunProjectile = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_GUN_PROJ]);
	Skin.m_SpriteWeaponShotgunProjectile = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_SHOTGUN_PROJ]);
	Skin.m_SpriteWeaponGrenadeProjectile = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_GRENADE_PROJ]);

	// these weapons have no projectiles
	Skin.m_SpriteWeaponHammerProjectile = IGraphics::CTextureHandle();
	Skin.m_SpriteWeaponNinjaProjectile = IGraphics::CTextureHandle();

	Skin.m_SpriteWeaponLaserProjectile = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_LASER_PROJ]);

	Skin.m_aSpriteWeaponProjectiles[0] = Skin.m_SpriteWeaponHammerProjectile;
	Skin.m_aSpriteWeaponProjectiles[1] = Skin.m_SpriteWeaponGunProjectile;
	Skin.m_aSpriteWeaponProjectiles[2] = Skin.m_SpriteWeaponShotgunProjectile;
	Skin.m_aSpriteWeaponProjectiles[3] = Skin.m_SpriteWeaponGrenadeProjectile;
	Skin.m_aSpriteWeaponProjectiles[4] = Skin.m_SpriteWeaponLaserProjectile;
	Skin.m_aSpriteWeaponProjectiles[5] = Skin.m_SpriteWeaponNinjaProjectile;

	// muzzles
	for(int i = 0; i < 3; ++i)
	{
		Skin.m_aSpriteWeaponGunMuzzles[i] = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_GUN_MUZZLE1 + i]);
		Skin.m_aSpriteWeaponShotgunMuzzles[i] = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_SHOTGUN_MUZZLE1 + i]);
		Skin.m_aaSpriteWeaponNinjaMuzzles[i] = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_WEAPON_NINJA_MUZZLE1 + i]);

		Skin.m_aaSpriteWeaponsMuzzles[1][i] = Skin.m_aSpriteWeaponGunMuzzles[i];
		Skin.m_aaSpriteWeaponsMuzzles[2][i] = Skin.m_aSpriteWeaponShotgunMuzzles[i];
		Skin.m_aaSpriteWeaponsMuzzles[5][i] = Skin.m_aaSpriteWeaponNinjaMuzzles[i];
	}

	// pickups
	Skin.m_SpritePickupHealth = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_PICKUP_HEALTH]);
	Skin.m_SpritePickupFreeze = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_PICKUP_FREEZE]);
	Skin.m_SpritePickupArmor = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_PICKUP_ARMOR]);
	Skin.m_SpritePickupHammer = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_PICKUP_HAMMER]);
	Skin.m_SpritePickupGun = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_PICKUP_GUN]);
	Skin.m_SpritePickupShotgun = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_PICKUP_SHOTGUN]);
	Skin.m_SpritePickupGrenade = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_PICKUP_GRENADE]);
	Skin.m_SpritePickupLaser = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_PICKUP_LASER]);
	Skin.m_SpritePickupNinja = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_PICKUP_NINJA]);
	Skin.m_SpritePickupArmorShotgun = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_PICKUP_ARMOR_SHOTGUN]);
	Skin.m_SpritePickupArmorGrenade = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_PICKUP_ARMOR_GRENADE]);
	Skin.m_SpritePickupArmorNinja = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_PICKUP_ARMOR_NINJA]);
	Skin.m_SpritePickupArmorLaser = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_PICKUP_ARMOR_LASER]);

	Skin.m_aSpritePickupWeapons[0] = Skin.m_SpritePickupHammer;
	Skin.m_aSpritePickupWeapons[1] = Skin.m_SpritePickupGun;
	Skin.m_aSpritePickupWeapons[2] = Skin.m_SpritePickupShotgun;
	Skin.m_aSpritePickupWeapons[3] = Skin.m_SpritePickupGrenade;
	Skin.m_aSpritePickupWeapons[4] = Skin.m_SpritePickupLaser;
	Skin.m_aSpritePickupWeapons[5] = Skin.m_SpritePickupNinja;

	Skin.m_aSpritePickupWeaponArmor[0] = Skin.m_SpritePickupArmorShotgun;
	Skin.m_aSpritePickupWeaponArmor[1] = Skin.m_SpritePickupArmorGrenade;
	Skin.m_aSpritePickupWeaponArmor[2] = Skin.m_SpritePickupArmorNinja;
	Skin.m_aSpritePickupWeaponArmor[3] = Skin.m_SpritePickupArmorLaser;

	// flags
	Skin.m_SpriteFlagBlue = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_FLAG_BLUE]);
	Skin.m_SpriteFlagRed = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_FLAG_RED]);

	// ninja bar (0.7)
	if(!Graphics()->IsSpriteTextureFullyTransparent(ImgInfo, &client_data7::g_pData->m_aSprites[client_data7::SPRITE_NINJA_BAR_FULL_LEFT]) ||
		!Graphics()->IsSpriteTextureFullyTransparent(ImgInfo, &client_data7::g_pData->m_aSprites[client_data7::SPRITE_NINJA_BAR_FULL]) ||
		!Graphics()->IsSpriteTextureFullyTransparent(ImgInfo, &client_data7::g_pData->m_aSprites[client_data7::SPRITE_NINJA_BAR_EMPTY]) ||
		!Graphics()->IsSpriteTextureFullyTransparent(ImgInfo, &client_data7::g_pData->m_aSprites[client_data7::SPRITE_NINJA_BAR_EMPTY_RIGHT]))
	{
		Skin.m_SpriteNinjaBarFullLeft = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &client_data7::g_pData->m_aSprites[client_data7::SPRITE_NINJA_BAR_FULL_LEFT]);
		Skin.m_SpriteNinjaBarFull = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &client_data7::g_pData->m_aSprites[client_data7::SPRITE_NINJA_BAR_FULL]);
		Skin.m_SpriteNinjaBarEmpty = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &client_data7::g_pData->m_aSprites[client_data7::SPRITE_NINJA_BAR_EMPTY]);
		Skin.m_SpriteNinjaBarEmptyRight = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &client_data7::g_pData->m_aSprites[client_data7::SPRITE_NINJA_BAR_EMPTY_RIGHT]);
	}

	Skin.m_Loaded = true;
}

bool CGameClient::LoadGameSkinFile(CGameSkin &Skin, const char *pFile, int StorageType)
{
	if(Skin.m_Loaded)
		UnloadGameSkin(Skin);
	CImageInfo ImgInfo;
	if(!Graphics()->LoadPng(ImgInfo, pFile, StorageType))
		return false;
	std::optional<CImageInfo> FallbackImgInfo;
	CImageInfo ImgDefaultInfo;
	if(Graphics()->LoadPng(ImgDefaultInfo, g_pData->m_aImages[IMAGE_GAME].m_pFilename, IStorage::TYPE_ALL))
		FallbackImgInfo = std::move(ImgDefaultInfo);
	bool Ok = false;
	if(Graphics()->CheckImageDivisibility(pFile, ImgInfo, g_pData->m_aSprites[SPRITE_HEALTH_FULL].m_pSet->m_Gridx, g_pData->m_aSprites[SPRITE_HEALTH_FULL].m_pSet->m_Gridy, true) && Graphics()->IsImageFormatRgba(pFile, ImgInfo))
	{
		FillGameSkin(Skin, ImgInfo, FallbackImgInfo);
		Ok = true;
	}
	ImgInfo.Free();
	if(FallbackImgInfo.has_value())
		FallbackImgInfo.value().Free();
	return Ok;
}

void CGameClient::LoadGameSkin(const char *pPath, bool AsDir)
{
	if(m_GameSkin.m_Loaded)
		UnloadGameSkin(m_GameSkin);
	m_aCcGameSkinFile[0] = '\0';

	CImageAsset LoadedAsset = LoadAssetFromPath(pPath, AsDir, IMAGE_GAME, "game");
	CImageInfo &ImgInfo = LoadedAsset.m_ImageInfo;
	std::optional<CImageInfo> &FallbackImgInfo = LoadedAsset.m_FallbackImageInfo;
	if(!LoadedAsset.IsLoaded() && !LoadedAsset.m_IsDefault)
	{
		if(AsDir)
			LoadGameSkin("default");
		else
			LoadGameSkin(pPath, true);
	}
	else if(LoadedAsset.IsLoaded() && Graphics()->CheckImageDivisibility(LoadedAsset.m_aPath, ImgInfo, g_pData->m_aSprites[SPRITE_HEALTH_FULL].m_pSet->m_Gridx, g_pData->m_aSprites[SPRITE_HEALTH_FULL].m_pSet->m_Gridy, true) && Graphics()->IsImageFormatRgba(LoadedAsset.m_aPath, ImgInfo))
	{
		FillGameSkin(m_GameSkin, ImgInfo, FallbackImgInfo);
		// C-Client: remember the file, other Cr1stal players can see this pack
		if(!LoadedAsset.m_IsDefault)
			str_copy(m_aCcGameSkinFile, LoadedAsset.m_aPath);
	}
	ImgInfo.Free();
	if(FallbackImgInfo.has_value())
		FallbackImgInfo.value().Free();
}

void CGameClient::LoadEmoticonsSkin(const char *pPath, bool AsDir)
{
	if(m_EmoticonsSkin.m_Loaded)
	{
		for(auto &SpriteEmoticon : m_EmoticonsSkin.m_aSpriteEmoticons)
			Graphics()->UnloadTexture(&SpriteEmoticon);

		m_EmoticonsSkin.m_Loaded = false;
	}

	CImageAsset LoadedAsset = LoadAssetFromPath(pPath, AsDir, IMAGE_EMOTICONS, "emoticons");
	CImageInfo &ImgInfo = LoadedAsset.m_ImageInfo;
	std::optional<CImageInfo> &FallbackImgInfo = LoadedAsset.m_FallbackImageInfo;
	if(!LoadedAsset.IsLoaded() && !LoadedAsset.m_IsDefault)
	{
		if(AsDir)
			LoadEmoticonsSkin("default");
		else
			LoadEmoticonsSkin(pPath, true);
	}
	else if(LoadedAsset.IsLoaded() && Graphics()->CheckImageDivisibility(LoadedAsset.m_aPath, ImgInfo, g_pData->m_aSprites[SPRITE_OOP].m_pSet->m_Gridx, g_pData->m_aSprites[SPRITE_OOP].m_pSet->m_Gridy, true) && Graphics()->IsImageFormatRgba(LoadedAsset.m_aPath, ImgInfo))
	{
		for(int i = 0; i < 16; ++i)
			m_EmoticonsSkin.m_aSpriteEmoticons[i] = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_OOP + i]);

		m_EmoticonsSkin.m_Loaded = true;
	}
	ImgInfo.Free();
	if(FallbackImgInfo.has_value())
		FallbackImgInfo.value().Free();
}

void CGameClient::LoadParticlesSkin(const char *pPath, bool AsDir)
{
	if(m_ParticlesSkin.m_Loaded)
	{
		Graphics()->UnloadTexture(&m_ParticlesSkin.m_SpriteParticleSlice);
		Graphics()->UnloadTexture(&m_ParticlesSkin.m_SpriteParticleBall);
		for(auto &SpriteParticleSplat : m_ParticlesSkin.m_aSpriteParticleSplat)
			Graphics()->UnloadTexture(&SpriteParticleSplat);
		Graphics()->UnloadTexture(&m_ParticlesSkin.m_SpriteParticleSmoke);
		Graphics()->UnloadTexture(&m_ParticlesSkin.m_SpriteParticleShell);
		Graphics()->UnloadTexture(&m_ParticlesSkin.m_SpriteParticleExpl);
		Graphics()->UnloadTexture(&m_ParticlesSkin.m_SpriteParticleAirJump);
		Graphics()->UnloadTexture(&m_ParticlesSkin.m_SpriteParticleHit);

		std::fill(std::begin(m_ParticlesSkin.m_aSpriteParticles), std::end(m_ParticlesSkin.m_aSpriteParticles), IGraphics::CTextureHandle());

		m_ParticlesSkin.m_Loaded = false;
	}

	CImageAsset LoadedAsset = LoadAssetFromPath(pPath, AsDir, IMAGE_PARTICLES, "particles");
	CImageInfo &ImgInfo = LoadedAsset.m_ImageInfo;
	std::optional<CImageInfo> &FallbackImgInfo = LoadedAsset.m_FallbackImageInfo;
	if(!LoadedAsset.IsLoaded() && !LoadedAsset.m_IsDefault)
	{
		if(AsDir)
			LoadParticlesSkin("default");
		else
			LoadParticlesSkin(pPath, true);
	}
	else if(LoadedAsset.IsLoaded() && Graphics()->CheckImageDivisibility(LoadedAsset.m_aPath, ImgInfo, g_pData->m_aSprites[SPRITE_PART_SLICE].m_pSet->m_Gridx, g_pData->m_aSprites[SPRITE_PART_SLICE].m_pSet->m_Gridy, true) && Graphics()->IsImageFormatRgba(LoadedAsset.m_aPath, ImgInfo))
	{
		m_ParticlesSkin.m_SpriteParticleSlice = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_PART_SLICE]);
		m_ParticlesSkin.m_SpriteParticleBall = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_PART_BALL]);
		for(int i = 0; i < 3; ++i)
			m_ParticlesSkin.m_aSpriteParticleSplat[i] = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_PART_SPLAT01 + i]);
		m_ParticlesSkin.m_SpriteParticleSmoke = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_PART_SMOKE]);
		m_ParticlesSkin.m_SpriteParticleShell = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_PART_SHELL]);
		m_ParticlesSkin.m_SpriteParticleExpl = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_PART_EXPL01]);
		m_ParticlesSkin.m_SpriteParticleAirJump = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_PART_AIRJUMP]);
		m_ParticlesSkin.m_SpriteParticleHit = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_PART_HIT01]);

		m_ParticlesSkin.m_aSpriteParticles[0] = m_ParticlesSkin.m_SpriteParticleSlice;
		m_ParticlesSkin.m_aSpriteParticles[1] = m_ParticlesSkin.m_SpriteParticleBall;
		for(int i = 0; i < 3; ++i)
			m_ParticlesSkin.m_aSpriteParticles[2 + i] = m_ParticlesSkin.m_aSpriteParticleSplat[i];
		m_ParticlesSkin.m_aSpriteParticles[5] = m_ParticlesSkin.m_SpriteParticleSmoke;
		m_ParticlesSkin.m_aSpriteParticles[6] = m_ParticlesSkin.m_SpriteParticleShell;
		m_ParticlesSkin.m_aSpriteParticles[7] = m_ParticlesSkin.m_SpriteParticleExpl;
		m_ParticlesSkin.m_aSpriteParticles[8] = m_ParticlesSkin.m_SpriteParticleAirJump;
		m_ParticlesSkin.m_aSpriteParticles[9] = m_ParticlesSkin.m_SpriteParticleHit;

		m_ParticlesSkin.m_Loaded = true;
	}
	ImgInfo.Free();
	if(FallbackImgInfo.has_value())
		FallbackImgInfo.value().Free();
}

void CGameClient::LoadHudSkin(const char *pPath, bool AsDir)
{
	if(m_HudSkin.m_Loaded)
	{
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudAirjump);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudAirjumpEmpty);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudSolo);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudCollisionDisabled);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudEndlessJump);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudEndlessHook);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudJetpack);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudFreezeBarFullLeft);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudFreezeBarFull);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudFreezeBarEmpty);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudFreezeBarEmptyRight);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudNinjaBarFullLeft);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudNinjaBarFull);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudNinjaBarEmpty);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudNinjaBarEmptyRight);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudHookHitDisabled);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudHammerHitDisabled);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudShotgunHitDisabled);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudGrenadeHitDisabled);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudLaserHitDisabled);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudGunHitDisabled);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudDeepFrozen);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudLiveFrozen);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudTeleportGrenade);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudTeleportGun);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudTeleportLaser);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudPracticeMode);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudLockMode);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudTeam0Mode);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudDummyHammer);
		Graphics()->UnloadTexture(&m_HudSkin.m_SpriteHudDummyCopy);
		m_HudSkin.m_Loaded = false;
	}

	CImageAsset LoadedAsset = LoadAssetFromPath(pPath, AsDir, IMAGE_HUD, "hud");
	CImageInfo &ImgInfo = LoadedAsset.m_ImageInfo;
	std::optional<CImageInfo> &FallbackImgInfo = LoadedAsset.m_FallbackImageInfo;
	if(!LoadedAsset.IsLoaded() && !LoadedAsset.m_IsDefault)
	{
		if(AsDir)
			LoadHudSkin("default");
		else
			LoadHudSkin(pPath, true);
	}
	else if(LoadedAsset.IsLoaded() && Graphics()->CheckImageDivisibility(LoadedAsset.m_aPath, ImgInfo, g_pData->m_aSprites[SPRITE_HUD_AIRJUMP].m_pSet->m_Gridx, g_pData->m_aSprites[SPRITE_HUD_AIRJUMP].m_pSet->m_Gridy, true) && Graphics()->IsImageFormatRgba(LoadedAsset.m_aPath, ImgInfo))
	{
		m_HudSkin.m_SpriteHudAirjump = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HUD_AIRJUMP]);
		m_HudSkin.m_SpriteHudAirjumpEmpty = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HUD_AIRJUMP_EMPTY]);
		m_HudSkin.m_SpriteHudSolo = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HUD_SOLO]);
		m_HudSkin.m_SpriteHudCollisionDisabled = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HUD_COLLISION_DISABLED]);
		m_HudSkin.m_SpriteHudEndlessJump = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HUD_ENDLESS_JUMP]);
		m_HudSkin.m_SpriteHudEndlessHook = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HUD_ENDLESS_HOOK]);
		m_HudSkin.m_SpriteHudJetpack = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HUD_JETPACK]);
		m_HudSkin.m_SpriteHudFreezeBarFullLeft = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HUD_FREEZE_BAR_FULL_LEFT]);
		m_HudSkin.m_SpriteHudFreezeBarFull = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HUD_FREEZE_BAR_FULL]);
		m_HudSkin.m_SpriteHudFreezeBarEmpty = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HUD_FREEZE_BAR_EMPTY]);
		m_HudSkin.m_SpriteHudFreezeBarEmptyRight = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HUD_FREEZE_BAR_EMPTY_RIGHT]);
		m_HudSkin.m_SpriteHudNinjaBarFullLeft = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HUD_NINJA_BAR_FULL_LEFT]);
		m_HudSkin.m_SpriteHudNinjaBarFull = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HUD_NINJA_BAR_FULL]);
		m_HudSkin.m_SpriteHudNinjaBarEmpty = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HUD_NINJA_BAR_EMPTY]);
		m_HudSkin.m_SpriteHudNinjaBarEmptyRight = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HUD_NINJA_BAR_EMPTY_RIGHT]);
		m_HudSkin.m_SpriteHudHookHitDisabled = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HUD_HOOK_HIT_DISABLED]);
		m_HudSkin.m_SpriteHudHammerHitDisabled = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HUD_HAMMER_HIT_DISABLED]);
		m_HudSkin.m_SpriteHudShotgunHitDisabled = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HUD_SHOTGUN_HIT_DISABLED]);
		m_HudSkin.m_SpriteHudGrenadeHitDisabled = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HUD_GRENADE_HIT_DISABLED]);
		m_HudSkin.m_SpriteHudLaserHitDisabled = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HUD_LASER_HIT_DISABLED]);
		m_HudSkin.m_SpriteHudGunHitDisabled = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HUD_GUN_HIT_DISABLED]);
		m_HudSkin.m_SpriteHudDeepFrozen = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HUD_DEEP_FROZEN]);
		m_HudSkin.m_SpriteHudLiveFrozen = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HUD_LIVE_FROZEN]);
		m_HudSkin.m_SpriteHudTeleportGrenade = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HUD_TELEPORT_GRENADE]);
		m_HudSkin.m_SpriteHudTeleportGun = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HUD_TELEPORT_GUN]);
		m_HudSkin.m_SpriteHudTeleportLaser = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HUD_TELEPORT_LASER]);
		m_HudSkin.m_SpriteHudPracticeMode = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HUD_PRACTICE_MODE]);
		m_HudSkin.m_SpriteHudLockMode = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HUD_LOCK_MODE]);
		m_HudSkin.m_SpriteHudTeam0Mode = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HUD_TEAM0_MODE]);
		m_HudSkin.m_SpriteHudDummyHammer = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HUD_DUMMY_HAMMER]);
		m_HudSkin.m_SpriteHudDummyCopy = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_HUD_DUMMY_COPY]);

		m_HudSkin.m_Loaded = true;
	}
	ImgInfo.Free();
	if(FallbackImgInfo.has_value())
		FallbackImgInfo.value().Free();
}

void CGameClient::LoadExtrasSkin(const char *pPath, bool AsDir)
{
	if(m_ExtrasSkin.m_Loaded)
	{
		Graphics()->UnloadTexture(&m_ExtrasSkin.m_SpriteParticleSnowflake);
		Graphics()->UnloadTexture(&m_ExtrasSkin.m_SpriteParticleSparkle);
		Graphics()->UnloadTexture(&m_ExtrasSkin.m_SpritePulley);
		Graphics()->UnloadTexture(&m_ExtrasSkin.m_SpriteHectagon);

		std::fill(std::begin(m_ExtrasSkin.m_aSpriteParticles), std::end(m_ExtrasSkin.m_aSpriteParticles), IGraphics::CTextureHandle());

		m_ExtrasSkin.m_Loaded = false;
	}

	CImageAsset LoadedAsset = LoadAssetFromPath(pPath, AsDir, IMAGE_EXTRAS, "extras");
	CImageInfo &ImgInfo = LoadedAsset.m_ImageInfo;
	std::optional<CImageInfo> &FallbackImgInfo = LoadedAsset.m_FallbackImageInfo;
	if(!LoadedAsset.IsLoaded() && !LoadedAsset.m_IsDefault)
	{
		if(AsDir)
			LoadExtrasSkin("default");
		else
			LoadExtrasSkin(pPath, true);
	}
	else if(LoadedAsset.IsLoaded() && Graphics()->CheckImageDivisibility(LoadedAsset.m_aPath, ImgInfo, g_pData->m_aSprites[SPRITE_PART_SNOWFLAKE].m_pSet->m_Gridx, g_pData->m_aSprites[SPRITE_PART_SNOWFLAKE].m_pSet->m_Gridy, true) && Graphics()->IsImageFormatRgba(LoadedAsset.m_aPath, ImgInfo))
	{
		m_ExtrasSkin.m_SpriteParticleSnowflake = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_PART_SNOWFLAKE]);
		m_ExtrasSkin.m_SpriteParticleSparkle = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_PART_SPARKLE]);
		m_ExtrasSkin.m_SpritePulley = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_PART_PULLEY]);
		m_ExtrasSkin.m_SpriteHectagon = Graphics()->LoadSpriteTexture(ImgInfo, FallbackImgInfo, &g_pData->m_aSprites[SPRITE_PART_HECTAGON]);

		m_ExtrasSkin.m_aSpriteParticles[0] = m_ExtrasSkin.m_SpriteParticleSnowflake;
		m_ExtrasSkin.m_aSpriteParticles[1] = m_ExtrasSkin.m_SpriteParticleSparkle;
		m_ExtrasSkin.m_aSpriteParticles[2] = m_ExtrasSkin.m_SpritePulley;
		m_ExtrasSkin.m_aSpriteParticles[3] = m_ExtrasSkin.m_SpriteHectagon;

		m_ExtrasSkin.m_Loaded = true;
	}
	ImgInfo.Free();
	if(FallbackImgInfo.has_value())
		FallbackImgInfo.value().Free();
}
