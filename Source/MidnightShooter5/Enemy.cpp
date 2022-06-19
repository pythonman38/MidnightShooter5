// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "Particles/ParticleSystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/KismetMathLibrary.h"
#include "EnemyController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/SphereComponent.h"
#include "ShooterCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/SkeletalMeshSocket.h"

// Sets default values
AEnemy::AEnemy() :
	Health(100.f),
	MaxHealth(100.f),
	HealthBarDisplayTime(4.f),
	bCanHitReact(true),
	HitReactTimeMin(0.5f),
	HitReactTimeMax(0.75f),
	HitNumberDestroyTime(1.5f),
	bStunned(false),
	StunChance(0.75f),
	AttackLFast(TEXT("AttackLFast")),
	AttackRFast(TEXT("AttackRFast")),
	AttackL(TEXT("AttackL")),
	AttackR(TEXT("AttackR")),
	BaseDamage(20.f),
	LeftWeaponSocket(TEXT("FX_Trail_L_01")),
	RightWeaponSocket(TEXT("FX_Trail_R_01")),
	bCanAttack(true),
	AttackWaitTime(1.f),
	bDying(false),
	DeathTime(4.f)
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	AgroSphere = CreateDefaultSubobject<USphereComponent>(TEXT("AgroSphere"));
	AgroSphere->SetupAttachment(GetRootComponent());

	CombatRangeSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CombatRange"));
	CombatRangeSphere->SetupAttachment(GetRootComponent());

	LeftWeaponCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("LeftWeaponBox"));
	LeftWeaponCollision->SetupAttachment(GetMesh(), FName("LeftWeaponBone"));

	RightWeaponCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("RightWeaponBox"));
	RightWeaponCollision->SetupAttachment(GetMesh(), FName("RightWeaponBone"));
}

// Called when the game starts or when spawned
void AEnemy::BeginPlay()
{
	Super::BeginPlay();

	AgroSphere->OnComponentBeginOverlap.AddDynamic(this, &AEnemy::AgroSphereOverlap);

	CombatRangeSphere->OnComponentBeginOverlap.AddDynamic(this, &AEnemy::CombatSphereBeginOverlap);
	CombatRangeSphere->OnComponentEndOverlap.AddDynamic(this, &AEnemy::CombatSphereEndOverlap);

	LeftWeaponCollision->OnComponentBeginOverlap.AddDynamic(this, &AEnemy::OnWeaponLeftOverlap);
	RightWeaponCollision->OnComponentBeginOverlap.AddDynamic(this, &AEnemy::OnWeaponRightOverlap);

	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	LeftWeaponCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LeftWeaponCollision->SetCollisionObjectType(ECC_WorldDynamic);
	LeftWeaponCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	LeftWeaponCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	RightWeaponCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RightWeaponCollision->SetCollisionObjectType(ECC_WorldDynamic);
	RightWeaponCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	RightWeaponCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	EnemyController = Cast<AEnemyController>(GetController());

	const FVector WorldPatrolPoint = UKismetMathLibrary::TransformLocation(GetActorTransform(), PatrolPoint);
	const FVector WorldPatrolPoint2 = UKismetMathLibrary::TransformLocation(GetActorTransform(), PatrolPoint2);

	if (EnemyController)
	{
		EnemyController->GetBlackBoardComponent()->SetValueAsBool(FName("CanAttack"), true);
		EnemyController->GetBlackBoardComponent()->SetValueAsVector(TEXT("PatrolPoint"), WorldPatrolPoint);
		EnemyController->GetBlackBoardComponent()->SetValueAsVector(TEXT("PatrolPoint2"), WorldPatrolPoint2);
		EnemyController->RunBehaviorTree(BehaviorTree);
	}
}

void AEnemy::ShowHealthBar_Implementation()
{
	GetWorldTimerManager().ClearTimer(HealthBarTimer);
	GetWorldTimerManager().SetTimer(HealthBarTimer, this, &AEnemy::HideHealthBar, HealthBarDisplayTime);
}

