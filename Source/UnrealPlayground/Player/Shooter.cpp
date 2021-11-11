#include "Shooter.h"
#include "Components/InputComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "../Utility/DelayedActionManager.h"
#include "../ShooterGameMode.h"
#include "../State/FPS/ShooterStateMachine.h"
#include "../Gameplay/HealthComponent.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "../Gameplay/VaultTrigger.h"
#include "../Gameplay/InteractiveObject.h"
#include "../Gameplay/MeleeComponent.h"
#include "../Weapon/Gun.h"
#include "../Gameplay/HealthPickup.h"

void FShooterInput::Tick(const float DeltaTime)
{
	bIsTryingToCrouch = false;
	bIsTryingToProne = false;

	HandleCrouchInputStates(DeltaTime);
}

void FShooterInput::HandleCrouchInputStates(const float DeltaTime)
{
	if (bIsHoldingCrouch)
	{
		CurrentCrouchHoldTime += DeltaTime;
		if (CurrentCrouchHoldTime >= Owner->ShooterMovement->ProneInputTime)
		{
			CurrentCrouchHoldTime = 0;
			bIsHoldingCrouch = false;
			bIsTryingToProne = true;
		}
	}

	else
	{
		if (bWasHoldingCrouch)
		{
			bIsTryingToCrouch = true;
			CurrentCrouchHoldTime = 0;
		}
	}

	bWasHoldingCrouch = bIsHoldingCrouch;
}


AShooter::AShooter()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessPlayer = EAutoReceiveInput::Player0;

	Collider = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Collider"));
	CameraAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("Anchor"));
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	ShooterMovement = CreateDefaultSubobject<UShooterMovementComponent>(TEXT("Movement"));
	Melee = CreateDefaultSubobject<UMeleeComponent>(TEXT("Melee"));

	SetRootComponent(Collider);
	CameraAnchor->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	Camera->AttachToComponent(CameraAnchor, FAttachmentTransformRules::KeepRelativeTransform);
	Melee->AttachToComponent(Camera, FAttachmentTransformRules::KeepRelativeTransform);

	Collider->SetCollisionProfileName("Pawn");
	Collider->SetCapsuleHalfHeight(ShooterMovement->StandingHeight);
	Collider->SetCapsuleRadius(ShooterMovement->CollisionRadius);
	CameraAnchor->SetRelativeLocation(FVector(0, 0, ShooterMovement->CameraHeight));

	ShooterSkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Arms"));
	ShooterSkeletalMesh->AttachToComponent(Camera, FAttachmentTransformRules::KeepRelativeTransform);

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(ShooterSkeletalMesh, TEXT("WeaponSocket"));

	Combat = CreateDefaultSubobject<UCombatComponent>(TEXT("Combat"));
	Combat->SetUpConstruction(Camera, WeaponMesh, &InputState);

	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	PerceptionSource = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("Stimulus Source"));
}

void AShooter::BeginPlay()
{
	Super::BeginPlay();

	Collider->SetCapsuleHalfHeight(ShooterMovement->StandingHeight);
	Collider->SetCapsuleRadius(ShooterMovement->CollisionRadius);
	CameraAnchor->SetRelativeLocation(FVector(0, 0, ShooterMovement->CameraHeight));

	StateMachine = NewObject<UShooterStateMachine>();
	StateMachine->Initialize(this);
	OnStateLoadComplete.Broadcast();

	InputState.Owner = this;

	Combat->OnArsenalAddition.AddUObject(this, &AShooter::LoadAmmoOnWeaponGet);
	OnActorBeginOverlap.AddDynamic(this, &AShooter::OnTriggerEnter);
	OnActorEndOverlap.AddDynamic(this, &AShooter::OnTriggerExit);
	Health->OnDeath.AddDynamic(this, &AShooter::HandleDeath);
}

void AShooter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	InputState.Tick(DeltaTime);
	StateMachine->Tick(DeltaTime);
	ScanInteractiveObject();
}

void AShooter::ScanInteractiveObject()
{
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	const FVector TraceStart = Camera->GetComponentLocation();
	const FVector TraceEnd = TraceStart + Camera->GetForwardVector() * FMath::Min(ShooterMovement->StandingHeight * 2.f, ShooterMovement->InteractionDistance);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(ScanHitResult, TraceStart, TraceEnd, ECC_Camera, QueryParams);

	//We're looking at an object that is interactive
	if (bHit && ScanHitResult.Actor != nullptr && ScanHitResult.Actor->Implements<UInteractiveObject>())
	{
		//HACK THIS
		if (ScanHitResult.Actor->IsA(AHealthPickup::StaticClass()) && Health->bIsFullHealth)
		{
			return;
		}

		IInteractiveObject* InteractiveObjec = Cast<IInteractiveObject>(ScanHitResult.Actor);

		if (InteractiveObjec->bCanInteract)
		{
			//Lets us do UI things in blueprint
			OnScanHit.Broadcast(ScanHitResult);

			bIsScanningInteractiveObject = true;
		}

		if (InputState.bIsTryingToInteract)
		{
			IInteractiveObject* InteractiveObject = Cast<IInteractiveObject>(ScanHitResult.Actor);
			InteractiveObject->InteractionEvent(this);

			InputState.bIsTryingToInteract = false;
		}
	}
	else
	{
		/* Check if shooter was previously scanning an interactive object first.
		Otherwise OnScanMiss will always fire. */
		if (bIsScanningInteractiveObject)
		{
			OnScanMiss.Broadcast(ScanHitResult);
			bIsScanningInteractiveObject = false;
		}
	}
}

