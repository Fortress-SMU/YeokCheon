// Fill out your copyright notice in the Description page of Project Settings.


#include "BossAIController.h"
#include "Kismet/GameplayStatics.h"
#include "TigerBossCharacter.h"
#include "GameFramework/Character.h"
#include "NavigationSystem.h"
#include "AIController.h"


ABossAIController::ABossAIController()
{
    BehaviorComp = CreateDefaultSubobject<UBehaviorTreeComponent>(TEXT("BehaviorComp"));
    BlackboardComp = CreateDefaultSubobject<UBlackboardComponent>(TEXT("BlackboardComp"));
}

void ABossAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    if (ATigerBossCharacter* Boss = Cast<ATigerBossCharacter>(InPawn))
    {
        if (BehaviorTreeAsset)
        {
            BlackboardComp->InitializeBlackboard(*BehaviorTreeAsset->BlackboardAsset);

            BlackboardComp->SetValueAsBool(FName("bIsBusy"), false);

            RunBehaviorTree(BehaviorTreeAsset);
        }
    }
}
