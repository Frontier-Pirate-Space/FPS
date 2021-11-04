// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include <Runtime/Engine/Classes/Components/BoxComponent.h>
#include "BarkTrigger.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FShowBarkEvent, FString, PrefixText, FString, SuffixText, FString, KeyText);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHideBarkEvent);

class UUserWidget;

UCLASS()
class UNREALPLAYGROUND_API ABarkTrigger : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ABarkTrigger();

	virtual void BeginPlay() override;

	void Tick(float DeltaTime) override;

	/** The actor that will activate this trigger.Set this to shooter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	AActor* TargetActor;

	/** Used For the first text in a Bark UI display. I.E. 'Press' */
	UPROPERTY(EditAnywhere)
	FString PrefixText;

	/** Used for the key name in a Bark UI display. I.E. 'C'*/
	UPROPERTY(EditAnywhere)
	FString KeyText;

	/** Used For the last text in a Bark UI display.I.E. 'To Crouch' */
	UPROPERTY(EditAnywhere)
	FString SuffixText;

private:

	void ShowUIPrompt();

	void HideUIPrompt();

	// Overlap
	UFUNCTION()
	void BeginOverlap(AActor* OverlappedActor, AActor* OtherActor);

	UFUNCTION()
	void EndOverlap(AActor* OverlappedActor, AActor* OtherActor);

	// show delegate
	UPROPERTY(BlueprintAssignable)
	FShowBarkEvent ShowBarkEvent;

	// hide ui delegate
	UPROPERTY(BlueprintAssignable)
	FHideBarkEvent HideBarkEvent;
};