void AEnemy::Die()
{
	if (bDying) return;
	bDying = true;

	HideHealthBar();

	auto AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && DeathMontage) AnimInstance->Montage_Play(DeathMontage);

	if (EnemyController)
	{
		EnemyController->GetBlackBoardComponent()->SetValueAsBool(FName("IsDead"), true);
		EnemyController->StopMovement();
	}

}

void AEnemy::PlayHitMontage(FName Section, float PlayRate)
{
	if (bCanHitReact)
	{
		auto AnimInstance = GetMesh()->GetAnimInstance();
		if (AnimInstance && HitMontage)
		{
			AnimInstance->Montage_Play(HitMontage, PlayRate);
			AnimInstance->Montage_JumpToSection(Section, HitMontage);
		}

		bCanHitReact = false;
		const float HitReactTime{ FMath::FRandRange(HitReactTimeMin, HitReactTimeMax) };
		GetWorldTimerManager().SetTimer(HitReactTimer, this, &AEnemy::ResetHitReactTimer, HitReactTime);
	}
}

void AEnemy::ResetHitReactTimer()
{
	bCanHitReact = true;
}

void AEnemy::StoreHitNumber(UUserWidget* HitNumber, FVector Location)
{
	HitNumbers.Add(HitNumber, Location);

	FTimerHandle HitNumberTimer;
	FTimerDelegate HitNumberDelegate;
	HitNumberDelegate.BindUFunction(this, FName("DestroyHitNumber"), HitNumber);
	GetWorld()->GetTimerManager().SetTimer(HitNumberTimer, HitNumberDelegate, HitNumberDestroyTime, false);
}

void AEnemy::DestroyHitNumber(UUserWidget* HitNumber)
{
	HitNumbers.Remove(HitNumber);
	HitNumber->RemoveFromParent();
}

void AEnemy::UpdateHitNumbers()
{
	for (auto& HitPair : HitNumbers)
	{
		UUserWidget* HitNumber{ HitPair.Key };
		const FVector Location{ HitPair.Value };
		FVector2D ScreenPosition;
		UGameplayStatics::ProjectWorldToScreen(GetWorld()->GetFirstPlayerController(), Location, ScreenPosition);
		HitNumber->SetPositionInViewport(ScreenPosition);
	}
}

void AEnemy::AgroSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor == nullptr) return;

	auto Character = Cast<AShooterCharacter>(OtherActor);
	if (Character)
	{
		if (EnemyController)
		{
			if (EnemyController->GetBlackBoardComponent()) EnemyController->GetBlackBoardComponent()->SetValueAsObject(TEXT("Target"), Character);
		}
	}
}

void AEnemy::SetStunned(bool Stunned)
{
	bStunned = Stunned;

	if (EnemyController) EnemyController->GetBlackBoardComponent()->SetValueAsBool(TEXT("Stunned"), Stunned);
}

void AEnemy::CombatSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor == nullptr) return;

	auto ShooterCharacter = Cast <AShooterCharacter>(OtherActor);
	if (ShooterCharacter)
	{
		bInAttackRange = true;

		if (EnemyController) EnemyController->GetBlackBoardComponent()->SetValueAsBool(TEXT("InAttackRange"), true);
	}
}

void AEnemy::CombatSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor == nullptr) return;

	auto ShooterCharacter = Cast<AShooterCharacter>(OtherActor);
	if (ShooterCharacter)
	{
		bInAttackRange = false;

		if (EnemyController) EnemyController->GetBlackBoardComponent()->SetValueAsBool(TEXT("InAttackRange"), false);
	}
}

void AEnemy::PlayAttackMontage(FName Section, float PlayRate)
{
	auto AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && AttackMontage)
	{
		AnimInstance->Montage_Play(AttackMontage);
		AnimInstance->Montage_JumpToSection(Section, AttackMontage);
	}

	bCanAttack = false;
	GetWorldTimerManager().SetTimer(AttackWaitTimer, this, &AEnemy::ResetCanAttack, AttackWaitTime);

	if (EnemyController) EnemyController->GetBlackBoardComponent()->SetValueAsBool(FName("CanAttack"), false);
}

