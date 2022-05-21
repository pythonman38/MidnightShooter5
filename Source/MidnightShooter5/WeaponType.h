#pragma once

UENUM(BlueprintType)
enum class EWeaponType : uint8
{
	EWT_SubmachingGun UMETA(DisplayName = "SubmachineGun"),
	EWT_AssaultRifle UMETA(DisplayName = "AssualtRifle"),
	EWT_Pistol UMETA(DisplayName = "Pistol"),
	EWT_MAX UMETA(DisplayName = "DefaultMAX")
};