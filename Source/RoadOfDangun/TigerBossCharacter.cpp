#include "TigerBossCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "ABossCloneCharacter.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"



ATigerBossCharacter::ATigerBossCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    DesiredDistanceFromPlayer = 600.0f;
    EvasionProbability = 0.35f;
    EvasionDistance = 450.0f;

    bCanAttack = true;
    bIsInCombo = false;
    ComboStep = 0;
    CurrentPhase = EBossPhase::Phase1;
    CurrentPattern = EAttackPattern::Idle;
    CombatState = EBossCombatState::Idle;

    MoveWeightFactor = 1.0f;
    AttackWeightFactor = 1.0f;
    EvadeWeightFactor = 1.0f;

    // 체력 기본값 안전 초기화
    CachedMaxHp = 1000.f;
    CachedCurrentHp = CachedMaxHp;
    CurrentHealth = CachedCurrentHp;
    PercentHP = 1.f;
    bIsDead = false;

    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}



void ATigerBossCharacter::BeginPlay()
{
    Super::BeginPlay();
    //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("C++ BeginPlay called"));

    // 플레이어 추적
    PlayerActor = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    CombatState = EBossCombatState::Idle;
    bCanAttack = false;
    bIsInCombo = false;
    ComboStep = 0;


    // 낙하 시작 위치 저장
    FallStartLocation = GetActorLocation();

    // 바닥 찾기 (라인 트레이스)
    FVector Start = FallStartLocation;
    FVector End = Start - FVector(0, 0, 10000.0f);

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
    {
        float CapsuleHalf = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        FallTargetLocation = Hit.ImpactPoint + FVector(0, 0, CapsuleHalf + 1.f);
    }
    else
    {
        FallTargetLocation = FallStartLocation - FVector(0, 0, 300.0f);
    }

    // 중력 제거
    GetCharacterMovement()->SetMovementMode(MOVE_None);

    // 입장 애니메이션 재생
    if (EntryMontage && GetMesh()->GetAnimInstance())
    {
        GetMesh()->GetAnimInstance()->Montage_Play(EntryMontage);

        FTimerHandle VoiceDelayHandle;
        GetWorldTimerManager().SetTimer(VoiceDelayHandle, [this]()
            {
                if (Voice_Claw)
                {
                    UGameplayStatics::PlaySoundAtLocation(this, Voice_Claw, GetActorLocation());
                }
            }, 1.0f, false);
    }

    bIsFalling = true;
    FallElapsed = 0.f;

}





void ATigerBossCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);


    if (bIsFalling)
    {
        FVector CurrentLocation = GetActorLocation();
        FVector ToTarget = FallTargetLocation - CurrentLocation;
        float DistanceToTarget = ToTarget.Size();

        if (DistanceToTarget < 2.0f)
        {
            bIsFalling = false;

            // 위치 강제 고정
            TeleportTo(FallTargetLocation, GetActorRotation(), false, true);

            GetCharacterMovement()->SetMovementMode(MOVE_Walking);

            GetWorldTimerManager().SetTimer(WaitTimer, this, &ATigerBossCharacter::StartFallSequence, 2.4f, false);
            return;
        }

        float FallDuration = 0.6f;
        float FallSpeed = (FallStartLocation - FallTargetLocation).Size() / FallDuration;
        FVector MoveDir = ToTarget.GetSafeNormal();
        FVector NewLocation = CurrentLocation + MoveDir * FallSpeed * DeltaTime;

        TeleportTo(NewLocation, GetActorRotation(), false, true);
        return;
    }

    // 여기 추가: 체력 0 이면 즉시 Dead() 중복 방지
    if (CurrentHealth <= 0.f && !bIsDead)
    {
        Dead();
        return;
    }

    //  낙하가 끝난 경우에만 FSM 실행
    CheckPhaseTransition();
    TickCombatFSM(DeltaTime);

    // 회전 처리
    if (bWantsToRotateToPlayer)
    {
        FRotator Current = GetActorRotation();
        FRotator NewRot = FMath::RInterpTo(Current, TargetRotationForAttack, DeltaTime, 12.0f);
        SetActorRotation(NewRot);

        float DeltaYaw = FRotator::NormalizeAxis(NewRot.Yaw - TargetRotationForAttack.Yaw);
        if (FMath::Abs(DeltaYaw) < 1.0f)
        {
            bWantsToRotateToPlayer = false;
            if (PostRotationAction)
            {
                PostRotationAction();
                PostRotationAction = nullptr;
            }
        }
    }
}



void ATigerBossCharacter::CheckPhaseTransition()
{
    if (CurrentPhase == EBossPhase::Phase1 && PercentHP <= 0.5f)
    {
        CurrentPhase = EBossPhase::Phase2;
        //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Phase 2 진입!"));
    }
}