void AShooter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	InputComponent->BindAxis("MoveX", this, &AShooter::MoveInputX);
	InputComponent->BindAxis("MoveY", this, &AShooter::MoveInputY);
	InputComponent->BindAxis("LookX", this, &AShooter::LookInputX);
	InputComponent->BindAxis("LookY", this, &AShooter::LookInputY);
	InputComponent->BindAction("Vault", IE_Pressed, this, &AShooter::VaultPress);
	InputComponent->BindAction("Vault", IE_Released, this, &AShooter::VaultRelease);
	InputComponent->BindAction("Crouch", IE_Pressed, this, &AShooter::CrouchPress);
	InputComponent->BindAction("Crouch", IE_Released, this, &AShooter::CrouchRelease);
	InputComponent->BindAction("Aim", IE_Pressed, this, &AShooter::AimPress);
	InputComponent->BindAction("Aim", IE_Released, this, &AShooter::AimRelease);
	InputComponent->BindAction("Interact", IE_Pressed, this, &AShooter::InteractPress);
	InputComponent->BindAction("Interact", IE_Released, this, &AShooter::InteractRelease);
	InputComponent->BindAction("Swap Up", IE_Pressed, this, &AShooter::SwapPressUp);
	InputComponent->BindAction("Swap Down", IE_Pressed, this, &AShooter::SwapPressDown);
	InputComponent->BindAction("Shoot", IE_Pressed, this, &AShooter::ShootPress);
	InputComponent->BindAction("Shoot", IE_Released, this, &AShooter::ShootRelease);
	InputComponent->BindAction("Sprint", IE_Pressed, this, &AShooter::SprintPress);
	InputComponent->BindAction("Sprint", IE_Released, this, &AShooter::SprintRelease);
	InputComponent->BindAction("Reload", IE_Pressed, this, &AShooter::ReloadPress);
	InputComponent->BindAction("Reload", IE_Released, this, &AShooter::ReloadRelease);
	// bind pause to game mode because pausing is not a shooter behavior
	InputComponent->BindAction("Pause", IE_Pressed, GetWorld()->GetAuthGameMode<AShooterGameMode>(), &AShooterGameMode::PauseGame)
		.bExecuteWhenPaused = true;;
}

void AShooter::OnTriggerEnter(AActor* OverlappedActor, AActor* OtherActor)
{
	if (OtherActor->IsA(AVaultTrigger::StaticClass()))
	{
		bIsInsideVaultTrigger = true;
	}
}

void AShooter::OnTriggerExit(AActor* OverlappedActor, AActor* OtherActor)
{
	if (OtherActor->IsA(AVaultTrigger::StaticClass()))
	{
		bIsInsideVaultTrigger = false;
		// force player to stop being able to scan vault object by broadcasting a miss scan. Is there a better way we could do this?
		OnScanMiss.Broadcast(ScanHitResult);
	}
}

void AShooter::ShooterMakeNoise(FVector Location, float Volume)
{
	OnMakeNoise.Broadcast(Location, Volume);
}

void AShooter::MakeSound(const float Volume)
{
	FHitResult SoundHit;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	const FVector TraceStart = Camera->GetComponentLocation();
	const FVector TraceEnd = TraceStart + Camera->GetForwardVector() * 10000.f;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(SoundHit, TraceStart, TraceEnd, ECC_Camera, QueryParams);

	if (bHit)
	{
		ShooterMakeNoise(SoundHit.ImpactPoint, Volume);
	}
}

void AShooter::HandleDeath()
{
	StateMachine->SetState("Death");
}

bool AShooter::GetCanVault()
{
	return bIsInsideVaultTrigger && bIsLookingAtVaultObject;
}

void AShooter::LoadAmmoOnPickup(const EGunClass GunType)
{
	LoadAmmoOnWeaponGet(Combat->GetGunOfType(GunType));
}

void AShooter::LoadAmmoOnWeaponGet(AWeapon* NewWeapon)
{
	AGun* WeaponAsGun = Cast<AGun>(NewWeapon);

	if (WeaponAsGun != nullptr)
	{
		switch (WeaponAsGun->GunClass)
		{
		case WC_Pistol:
			WeaponAsGun->AddExcessAmmo(Inventory.PistolAmmo);
			Inventory.PistolAmmo = 0;
		case WC_Shotgun:
			WeaponAsGun->AddExcessAmmo(Inventory.ShotgunAmmo);
			Inventory.ShotgunAmmo = 0;
		case WC_Rifle:
			WeaponAsGun->AddExcessAmmo(Inventory.RifleAmmo);
			Inventory.RifleAmmo = 0;
		}
	}
}

bool AShooter::HasGunOfType(const EGunClass GunType) const
{
	return Combat->GetGunOfType(GunType) != nullptr;
}