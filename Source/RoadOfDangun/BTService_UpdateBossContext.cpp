// Fill out your copyright notice in the Description page of Project Settings.


#include "BTService_UpdateBossContext.h"

#include "AIController.h"
#include "TigerBossCharacter.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"

UBTService_UpdateBossContext::UBTService_UpdateBossContext()
{
    bNotifyTick = true;
    Interval = 0.1f;
    RandomDeviation = 0.0f;
    NodeName = TEXT("Update Boss Context");
}

void UBTService_UpdateBossContext::TickNode(
    UBehaviorTreeComponent& OwnerComp,
    uint8* NodeMemory,
    float DeltaSeconds)
{
    Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

    AAIController* AICon = OwnerComp.GetAIOwner();
    if (!AICon) return;

    ATigerBossCharacter* Boss = Cast<ATigerBossCharacter>(AICon->GetPawn());
    if (!Boss) return;

    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!BB) return;

    APawn* PlayerPawn = Boss->GetPlayerActor();
    if (!PlayerPawn)
        PlayerPawn = UGameplayStatics::GetPlayerPawn(Boss, 0);

    float Distance = 0.f;
    if (PlayerPawn)
    {
        Distance = FVector::Dist(
            Boss->GetActorLocation(),
            PlayerPawn->GetActorLocation()
        );
    }

    // 상태 업데이트
    BB->SetValueAsObject(FName("PlayerActor"), PlayerPawn);
    BB->SetValueAsFloat(FName("DistanceToPlayer"), Distance);
    BB->SetValueAsBool(FName("bIsBusy"), Boss->IsBusy());
    BB->SetValueAsInt(FName("BossPhase"), (int32)Boss->GetCurrentPhase());

}