void ATigerBossCharacter::TickCombatFSM(float DeltaTime)
{
    if (bIsDead) return;  // 죽었으면 FSM 비활성화
    if (!PlayerActor) return;
    if (!GetMesh() || !GetMesh()->GetAnimInstance()) return;
    // 공격 중일 땐 회전 금지
    if (CombatState != EBossCombatState::Attacking &&
        CombatState != EBossCombatState::Cooldown &&
        GetMesh() && !GetMesh()->GetAnimInstance()->Montage_IsPlaying(nullptr))
    {
        FVector ToPlayer = PlayerActor->GetActorLocation() - GetActorLocation();
        FRotator LookAtRotation = FRotationMatrix::MakeFromX(ToPlayer).Rotator();
        LookAtRotation.Pitch = 0.0f;
        LookAtRotation.Roll = 0.0f;

        SetActorRotation(FMath::RInterpTo(GetActorRotation(), LookAtRotation, DeltaTime, 5.0f));
    }

    if (!PlayerActor) return;

    float Distance = FVector::Dist(GetActorLocation(), PlayerActor->GetActorLocation());

    switch (CombatState)
    {
    case EBossCombatState::Idle:
        EnterState(EBossCombatState::Approaching);
        break;

    case EBossCombatState::Approaching:
    {
        if (Distance > 1500.f)
        {
            AddMovementInput((PlayerActor->GetActorLocation() - GetActorLocation()).GetSafeNormal(), 1.0f);

            TimeSinceLastCombat += DeltaTime;
            if (TimeSinceLastCombat > 4.0f && FMath::FRand() < 0.2f)
            {
                EnterState(EBossCombatState::Repositioning);
                TimeSinceLastCombat = 0.f;
                return;
            }
        }
        else if (Distance < 300.f)
        {
            MoveAwayFromPlayer(1.0f);
            TimeSinceLastCombat = 0.f;
        }
        else
        {
            EnterState(EBossCombatState::CombatReady);
            TimeSinceLastCombat = 0.f;
        }
        break;
    }

    case EBossCombatState::RotatingToAttack:
    {
        if (!PlayerActor || !GetMesh()) break;

        FVector ToPlayer = PlayerActor->GetActorLocation() - GetActorLocation();
        FRotator TargetRot = FRotationMatrix::MakeFromX(ToPlayer).Rotator();
        TargetRot.Pitch = 0.f;
        TargetRot.Roll = 0.f;

        FRotator CurrentRot = GetActorRotation();
        FRotator NewRot = FMath::RInterpTo(CurrentRot, TargetRot, DeltaTime, 12.0f);
        SetActorRotation(NewRot);

        float DeltaYaw = FRotator::NormalizeAxis(NewRot.Yaw - TargetRot.Yaw);
        if (FMath::Abs(DeltaYaw) < 1.0f)
        {
            StartDashSequence();                 //  회전 완료 후 직접 호출
            EnterState(EBossCombatState::Attacking);
        }

        break;
    }

    case EBossCombatState::CombatReady:
        EvaluateCombatReady(); // 공격 조건 검사 및 실행
        break;

    case EBossCombatState::Attacking:
    {
        if (!GetMesh()->GetAnimInstance()->Montage_IsPlaying(nullptr))
        {
            EnterState(EBossCombatState::Cooldown);
        }
        else
        {
            // 돌진 중이라면 전진 유지
            if (CurrentPattern == EAttackPattern::Dash)
            {
                FVector Direction = (PlayerActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
                AddMovementInput(Direction, 1.0f);
            }
        }
        break;
    }


    case EBossCombatState::Cooldown:
        // 이 상태에서는 자동 전이 안 함 -> 타이머로 제어
        break;

    case EBossCombatState::Repositioning:
    {
        //  이동 서브 상태가 설정되어 있으면 패턴 유지
        if (CurrentMoveBehavior != EMoveBehavior::None && MoveBehaviorTimeRemaining > 0.f)
        {
            MoveBehaviorTimeRemaining -= DeltaTime;

            switch (CurrentMoveBehavior)
            {
            case EMoveBehavior::Approach:
                MoveTowardsPlayer(1.0f);
                break;
            case EMoveBehavior::BackAway:
                MoveAwayFromPlayer(1.0f);
                break;
            case EMoveBehavior::Strafe:
                StrafeAroundPlayer(0.9f, 0.2f);
                break;
            default:
                break;
            }

            // 시간이 다 끝나면 CombatReady로 복귀
            if (MoveBehaviorTimeRemaining <= 0.f)
            {
                CurrentMoveBehavior = EMoveBehavior::None;
                EnterCombatReady();
            }

            // break 해야 아래 랜덤 Behavior 로 안 내려감
            break;
        }

        // 기존 Repositioning 로직 (FakeIdle / Retreat / CircleAround 등)
        if (!bCanAttack || GetWorldTimerManager().IsTimerActive(WaitTimer))
            break;

        int32 Behavior = FMath::RandRange(0, 2); // 0:정지, 1:회피, 2:원형 이동

        switch (Behavior)
        {
        case 0:
            GetWorldTimerManager().SetTimer(
                WaitTimer,
                this,
                &ATigerBossCharacter::EnterCombatReady,
                0.4f,
                false
            );
            break;

        case 1:
            Retreat();
            GetWorldTimerManager().SetTimer(
                WaitTimer,
                this,
                &ATigerBossCharacter::EnterCombatReady,
                0.7f,
                false
            );
            break;

        case 2:
            CircleAroundPlayer();
            GetWorldTimerManager().SetTimer(
                WaitTimer,
                this,
                &ATigerBossCharacter::EnterCombatReady,
                0.7f,
                false
            );
            break;
        }

        break;
    }

    case EBossCombatState::Staggered:
        // 피격 리액션 or 멈춤 등 처리
        break;
    }
}

void ATigerBossCharacter::StartFallSequence()
{
    // EntryMontage가 끝났는지 확인 후 전투 시작
    if (GetMesh() && GetMesh()->GetAnimInstance() &&
        GetMesh()->GetAnimInstance()->Montage_IsPlaying(EntryMontage))
    {
        // 아직 재생 중이면 조금 있다가 다시 확인
        GetWorldTimerManager().SetTimer(WaitTimer, this, &ATigerBossCharacter::StartFallSequence, 0.1f, false);
        return;
    }

    // 애니메이션이 끝났다면 전투 FSM 시작
    EnterCombatReady();
}



void ATigerBossCharacter::EnterCombatReady()
{
    bCanAttack = true;
    EnterState(EBossCombatState::CombatReady);
    EvaluateCombatReady(); // 첫 공격 바로 평가
}



void ATigerBossCharacter::EnterState(EBossCombatState NewState)
{
    CombatState = NewState;

    switch (NewState)
    {
    case EBossCombatState::Cooldown:
        // 기존 1.0f -> 0.4f 로 단축
        GetWorldTimerManager().SetTimer(WaitTimer, this, &ATigerBossCharacter::ResetAttack, 0.4f, false);
        break;

    case EBossCombatState::Idle:
        bCanAttack = true;
        break;

    default:
        break;
    }
}

void ATigerBossCharacter::DecideAndExecuteAttack()
{
    if (!PlayerActor) return;

    float Distance = FVector::Dist(GetActorLocation(), PlayerActor->GetActorLocation());
    TArray<EAttackPattern> PossiblePatterns;

    // Phase1 공격 패턴 (Claw / Dash / Jump만 사용)
    if (CurrentPhase == EBossPhase::Phase1)
    {
        if (Distance < 280.f)
        {
            // 근접  할퀴기 중심, 돌진은 약간
            PossiblePatterns = {
                EAttackPattern::Claw,
                EAttackPattern::Dash
            };
        }
        else if (Distance < 750.f)
        {
            // 중거리  점프 / 돌진
            PossiblePatterns = {
                EAttackPattern::Jump,
                EAttackPattern::Dash
            };
        }
        else
        {
            // 장거리  점프 비중을 높게, 돌진은 보조
            PossiblePatterns = {
                EAttackPattern::Jump,
                EAttackPattern::Dash
            };
        }
    }
    // Phase2  나중에 수정 예정, 일단 원본 유지
    else if (CurrentPhase == EBossPhase::Phase2)
    {
        if (FMath::FRand() < 0.3f)
        {
            PerformCloneSpawn();
            bCanAttack = false;
            LastIntent = EBossIntent::Attack;
            LastAttackPattern = EAttackPattern::CloneSpawn;
            EnterState(EBossCombatState::Cooldown);
            return;
        }

        PossiblePatterns = {
            EAttackPattern::CloneSpawn,
            EAttackPattern::TeleportAndDash,
            EAttackPattern::MultiDash
        };
    }

    if (PossiblePatterns.Num() == 0) return;

    //Dash 남발 방지
    EAttackPattern Chosen;
    {
        int32 Ix = FMath::RandRange(0, PossiblePatterns.Num() - 1);
        Chosen = PossiblePatterns[Ix];

        // 직전 공격이 Dash면 70% 확률로 Dash 제외하고 다시 선택
        if (Chosen == EAttackPattern::Dash &&
            LastAttackPattern == EAttackPattern::Dash &&
            PossiblePatterns.Num() > 1 &&
            FMath::FRand() < 0.7f)
        {
            TArray<EAttackPattern> NonDashPatterns;

            for (EAttackPattern P : PossiblePatterns)
            {
                if (P != EAttackPattern::Dash)
                    NonDashPatterns.Add(P);
            }

            if (NonDashPatterns.Num() > 0)
            {
                int32 Ix2 = FMath::RandRange(0, NonDashPatterns.Num() - 1);
                Chosen = NonDashPatterns[Ix2];
            }
        }
    }

    CurrentPattern = Chosen;

    // 콤보는 Claw / Dash / Jump 모두 가능
    if (!bIsInCombo && FMath::FRand() < 0.4f)
    {
        StartComboPattern();
    }
    else
    {
        SelectAndExecuteAttack();
    }

    LastIntent = EBossIntent::Attack;
    LastAttackPattern = CurrentPattern;
}



// 실행만 담당
void ATigerBossCharacter::SelectAndExecuteAttack()
{
    switch (CurrentPattern)
    {
    case EAttackPattern::Claw: PerformClawAttack(); break;
    case EAttackPattern::Jump: PerformJumpAttack(); break;
    case EAttackPattern::Dash: PerformDashAttack(); break;
    case EAttackPattern::TeleportAndDash: PerformTeleportAndDash(); break;
    case EAttackPattern::CloneSpawn: PerformCloneSpawn(); break;
    case EAttackPattern::MultiDash: PerformMultiDash(); break;
    default: break;
    }

    bCanAttack = false;
    EnterState(EBossCombatState::Attacking);
}


void ATigerBossCharacter::ResetAttack()
{
    bCanAttack = true;
    EnterCombatReady();  // Idle → Approaching 경로 생략, 바로 의사결정
}


void ATigerBossCharacter::SlowApproach()
{
    if (!PlayerActor) return;

    FVector ToPlayer = PlayerActor->GetActorLocation() - GetActorLocation();
    FVector Dir = ToPlayer.GetSafeNormal();

    // AddMovementInput의 스케일은 보통 0~1 사이  0.9 정도면 자연스러운 걷기/뛰기 느낌
    AddMovementInput(Dir, 0.9f);
}


void ATigerBossCharacter::Retreat()
{
    if (!PlayerActor) return;

    LastIntent = EBossIntent::Evade;

    // 방향 및 몽타주 재생 / 이동을 살짝 딜레이 후에 처리
    FTimerHandle RetreatMoveTimer;
    GetWorldTimerManager().SetTimer(
        RetreatMoveTimer,
        [this]()
        {
            if (!PlayerActor) return;

            // 거리 체크
            const float Distance = FVector::Dist(GetActorLocation(), PlayerActor->GetActorLocation());
            const float NearEvadeThreshold = 450.f;  // 이 이하면 근접으로 판단

            FVector ToPlayer = PlayerActor->GetActorLocation() - GetActorLocation();
            FVector ForwardDir = ToPlayer.GetSafeNormal();
            FVector BackDir = -ForwardDir;
            FVector LeftDir = FVector::CrossProduct(ToPlayer, FVector::UpVector).GetSafeNormal();
            FVector RightDir = -LeftDir;

            TArray<FVector> CandidateDirs;

            // 근거리에서는 전방 회피 금지
            const bool bAllowForward = Distance >= NearEvadeThreshold;

            if (!bLastRetreatWasBack)
            {
                // 후방(조금) + 좌/우(많이)
                CandidateDirs.Add(BackDir);

                CandidateDirs.Add(LeftDir);
                CandidateDirs.Add(LeftDir);
                CandidateDirs.Add(RightDir);
                CandidateDirs.Add(RightDir);

                // 중거리 이상일 때만 전방 회피 후보 추가
                if (bAllowForward)
                {
                    CandidateDirs.Add(ForwardDir);
                }
            }
            else
            {
                // 직전이 뒤였다면 이번엔 뒤 금지
                CandidateDirs.Add(LeftDir);
                CandidateDirs.Add(RightDir);

                if (bAllowForward)
                {
                    CandidateDirs.Add(ForwardDir);
                }
            }

            if (CandidateDirs.Num() == 0)
            {
                CandidateDirs.Add(BackDir);
            }

            const int32 Index = FMath::RandRange(0, CandidateDirs.Num() - 1);
            const FVector RetreatDir = CandidateDirs[Index];

            // 연속 후방 도약 여부 기록
            bLastRetreatWasBack = RetreatDir.Equals(BackDir, 0.1f);

            if (EvadeMontage && GetMesh() && GetMesh()->GetAnimInstance())
            {
                UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();

                // 재생 속도는 살짝 빠르게
                const float PlayRate = 1.25f;

                AnimInstance->Montage_Play(EvadeMontage, PlayRate);

                // 몽타주 안에서 evade 섹션의 시작 시점으로 점프
                AnimInstance->Montage_JumpToSection(FName("evade"), EvadeMontage);

            }

            // 실제 도약 이동
            const float RetreatSpeed = 1600.f;
            const FVector LaunchVel = RetreatDir * RetreatSpeed + FVector(0.f, 0.f, 150.f);

            LaunchCharacter(LaunchVel, true, true);
        },
        0.1f,
        false
    );

    // 회피 중에는 공격 잠시 봉인
    bCanAttack = false;
    GetWorldTimerManager().SetTimer(
        AttackCooldownTimer,
        this,
        &ATigerBossCharacter::ResetAttack,
        0.6f,
        false
    );
}




void ATigerBossCharacter::FastCharge()
{
    FVector Direction = (PlayerActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
    LaunchCharacter(Direction * 1200.0f, true, true);
}

void ATigerBossCharacter::CircleAroundPlayer()
{
    if (!PlayerActor) return;

    FVector ToPlayer = PlayerActor->GetActorLocation() - GetActorLocation();
    FVector ToPlayerDir = ToPlayer.GetSafeNormal();
    FVector Tangent = FVector::CrossProduct(ToPlayerDir, FVector::UpVector).GetSafeNormal();

    // 플레이어를 기준으로 약간 앞으로 접근 + 옆으로 도는 방향
    FVector FinalDirection = (Tangent + ToPlayerDir * 0.25f).GetSafeNormal();
    AddMovementInput(FinalDirection, 1.0f);
}



void ATigerBossCharacter::HandleClawDamageNotify()
{
    FVector Origin = GetActorLocation();
    float Radius = 150.0f;

    TArray<FOverlapResult> Overlaps;
    FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);

    bool bHit = GetWorld()->OverlapMultiByChannel(
        Overlaps,
        Origin + GetActorForwardVector() * 150.f,
        FQuat::Identity,
        ECC_Pawn,
        Shape
    );

    if (bHit)
    {
        for (auto& Result : Overlaps)
        {
            AActor* HitActor = Result.GetActor();
            if (HitActor && HitActor != this)
            {
                UGameplayStatics::ApplyDamage(HitActor, 10.0f, GetController(), this, nullptr);
            }
        }
    }
}

void ATigerBossCharacter::PerformClawAttack()
{
    if (!PlayerActor) return;

    FVector Dir = PlayerActor->GetActorLocation() - GetActorLocation();
    TargetRotationForAttack = FRotationMatrix::MakeFromX(Dir).Rotator();
    TargetRotationForAttack.Pitch = 0.f;
    TargetRotationForAttack.Roll = 0.f;

    bWantsToRotateToPlayer = true;

    PostRotationAction = [this]()
        {
            if (ClawMontage && GetMesh() && GetMesh()->GetAnimInstance())
            {
                GetMesh()->GetAnimInstance()->Montage_Play(ClawMontage);
                UGameplayStatics::PlaySoundAtLocation(this, Voice_Claw, GetActorLocation());
            }
        };
}


void ATigerBossCharacter::PerformJumpAttack()
{
    if (!PlayerActor) return;

    FVector Dir = PlayerActor->GetActorLocation() - GetActorLocation();
    TargetRotationForAttack = FRotationMatrix::MakeFromX(Dir).Rotator();
    TargetRotationForAttack.Pitch = 0.f;
    TargetRotationForAttack.Roll = 0.f;

    bWantsToRotateToPlayer = true;

    TWeakObjectPtr<ATigerBossCharacter> WeakThis(this);

    PostRotationAction = [WeakThis]()
        {
            if (!WeakThis.IsValid()) return;

            ATigerBossCharacter* Boss = WeakThis.Get();

            if (Boss->JumpMontage && Boss->GetMesh() && Boss->GetMesh()->GetAnimInstance())
            {
                UAnimInstance* Anim = Boss->GetMesh()->GetAnimInstance();
                Anim->Montage_Play(Boss->JumpMontage);

                FOnMontageEnded End;
                End.BindLambda([WeakThis](UAnimMontage*, bool)
                    {
                        if (!WeakThis.IsValid()) return;
                        WeakThis->ConsiderFollowUp(EAttackPattern::Jump);
                    });
                Anim->Montage_SetEndDelegate(End, Boss->JumpMontage);
            }

            // 0.9초 후 실제 점프 (몽타주 타이밍 유지)
            FTimerHandle JumpDelayTimer;
            Boss->GetWorldTimerManager().SetTimer(
                JumpDelayTimer,
                [WeakThis]()
                {
                    if (!WeakThis.IsValid()) return;

                    ATigerBossCharacter* BossInner = WeakThis.Get();
                    if (!BossInner->PlayerActor) return;

                    // 점프 직전 기준 위치 계산
                    const FVector StartLocation = BossInner->GetActorLocation();
                    const FVector TargetLocation = BossInner->PlayerActor->GetActorLocation();

                    // 공중에 떠 있는 시간 (원하는 점프 체감 속도)
                    const float FlightTime = 0.7f;

                    // 중력 값 얻기
                    float GravityZ = 0.0f;
                    if (UCharacterMovementComponent* MoveComp = BossInner->GetCharacterMovement())
                    {
                        GravityZ = MoveComp->GetGravityZ();
                    }
                    else if (UWorld* World = BossInner->GetWorld())
                    {
                        GravityZ = World->GetGravityZ();
                    }
                    if (FMath::IsNearlyZero(GravityZ))
                    {
                        GravityZ = -980.f; // 안전 기본값
                    }

                    const FVector ToTarget = TargetLocation - StartLocation;
                    FVector       ToTargetXY = FVector(ToTarget.X, ToTarget.Y, 0.f);
                    float         DistanceXY = ToTargetXY.Size();

                    // 너무 가까우면 살짝 앞으로 점프
                    if (DistanceXY < 200.f)
                    {
                        ToTargetXY = BossInner->GetActorForwardVector() * 400.f;
                        DistanceXY = ToTargetXY.Size();
                    }

                    // 너무 멀면 최대 거리 제한
                    const float MaxJumpDistance = 2500.f;
                    if (DistanceXY > MaxJumpDistance)
                    {
                        ToTargetXY = ToTargetXY.GetSafeNormal() * MaxJumpDistance;
                        DistanceXY = MaxJumpDistance;
                    }

                    // 수평 속도 FlightTime 안에 XY 목표까지 도달하도록
                    const FVector HorizontalVelocity = ToTargetXY / FlightTime;

                    // 수직 속도 FlightTime 후 TargetLocation.Z 에 도달하도록
                    const float DeltaZ = ToTarget.Z;
                    const float Vz = (DeltaZ - 0.5f * GravityZ * FlightTime * FlightTime) / FlightTime;

                    FVector LaunchVelocity = HorizontalVelocity;
                    LaunchVelocity.Z = Vz;

                    // 실제 점프
                    BossInner->LaunchCharacter(LaunchVelocity, true, true);

                    if (BossInner->Voice_Jump)
                    {
                        UGameplayStatics::PlaySoundAtLocation(BossInner, BossInner->Voice_Jump, StartLocation);
                    }

                    // 점프 중 충돌 감지 넉백 처리 (1초간 감지)
                    TArray<AActor*> AlreadyHit;

                    BossInner->GetWorldTimerManager().SetTimer(
                        BossInner->AirHitTimer,
                        FTimerDelegate::CreateLambda([WeakThis, AlreadyHit]() mutable
                            {
                                if (!WeakThis.IsValid()) return;

                                ATigerBossCharacter* BossHit = WeakThis.Get();

                                FVector Origin = BossHit->GetActorLocation();
                                float Radius = 420.f;

                                TArray<FOverlapResult> Overlaps;
                                FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);

                                if (BossHit->GetWorld()->OverlapMultiByChannel(
                                    Overlaps, Origin, FQuat::Identity, ECC_Pawn, Shape))
                                {
                                    if (IsValid(BossHit))
                                    {
                                        BossHit->CreateAttackTraceCPP();
                                    }

                                    for (auto& Result : Overlaps)
                                    {
                                        AActor* HitActor = Result.GetActor();
                                        if (!HitActor || HitActor == BossHit || AlreadyHit.Contains(HitActor))
                                            continue;

                                        AlreadyHit.Add(HitActor);

                                        if (ACharacter* HitChar = Cast<ACharacter>(HitActor))
                                        {
                                            FVector KnockbackDir = (HitChar->GetActorLocation() - BossHit->GetActorLocation()).GetSafeNormal();
                                            FVector KnockbackForce = KnockbackDir * 2000.f + FVector(0, 0, 300.f);
                                            HitChar->LaunchCharacter(KnockbackForce, true, true);
                                        }
                                    }
                                }
                            }),
                        0.1f,
                        true
                    );

                    // 1초 후 AirHitTimer 정리
                    FTimerHandle ClearAirHitTimer;
                    BossInner->GetWorldTimerManager().SetTimer(
                        ClearAirHitTimer,
                        [WeakThis]()
                        {
                            if (!WeakThis.IsValid()) return;
                            WeakThis->GetWorldTimerManager().ClearTimer(WeakThis->AirHitTimer);
                        },
                        1.0f,
                        false
                    );

                    // 착지 충격파 (1초 후)
                    FTimerHandle LandTimer;
                    BossInner->GetWorldTimerManager().SetTimer(
                        LandTimer,
                        [WeakThis]()
                        {
                            if (!WeakThis.IsValid()) return;

                            ATigerBossCharacter* BossLand = WeakThis.Get();

                            FVector ImpactLocation = BossLand->GetActorLocation();
                            float Radius = 1000.0f;

                            TArray<FOverlapResult> Overlaps;
                            FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);

                            if (BossLand->GetWorld()->OverlapMultiByChannel(
                                Overlaps, ImpactLocation, FQuat::Identity, ECC_Pawn, Shape))
                            {
                                for (auto& Result : Overlaps)
                                {
                                    AActor* HitActor = Result.GetActor();
                                    if (!HitActor || HitActor == BossLand)
                                        continue;

                                    if (ACharacter* HitChar = Cast<ACharacter>(HitActor))
                                    {
                                        if (UCharacterMovementComponent* MoveComp = HitChar->GetCharacterMovement())
                                        {
                                            if (!MoveComp->IsFalling())
                                            {
                                                FVector UpForce = FVector(0, 0, 1000.0f);
                                                HitChar->LaunchCharacter(UpForce, true, true);
                                            }
                                        }
                                    }
                                }
                            }
                        },
                        1.0f,
                        false
                    );

                },
                0.9f,
                false
            );
        };

    StopAttackTraceCPP();
}




