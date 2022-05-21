// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterAnimInstance.h"
#include "ShooterCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Weapon.h"

UShooterAnimInstance::UShooterAnimInstance() :
	Speed(0.f),
	bIsInAir(false),
	bIsAccelerating(false),
	MovementOffsetYaw(0.f),
	LastMovementOffsetYaw(0.f),
	bAiming(false),
	CharacterRotation(FRotator(0.f)),
	CharacterRotationLastFrame(FRotator(0.f)),
	TIP_CharacterYaw(0.f),
	TIP_CharacterYawLastFrame(0.f),
	YawDelta(0.f),
	RootYawOffset(0.f),
	Pitch(0.f),
	bReloading(false),
	OffsetState(EOffsetState::EOS_Hip),
	RecoilWeight(1.0f),
	bTurningInPlace(false),
	bEquipping(false)
{
}

void UShooterAnimInstance::UpdateAnimationProperties(float DeltaTime)
{
	if (ShooterCharacter == nullptr) ShooterCharacter = Cast<AShooterCharacter>(TryGetPawnOwner());

	if (ShooterCharacter)
	{
		// Get the lateral speed of the character from velocity
		FVector Velocity{ ShooterCharacter->GetVelocity() };
		Velocity.Z = 0;
		Speed = Velocity.Size();

		// Is the character in the air?
		bIsInAir = ShooterCharacter->GetCharacterMovement()->IsFalling();

		// Is the character moving?
		bIsAccelerating = ShooterCharacter->GetCharacterMovement()->GetCurrentAcceleration().Size() > 0.f;

		// Is the character aiming?
		bAiming = ShooterCharacter->GetAiming();

		// Is the character reloading?
		bReloading = ShooterCharacter->GetCombatState() == ECombatState::ECS_Reloading;

		// Is the character crouching?
		bCrouching = ShooterCharacter->GetCrouching();

		// Is the character equipping a weapon?
		bEquipping = ShooterCharacter->GetCombatState() == ECombatState::ECS_Equipping;
		
		FRotator AimRotation = ShooterCharacter->GetBaseAimRotation(), MovementRotation = UKismetMathLibrary::MakeRotFromX(ShooterCharacter->GetVelocity());
		MovementOffsetYaw = UKismetMathLibrary::NormalizedDeltaRotator(MovementRotation, AimRotation).Yaw;

		if (ShooterCharacter->GetVelocity().Size() > 0.f) LastMovementOffsetYaw = MovementOffsetYaw;

		if (bReloading) OffsetState = EOffsetState::EOS_Reloading;
		else if (bIsInAir) OffsetState = EOffsetState::EOS_InAir;
		else if (bAiming) OffsetState = EOffsetState::EOS_Aiming;
		else OffsetState = EOffsetState::EOS_Hip;

		if (ShooterCharacter->GetEquippedWeapon()) EquippedWeaponType = ShooterCharacter->GetEquippedWeapon()->GetWeaponType();
	}

	TurnInPlace();
	Lean(DeltaTime);
}

void UShooterAnimInstance::NativeInitializeAnimation()
{
	ShooterCharacter = Cast<AShooterCharacter>(TryGetPawnOwner());
}

void UShooterAnimInstance::TurnInPlace()
{
	if (ShooterCharacter == nullptr) return;

	Pitch = ShooterCharacter->GetBaseAimRotation().Pitch;

	if (Speed > 0 || bIsInAir)
	{
		RootYawOffset = 0;
		TIP_CharacterYaw = ShooterCharacter->GetActorRotation().Yaw;
		TIP_CharacterYawLastFrame = TIP_CharacterYaw;
		RotationCurveLastFrame = 0.f;
		RotationCurve = 0.f;
	}
	else
	{
		TIP_CharacterYawLastFrame = TIP_CharacterYaw;
		TIP_CharacterYaw = ShooterCharacter->GetActorRotation().Yaw;
		const float TIP_YawDelta{ TIP_CharacterYaw - TIP_CharacterYawLastFrame };
		RootYawOffset = UKismetMathLibrary::NormalizeAxis(RootYawOffset - TIP_YawDelta);

		const float Turning{ GetCurveValue(TEXT("Turning")) };
		if (Turning > 0)
		{
			bTurningInPlace = true;
			RotationCurveLastFrame = RotationCurve;
			RotationCurve = GetCurveValue(TEXT("Rotation"));
			const float DeltaRotation{ RotationCurve - RotationCurveLastFrame };

			(RootYawOffset > 0) ? RootYawOffset -= DeltaRotation : RootYawOffset += DeltaRotation;

			const float ABSRootYawOffset{ FMath::Abs(RootYawOffset) };
			if (ABSRootYawOffset > 90.f)
			{
				const float YawExcess{ ABSRootYawOffset - 90.f };
				(RootYawOffset > 0) ? RootYawOffset -= YawExcess : RootYawOffset += YawExcess;
			}
		}
		else bTurningInPlace = false;
	}

	if (bTurningInPlace) (bReloading || bEquipping) ? RecoilWeight = 1.f : RecoilWeight = 0.f;
	else
	{
		if (bCrouching)
		{
			(bReloading || bEquipping) ? RecoilWeight = 1.f : RecoilWeight = 0.1f;
		}
		else
		{
			(bAiming || bReloading || bEquipping) ? RecoilWeight = 1.f : RecoilWeight = 0.5f;
		}
	}
}

void UShooterAnimInstance::Lean(float DeltaTime)
{
	if (ShooterCharacter == nullptr) return;

	CharacterRotationLastFrame = CharacterRotation;
	CharacterRotation = ShooterCharacter->GetActorRotation();
	const FRotator Delta{ UKismetMathLibrary::NormalizedDeltaRotator(CharacterRotation, CharacterRotationLastFrame) };

	const float Target = Delta.Yaw / DeltaTime, Interp = FMath::FInterpTo(YawDelta, Target, DeltaTime, 6.f);
	YawDelta = FMath::Clamp(Interp, -90.f, 90.f);
}
