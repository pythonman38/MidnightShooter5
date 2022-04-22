// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Item.h"
#include "Weapon.generated.h"

/**
 * 
 */
UCLASS()
class MIDNIGHTSHOOTER5_API AWeapon : public AItem
{
	GENERATED_BODY()
	
public:
	// Sets default values for the weapon's properties
	AWeapon();

protected:
	void StopFalling();

public:
	virtual void Tick(float DelaTime) override;

	// Adds an impulse to the Weapon
	void ThrowWeapon();

private:
	FTimerHandle ThrowWeaponTimer;
	float ThrowWeaponTime;
	bool bFalling;

public:
	// Getters for private variables

};
