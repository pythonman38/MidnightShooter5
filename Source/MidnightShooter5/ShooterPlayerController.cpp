// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterPlayerController.h"
#include "Blueprint/UserWidget.h"

AShooterPlayerController::AShooterPlayerController()
{
}

void AShooterPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Check out HUD Overlay Class TSubclassOf variable
	if (HUD_OverlayClass)
	{
		HUD_Overlay = CreateWidget<UUserWidget>(this, HUD_OverlayClass);
		if (HUD_Overlay)
		{
			HUD_Overlay->AddToViewport();
			HUD_Overlay->SetVisibility(ESlateVisibility::Visible);
		}
	}
}