FName AEnemy::GetAttackSectionName()
{
	FName SectionName;
	const int32 Section{ FMath::RandRange(1, 4) };
	switch (Section)
	{
	case 1:
		SectionName = AttackLFast;
		break;
	case 2:
		SectionName = AttackRFast;
		break;
	case 3:
		SectionName = AttackL;
		break;
	case 4:
		SectionName = AttackR;
		break;
	}

	return SectionName;
}

void AEnemy::DoDamage(AShooterCharacter* Target)
{
	if (Target == nullptr) return;

	UGameplayStatics::ApplyDamage(Target, BaseDamage, EnemyController, this, UDamageType::StaticClass());

	if (Target->GetMeleeImpactSound()) UGameplayStatics::PlaySoundAtLocation(this, Target->GetMeleeImpactSound(), GetActorLocation());
}

void AEnemy::OnWeaponLeftOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	auto Character = Cast<AShooterCharacter>(OtherActor);
	if (Character)
	{
		DoDamage(Character);

		SpawnBlood(Character, LeftWeaponSocket);

		StunCharacter(Character);
	}
}

void AEnemy::OnWeaponRightOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	auto Character = Cast<AShooterCharacter>(OtherActor);
	if (Character)
	{
		DoDamage(Character);

		SpawnBlood(Character, RightWeaponSocket);

		StunCharacter(Character);
	}
}

void AEnemy::SpawnBlood(AShooterCharacter* Target, FName SocketName)
{
	const USkeletalMeshSocket* TipSocket{ GetMesh()->GetSocketByName(SocketName) };
	if (TipSocket)
	{
		const FTransform SocketTransform{ TipSocket->GetSocketTransform(GetMesh()) };
		if (Target->GetBloodParticles()) UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Target->GetBloodParticles(), SocketTransform);
	}
}

void AEnemy::StunCharacter(AShooterCharacter* Target)
{
	if (Target)
	{
		const float Stun{ FMath::FRandRange(0.f, 1.f) };
		if (Stun <= Target->GetStunChance()) Target->Stun();
	}
}

void AEnemy::ResetCanAttack()
{
	bCanAttack = true;

	if (EnemyController) EnemyController->GetBlackBoardComponent()->SetValueAsBool(FName("CanAttack"), true);
}

void AEnemy::FinishDeath()
{
	GetMesh()->bPauseAnims = true;
	GetWorldTimerManager().SetTimer(DeathTimer, this, &AEnemy::DestroyEnemy, DeathTime);
}

void AEnemy::DestroyEnemy()
{
	Destroy();
}

void AEnemy::ActivateLeftWeapon()
{
	LeftWeaponCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void AEnemy::ActivateRightWeapon()
{
	RightWeaponCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void AEnemy::DeactivateLeftWeapon()
{
	LeftWeaponCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AEnemy::DeactivateRightWeapon()
{
	RightWeaponCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

// Called every frame
void AEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateHitNumbers();
}

// Called to bind functionality to input
void AEnemy::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

void AEnemy::BulletHit_Implementation(FHitResult HitResult, AActor* Shooter, AController* ShooterController)
{
	if (ImpactSound) UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, GetActorLocation());

	if (ImpactParticles) UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactParticles, HitResult.Location, FRotator(0.f), true);
}

float AEnemy::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (EnemyController) EnemyController->GetBlackBoardComponent()->SetValueAsObject(FName("Target"), DamageCauser);

	if (Health - DamageAmount <= 0.f)
	{
		Health = 0.f;
		Die();
	}
	else Health -= DamageAmount;

	if (bDying) return DamageAmount;

	ShowHealthBar();

	const float Stunned = FMath::FRandRange(0.f, 1.f);
	if (Stunned <= StunChance)
	{
		PlayHitMontage(FName("HitFront"));
		SetStunned(true);
	}

	return DamageAmount;
}

