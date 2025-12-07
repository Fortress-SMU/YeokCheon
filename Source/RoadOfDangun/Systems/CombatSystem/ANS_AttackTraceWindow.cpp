#include "ANS_AttackTraceWindow.h"
#include "AttackTraceComponent.h"

/// <summary>
/// AttackTraceWindow ANS가 시작할 때 발생
/// </summary>
void UANS_AttackTraceWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration)
{
    if (!MeshComp) return;
    if (AActor* Owner = MeshComp->GetOwner())
    {
        if (UAttackTraceComponent* C = Owner->FindComponentByClass<UAttackTraceComponent>())
        {
            C->StartTraceWindow();
        }
    }
}

/// <summary>
/// AttackTraceWindow ANS가 종료될 때 발생
/// </summary>
void UANS_AttackTraceWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
    if (!MeshComp) return;
    if (AActor* Owner = MeshComp->GetOwner())
    {
        if (UAttackTraceComponent* C = Owner->FindComponentByClass<UAttackTraceComponent>())
        {
            C->StopTraceWindow();
        }
    }
}