void ATigerBossCharacter::PerformDashAttack()
{
    FVector Dir = PlayerActor->GetActorLocation() - GetActorLocation();
    TargetRotationForAttack = FRotationMatrix::MakeFromX(Dir).Rotator();
    TargetRotationForAttack.Pitch = 0.f;
    TargetRotationForAttack.Roll = 0.f;

    bWantsToRotateToPlayer = true;

    PostRotationAction = [this]()
        {
            StartDashSequence(); // 회전 완료 후 실행
        };
}


void ATigerBossCharacter::StartDashSequence()
{
    if (!PlayerActor) return;

    AActor* PlayerRef = PlayerActor;
    TWeakObjectPtr<ATigerBossCharacter> WeakThis(this);

    if (DashMontage && GetMesh() && GetMesh()->GetAnimInstance())
    {
        GetMesh()->GetAnimInstance()->Montage_Play(DashMontage);
        if (Voice_Dash)
        {
            UGameplayStatics::PlaySoundAtLocation(this, Voice_Dash, GetActorLocation());
        }
    }

    // 기존 타이머 제거
    GetWorldTimerManager().ClearTimer(DashStartTimer);
    GetWorldTimerManager().ClearTimer(MovementTimer);
    GetWorldTimerManager().ClearTimer(CollisionTimer);

    // 돌진 시작 타이머 (1.9초 뒤)
    GetWorldTimerManager().SetTimer(DashStartTimer, FTimerDelegate::CreateLambda([WeakThis, PlayerRef]()
        {
            if (!WeakThis.IsValid()) return;

            FVector Forward = WeakThis->GetActorForwardVector();


            //  마찰 제거
            if (UCharacterMovementComponent* MoveComp = WeakThis->GetCharacterMovement())
            {
                MoveComp->BrakingFrictionFactor = 0.f;
                MoveComp->BrakingDecelerationWalking = 0.f;
            }

            //  충돌을 Overlap으로 설정 (플레이어와 안 막히게)
            if (UCapsuleComponent* Capsule = WeakThis->GetCapsuleComponent())
            {
                Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
            }

            //  돌진
            WeakThis->LaunchCharacter(Forward * 1800.0f, true, true);


            //  마찰 + 충돌 복구 타이머 (1초 뒤)
            FTimerHandle RestoreTimer;
            WeakThis->GetWorldTimerManager().SetTimer(RestoreTimer, FTimerDelegate::CreateLambda([WeakThis]()
                {
                    if (!WeakThis.IsValid()) return;

                    if (UCharacterMovementComponent* MoveComp = WeakThis->GetCharacterMovement())
                    {
                        MoveComp->BrakingFrictionFactor = 1.f;
                        MoveComp->BrakingDecelerationWalking = 2048.f;
                    }

                    if (UCapsuleComponent* Capsule = WeakThis->GetCapsuleComponent())
                    {
                        Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
                    }
                    //  돌진 끝났으니 충돌 감지도 종료
                    WeakThis->GetWorldTimerManager().ClearTimer(WeakThis->CollisionTimer);

                }), 1.0f, false);

            //  방향 유지
            WeakThis->GetWorldTimerManager().SetTimer(WeakThis->MovementTimer, FTimerDelegate::CreateLambda([WeakThis]()
                {
                    if (!WeakThis.IsValid()) return;

                    if (WeakThis->GetMesh() && WeakThis->GetMesh()->GetAnimInstance() &&
                        WeakThis->GetMesh()->GetAnimInstance()->Montage_IsPlaying(WeakThis->DashMontage))
                    {
                        FVector ForwardDir = WeakThis->GetActorForwardVector();
                        WeakThis->AddMovementInput(ForwardDir, 1.0f);
                    }

                }), 0.02f, true);

            //  충돌 감지 및 넉백
            TWeakObjectPtr<AActor> WeakPlayer(PlayerRef);
            WeakThis->GetWorldTimerManager().SetTimer(WeakThis->CollisionTimer, FTimerDelegate::CreateLambda([WeakThis, WeakPlayer]()
                {
                    if (!WeakThis.IsValid() || !WeakPlayer.IsValid()) return;

                    FVector Origin = WeakThis->GetActorLocation();
                    float Radius = 420.0f;

                    TArray<FOverlapResult> Overlaps;
                    FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);

                    if (WeakThis->GetWorld()->OverlapMultiByChannel(
                        Overlaps,
                        Origin + WeakThis->GetActorForwardVector() * 100.f,
                        FQuat::Identity,
                        ECC_Pawn,
                        Shape))
                    {
                        // 넉백 후 공격 판정 발생
                        if (WeakThis.IsValid())
                        {
                            WeakThis->CreateAttackTraceCPP();  // 블루프린트 함수 호출
                        }
                        for (auto& Result : Overlaps)
                        {
                            AActor* HitActor = Result.GetActor();
                            if (HitActor == WeakPlayer.Get())
                            {
                                FVector KnockbackDir = (HitActor->GetActorLocation() - WeakThis->GetActorLocation()).GetSafeNormal();
                                FVector KnockbackForce = KnockbackDir * 2000.0f + FVector(0, 0, 300.0f);

                                if (ACharacter* HitChar = Cast<ACharacter>(HitActor))
                                {
                                    HitChar->LaunchCharacter(KnockbackForce, true, true);
                                    //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("player knockback by dash"));


                                }

                                // 넉백 후 충돌 감지 종료
                                if (WeakThis->CollisionTimer.IsValid())
                                {
                                    WeakThis->GetWorldTimerManager().ClearTimer(WeakThis->CollisionTimer);
                                }
                            }
                        }
                    }

                }), 0.05f, true, 0.0f); // 넉백은 돌진 시작 1초 후부터 감지

        }), 1.9f, false);
    StopAttackTraceCPP();
}





