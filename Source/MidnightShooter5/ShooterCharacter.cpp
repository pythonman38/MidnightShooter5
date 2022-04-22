// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Particles/ParticleSystemComponent.h"
#include "Item.h"
#include "Components/WidgetComponent.h"
#include "Weapon.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"

// Sets default values
AShooterCharacter::AShooterCharacter() :
	// Base rates for turning and looking around
	BaseTurnRate(45.f),
	BaseLookUpRate(45.f),
	// Turn rates for aiming and not aiming
	HipTurnRate(90.f),
	HipLookUpRate(90.f),
	AimingTurnRate(50.f),
	AimingLookUpRate(50.f),
	// Mouse look sensitivity scale factors
	MouseHipTurnRate(1.0f),
	MouseHipLookUpRate(1.0f),
	MouseAimingTurnRate(0.5f),
	MouseAimingLookUpRate(0.5f),
	bAiming(false),
	// Camera field of view values
	CameraDefaultFOV(0.f),
	CameraZoomedFOV(35.f),
	CameraCurrentFOV(0.f),
	ZoomInterpSpeed(20.f),
	// Crosshair spread factors
	CrosshairSpreadMultiplier(0.f),
	CrosshairVelocityFactor(0.f),
	CrosshairInAirFactor(0.f),
	CrosshairAimFactor(0.f),
	CrosshairShootingFactor(0.f),
	// Bullet fire timer variables
	ShootTimeDuration(0.05f),
	bFiringBullet(false),
	// Automatic fire variables
	AutomaticFireRate(0.1f),
	bShouldFire(true),
	bFireButtonPressed(false),
	// Item trace variables
	bShouldTraceForItems(false),
	OverlappedItemCount(0),
	// Camera interp location variables
	CameraInterpDistance(250.f),
	CameraInterpElevation(65.f)
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Create a camera boom and make it the root component
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 180.f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->SocketOffset = FVector(0.f, 50.f, 70.f);

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Temporarily stop camera from moving with controller
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;
}

// Called when the game starts or when spawned
void AShooterCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	CameraDefaultFOV = GetFollowCamera()->FieldOfView;
	CameraCurrentFOV = CameraDefaultFOV;

	EquipWeapon(SpawnDefaultWeapon());
}

void AShooterCharacter::MoveForward(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		const FRotator Rotation{ Controller->GetControlRotation() }, YawRotation{ 0, Rotation.Yaw, 0 };
		const FVector Direction{ FRotationMatrix{YawRotation}.GetUnitAxis(EAxis::X) };
		AddMovementInput(Direction, Value);
	}
}

void AShooterCharacter::MoveRight(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		const FRotator Rotation{ Controller->GetControlRotation() }, YawRotation{ 0, Rotation.Yaw, 0 };
		const FVector Direction{ FRotationMatrix{YawRotation}.GetUnitAxis(EAxis::Y) };
		AddMovementInput(Direction, Value);
	}
}

void AShooterCharacter::TurnAtRate(float Rate)
{
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AShooterCharacter::Turn(float Value)
{
	float TurnScaleFactor{};
	bAiming ? TurnScaleFactor = MouseAimingTurnRate : TurnScaleFactor = MouseHipTurnRate;
	AddControllerYawInput(Value * TurnScaleFactor);
}

void AShooterCharacter::LookUpAtRate(float Rate)
{
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void AShooterCharacter::LookUp(float Value)
{
	float LookUpScaleFactor{};
	bAiming ? LookUpScaleFactor = MouseAimingLookUpRate : LookUpScaleFactor = MouseHipLookUpRate;
	AddControllerPitchInput(Value * LookUpScaleFactor);
}

void AShooterCharacter::FireWeapon()
{
	if (FireSound) UGameplayStatics::PlaySound2D(this, FireSound);

	auto BarrelSocket = GetMesh()->GetSocketByName("BarrelSocket");
	if (BarrelSocket)
	{
		const FTransform SocketTransform = BarrelSocket->GetSocketTransform(GetMesh());
		if (MuzzleFlash) UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), MuzzleFlash, SocketTransform);

		FVector BeamEnd;
		bool bBeamEnd = GetBeamEndLocation(SocketTransform.GetLocation(), BeamEnd);
		if (bBeamEnd)
		{
			if (ImpactParticles) UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactParticles, BeamEnd);
		}		

		if (BeamParticles)
		{
			UParticleSystemComponent* Beam = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BeamParticles, SocketTransform);
			if (Beam) Beam->SetVectorParameter(FName("Target"), BeamEnd);
		}
	}

	auto AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && HipFireMontage)
	{
		AnimInstance->Montage_Play(HipFireMontage);
		AnimInstance->Montage_JumpToSection(FName("StartFire"));
	}

	// Start bullet fire timer for crosshairs
	StartCrosshairBulletFire();
}

