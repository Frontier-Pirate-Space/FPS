#include "AIBaseController.h"
#include "AIBase.h"
#include "Perception/AIPerceptionComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Sense.h"
#include "../State/AI/AIStateMachine.h"
#include "../Gameplay/HealthComponent.h"
#include "Components/SplineComponent.h"

AAIBaseController::AAIBaseController()
{
	PrimaryActorTick.bCanEverTick = true;

	PerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));

	//Maybe each controller should choose their own default configs?
	//Register the corresponding perception sense on the component
	UAISenseConfig_Sight* SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("Sight Config"));
	UAISenseConfig_Hearing* HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("Hearing Config"));
	UAISenseConfig_Damage* DamageConfig = CreateDefaultSubobject<UAISenseConfig_Damage>(TEXT("Damage Config"));
	UAISenseConfig_Touch* TouchConfig = CreateDefaultSubobject<UAISenseConfig_Touch>(TEXT("Touch Config"));

	PerceptionComponent->ConfigureSense(*SightConfig);
	PerceptionComponent->ConfigureSense(*HearingConfig);
	PerceptionComponent->ConfigureSense(*DamageConfig);
	PerceptionComponent->ConfigureSense(*TouchConfig);
	PerceptionComponent->SetDominantSense(UAISense_Sight::StaticClass());

	TrackVolume = 80.f;
	InvestigateVolume = 50.f;
	FollowVolume = 100.f;
	AggressionDuration = 20.f;
	FollowDuration = 15.f;
	AnnoyanceThreshold = 3;
	
	MinimumHearingThreshold = 50.f;
	MinimumPropagationDistance = 10000.f;
	MinimumIntensityThreshold = 10e-12f;
}

void AAIBaseController::BeginPlay()
{
	Super::BeginPlay();
	AIOwner = Cast<AAIBase>(GetPawn());

	StateMachine = NewObject<UAIStateMachine>();
	StateMachine->Initialize(this);
	LoadStateTrees();

	AIOwner->GetHealth()->OnDeath.AddDynamic(this, &AAIBaseController::OnOwnerDeath);
	PerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &AAIBaseController::DispatchStimulus);

	AActor* const PatrolActor = AIOwner->GetPatrolActor();
	bool bHasPatrolPath = false;

	if (PatrolActor != nullptr)
	{
		PatrolPath = PatrolActor->FindComponentByClass<USplineComponent>();
		bHasPatrolPath = PatrolPath != nullptr;
	}
	
	if (!bHasPatrolPath && Home.IsNearlyZero())
	{
		Home = AIOwner->GetActorLocation();
	}

	Blackboard->SetValueAsBool("bHasPatrolPath", bHasPatrolPath);
	Blackboard->SetValueAsVector("HomeLocation", Home);
}

void AAIBaseController::LoadStateTrees()
{
	TMap<FString, UBehaviorTree*> TreeMap;

	TreeMap.Add("Patrol", PatrolTree);
	TreeMap.Add("Attack", AttackTree);
	TreeMap.Add("Agitated", AgitatedTree);
	TreeMap.Add("Investigate", InvestigationTree);
	TreeMap.Add("Track", TrackTree);
	TreeMap.Add("Follow", FollowTree);

	StateMachine->PostInitLoadTrees(TreeMap);
}

void AAIBaseController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	StateMachine->Tick(DeltaTime);
}

void AAIBaseController::OnOwnerDeath()
{
	//TODO unregister senses, blackboards, etc

	//FAISenseID HearingSenseID = UAISense::GetSenseID<
	//GetBrainComponent()->Cleanup();
	//Perception->UpdatePerceptionWhitelist(FAISenseID::FAINamedID("Sight"), false);
}

void AAIBaseController::SetState(const FString Key)
{
	if (StateMachine->GetActiveState() != StateMachine->GetStateAtKey(Key))
	{
		StateMachine->SetState(Key);
	}
}

void AAIBaseController::ClearSearchParameters()
{
	Annoyance = 0;
	InvestigationScore = 0;
}

void AAIBaseController::SetSearchParameters(const bool bUseTarget)
{
	Annoyance = 0;
	const FVector MoveLocation = bUseTarget ? Target->GetActorLocation() : InvestigationLocation;
	Blackboard->SetValueAsVector("MoveLocation", MoveLocation);
	Blackboard->SetValueAsVector("InvestigationLocation", InvestigationLocation);
}

void AAIBaseController::SetAggression(const bool Value)
{
	if (bAggressive != Value)
	{
		OnAggression.Broadcast(Value);
		bAggressive = Value;
	}	
}

void AAIBaseController::SetInvestigation(const bool Value)
{
	if (bInvestigating != Value)
	{
		OnInvestigation.Broadcast(Value);
		bInvestigating = Value;
	}
}

void AAIBaseController::SetStartingSearch(const bool Value)
{
	Blackboard->SetValueAsBool("bStartingSearch", Value);
}

void AAIBaseController::SetMoveLocation(const FVector Location)
{
	Blackboard->SetValueAsVector("MoveLocation", Location);
}