void ATigerBossCharacter::PerformTeleportAndDash()
{
    // 플레이어 없으면 의미 없음
    if (!PlayerActor) return;

    // 먼저 ActiveClones 배열에서 죽은(무효) 클론 정리
    for (int32 i = ActiveClones.Num() - 1; i >= 0; --i)
    {
        if (!ActiveClones[i].IsValid())
        {
            ActiveClones.RemoveAt(i);
        }
    }

    // 유효한 클론이 하나도 없으면 종료
    if (ActiveClones.Num() == 0) return;

    // 50% 확률로 텔포 스킵 (기존 > 1.0f 는 절대 true가 안 됨)
    if (FMath::FRand() > 0.5f)
    {
        //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("TP Skip"));
        return;
    }

    // 유효한 클론 중 하나 랜덤 선택
    const int32 Index = FMath::RandRange(0, ActiveClones.Num() - 1);
    ABossCloneCharacter* TargetClone = ActiveClones[Index].Get();

    // 무효 포인터 한 번 더 체크
    if (!IsValid(TargetClone))
    {
        ActiveClones.RemoveAt(Index);
        return;
    }

    //  위치 교환
    const FVector BossOldLocation = GetActorLocation();
    const FVector CloneLocation = TargetClone->GetActorLocation();

    // 보스 -> 분신 위치로
    SetActorLocation(CloneLocation);

    // 분신 -> 보스 원래 위치로
    TargetClone->SetActorLocation(BossOldLocation);

    //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("swap with clone"));

    //  텔포 후 바로 돌진
    PerformDashAttack();
}



