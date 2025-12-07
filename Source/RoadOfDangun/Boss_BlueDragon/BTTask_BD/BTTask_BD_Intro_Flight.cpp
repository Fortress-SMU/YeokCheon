// BTTask_BD_Intro_Flight.cpp
#include "BTTask_BD_Intro_Flight.h"
#include "../BossBlueDragon.h"
#include "../DragonFlightComponent.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"

EBTNodeResult::Type UBTTask_BD_Intro_Flight::ExecuteTask(UBehaviorTreeComponent& Owner, uint8* NodeMemory)
{
	ABossBlueDragon* Boss = Cast<ABossBlueDragon>(Owner.GetAIOwner()->GetPawn());
	if (!Boss) return EBTNodeResult::Failed;

	if (UDragonFlightComponent* Flight = Boss->FindComponentByClass<UDragonFlightComponent>())
	{
		Flight->BeginIntroFlight(); // [Calls existing API] :contentReference[oaicite:6]{index=6}
		return EBTNodeResult::InProgress;
	}
	return EBTNodeResult::Failed;
}

void UBTTask_BD_Intro_Flight::TickTask(UBehaviorTreeComponent& Owner, uint8* NodeMemory, float DeltaSeconds)
{
	ABossBlueDragon* Boss = Cast<ABossBlueDragon>(Owner.GetAIOwner()->GetPawn());
	if (!Boss) { FinishLatentTask(Owner, EBTNodeResult::Failed); return; }

	if (UDragonFlightComponent* Flight = Boss->FindComponentByClass<UDragonFlightComponent>())
	{
		if (Flight->IsIntroFinished()) // [Uses existing API] :contentReference[oaicite:7]{index=7}
		{
			FinishLatentTask(Owner, EBTNodeResult::Succeeded);
		}
	}
}

EBTNodeResult::Type UBTTask_BD_Intro_Flight::AbortTask(UBehaviorTreeComponent& Owner, uint8* NodeMemory)
{
	// [Added] 인트로 중단 시 별도 정리 필요하면 여기에
	return EBTNodeResult::Aborted;
}