bool AShooterCharacter::GetBeamEndLocation(const FVector& MuzzleSocketLocation, FVector& OutBeamLocation)
{
	// Check for crosshair trace hit
	FHitResult CrosshairHitResult;
	bool bCrosshairHit = TraceUnderCrosshairs(CrosshairHitResult, OutBeamLocation);
	if (bCrosshairHit) OutBeamLocation = CrosshairHitResult.Location;

	// Perform a second trace, this time from the gun barrel
	FHitResult WeaponTraceHit;
	const FVector WeaponTraceStart{ MuzzleSocketLocation }, StartToEnd{ OutBeamLocation - MuzzleSocketLocation }, WeaponTraceEnd{ MuzzleSocketLocation + StartToEnd * 1.25f };
	GetWorld()->LineTraceSingleByChannel(WeaponTraceHit, WeaponTraceStart, WeaponTraceEnd, ECC_Visibility);
	if (WeaponTraceHit.bBlockingHit)
	{
		OutBeamLocation = WeaponTraceHit.Location;
		return true;
	}

	return false;
}

void AShooterCharacter::AimingButtonPressed()
{
	bAiming = true;
}

void AShooterCharacter::AimingButtonReleased()
{
	bAiming = false;
}

// Called every frame
void AShooterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	CameraInterpZoom(DeltaTime);

	SetLookRates();

	CalculateCrosshairSpread(DeltaTime);

	TraceForItems();
	
}

void AShooterCharacter::TraceForItems()
{
	if (bShouldTraceForItems)
	{
		FHitResult ItemTraceResult;
		FVector HitLocation;
		TraceUnderCrosshairs(ItemTraceResult, HitLocation);
		if (ItemTraceResult.bBlockingHit)
		{
			TraceHitItem = Cast<AItem>(ItemTraceResult.GetActor());
			if (TraceHitItem && TraceHitItem->GetPickupWidget()) TraceHitItem->GetPickupWidget()->SetVisibility(true);

			if (TraceHitItemLastFrame)
			{
				if (TraceHitItem != TraceHitItemLastFrame) TraceHitItemLastFrame->GetPickupWidget()->SetVisibility(false);
			}

			TraceHitItemLastFrame = TraceHitItem;
		}
	}
	else if (TraceHitItemLastFrame) TraceHitItemLastFrame->GetPickupWidget()->SetVisibility(false);
}

AWeapon* AShooterCharacter::SpawnDefaultWeapon()
{
	if (DefaultWeaponClass) return GetWorld()->SpawnActor<AWeapon>(DefaultWeaponClass);
	else return nullptr;
}

void AShooterCharacter::EquipWeapon(AWeapon* WeaponToEquip)
{
	if (WeaponToEquip)
	{
		// Get the hand socket
		const USkeletalMeshSocket* HandSocket = GetMesh()->GetSocketByName(FName("RightHandSocket"));
		// Attach the Weapon to the hand socket RightHandSocket
		if (HandSocket) HandSocket->AttachActor(WeaponToEquip, GetMesh());
		// Set EquippedWeapon to the newly spawned Weapon
		EquippedWeapon = WeaponToEquip;
		EquippedWeapon->SetItemState(EItemState::EIS_Equipped);
	}
}

