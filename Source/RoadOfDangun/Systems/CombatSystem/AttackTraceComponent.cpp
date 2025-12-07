// UAttackTraceComponent.cpp
#include "AttackTraceComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ArrowComponent.h"

UAttackTraceComponent::UAttackTraceComponent()
{
    // Tick 사용
    PrimaryComponentTick.bCanEverTick = true;
}

void UAttackTraceComponent::BeginPlay()
{
    Super::BeginPlay();
}

/// <summary>
/// 트레이스 시작: 중복 히트 목록 초기화
/// Call By AttackTraceWindow.NotifyBegin()
/// </summary>
void UAttackTraceComponent::StartTraceWindow()
{
    bTracing = true;
    HitActorsThisSwing.Empty();
}

/// <summary>
/// 트레이스 종료: 중복 히트 목록 초기화
/// Call By AttackTraceWindow.NotifyEnd()
/// </summary>
void UAttackTraceComponent::StopTraceWindow()
{
    
    bTracing = false;
    HitActorsThisSwing.Empty();
}

void UAttackTraceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // 1) 윈도우가 열려 있고(트레이스가 진행 중) 2) 트레이스가 하나 이상 설정되어 있을 때만 -> 수행
    if (bTracing && Traces.Num() > 0)
    {
        DoTraces();
    }
}

/// <summary>
/// 기준: 기본은 이 컴포넌트, 가능하면 소켓 기준
/// </summary>
void UAttackTraceComponent::GetBasisTransform(const FAttackTraceSpec& Spec, FVector& OutLocation, FRotator& OutRotation) const
{
    const USceneComponent* Base = this;

    if (AActor* Owner = GetOwner())
    {
        // 스켈레탈 메시에서 소켓을 찾음
        if (USkeletalMeshComponent* Skel = Owner->FindComponentByClass<USkeletalMeshComponent>())
        {
            if (Spec.SocketName != NAME_None && Skel->DoesSocketExist(Spec.SocketName))
            {
                const FTransform SocketTM = Skel->GetSocketTransform(Spec.SocketName, RTS_World);
                OutLocation = SocketTM.GetLocation();
                // 간단히 Rotator 더하기(정확한 회전 결합이 필요하면 쿼터니언 곱을 권장)
                OutRotation = (SocketTM.Rotator() + Spec.RotationOffset);
                return;
            }
        }
    }

    // 소켓이 없으면 컴포넌트 트랜스폼 기준
    OutLocation = Base->GetComponentLocation();
    OutRotation = (Base->GetComponentRotation() + Spec.RotationOffset);
}

