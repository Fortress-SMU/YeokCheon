// UAttackTraceComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameFramework/DamageType.h"
// ※ 헤더 경량화를 원하면 아래 4개는 .cpp로 옮기고 여기선 전방선언만 사용해도 됩니다.
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ArrowComponent.h"
#include "DamageSystemTypes.h"   // FDamageRequest
#include "Damageable.h"          // IDamageable
#include "AttackTraceComponent.generated.h"

/** 트레이스(피격판정) 도형 종류 */
UENUM(BlueprintType)
enum class EAttackTraceShape : uint8
{
    Line    UMETA(DisplayName = "Line (ray)"),  // 선형(레이캐스트)
    Box     UMETA(DisplayName = "Box"),         // 박스 스윕
    Sphere  UMETA(DisplayName = "Sphere"),      // 스피어 스윕
    Capsule UMETA(DisplayName = "Capsule")      // 캡슐 스윕
};

/** 개별 트레이스 설정(도형/사이즈/오프셋/데미지 등) */
USTRUCT(BlueprintType)
struct FAttackTraceSpec
{
    GENERATED_BODY()

    /** 기준 소켓명(없으면 이 컴포넌트 위치/회전 기준) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace")
    FName SocketName = NAME_None;

    /** 트레이스 도형 선택 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace")
    EAttackTraceShape Shape = EAttackTraceShape::Box;

    /** (Line 전용) 시작/끝 오프셋(로컬 기준) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace", meta = (EditCondition = "Shape==EAttackTraceShape::Line"))
    FVector StartOffset = FVector::ZeroVector;

    /** (Line 전용) 끝 오프셋(로컬 기준) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace", meta = (EditCondition = "Shape==EAttackTraceShape::Line"))
    FVector EndOffset = FVector(100.f, 0.f, 0.f);

    /** (Box 전용) 박스 반크기(Extents) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace", meta = (EditCondition = "Shape==EAttackTraceShape::Box"))
    FVector BoxExtents = FVector(30.f, 20.f, 20.f);

    /** (Sphere 전용) 반지름 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace", meta = (EditCondition = "Shape==EAttackTraceShape::Sphere"))
    float SphereRadius = 35.f;

    /** (Capsule 전용) 반높이 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace", meta = (EditCondition = "Shape==EAttackTraceShape::Capsule"))
    float CapsuleHalfHeight = 45.f;

    /** (Capsule 전용) 반지름 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace", meta = (EditCondition = "Shape==EAttackTraceShape::Capsule"))
    float CapsuleRadius = 25.f;

    /** 추가 회전 오프셋(로컬) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace")
    FRotator RotationOffset = FRotator::ZeroRotator;

    // 디테일 패널에서 편집할 최종 데미지 요청 값(이 값이 최우선)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
    FDamageRequest DamageRequest;

    /** 데미지 타입(미지정 시 기본 DamageType) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
    TSubclassOf<UDamageType> DamageType = UDamageType::StaticClass(); // 기본값 지정

    /** 충돌 채널(무엇을 때릴지) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Pawn;

    /** 디버그 라인/도형 그리기 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bDrawDebug = false;
};

/**
 * 공격 판정(트레이스) 컴포넌트
 * - AnimNotifyState 등에서 Start/Stop 호출로 “타격창”을 열고 닫음
 * - Tick 동안 설정된 도형으로 Line/Sweep을 수행하고 피격 처리
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class ROADOFDANGUN_API UAttackTraceComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    /** 생성자: Tick 활성화 설정 */
    UAttackTraceComponent();

    /** 트레이스 윈도우 시작(한 스윙 시작 시점) */
    UFUNCTION(BlueprintCallable, Category = "AttackTrace")
    void StartTraceWindow();

    /** 트레이스 윈도우 종료(한 스윙 종료 시점) */
    UFUNCTION(BlueprintCallable, Category = "AttackTrace")
    void StopTraceWindow();

    /** 액터당 스윙 1타만 허용할지(중복 히트 방지) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackTrace")
    bool bSingleHitPerActorPerSwing = true;

    /** 동시에 수행할 트레이스들(양손, 궤적 끝 등) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackTrace")
    TArray<FAttackTraceSpec> Traces;

    /** 시전자/그 외 무시할 대상 기본 필터링 여부 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackTrace")
    bool bIgnoreInstigator = true;

    /** (에디터 전용) 미리보기 온/오프 */
    UPROPERTY(EditAnywhere, Category = "AttackTrace|Preview")
    bool bShowPreviewInEditor = true;

protected:
    /** BeginPlay: 초기화 지점 */
    virtual void BeginPlay() override;

    /** 매 프레임 트레이스 수행(윈도우가 열려 있을 때만) */
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#if WITH_EDITOR
    /** 컴포넌트 등록 시(에디터) 프리뷰 재생성 */
    virtual void OnRegister() override;

    /** 에디터에서 속성 변경 시 프리뷰 갱신 */
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
    /** 현재 트레이스 윈도우 열림 여부 */
    bool bTracing = false;

    /** 이번 스윙 동안 이미 맞은 액터 목록(중복 타격 방지) */
    TSet<TWeakObjectPtr<AActor>> HitActorsThisSwing;

    /** Traces에 따라 실제 라인/스윕을 수행 */
    void DoTraces();

    /** 피격 처리: 데미지 가하기 등 */
    void ApplyHit(const FHitResult& Hit, const FAttackTraceSpec& Spec, const FVector& HitPoint);

    /** 기준 위치/회전 계산(소켓 또는 컴포넌트 기준 + 회전 오프셋) */
    void GetBasisTransform(const FAttackTraceSpec& Spec, FVector& OutLocation, FRotator& OutRotation) const;

#if WITH_EDITORONLY_DATA
    /** (에디터 전용) 생성한 프리뷰 컴포넌트들 */
    UPROPERTY(Transient)
    TArray<TObjectPtr<UPrimitiveComponent>> PreviewComponents;

    /** (에디터 전용) 프리뷰 재구성 */
    void RebuildPreview();

    /** (에디터 전용) 프리뷰 제거 */
    void ClearPreview();
#endif
};