void ATigerBossCharacter::PerformCloneSpawn()
{
    if (!PlayerActor || !CloneClass) return;

    ActiveClones.Empty();  // 기존 소환된 클론 목록 비우기

    FVector PlayerLocation = PlayerActor->GetActorLocation();

    for (int32 i = 0; i < NumClones; ++i)
    {
        float Angle = FMath::DegreesToRadians(360.f / NumClones * i);
        float RandomOffset = FMath::FRandRange(-100.f, 100.f);
        FVector Offset = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * (CloneRadius + RandomOffset);
        FVector SpawnLocation = PlayerLocation + Offset;

        // 지면 감지
        FHitResult Hit;
        FVector TraceStart = SpawnLocation + FVector(0, 0, 500.f);
        FVector TraceEnd = SpawnLocation - FVector(0, 0, 1000.f);
        if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility))
        {
            SpawnLocation.Z = Hit.ImpactPoint.Z + 5.f;
        }
        else
        {
            SpawnLocation.Z += 150.f;
        }

        FActorSpawnParameters Params;
        ABossCloneCharacter* Clone = GetWorld()->SpawnActor<ABossCloneCharacter>(CloneClass, SpawnLocation, FRotator::ZeroRotator, Params);

        if (!Clone)
        {
            //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("can't spawn"));
            continue;
        }

        //  클론 이동 보장 + 배열에 저장
        if (UCharacterMovementComponent* MoveComp = Clone->GetCharacterMovement())
        {
            MoveComp->SetMovementMode(MOVE_Walking);
            MoveComp->GravityScale = 1.0f;
        }

        ActiveClones.Add(Clone);  // 추적용 저장

        // 파괴 타이머 (배열에서도 제거)
        FTimerHandle DestroyHandle;
        GetWorldTimerManager().SetTimer(DestroyHandle, FTimerDelegate::CreateLambda([this, Clone]()
            {
                if (IsValid(Clone))
                {
                    ActiveClones.Remove(Clone);
                    Clone->Destroy();
                }
            }), CloneLifetime, false);
    }

    //UE_LOG(LogTemp, Warning, TEXT("CloneSpawn: %d clones around player"), NumClones);

    if (FMath::FRand() < 1.0f)
    {
        PerformTeleportAndDash();
    }
}


