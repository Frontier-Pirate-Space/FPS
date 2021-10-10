#include "ShooterGameMode.h"
#include "Utility/DelayedActionManager.h"
#include "ShooterController.h"
#include "ShooterHUD.h"
#include "Player/Shooter.h"
#include "AI/AICell.h"
#include "Gameplay/HealthComponent.h"

AShooterGameMode::AShooterGameMode()
{
	PrimaryActorTick.bCanEverTick = true;

	PlayerControllerClass = AShooterController::StaticClass();
	HUDClass = AShooterHUD::StaticClass();
}

void AShooterGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	//Super does not compile ?
	AGameModeBase::InitGame(MapName, Options, ErrorMessage);

	DelayedActionManager = NewObject<UDelayedActionManager>();
	DelayedActionManager->Initialize();

	//TODO - I HATE using this function EVER. Ensure serialization in final builds/levels
#if WITH_EDITOR
	if (Player == nullptr)
	{
		Player = FindActor<AShooter>(GetWorld());
	}

	if (AICells.Num() == 0)
	{
		FindAllActors<AAICell>(GetWorld(), AICells);
	}
#endif

	for (AAICell* Cell : AICells)
	{
		Cell->SetPlayer(Player);
	}

	Player->GetHealth()->OnDeath.AddDynamic(this, &AShooterGameMode::PlayerDeath);
}

void AShooterGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	DelayedActionManager->TickRunningActions(DeltaTime);
}

void AShooterGameMode::PlayerDeath()
{
	//Do reset pipeline stuff here when player dies
}