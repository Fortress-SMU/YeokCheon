// BossBlueDragonAIController.cpp
#include "BossBlueDragonAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "Kismet/GameplayStatics.h"
#include "BossBlueDragon.h"
#include "AIController.h"

const FName ABossBlueDragonAIController::BBKey_PlayerActor = TEXT("PlayerActor");
const FName ABossBlueDragonAIController::BBKey_MoveTarget = TEXT("MoveTarget");
const FName ABossBlueDragonAIController::BBKey_State = TEXT("State");
const FName ABossBlueDragonAIController::BBKey_IsDead = TEXT("IsDead");
const FName ABossBlueDragonAIController::BBKey_DoIntro = TEXT("DoIntro");
const FName ABossBlueDragonAIController::BBKey_ThreatActor = TEXT("ThreatActor"); // [Added]

ABossBlueDragonAIController::ABossBlueDragonAIController()
{
	BlackboardComp = CreateDefaultSubobject<UBlackboardComponent>(TEXT("BlackboardComp"));
	BehaviorComp = CreateDefaultSubobject<UBehaviorTreeComponent>(TEXT("BehaviorComp"));
}

void ABossBlueDragonAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	ABossBlueDragon* Boss = Cast<ABossBlueDragon>(InPawn);
	if (!Boss) return;

	if (UBehaviorTree* BT = Boss->GetBehaviorTree())
	{
		if (UseBlackboard(BT->BlackboardAsset, BlackboardComp))
		{
			AActor* Player = UGameplayStatics::GetPlayerPawn(this, 0);
			BlackboardComp->SetValueAsObject(BBKey_PlayerActor, Player);
			BlackboardComp->SetValueAsBool(BBKey_DoIntro, Boss->IsDead() ? false : true);
			BlackboardComp->SetValueAsBool(BBKey_IsDead, Boss->IsDead());
			BlackboardComp->SetValueAsObject(BBKey_ThreatActor, nullptr); // [Added] ÃÊ±âÈ­

			BehaviorComp->StartTree(*BT);
		}
	}
}
