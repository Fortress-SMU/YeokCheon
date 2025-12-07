// BTTask_BD_Random_Flight.cpp
/*
#include "BTTask_BD_Random_Flight.h"
#include "AIController.h"
#include "../BossBlueDragon.h"
#include "../DragonFlightComponent.h"

EBTNodeResult::Type UBTTask_BD_Random_Flight::ExecuteTask(UBehaviorTreeComponent& Owner, uint8* NodeMemory)
{
	if (ABossBlueDragon* Boss = Cast<ABossBlueDragon>(Owner.GetAIOwner()->GetPawn()))
	{
		if (auto* Flight = Boss->FindComponentByClass<UDragonFlightComponent>())
		{
			Flight->SetRandomFlyTarget(); // [Uses existing API] :contentReference[oaicite:8]{index=8}
			return EBTNodeResult::InProgress;
		}
	}
	return EBTNodeResult::Failed;
}

void UBTTask_BD_Random_Flight::TickTask(UBehaviorTreeComponent& Owner, uint8* NodeMemory, float DeltaSeconds)
{
	if (ABossBlueDragon* Boss = Cast<ABossBlueDragon>(Owner.GetAIOwner()->GetPawn()))
	{
		if (auto* Flight = Boss->FindComponentByClass<UDragonFlightComponent>())
		{
			if (Flight->HasArrived()) // [Uses existing API] :contentReference[oaicite:9]{index=9}
			{
				FinishLatentTask(Owner, EBTNodeResult::Succeeded);
			}
		}
	}
}
*/