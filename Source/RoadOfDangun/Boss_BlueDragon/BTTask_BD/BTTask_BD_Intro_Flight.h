// BTTask_BD_Intro_Flight.h
#pragma once
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_BD_Intro_Flight.generated.h"

UCLASS()
class RoadOfDangun_API UBTTask_BD_Intro_Flight : public UBTTaskNode
{
	GENERATED_BODY()
public:
	UBTTask_BD_Intro_Flight() { bNotifyTick = true; }

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override; // [Added] 피격 시 중단 가정 정리
};
