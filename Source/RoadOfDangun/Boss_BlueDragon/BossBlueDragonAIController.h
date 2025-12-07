// BossBlueDragonAIController.h
#pragma once
#include "CoreMinimal.h"
#include "AIController.h"
#include "BossBlueDragonAIController.generated.h"

class UBlackboardComponent;
class UBehaviorTreeComponent;

UCLASS()
class RoadOfDangun_API ABossBlueDragonAIController : public AAIController
{
	GENERATED_BODY()
public:
	ABossBlueDragonAIController();

protected:
	virtual void OnPossess(APawn* InPawn) override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	UBlackboardComponent* BlackboardComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	UBehaviorTreeComponent* BehaviorComp;

	static const FName BBKey_PlayerActor;
	static const FName BBKey_MoveTarget;
	static const FName BBKey_State;
	static const FName BBKey_IsDead;
	static const FName BBKey_DoIntro;
	static const FName BBKey_ThreatActor; // [Added]
};