void AShooterCharacter::DropWeapon()
{
	if (EquippedWeapon)
	{
		FDetachmentTransformRules DetachmentTransformRules(EDetachmentRule::KeepWorld, true);
		EquippedWeapon->GetItemMesh()->DetachFromComponent(DetachmentTransformRules);
		EquippedWeapon->SetItemState(EItemState::EIS_Falling);
		EquippedWeapon->ThrowWeapon();
	}
}

void AShooterCharacter::EquipButtonPressed()
{
	if (TraceHitItem) TraceHitItem->StartItemCurve(this);
}

void AShooterCharacter::SwapWeapon(AWeapon* WeapontoSwap)
{
	DropWeapon();
	EquipWeapon(WeapontoSwap);
	TraceHitItem = nullptr;
	TraceHitItemLastFrame = nullptr;
}


void AShooterCharacter::SetLookRates()
{
	bAiming ? BaseTurnRate = AimingTurnRate : BaseTurnRate = HipTurnRate;
	bAiming ? BaseLookUpRate = AimingLookUpRate : BaseLookUpRate = HipLookUpRate;
}

void AShooterCharacter::CalculateCrosshairSpread(float DeltaTime)
{
	FVector2D WalkSpeedRange{ 0.f, 600.f }, VelocityMultiplierRange{ 0.f, 1.f };
	FVector Velocity{ GetVelocity() };
	Velocity.Z = 0.f;

	// Calculate crosshair velocity factor
	CrosshairVelocityFactor = FMath::GetMappedRangeValueClamped(WalkSpeedRange, VelocityMultiplierRange, Velocity.Size());

	// Calculate crosshair in air factor
	(GetCharacterMovement()->IsFalling()) ? CrosshairInAirFactor = FMath::FInterpTo(CrosshairInAirFactor, 2.25f, DeltaTime, 2.25f) 
		: CrosshairInAirFactor = FMath::FInterpTo(CrosshairInAirFactor, 0.f, DeltaTime, 30.f);

	// Calculate crosshair aim factor
	bAiming ? CrosshairAimFactor = FMath::FInterpTo(CrosshairAimFactor, 0.6f, DeltaTime, 30.f)
		: CrosshairAimFactor = FMath::FInterpTo(CrosshairAimFactor, 0.f, DeltaTime, 30.f);

	// Calculate crosshair shooting factor
	bFiringBullet ? CrosshairShootingFactor = FMath::FInterpTo(CrosshairShootingFactor, 0.3f, DeltaTime, 60.f)
		: CrosshairShootingFactor = FMath::FInterpTo(CrosshairShootingFactor, 0.0f, DeltaTime, 60.f);

	CrosshairSpreadMultiplier = 0.5f + CrosshairVelocityFactor + CrosshairInAirFactor - CrosshairAimFactor + CrosshairShootingFactor;
}

void AShooterCharacter::StartCrosshairBulletFire()
{
	bFiringBullet = true;
	GetWorldTimerManager().SetTimer(CrosshairShooterTimer, this, &AShooterCharacter::FinishCrosshairBulletFire, ShootTimeDuration);
}

void AShooterCharacter::FinishCrosshairBulletFire()
{
	bFiringBullet = false;
}

void AShooterCharacter::FireButtonPressed()
{
	bFireButtonPressed = true;
	StartFireWeaponTimer();
}

void AShooterCharacter::FireButtonReleased()
{
	bFireButtonPressed = false;
}

void AShooterCharacter::StartFireWeaponTimer()
{
	if (bShouldFire)
	{
		FireWeapon();
		bShouldFire = false;
		GetWorldTimerManager().SetTimer(AutoFireTimer, this, &AShooterCharacter::AutoFireReset, AutomaticFireRate);
	}
}

void AShooterCharacter::AutoFireReset()
{
	bShouldFire = true;
	if (bFireButtonPressed) StartFireWeaponTimer();
}