void ATigerBossCharacter::PerformMultiDash() {}

void ATigerBossCharacter::ExecuteComboStep()
{
    if (!PlayerActor) return;

    switch (ComboStep)
    {
    case 0:
        PerformClawAttack();
        break;
    case 1:
        PerformDashAttack();
        break;
    case 2:
        PerformJumpAttack();
        break;
    default:
        bIsInCombo = false;
        ComboStep = 0;
        return;
    }

    ComboStep++;
    GetWorldTimerManager().SetTimer(WaitTimer, this, &ATigerBossCharacter::ExecuteComboStep, 1.2f, false);
}


void ATigerBossCharacter::StartComboPattern()
{
    bIsInCombo = true;
    ComboStep = 0;
    ExecuteComboStep();
}

void ATigerBossCharacter::EvaluateCombatReady()
{
    if (!PlayerActor) return;
    if (!bCanAttack) return;

    USkeletalMeshComponent* MeshComp = GetMesh();
    if (!MeshComp) return;

    UAnimInstance* AnimInst = MeshComp->GetAnimInstance();
    if (!AnimInst) return;

    // 어떤 몽타주라도 재생 중이면 새 의사결정은 잠시 보류
    if (AnimInst->Montage_IsPlaying(nullptr)) return;

    const float Distance = FVector::Dist(GetActorLocation(), PlayerActor->GetActorLocation());

    // 지금은 Phase1 디테일에 집중 Phase2 특수 패턴은 나중에 여기서 추가
    CurrentIntent = ChooseIntent(Distance);

    switch (CurrentIntent)
    {
    case EBossIntent::Move:
        ExecuteMoveIntent(Distance);
        break;

    case EBossIntent::Attack:
        ExecuteAttackIntent(Distance);
        break;

    case EBossIntent::Evade:
        ExecuteEvadeIntent(Distance);
        break;

    default:
        break;
    }

    UpdateIntentWeights(CurrentIntent);
    LastIntent = CurrentIntent;  // 어떤 Intent를 썼는지 기록

    // 공격 Intent일 때만 FSM 상태를 Attacking으로 고정
    if (CurrentIntent == EBossIntent::Attack)
    {
        bCanAttack = false;
        EnterState(EBossCombatState::Attacking);
    }

    // 공격 Intent일 때만 FSM 상태를 Attacking으로 고정
    if (CurrentIntent == EBossIntent::Attack)
    {
        bCanAttack = false;
        EnterState(EBossCombatState::Attacking);
    }
    /*GEngine->AddOnScreenDebugMessage(
        1,
        1.0f,
        FColor::Yellow,
        FString::Printf(
            TEXT("Move: %.2f | Attack: %.2f | Evade: %.2f"),
            MoveWeightFactor,
            AttackWeightFactor,
            EvadeWeightFactor
        )
    );*/
}




