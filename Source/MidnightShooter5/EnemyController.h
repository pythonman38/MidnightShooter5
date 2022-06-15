// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EnemyController.generated.h"

/**
 * 
 */
UCLASS()
class MIDNIGHTSHOOTER5_API AEnemyController : public AAIController
{
	GENERATED_BODY()
	
public:
	// Sets default values for this class's properties
	AEnemyController();

	virtual void OnPossess(APawn* InPawn) override;

protected:


private:
	/* Blackboard component for this Enemy */
	UPROPERTY(BlueprintReadWrite, Category = "AI Behavior", meta = (AllowPrivateAccess = "true"))
	class UBlackboardComponent* BlackboardComponent;

	/* Behavior tree component for this Enemy*/
	UPROPERTY(BlueprintReadWrite, Category = "AI Behavior", meta = (AllowPrivateAccess = "true"))
	class UBehaviorTreeComponent* BehaviorTreeComponent;


public:
	// Getters for private variables
	FORCEINLINE UBlackboardComponent* GetBlackBoardComponent() const { return BlackboardComponent; }
};