bool AShooterCharacter::TraceUnderCrosshairs(FHitResult& OutHitResult, FVector& OutHitLocation)
{
	// Get current size of the viewport
	FVector2D ViewportSize;
	if (GEngine && GEngine->GameViewport) GEngine->GameViewport->GetViewportSize(ViewportSize);

	// Get screen space location of crosshairs
	FVector2D CrosshairLocation(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);
	FVector CrosshairWorldPosition, CrosshairWorldDirection;

	// Get world position and direction of crosshairs
	bool bScreenToWorld = UGameplayStatics::DeprojectScreenToWorld(UGameplayStatics::GetPlayerController(this, 0), CrosshairLocation, CrosshairWorldPosition, CrosshairWorldDirection);
	if (bScreenToWorld)
	{
		// Trace from crosshair world location outward
		const FVector Start{ CrosshairWorldPosition }, End{ Start + CrosshairWorldDirection * 50'000.f };
		OutHitLocation = End;
		GetWorld()->LineTraceSingleByChannel(OutHitResult, Start, End, ECC_Visibility);

		if (OutHitResult.bBlockingHit)
		{
			OutHitLocation = OutHitResult.Location;
			return true;
		}
	}

	return false;
}

void AShooterCharacter::CameraInterpZoom(float DeltaTime)
{
	bAiming ? CameraCurrentFOV = FMath::FInterpTo(CameraCurrentFOV, CameraZoomedFOV, DeltaTime, ZoomInterpSpeed)
		: CameraCurrentFOV = FMath::FInterpTo(CameraCurrentFOV, CameraDefaultFOV, DeltaTime, ZoomInterpSpeed);
	GetFollowCamera()->SetFieldOfView(CameraCurrentFOV);
}

// Called to bind functionality to input
void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	check(PlayerInputComponent);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
	PlayerInputComponent->BindAction("FireWeapon", IE_Pressed, this, &AShooterCharacter::FireButtonPressed);
	PlayerInputComponent->BindAction("FireWeapon", IE_Released, this, &AShooterCharacter::FireButtonReleased);
	PlayerInputComponent->BindAction("TakeAim", IE_Pressed, this, &AShooterCharacter::AimingButtonPressed);
	PlayerInputComponent->BindAction("TakeAim", IE_Released, this, &AShooterCharacter::AimingButtonReleased);
	PlayerInputComponent->BindAction("Equip", IE_Pressed, this, &AShooterCharacter::EquipButtonPressed);

	PlayerInputComponent->BindAxis("MoveForward", this, &AShooterCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AShooterCharacter::MoveRight);
	PlayerInputComponent->BindAxis("TurnRate", this, &AShooterCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUpAtRate", this, &AShooterCharacter::LookUpAtRate);
	PlayerInputComponent->BindAxis("Turn", this, &AShooterCharacter::Turn);
	PlayerInputComponent->BindAxis("LookUp", this, &AShooterCharacter::LookUp);
}

float AShooterCharacter::GetCrosshairSpreadMultipler() const
{
	return CrosshairSpreadMultiplier;
}

void AShooterCharacter::IncrementOverlappedItemCount(int8 Amount)
{
	if (OverlappedItemCount + Amount <= 0)
	{
		OverlappedItemCount = 0;
		bShouldTraceForItems = false;
	}
	else
	{
		OverlappedItemCount += Amount;
		bShouldTraceForItems = true;
	}
}

FVector AShooterCharacter::GetCameraInterpLocation()
{
	const FVector CameraWorldLocation{ FollowCamera->GetComponentLocation() }, CameraForward{ FollowCamera->GetForwardVector() };

	return CameraWorldLocation + CameraForward * CameraInterpDistance + FVector(0.f, 0.f, CameraInterpElevation);
}

void AShooterCharacter::GetPickupItem(AItem* Item)
{
	auto Weapon = Cast<AWeapon>(Item);
	if (Weapon) SwapWeapon(Weapon);
}

