#include "AIAttackState.h"
#include "../../AI/AIBase.h"
#include "GameFramework/CharacterMovementComponent.h"

void UAIAttackState::Initialize(UStateMachine* StateMachine, UObject* ContextObject)
{
	Super::Initialize(StateMachine, ContextObject);
	Weight = 6;
}

void UAIAttackState::OnEnter()
{
	Super::OnEnter();
	AI->ClearSearchParameters();
	AI->SetMoveLocation(AI->GetTarget()->GetActorLocation());
	AI->SetLastAggressionTime(AI->GetWorld()->GetTimeSeconds());
	AI->SetAggression(true);

	AI->GetAIOwner()->GetCharacterMovement()->MaxWalkSpeed = AI->GetAIOwner()->AttackMoveSpeed;
}

void UAIAttackState::OnExit()
{
	Super::OnExit();

	AI->GetAIOwner()->GetCharacterMovement()->MaxWalkSpeed = AI->GetAIOwner()->DefaultMoveSpeed;
}

void UAIAttackState::Tick(const float DeltaTime)
{
	AI->SetMoveLocation(AI->GetTarget()->GetActorLocation());

	//It's been a long time since the target mad us angry. Stop being aggressive
	if (!AI->CanSeeTarget() && AI->GetWorld()->GetTimeSeconds() > AI->GetLastAggressionTime() + AI->AggressionDuration)
	{
		AI->SetAggression(false);
	}
}