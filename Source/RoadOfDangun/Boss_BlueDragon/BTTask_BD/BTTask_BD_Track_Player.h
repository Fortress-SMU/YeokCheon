// BTTask_BD_Track_Player.h
#pragma once
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_BD_Track_Player.generated.h"

UCLASS()
class RoadOfDangun_API UBTTask_BD_Track_Player : public UBTTaskNode
{
	GENERATED_BODY()
public:
	UBTTask_BD_Track_Player() { bNotifyTick = true; }

	// [Added] 추적 유지 시간(테스트용)
	UPROPERTY(EditAnywhere, Category = "Track") float TrackDuration = 2.0f;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	float Elapsed = 0.f; // [Added]
};
#pragma once
