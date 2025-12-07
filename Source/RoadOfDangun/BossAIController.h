// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BossAIController.generated.h"

UCLASS()
class ROADOFDANGUN_API ABossAIController : public AAIController
{
    GENERATED_BODY()
public:
    ABossAIController();

    virtual void OnPossess(APawn* InPawn) override;

    void SetBusyBB(bool bBusy)
    {
        if (BlackboardComp)
            BlackboardComp->SetValueAsBool(FName("bIsBusy"), bBusy);
    }

protected:
    UPROPERTY(Transient)
    UBehaviorTreeComponent* BehaviorComp;

    UPROPERTY(Transient)
    UBlackboardComponent* BlackboardComp;

    UPROPERTY(EditDefaultsOnly, Category = "AI")
    UBehaviorTree* BehaviorTreeAsset;
};