EBossIntent ATigerBossCharacter::ChooseIntent(float DistanceToPlayer)
{
    // 거리 기반 기본 가중치 먼저 계산
    float MoveBase = 0.0f;
    float AttackBase = 0.0f;
    float EvadeBase = 0.0f;

    if (CurrentPhase == EBossPhase::Phase1)
    {
        //거리별 가중치 수치
        if (DistanceToPlayer > 900.f)
        {
            MoveBase = 3.0f;
            AttackBase = 0.8f;
            EvadeBase = 0.5f;
        }
        else if (DistanceToPlayer > 450.f)
        {
            MoveBase = 2.0f;
            AttackBase = 2.5f;
            EvadeBase = 0.8f;
        }
        else
        {
            MoveBase = 1.0f;
            AttackBase = 2.0f;
            EvadeBase = 0.5f;
        }
    }
    else
    {
        // Phase2 일단 기존 값 유지
        if (DistanceToPlayer > 900.f)
        {
            MoveBase = 3.0f;
            AttackBase = 1.0f;
            EvadeBase = 0.5f;
        }
        else if (DistanceToPlayer > 450.f)
        {
            MoveBase = 1.5f;
            AttackBase = 2.5f;
            EvadeBase = 1.0f;
        }
        else
        {
            MoveBase = 0.8f;
            AttackBase = 2.0f;
            EvadeBase = 1.5f;
        }
    }

    // 여기서부터는 동적 가중치 계수를 곱해서 최종 가중치 계산
    float MoveWeight = MoveBase * MoveWeightFactor;
    float AttackWeight = AttackBase * AttackWeightFactor;
    float EvadeWeight = EvadeBase * EvadeWeightFactor;

    //  LastIntent 패널티는 제거 (이제 계수 시스템으로 대체)

    const float Total = MoveWeight + AttackWeight + EvadeWeight;
    if (Total <= KINDA_SMALL_NUMBER)
    {
        // 혹시라도 모든 가중치가 0에 가까우면 기본으로 공격
        return EBossIntent::Attack;
    }

    float R = FMath::FRand() * Total;

    if (R < MoveWeight)
    {
        return EBossIntent::Move;
    }
    else if (R < MoveWeight + AttackWeight)
    {
        return EBossIntent::Attack;
    }
    else
    {
        return EBossIntent::Evade;
    }
}

void ATigerBossCharacter::RelaxWeightTowardsOne(float& Weight, float Step)
{
    if (Weight < 1.0f)
    {
        Weight = FMath::Min(1.0f, Weight + Step);
    }
    else if (Weight > 1.0f)
    {
        Weight = FMath::Max(1.0f, Weight - Step);
    }
    // 이미 1.0 이면 그대로
}

void ATigerBossCharacter::UpdateIntentWeights(EBossIntent UsedIntent)
{
    // 한 번 쓴 Intent의 가중치 감소 비율
    const float NormalDecay = 0.8f;  // 첫 사용 1.0 -> 0.8 정도
    const float StrongDecay = 0.4f;  // 같은 Intent 연속 사용: 0.8 -> 0.32 정도

    // 다른 Intent들의 회복 속도
    const float RecoverStep = 0.4f; // 가중치 회복

    float* UsedWeightPtr = nullptr;

    switch (UsedIntent)
    {
    case EBossIntent::Move:
        UsedWeightPtr = &MoveWeightFactor;

        // 나머지 두 Intent는 회복
        RelaxWeightTowardsOne(AttackWeightFactor, RecoverStep);
        RelaxWeightTowardsOne(EvadeWeightFactor, RecoverStep);
        break;

    case EBossIntent::Attack:
        UsedWeightPtr = &AttackWeightFactor;

        RelaxWeightTowardsOne(MoveWeightFactor, RecoverStep);
        RelaxWeightTowardsOne(EvadeWeightFactor, RecoverStep);
        break;

    case EBossIntent::Evade:
        UsedWeightPtr = &EvadeWeightFactor;

        RelaxWeightTowardsOne(MoveWeightFactor, RecoverStep);
        RelaxWeightTowardsOne(AttackWeightFactor, RecoverStep);
        break;

    default:
        return;
    }

    if (!UsedWeightPtr)
    {
        return;
    }

    // 동일 Intent 두 번 연속이면 훨씬 더 세게 감소
    const bool bSameAsLast = (UsedIntent == LastIntent);
    const float Decay = bSameAsLast ? StrongDecay : NormalDecay;

    *UsedWeightPtr *= Decay;

    // 너무 바닥/폭주 방지
    *UsedWeightPtr = FMath::Clamp(*UsedWeightPtr, 0.2f, 2.0f);
}




