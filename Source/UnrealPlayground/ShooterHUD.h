#pragma once

#include "CoreMinimal.h"
#include "Gameplay/VaultObject.h"
#include "GameFramework/HUD.h"
#include "ShooterHUD.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUpdateHUD);

class AShooter;
class UCombatComponent;
class AWeapon;
class UUserWidget;
class AShooterGameMode;

UCLASS()
class UNREALPLAYGROUND_API AShooterHUD : public AHUD
{
	GENERATED_BODY()

public:
	AShooterHUD();

	void Initialize();

	void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable)
	void ShowDeathScreen();

protected:

	/** Invoked when a UI update is requested*/
	UPROPERTY(BlueprintAssignable)
	FUpdateHUD OnUpdate;

	/** Invoked when we need to push new data to the interaction prompt*/
	UPROPERTY(BlueprintAssignable)
	FUpdateHUD OnInteractionPromptChange;

	/** Invoked when the combat component adds a new weapon to the arsenal*/
	void ReceiveWeapon(AWeapon* NewWeapon);

	/** Invoked when the combat component removes a weapon from the arsenal*/
	void ReleaseWeapon(AWeapon* DiscardedWeapon);

	/** Called when OnUpdate is invoked. Updates member fields below so they can be read from blueprint*/
	void InternalUpdate();

	UFUNCTION()
	void ShowPauseMenu();

	void HideUI();

	uint8 bIsPaused : 1;

	/** Called when shooter looks at something interactable*/
	UFUNCTION(BlueprintCallable)
	void ShowInteractionPrompt(const FInteractionPrompt& Prompt);

	/** Called when shooter is not looking at something interactable*/
	UFUNCTION(BlueprintCallable)
	void HideInteractionPrompt(const FInteractionPrompt& Prompt);

	UPROPERTY(BlueprintReadWrite)
	UUserWidget* InteractionPromptWidget;

	UPROPERTY(BlueprintReadWrite)
	UUserWidget* PauseMenuWidget;

	UPROPERTY(BlueprintReadWrite)
	UUserWidget* DeathScreenWidget;

	UPROPERTY(BlueprintReadWrite)
	UUserWidget* ExitConfirmationWidget;

	UPROPERTY(BlueprintReadWrite)
	UUserWidget* SettingsMenuWidget;

	UPROPERTY(BlueprintReadWrite)
	UUserWidget* ControlsMenuWidget;

	/** Max ammo that can be stored in this weapon after reloading*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	int MaxAmmoInWeapon;

	/** Current amount of ammo ready to be fired*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	int AmmoInWeapon;

	/** The amount of extra ammo the player has*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	int ExcessAmmo;

	/** The amount of bloom currently attached to the combat component*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	float Bloom;

	UPROPERTY(BlueprintReadOnly)
	uint8 bPlayerHasGun : 1;

	UPROPERTY(BlueprintReadOnly)
	AShooter* Shooter;

	/** Prompt data for the last interactive object that was scanned*/
	UPROPERTY(BlueprintReadOnly)
	FInteractionPrompt LastScan;

private:

	/** Combat component attached to player*/
	UCombatComponent* Combat;

	uint8 bInteractionPromptActive : 1;
};
