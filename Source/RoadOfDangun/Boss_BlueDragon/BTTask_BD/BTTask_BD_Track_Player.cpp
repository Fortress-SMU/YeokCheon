// BTTask_BD_Track_Player.cpp
#include "BTTask_BD_Track_Player.h"
#include "AIController.h"
#include "../BossBlueDragon.h"
#include "../DragonFlightComponent.h"

EBTNodeResult::Type UBTTask_BD_Track_Player::ExecuteTask(UBehaviorTreeComponent& Owner, uint8* NodeMemory)
{
	if (ABossBlueDragon* Boss = Cast<ABossBlueDragon>(Owner.GetAIOwner()->GetPawn()))
	{
		if (auto* Flight = Boss->FindComponentByClass<UDragonFlightComponent>())
		{
			Flight->StartTrackPlayer(false/*bFireballMode*/); // [Uses existing API] :contentReference[oaicite:10]{index=10}
			Elapsed = 0.f;
			return EBTNodeResult::InProgress;
		}
	}
	return EBTNodeResult::Failed;
}

void UBTTask_BD_Track_Player::TickTask(UBehaviorTreeComponent& Owner, uint8* NodeMemory, float DeltaSeconds)
{
	Elapsed += DeltaSeconds;
	if (Elapsed >= TrackDuration)
	{
		if (ABossBlueDragon* Boss = Cast<ABossBlueDragon>(Owner.GetAIOwner()->GetPawn()))
		{
			if (auto* Flight = Boss->FindComponentByClass<UDragonFlightComponent>())
			{
				Flight->StopTrackPlayer(); // [Clean up] :contentReference[oaicite:11]{index=11}
			}
		}
		FinishLatentTask(Owner, EBTNodeResult::Succeeded);
	}
}

EBTNodeResult::Type UBTTask_BD_Track_Player::AbortTask(UBehaviorTreeComponent& Owner, uint8* NodeMemory)
{
	if (ABossBlueDragon* Boss = Cast<ABossBlueDragon>(Owner.GetAIOwner()->GetPawn()))
	{
		if (auto* Flight = Boss->FindComponentByClass<UDragonFlightComponent>())
		{
			Flight->StopTrackPlayer(); // [Abort clean up]
		}
	}
	return EBTNodeResult::Aborted;
}