void AAIBaseController::DispatchStimulus(AActor* Invoker, FAIStimulus Stimulus)
{
	const FAISenseID ID = Stimulus.Type;

	if (ID == UAISense::GetSenseID(UAISense_Damage::StaticClass()) || ID == UAISense::GetSenseID(UAISense_Touch::StaticClass()))
	{
		ReactToPhysical(Invoker, Stimulus);
	}

	else if (ID == UAISense::GetSenseID(UAISense_Hearing::StaticClass()))
	{
		//Perception system seems to have a weird quirk of picking up random sounds of strength 1
		//Discard these
		if (Stimulus.Strength == 1)
		{
			return;
		}
		ReactToSound(Invoker, Stimulus);
	}

	else if (ID == UAISense::GetSenseID(UAISense_Sight::StaticClass()))
	{
		ReactToSight(Invoker, Stimulus);
	}
}

void AAIBaseController::ReactToSight(AActor* const Invoker, const FAIStimulus& Stimulus)
{
	bHasVisionOfTarget = Stimulus.WasSuccessfullySensed();

	if (bHasVisionOfTarget)
	{
		SetState("Attack");
		SetAggression(true);
	}

	else
	{
		LastAgressiveTime = GetWorld()->GetTimeSeconds();
	}
}

void AAIBaseController::ReactToPhysical(AActor* const Invoker, const FAIStimulus& Stimulus)
{
	//not very extensible
	if (Invoker == Target)
	{
		SetState("Attack");
		SetAggression(true);
		LastAgressiveTime = GetWorld()->GetTimeSeconds();
	}
}

void AAIBaseController::ReactToSound(AActor* const Invoker, const FAIStimulus& Stimulus)
{
	//Not extensible, we should use a weighting system
	if (StateMachine->GetActiveState() != StateMachine->GetStateAtKey("Attack"))
	{
		OnHeardSound.Broadcast(Stimulus.StimulusLocation, Stimulus.Strength);
	}
}

bool AAIBaseController::DetermineSearch(const FVector SourceLocation, const float SourceVolume)
{
	const float Score = CalculateAudioScore(SourceLocation, SourceVolume);

	if (Score >= MinimumHearingThreshold && Score > InvestigationScore)
	{
		DetermineSearchInternal(SourceLocation, Score);
	}

	return false;
}

void AAIBaseController::DetermineSearchInternal(const FVector& SourceLocation, const float IncomingScore)
{
	InvestigationScore = IncomingScore;
	InvestigationLocation = SourceLocation;

	UState* const Current = StateMachine->GetActiveState();
	FString Staged = "";

	if (IncomingScore >= FollowVolume) Staged = "Follow";
	else if (IncomingScore >= TrackVolume) Staged = "Track";
	else if(IncomingScore >= InvestigateVolume) Staged = "Investigate";

	//Higher volume state, switch to it
	if (Current != StateMachine->GetStateAtKey(Staged))
	{
		SetState(Staged);
		return;
	}

	else
	{
		Annoyance++;
	}

	//The AI is now annoyed, increase search aggression
	if (Annoyance > AnnoyanceThreshold)
	{
		//TODO
	}
}

float AAIBaseController::CalculateAudioScore(const FVector& SourceLocation, float Volume)
{
	// Calculate the relative sound pressure from the source using an inverse linear relationship
	// Essentially, everytime the distance from the minimum propagation distance (100 units) doubles
	// The relative intensity of the audio source is cut in half. Distance to relative volume are as follows
	// 100 units - Full volume
	// 200 units - 1/2 volume
	// 400 units - 1/4 volume
	// We use decibels here so design has an easier time understanding how loud a sound really was to the listener

	const float SquareDistance = FVector::DistSquared(SourceLocation, GetOwner()->GetActorLocation());
	float RawIntensity = MinimumIntensityThreshold * FMath::Pow(10.f, Volume / 10.f);

	if (SquareDistance > MinimumPropagationDistance)
	{
		RawIntensity = RawIntensity * MinimumPropagationDistance / SquareDistance;
	}

	const float OverNaught = RawIntensity / MinimumIntensityThreshold;
	const float Decibel = 10.f * FMath::LogX(10, OverNaught);

	return Decibel;
}

FVector AAIBaseController::GetNavigablePatrolPoint() const
{
	return PatrolPath->GetLocationAtSplinePoint(PatrolIndex, ESplineCoordinateSpace::World);
}

int32 AAIBaseController::GetNextPatrolPoint(const int32 CurrentPoint) const
{
	const int32 PointCount = PatrolPath->GetNumberOfSplinePoints();

	if (PatrolPath->IsClosedLoop())
	{
		//We are at the last point in the loop, move back to the start
		if (CurrentPoint == PointCount - 1)
		{
			return 0;
		}

		//Progress to the next point
		else
		{
			return CurrentPoint + 1;
		}
	}

	else
	{
		if (!bIsReversedOnPath)
		{
			//We are at the last point, turn around
			if (CurrentPoint == PointCount - 1)
			{
				return PointCount - 2;
			}

			//Just advanced forwards
			else
			{
				return CurrentPoint + 1;
			}
		}

		else
		{
			//We reached the beginning point, turn around
			if (CurrentPoint == 0)
			{
				return 1;
			}

			//Move towards the first point
			else
			{
				return CurrentPoint - 1;
			}
		}
	}
}

void AAIBaseController::UpdatePatrolIndex()
{
	const int32 Previous = PatrolIndex;
	PatrolIndex = GetNextPatrolPoint(PatrolIndex);

	if (!bIsReversedOnPath && PatrolIndex < Previous)
	{
		bIsReversedOnPath = true;
	}

	else if (bIsReversedOnPath && PatrolIndex > Previous)
	{
		bIsReversedOnPath = false;
	}
}