/// <summary>
/// 실제 충돌 쿼리 수행
/// </summary>
void UAttackTraceComponent::DoTraces()
{
    UWorld* World = GetWorld();
    if (!World) return;

    AActor* InstigatorActor = GetOwner();   //현재 컴포넌트의 소유자 Actor 반환

    // 각 트레이스 스펙을 순회하며 개별 수행
    for (const FAttackTraceSpec& Spec : Traces)
    {
        FVector Origin; FRotator Rot;
        GetBasisTransform(Spec, Origin, Rot);
        const FQuat Q = Rot.Quaternion();

        TArray<FHitResult> Hits; // 이번 스펙에서의 모든 히트 결과

        //트레이스 도형에 따라, 히트 적용
        switch (Spec.Shape)
        {
            // 선형 트레이스(레이)
            case EAttackTraceShape::Line:
            {
                const FVector Start = Origin + Q.RotateVector(Spec.StartOffset);
                const FVector End = Origin + Q.RotateVector(Spec.EndOffset);

                FHitResult Hit;
                FCollisionQueryParams Params(SCENE_QUERY_STAT(AttackTraceLine), false, InstigatorActor);
                if (bIgnoreInstigator && InstigatorActor) Params.AddIgnoredActor(InstigatorActor);

                const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, Spec.TraceChannel, Params);

                if (Spec.bDrawDebug)
                    DrawDebugLine(World, Start, End, FColor::Red, false, 0.f, 0, 1.5f); // 필요시 duration 0.1f 권장

                if (bHit) Hits.Add(Hit);
                break;
            }

            // 박스 스윕(제자리 Sweep으로 겹침 판정)
            case EAttackTraceShape::Box:
            {
                const FVector Center = Origin;
                const FCollisionShape Shape = FCollisionShape::MakeBox(Spec.BoxExtents);

                FCollisionQueryParams Params(SCENE_QUERY_STAT(AttackTraceBox), false, InstigatorActor);
                if (bIgnoreInstigator && InstigatorActor) Params.AddIgnoredActor(InstigatorActor);

                World->SweepMultiByChannel(Hits, Center, Center, Q, Spec.TraceChannel, Shape, Params);

                if (Spec.bDrawDebug)
                    DrawDebugBox(World, Center, Spec.BoxExtents, Q, FColor::Green, false, 0.f, 0, 1.5f);

                break;
            }

            // 스피어 스윕(제자리)
            case EAttackTraceShape::Sphere:
            {

                const FVector Center = Origin;
                const FCollisionShape Shape = FCollisionShape::MakeSphere(Spec.SphereRadius);

                FCollisionQueryParams Params(SCENE_QUERY_STAT(AttackTraceSphere), false, InstigatorActor);
                if (bIgnoreInstigator && InstigatorActor) Params.AddIgnoredActor(InstigatorActor);

                World->SweepMultiByChannel(Hits, Center, Center, Q, Spec.TraceChannel, Shape, Params);

                if (Spec.bDrawDebug)
                    DrawDebugSphere(World, Center, Spec.SphereRadius, 16, FColor::Blue, false, 0.f, 0, 1.5f);

                break;
            }

            // 캡슐 스윕(제자리)
            case EAttackTraceShape::Capsule:
            {
                const FVector Center = Origin;
                const FCollisionShape Shape = FCollisionShape::MakeCapsule(Spec.CapsuleRadius, Spec.CapsuleHalfHeight);

                FCollisionQueryParams Params(SCENE_QUERY_STAT(AttackTraceCapsule), false, InstigatorActor);
                if (bIgnoreInstigator && InstigatorActor) Params.AddIgnoredActor(InstigatorActor);

                World->SweepMultiByChannel(Hits, Center, Center, Q, Spec.TraceChannel, Shape, Params);

                if (Spec.bDrawDebug)
                    DrawDebugCapsule(World, Center, Spec.CapsuleHalfHeight, Spec.CapsuleRadius, Q, FColor::Cyan, false, 0.f, 0, 1.5f);

                break;
            }
        } // switch

        // 히트 결과 처리
        for (const FHitResult& Hit : Hits)
        {
            AActor* Other = Hit.GetActor();
            if (!Other || Other == InstigatorActor) continue;

            // 스윙당 1회 제한이면 이미 맞은 액터는 스킵
            if (bSingleHitPerActorPerSwing && HitActorsThisSwing.Contains(Other))
                continue;

            // 데미지 적용
            ApplyHit(Hit, Spec, Hit.ImpactPoint);

            // 중복 히트 방지 목록에 추가
            HitActorsThisSwing.Add(Other);
        }
    }
}

/// <summary>
/// 데미지 적용
/// </summary>
void UAttackTraceComponent::ApplyHit(const FHitResult& Hit, const FAttackTraceSpec& Spec, const FVector& HitPoint)
{
    AActor* InstigatorActor = GetOwner();
    AActor* DamagedActor = Hit.GetActor();
    if (!DamagedActor) return;

    //  1) FDamageRequest 구성
    FDamageRequest Req = Spec.DamageRequest;
    
    // 가해자 지정 (자기 자신) (약한 참조)
    Req.DamageCauser = InstigatorActor; 

    // 피격자가 Damageable을 구현한 컴포넌트를 가지고 있는지
    TArray<UActorComponent*> Components;
    DamagedActor->GetComponents(Components);

    for (UActorComponent* Comp : Components)
    {
        if (!Comp) continue;

        if (Comp->GetClass()->ImplementsInterface(UDamageable::StaticClass()))
        {
            // BP/C++ 구현 모두 커버하려면 Execute_* 사용
            IDamageable::Execute_TakeDamage(Comp, Req);
            return; // 한 컴포넌트에만 보내고 종료(원하면 계속 돌려도 됨)
        }
    }
}


#if WITH_EDITOR
void UAttackTraceComponent::OnRegister()
{
    Super::OnRegister();
#if WITH_EDITORONLY_DATA
    // 에디터에서 컴포넌트 등록 시 프리뷰 재생성
    RebuildPreview();
#endif
}

void UAttackTraceComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
#if WITH_EDITORONLY_DATA
    // Traces 변경 또는 프리뷰 토글 변경 시 재생성
    static const FName TracesName = GET_MEMBER_NAME_CHECKED(UAttackTraceComponent, Traces);
    static const FName PreviewName = GET_MEMBER_NAME_CHECKED(UAttackTraceComponent, bShowPreviewInEditor);

    if (!PropertyChangedEvent.Property)
    {
        RebuildPreview();
        return;
    }

    const FName Changed = PropertyChangedEvent.Property->GetFName();
    if (Changed == TracesName || Changed == PreviewName)
    {
        RebuildPreview();
    }