void ATigerBossCharacter::ExecuteMoveIntent(float DistanceToPlayer)
{
    if (!PlayerActor) return;

    // 원하는 기본 거리 기준
    const float TooFar = DesiredDistanceFromPlayer + 250.f;  // 이 이상이면 확실히 멂
    const float TooClose = DesiredDistanceFromPlayer - 150.f;  // 이 이하면 조금 가까움
    const float VeryClose = DesiredDistanceFromPlayer - 350.f;  // 이 이하면 너무 가까움

    // 이번 이동 행동 1~2초 랜덤 지속
    MoveBehaviorTimeRemaining = FMath::FRandRange(0.5f, 1.0f);

    if (DistanceToPlayer > TooFar)
    {
        // 멀리 떨어져 있으면 접근
        CurrentMoveBehavior = EMoveBehavior::Approach;
    }
    else if (DistanceToPlayer < VeryClose)
    {
        // 너무 가까우면 강하게 뒤로 빠지기 (도약 말고 보통 이동)
        CurrentMoveBehavior = EMoveBehavior::BackAway;
    }
    else if (DistanceToPlayer < TooClose)
    {
        // 조금 가까우면 살짝 뒤로 빠져서 거리 맞추기
        CurrentMoveBehavior = EMoveBehavior::BackAway;
        MoveBehaviorTimeRemaining *= 0.6f; // 너무 오래 도망가지 않게
    }
    else
    {
        // 적당한 거리면 탐색전 하듯 주위를 도는 움직임
        CurrentMoveBehavior = EMoveBehavior::Strafe;
    }

    LastIntent = EBossIntent::Move;

    // 이동 중 상태
    EnterState(EBossCombatState::Repositioning);
}




void ATigerBossCharacter::ExecuteAttackIntent(float DistanceToPlayer)
{
    // 기존 DecideAndExecuteAttack()를 일단 재활용
    DecideAndExecuteAttack();

    LastIntent = EBossIntent::Attack;
}

void ATigerBossCharacter::ExecuteEvadeIntent(float DistanceToPlayer)
{
    if (!PlayerActor) return;

    // Evade = 무조건 빠른 회피 도약 (Retreat)
    Retreat();

    LastIntent = EBossIntent::Evade;
    // Retreat() 안에서 쿨다운(bCanAttack=false, ResetAttack 타이머) 처리 이미 함
    // 상태는 회피 후 자연스럽게 Repositioning / CombatReady 로 넘어가게 유지
}





void ATigerBossCharacter::HandleHealthChanged(float CurrentHP, float MaxHP)
{
    CachedCurrentHp = CurrentHP;
    CachedMaxHp = MaxHP;

    CurrentHealth = CurrentHP;
    PercentHP = (MaxHP > 0.f) ? (CurrentHP / MaxHP) : 0.f;

    //UE_LOG(LogTemp, Warning, TEXT("보스 체력 갱신: %.1f / %.1f"), CurrentHP, MaxHP);

    CheckPhaseTransition();

    if (CurrentHealth <= 0.f && !bIsDead)
    {
        Dead();
    }
}

void ATigerBossCharacter::callDelegate(float CurrentHP, float MaxHP)
{
    // BP/BPC_Health 에서 이 함수만 호출.
    HandleHealthChanged(CurrentHP, MaxHP);

    // UI나 다른 오브젝트가 이 델리게이트를 듣고 있다면 사용
    OnHealthChanged.Broadcast(CurrentHP, MaxHP);
}

void ATigerBossCharacter::TestPrint_Two(float CurrentHP, float MaxHP)
{
    // 예전 구조 호환용. 지금은 HandleHealthChanged로 통일.
    HandleHealthChanged(CurrentHP, MaxHP);
}


bool ATigerBossCharacter::IsBusy() const
{
    // 낙하 중이면 busy
    if (bIsFalling) return true;
    // 사망시 busy 혹시 몰라서
    if (bIsDead) return true;

    // 전투 상태가 Idle / CombatReady 가 아니면 "행동 중"으로 간주
    switch (CombatState)
    {
    case EBossCombatState::Idle:
    case EBossCombatState::CombatReady:
        return false;

    default:
        return true;
    }
}

void ATigerBossCharacter::ConsiderFollowUp(EAttackPattern LastPattern)
{
    // 추후 LastPattern 다른 콤보
    // 일단 40% 확률로 콤보 아니면 쿨다운

    const float FollowUpChance = 0.4f;

    if (FMath::FRand() < FollowUpChance)
    {
        // 이미 콤보 중이 아니면 콤보 시작
        if (!bIsInCombo)
        {
            StartComboPattern();
        }
        // 이미 콤보 중이면 여기에서 다음 콤보 단계로 넘기거나 유지
    }
    else
    {
        // 콤보 종료, 쿨다운 진입
        bIsInCombo = false;
        EnterState(EBossCombatState::Cooldown);
    }
}

void ATigerBossCharacter::MoveTowardsPlayer(float Scale)
{
    if (!PlayerActor) return;

    const FVector ToPlayer = PlayerActor->GetActorLocation() - GetActorLocation();
    const FVector Dir = ToPlayer.GetSafeNormal();

    AddMovementInput(Dir, Scale);
}

void ATigerBossCharacter::MoveAwayFromPlayer(float Scale)
{
    if (!PlayerActor) return;

    const FVector ToPlayer = PlayerActor->GetActorLocation() - GetActorLocation();
    const FVector BackDir = -ToPlayer.GetSafeNormal();

    AddMovementInput(BackDir, Scale);
}

void ATigerBossCharacter::StrafeAroundPlayer(float Scale, float ForwardBias)
{
    if (!PlayerActor) return;

    const FVector ToPlayer = PlayerActor->GetActorLocation() - GetActorLocation();
    const FVector ToPlayerDir = ToPlayer.GetSafeNormal();

    FVector Tangent = FVector::CrossProduct(ToPlayerDir, FVector::UpVector).GetSafeNormal();

    // 좌/우 랜덤
    if (FMath::RandBool())
    {
        Tangent *= -1.f;
    }

    FVector FinalDir = (Tangent + ToPlayerDir * ForwardBias).GetSafeNormal();

    AddMovementInput(FinalDir, Scale);
}



void ATigerBossCharacter::Dead()
{
    if (bIsDead) return;  // 중복 방지

    bIsDead = true;

    // FSM 및 이동 정지
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->DisableMovement();
    }
    SetActorTickEnabled(false);

    // 충돌 비활성화
    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    // 모든 타이머 정지
    GetWorldTimerManager().ClearAllTimersForObject(this);

    // 애니메이션 중단 후 사망 애니 재생
    if (DeathMontage && GetMesh() && GetMesh()->GetAnimInstance())
    {
        UAnimInstance* Anim = GetMesh()->GetAnimInstance();
        Anim->StopAllMontages(0.2f);
        Anim->Montage_Play(DeathMontage);
    }
}
