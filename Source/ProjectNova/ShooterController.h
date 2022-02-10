#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GenericTeamAgentInterface.h"
#include "ShooterController.generated.h"

class AShooterHUD;

UCLASS()
class PROJECTNOVA_API AShooterController : public APlayerController, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

private:

	/** Cached cast version of MyHUD for ease of use*/
	AShooterHUD* ShooterHUD;

	/** Team ID used for AI threat/friend detection*/
	FGenericTeamId TeamID;

public:

	void ReceivePause();

	/// Begin APawn Interface ///

	void OnUnPossess() override;
	void OnPossess(APawn* InPawn) override;

	void SetGenericTeamId(const FGenericTeamId& NewTeamID) override;
	FGenericTeamId GetGenericTeamId() const override { return TeamID; }


private:

	/** Handle to pause input bind*/
	int32 BindingHandle;
};
