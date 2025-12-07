// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_UpdateBossContext.generated.h"

/**
 *
 */
UCLASS()
class ROADOFDANGUN_API UBTService_UpdateBossContext : public UBTService
{
    GENERATED_BODY()

public:
    UBTService_UpdateBossContext();

protected:
    virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};