#endif
}
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
void UAttackTraceComponent::ClearPreview()
{
    // 생성해뒀던 미리보기 컴포넌트들을 정리
    if (PreviewComponents.Num() == 0) return;

    for (UPrimitiveComponent* Comp : PreviewComponents)
    {
        if (Comp)
        {
            Comp->DestroyComponent();
        }
    }
    PreviewComponents.Empty();
}

/** 에디터에서 트레이스 형태를 눈으로 확인하기 위한 프리뷰 생성 */
void UAttackTraceComponent::RebuildPreview()
{
    ClearPreview();
    if (!bShowPreviewInEditor) return;

    AActor* Owner = GetOwner();
    if (!Owner) return;

    // 각 트레이스 스펙에 맞춰 비주얼라이제이션 컴포넌트 생성
    for (const FAttackTraceSpec& Spec : Traces)
    {
        FVector BasisLoc; FRotator BasisRot;
        GetBasisTransform(Spec, BasisLoc, BasisRot);
        const FQuat Q = BasisRot.Quaternion();

        // 공통 설정 람다: 게임 중 비표시, 에디터에서만 보이게, 비주얼 컴포넌트 플래그 등
        auto CommonSetup = [&](UPrimitiveComponent* Prim)
            {
                Prim->SetMobility(EComponentMobility::Movable);
                Prim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                Prim->bHiddenInGame = true;
                Prim->SetVisibility(true);
                Prim->SetIsVisualizationComponent(true);
                Prim->AttachToComponent(this, FAttachmentTransformRules::KeepWorldTransform);
                Prim->RegisterComponent();
                PreviewComponents.Add(Prim);
            };

        switch (Spec.Shape)
        {
        case EAttackTraceShape::Box:
        {
            // 박스 프리뷰
            UBoxComponent* Box = NewObject<UBoxComponent>(Owner, NAME_None, RF_Transient);
            Box->InitBoxExtent(Spec.BoxExtents);
            Box->SetWorldLocation(BasisLoc);
            Box->SetWorldRotation(BasisRot);
            CommonSetup(Box);
            break;
        }
        case EAttackTraceShape::Sphere:
        {
            // 스피어 프리뷰
            USphereComponent* Sphere = NewObject<USphereComponent>(Owner, NAME_None, RF_Transient);
            Sphere->InitSphereRadius(Spec.SphereRadius);
            Sphere->SetWorldLocation(BasisLoc);
            Sphere->SetWorldRotation(BasisRot);
            CommonSetup(Sphere);
            break;
        }
        case EAttackTraceShape::Capsule:
        {
            // 캡슐 프리뷰
            UCapsuleComponent* Capsule = NewObject<UCapsuleComponent>(Owner, NAME_None, RF_Transient);
            Capsule->InitCapsuleSize(Spec.CapsuleRadius, Spec.CapsuleHalfHeight);
            Capsule->SetWorldLocation(BasisLoc);
            Capsule->SetWorldRotation(BasisRot);
            CommonSetup(Capsule);
            break;
        }
        case EAttackTraceShape::Line:
        {
            // 라인 프리뷰: 얇은 박스로 선 표현 + 화살표로 방향 표시
            const FVector Start = BasisLoc + Q.RotateVector(Spec.StartOffset);
            const FVector End = BasisLoc + Q.RotateVector(Spec.EndOffset);
            const FVector Mid = (Start + End) * 0.5f;
            const FVector Dir = (End - Start);
            const float   Len = Dir.Size();

            if (Len > KINDA_SMALL_NUMBER)
            {
                // 얇은 상자(가로 X길이)로 선 표현
                UBoxComponent* ThinBox = NewObject<UBoxComponent>(Owner, NAME_None, RF_Transient);
                ThinBox->InitBoxExtent(FVector(Len * 0.5f, 2.5f, 2.5f)); // Y/Z는 얇게
                ThinBox->SetWorldLocation(Mid);
                ThinBox->SetWorldRotation(Dir.Rotation());
                CommonSetup(ThinBox);

                // 끝점 방향 화살표
                UArrowComponent* Arrow = NewObject<UArrowComponent>(Owner, NAME_None, RF_Transient);
                Arrow->ArrowSize = 1.0f;
                Arrow->SetWorldLocation(End);
                Arrow->SetWorldRotation(Dir.Rotation());
                CommonSetup(Arrow);
            }
            break;
        }
        }
    }
}
#endif // WITH_EDITORONLY_DATA
