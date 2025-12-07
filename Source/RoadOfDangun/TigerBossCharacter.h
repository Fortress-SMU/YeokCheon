// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ABossCloneCharacter.h"
#include "TigerBossCharacter.generated.h"



UENUM(BlueprintType)
enum class EBossPhase : uint8
{
    Phase1,
    Phase2
};

UENUM(BlueprintType)
enum class EAttackPattern : uint8
{
    Idle,
    Claw,
    Jump,
    Dash,
    TeleportAndDash,
    CloneSpawn,
    Roar,
    MultiDash,
    WideClaw
};

UENUM(BlueprintType)
enum class EBossCombatState : uint8
{
    Idle,
    Approaching,
    Repositioning,
    CombatReady,
    Attacking,
    Cooldown,
    Staggered,
    RotatingToAttack
};

UENUM(BlueprintType)
enum class EBossIntent : uint8
{
    None,
    Move,
    Attack,
    Evade
};

UENUM(BlueprintType)
enum class EMoveBehavior : uint8
{
    None       UMETA(DisplayName = "None"),
    Approach   UMETA(DisplayName = "Approach"),
    BackAway   UMETA(DisplayName = "BackAway"),
    Strafe     UMETA(DisplayName = "Strafe")
};


// 델리게이트 선언: float 매개변수 하나 (ex. 새로운 체력값)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, CurrentHP, float, MaxHP);

UCLASS()
class ROADOFDANGUN_API ATigerBossCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ATigerBossCharacter();

    // 할퀴기 공격 (원래 있던 함수)
    UFUNCTION(BlueprintCallable, Category = "Boss|Attack")
    void PerformClawAttack();

    // 공격 가능 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Attack")
    bool bCanAttack = true;

    // 체력 변경 델리게이트 (UI, 이펙트 등에서 구독)
    UPROPERTY(BlueprintAssignable, Category = "Health")
    FOnHealthChanged OnHealthChanged;

    // BPC_Health / BP 에서 체력 변경 시 이 함수만 호출하면 됨
    UFUNCTION(BlueprintCallable, Category = "Health")
    void callDelegate(float CurrentHP, float MaxHP);

    // 실제 체력/페이즈/죽음 처리 담당
    UFUNCTION(BlueprintCallable, Category = "Health")
    void HandleHealthChanged(float CurrentHP, float MaxHP);

    // 예전 구조 호환용 (이미 BP에 묶어놨으면 그대로 둠)
    UFUNCTION(BlueprintCallable, Category = "Health")
    void TestPrint_Two(float CurrentHP, float MaxHP);

    // 상위 의사결정(이동/공격/회피 선택)
    UFUNCTION(BlueprintCallable, Category = "Boss|AI")
    void EvaluateCombatReady();

    // 연계 공격(콤보) 패턴 결정
    void ConsiderFollowUp(EAttackPattern LastPattern);


    // 현재 페이즈 조회 (BTService, 다른 코드에서 사용 가능)
    EBossPhase GetCurrentPhase() const { return CurrentPhase; }

    // 플레이어 Pawn 리턴
    APawn* GetPlayerActor() const { return PlayerActor; }

    // 현재 상태가 Idle/CombatReady 외에는 모두 Busy
    bool IsBusy() const;



protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // 상태
    EBossPhase CurrentPhase;
    EAttackPattern CurrentPattern;

    EBossIntent CurrentIntent = EBossIntent::None;
    // 상위 의도 선택
    EBossIntent ChooseIntent(float DistanceToPlayer);

    // Intent별 세부 행동 선택
    void ExecuteMoveIntent(float DistanceToPlayer);
    void ExecuteAttackIntent(float DistanceToPlayer);
    void ExecuteEvadeIntent(float DistanceToPlayer);

    // 최근 행동 기억 (가중치 감소용으로 나중에 활용)
    EBossIntent LastIntent = EBossIntent::None;
    EAttackPattern LastAttackPattern = EAttackPattern::Idle;

    // ================= Intent 가중치 계수 =================
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|AI|IntentWeight", meta = (ClampMin = "0.1", ClampMax = "3.0"))
    float MoveWeightFactor = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|AI|IntentWeight", meta = (ClampMin = "0.1", ClampMax = "3.0"))
    float AttackWeightFactor = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|AI|IntentWeight", meta = (ClampMin = "0.1", ClampMax = "3.0"))
    float EvadeWeightFactor = 1.0f;

    // Intent 사용 후 가중치 갱신용 함수
    void UpdateIntentWeights(EBossIntent UsedIntent);
    void RelaxWeightTowardsOne(float& Weight, float Step);


    // 체력
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
    float CachedCurrentHp = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
    float CachedMaxHp = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
    float PercentHP = 1.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
    float CurrentHealth = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss")
    bool bIsDead = false;

    UFUNCTION(BlueprintCallable)
    void Dead();

    //분신
    UPROPERTY(EditAnywhere, Category = "Boss|Clone")
    TSubclassOf<AActor> CloneClass;

    UPROPERTY(EditAnywhere, Category = "Boss|Clone")
    int32 NumClones = 3;

    UPROPERTY(EditAnywhere, Category = "Boss|Clone")
    float CloneRadius = 1500.f;

    UPROPERTY(EditAnywhere, Category = "Boss|Clone")
    float CloneLifetime = 4.0f;


    //분신관리
    UPROPERTY()
    TArray<TWeakObjectPtr<ABossCloneCharacter>> ActiveClones;

    // 보이스 사운드들
    UPROPERTY(EditAnywhere, Category = "Audio|Voice")
    USoundBase* Voice_Entry;

    UPROPERTY(EditAnywhere, Category = "Audio|Voice")
    USoundBase* Voice_Claw;

    UPROPERTY(EditAnywhere, Category = "Audio|Voice")
    USoundBase* Voice_Jump;

    UPROPERTY(EditAnywhere, Category = "Audio|Voice")
    USoundBase* Voice_Dash;



    // 쿨다운

    float AttackCooldown;
    FTimerHandle AttackCooldownTimer;
    FTimerHandle WaitTimer;
    FTimerHandle DashStartTimer;
    FTimerHandle MovementTimer;
    FTimerHandle CollisionTimer;



    // 공격
    bool bIsInCombo;
    int32 ComboStep;
    float TimeSinceLastCombat = 0.f;
    void TryAttack();

    //블프이벤트노드
    UFUNCTION(BlueprintImplementableEvent, category = "MyFunctions")
    void CreateAttackTraceCPP();

    UFUNCTION(BlueprintImplementableEvent, category = "MyFunctions")
    void StopAttackTraceCPP();


    UPROPERTY(EditAnywhere, Category = "Boss|Animation")
    UAnimMontage* ClawMontage;

    UFUNCTION(BlueprintCallable)
    void HandleClawDamageNotify();
    UPROPERTY(EditAnywhere, Category = "Boss|Animation")
    UAnimMontage* JumpMontage;

    UPROPERTY(EditAnywhere, Category = "Boss|Animation")
    UAnimMontage* DashMontage;
    EBossCombatState CombatState;

    UPROPERTY(EditAnywhere, Category = "Boss|Animation")
    UAnimMontage* EvadeMontage;


    UPROPERTY(EditDefaultsOnly, Category = "Animation")
    UAnimMontage* DeathMontage;

    //이동함수

    // 이동 서브 상태
    EMoveBehavior CurrentMoveBehavior = EMoveBehavior::None;

    // 이번 이동 행동이 유지될 남은 시간
    float MoveBehaviorTimeRemaining = 0.f;

    void MoveTowardsPlayer(float Scale = 1.0f);
    void MoveAwayFromPlayer(float Scale = 1.0f);
    void StrafeAroundPlayer(float Scale = 1.0f, float ForwardBias = 0.25f);


    // 거리 유지 / 회피
    UPROPERTY(EditAnywhere, Category = "Boss AI")
    float DesiredDistanceFromPlayer = 600.f;

    UPROPERTY(EditAnywhere, Category = "Boss AI")
    float EvasionProbability = 0.35f;

    UPROPERTY(EditAnywhere, Category = "Boss AI")
    float EvasionDistance = 450.f;
    // 회피 도약 방향 기억용
    UPROPERTY(VisibleAnywhere, Category = "Boss|Evade")
    bool bLastRetreatWasBack = false;


    APawn* PlayerActor = nullptr;

    // 출현 연출용 변수
    bool bIsFalling = false;
    float FallElapsed = 0.f;
    FVector FallStartLocation;
    FVector FallTargetLocation;

    // 출현 애니메이션
    UPROPERTY(EditAnywhere, Category = "Boss|Animation")
    UAnimMontage* EntryMontage;

    // 출현 연출용 함수
    void StartFallSequence();


    // 행동 로직
    void DecideAndExecuteAttack();
    void CheckPhaseTransition();
    void SelectAndExecuteAttack();
    void ResetAttack();
    void RotateTowardPlayerThen(TFunction<void()> Callback);

    FTimerHandle RotateCheckTimerHandle;
    FTimerHandle AirHitTimer;


    // 이동 행동
    void SlowApproach();
    void Retreat();
    void FastCharge();
    void CircleAroundPlayer();

    // 공격 패턴
    void PerformJumpAttack();
    void PerformDashAttack();
    void StartDashSequence(); // 돌진 본체
    void PerformTeleportAndDash();
    void PerformCloneSpawn();
    void PerformMultiDash();
    void StartComboPattern();
    void ExecuteComboStep();

    void TickCombatFSM(float DeltaTime);
    void EnterState(EBossCombatState NewState);

    void EnterCombatReady();

    //회전
    bool bWantsToRotateToPlayer = false;
    FRotator TargetRotationForAttack;
    TFunction<void()> PostRotationAction; // 회전 후 실행할 함수



};
