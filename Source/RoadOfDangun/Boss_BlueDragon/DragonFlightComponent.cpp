#include "DragonFlightComponent.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/ProjectileMovementComponent.h" // [Added for FireBreath fireballs]

UDragonFlightComponent::UDragonFlightComponent()
{
    // 틱 활성화: 매 프레임 TickComponent가 호출되어 이동이 진행된다.
    PrimaryComponentTick.bCanEverTick = true;
}

void UDragonFlightComponent::BeginPlay()
{
    Super::BeginPlay();

    // 소유자/플레이어 참조를 캐시하여 매 프레임 탐색 비용을 줄인다.
    Owner = GetOwner();
    Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);

    // 비행 기준이 되는 하위 컴포넌트를 이름으로 탐색해 캐시한다.
    FindDragonMousePoint();
}

void UDragonFlightComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (bIsDead || !Owner) return; // 사망 또는 소유자 누락 시 이동 중단

    // 플레이어 추적 모드에서는 매 프레임 목표 위치를 플레이어 위치로 갱신
    if (bTrackLivePlayer)
    {
        UpdateTargetForTracking();
    }

    // 목표 지점으로 이동(회전/충돌 고려 포함)
    MoveToTarget(DeltaTime);

    // 고착 감지 로직(사용 중지): 이전 구조에서 이동 정체를 감지해 재타깃하던 잔재
    // 유지가치가 없다면 완전 삭제 가능
    //const float MovementThreshold = 5.0f;
    //const float StuckTimeLimit = 1.5f;
    //const FVector CurrLoc = Owner->GetActorLocation();
    //const float MovedDist = FVector::Dist(CurrLoc, PreviousLocation);
    //if (MovedDist < MovementThreshold) StuckTimer += DeltaTime;
    //else                               StuckTimer = 0.0f;
    //PreviousLocation = CurrLoc;
}

void UDragonFlightComponent::BeginIntroFlight()
{
    // 인트로 경유점이 세팅되어 있으면 0번부터 이동 시작, 없으면 즉시 종료 이벤트
    if (IntroWaypoints.Num() > 0)
    {
        IntroIndex = 0;
        bDoingIntroMove = true;
        Speed = IntroSpeed;
        CurrentTarget = IntroWaypoints[IntroIndex]->GetActorLocation();
    }
    else
    {
        bDoingIntroMove = false;
        OnIntroFinished.Broadcast();
    }
}

void UDragonFlightComponent::SetRandomFlyTarget()
{
    // 캐릭터의 스켈레탈 메시 본 중 "D_Head" 위치를 현재 위치로 삼아
    // 그 기준에서 적절히 떨어진 랜덤 타깃을 선정한다.
    USkeletalMeshComponent* Mesh = Owner ? Owner->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
    if (!Mesh) return;

    const FVector CurrentLocation = Mesh->GetBoneLocation(FName("D_Head"));
    GoToRandomFly(CurrentLocation);

    // 디버그 시각화: 선택된 타깃과 거리 확인 (시연용으로 전체 주석 처리)
    //if (GEngine)
    //{
    //    const float Dist = FVector::Dist(CurrentLocation, CurrentTarget);
    //    GEngine->AddOnScreenDebugMessage(
    //        -1, 2.0f, FColor::Cyan,
    //        FString::Printf(TEXT("[RandomFly] Target=%s  Dist=%.1f"), *CurrentTarget.ToString(), Dist));
    //}
    //DrawDebugSphere(GetWorld(), CurrentTarget, 120.f, 16, FColor::Cyan, false, 2.0f, 0, 2.0f);
    //DrawDebugLine(GetWorld(), CurrentLocation, CurrentTarget, FColor::Blue, false, 2.0f, 0, 2.0f);
}

void UDragonFlightComponent::SetMoveTarget(const FVector& InTarget)
{
    // 외부에서 임의 목표를 지정할 때는 추적/공격 모드를 모두 해제하고 순수 이동으로 전환
    bTrackLivePlayer = false;
    bFireballMode = false;
    bArrived = false;
    CurrentTarget = InTarget;
}

void UDragonFlightComponent::StartTrackPlayer(bool bInFireballMode)
{
    // 추적 모드 시작 로직은 유지하되, "실시간 갱신" 대신 "주기적 샘플링"을 사용
    bTrackLivePlayer = true;
    bFireballMode = bInFireballMode;
    bArrived = false;

    OnHeadingModeTriggered.Broadcast();

    // 기존 타이머 정리 후, 샘플링 타이머 가동
    GetWorld()->GetTimerManager().ClearTimer(TrackSampleTimerHandle);
    SampleTrackedPlayerLocation(); // 즉시 1회 스냅샷 (한 박자 늦게 쫓아가도록 초기 기준 고정)
    GetWorld()->GetTimerManager().SetTimer(
        TrackSampleTimerHandle,
        this,
        &UDragonFlightComponent::SampleTrackedPlayerLocation,
        TrackSampleInterval,
        true
    );

    // 추적 모드 타임아웃 타이머
    GetWorld()->GetTimerManager().ClearTimer(HeadingTimeoutHandle);
    GetWorld()->GetTimerManager().SetTimer(
        HeadingTimeoutHandle,
        [this]()
        {
            if (bTrackLivePlayer && !bFireballMode)
            {
                StopTrackPlayer();
                OnHeadingModeEnded.Broadcast();
            }
        },
        HeadingTimeoutDuration, false
    );
}

void UDragonFlightComponent::StopTrackPlayer()
{
    // 샘플링 타이머도 함께 해제
    bTrackLivePlayer = false;
    bFireballMode = false;
    bArrived = false;

    GetWorld()->GetTimerManager().ClearTimer(HeadingTimeoutHandle);
    GetWorld()->GetTimerManager().ClearTimer(TrackSampleTimerHandle); // [Added]
    TrackedPlayerLocation = FVector::ZeroVector; // 스냅샷 초기화
}

void UDragonFlightComponent::UpdateTargetForTracking()
{
    // [Modified] 실시간 위치 사용 → "샘플링된 위치"만 사용
    // 한글 주석: 매 프레임 플레이어 위치를 읽지 않고, 타이머로 저장해 둔 스냅샷 좌표를 사용한다.
    if (!Player) return;

    // 샘플링이 아직 없으면 안전하게 1회 샘플링
    if (TrackedPlayerLocation.IsNearlyZero())
    {
        SampleTrackedPlayerLocation(); // [Added]
    }

    CurrentTarget = TrackedPlayerLocation; // 한 박자 늦은 타깃으로 이동
}

// ===== 플레이어 위치 스냅샷 저장 =====
void UDragonFlightComponent::SampleTrackedPlayerLocation()
{
    // 한글 주석: 플레이어의 현재 위치를 1초 간격으로 TrackedPlayerLocation에 저장한다.
    // 살짝 위(Z+25)를 바라보도록 보정해 시각적으로 자연스럽게 만든다.
    if (!Player) return;

    FVector P = Player->GetActorLocation();
    P.Z += 25.0f; // 기존 코드의 시선 보정 유지
    TrackedPlayerLocation = P;
}

void UDragonFlightComponent::MoveToTarget(float DeltaTime)
{
    // 비행 기준점(DragonMousePoint) 누락 시 동작 중단
    if (!Owner || !DragonMousePoint) return;

    // 1) 본 기준점 위치 보간: 급격한 본 진동을 줄이고 안정된 방향을 계산하기 위함
    const FVector RawLocation = DragonMousePoint->GetComponentLocation();
    const FVector SmoothLocation = FMath::VInterpTo(LastBoneLocation, RawLocation, DeltaTime, 8.0f);
    LastBoneLocation = SmoothLocation;

    // 2) 목표까지의 정규화 방향/잔여 거리/도착 판정 거리 계산
    const FVector Direction = (CurrentTarget - SmoothLocation).GetSafeNormal();
    const float RemainingDistance = FVector::Dist(SmoothLocation, CurrentTarget);
    const float ArrivalDist = bTrackLivePlayer
        ? (bFireballMode ? FireballAttackDistance : TrackingDistanceThreshold)
        : ArrivalDistance;

    // 3) 현재 회전과 목표 회전 비교(디버깅 용도): 급회전 상황 파악에 유용
    const FRotator CurrentRotation = Owner->GetActorRotation();
    const FRotator TargetRotation = Direction.Rotation();
    const float YawDiff = FMath::Abs(FRotator::NormalizeAxis(TargetRotation.Yaw - CurrentRotation.Yaw));

    // 디버그 메시지 출력 부분 비활성화
    //if (GEngine)
    //{
    //    GEngine->AddOnScreenDebugMessage(
    //        -1, 0.f, FColor::Green,
    //        FString::Printf(TEXT("[Flight] Remain=%.1f  Arrival=%.1f  Z(Dragon)=%.1f  Z(Target)=%.1f  YawDiff=%.1f"),
    //            RemainingDistance, ArrivalDist, SmoothLocation.Z, CurrentTarget.Z, YawDiff));
    //}

    // 4) 이동 벡터 계산과 충돌 고려 이동
    const FVector MoveDelta = Direction * Speed * DeltaTime;
    const FVector NewActorLocation =
        (MoveDelta.Size() > RemainingDistance)
        ? Owner->GetActorLocation() + Direction * RemainingDistance // 오버슈팅 방지
        : Owner->GetActorLocation() + MoveDelta;

    FHitResult Hit;
    Owner->SetActorLocation(NewActorLocation, true, &Hit); // Sweep=true로 충돌 고려 이동

    // 5) 회전 보간: 목표 방향을 천천히 따라가도록 보간(시각적으로 부드러운 선회)
    Owner->SetActorRotation(FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, 0.6f));

    // 6) 도착 판정 및 후속 처리
    // [Modified] FireBreath 2단계(플레이어 추적 중)에는 "도착 판정"을 무시한다.
    const bool bSkipArrivalByFireBreath = (bFireBreathActive && FireBreathPhase == 2);
    if (!bSkipArrivalByFireBreath && RemainingDistance < ArrivalDist)
    {
        bArrived = true;
        OnArrived(); // 공통 도착 이벤트

        // 인트로 비행 중이면 다음 경유점을 목표로 하거나, 종료 이벤트 브로드캐스트
        if (bDoingIntroMove)
        {
            IntroIndex++;
            if (IntroIndex < IntroWaypoints.Num())
            {
                Speed = IntroSpeed; // 인트로 구간 속도 재설정(일관성 유지)
                CurrentTarget = IntroWaypoints[IntroIndex]->GetActorLocation();
            }
            else
            {
                bDoingIntroMove = false;
                OnIntroFinished.Broadcast();
            }
            return;
        }
    }
}

void UDragonFlightComponent::OnArrived()
{
    // 공통 도착 시그널: 외부에서 상태 전환/다음 동작 트리거 등에 활용 가능
    OnArrivedTarget.Broadcast();

    // GroundSlam 상태 머신 처리
    if (bGroundSlamActive)
    {
        if (GroundSlamPhase == 1)
        {
            // 1단계(가장 먼 웨이포인트 도착) 완료 -> 플레이어 좌표 스냅샷 후 돌진 단계로 전환
            if (Player)
            {
                GroundSlamPlayerLock = Player->GetActorLocation();
            }
            else
            {
                GroundSlamPlayerLock = CurrentTarget; // 플레이어가 없으면 현재 목표를 그대로 사용(안전장치)
            }

            // 속도 변경 및 타깃 설정
            SavedNormalSpeed = (SavedNormalSpeed <= 0.0f) ? Speed : SavedNormalSpeed; // 최초 진입 시 저장
            Speed = GroundSlamSpeed;
            SetMoveTarget(GroundSlamPlayerLock); // 추적 모드 해제된 순수 이동으로 설정
            GroundSlamPhase = 2;

            // GroundSlam Phase 2 디버그 메시지/도형 비활성화
            //if (GEngine)
            //{
            //    GEngine->AddOnScreenDebugMessage(
            //        -1, 2.0f, FColor::Yellow,
            //        FString::Printf(TEXT("[GroundSlam] Phase 2: Dash to %s (Speed=%.1f)"),
            //            *GroundSlamPlayerLock.ToString(), GroundSlamSpeed));
            //}
            //DrawDebugSphere(GetWorld(), GroundSlamPlayerLock, 150.f, 20, FColor::Yellow, false, 2.0f, 0, 3.0f);
        }
        else if (GroundSlamPhase == 2)
        {
            // 2단계(플레이어 좌표로의 돌진) 도착 -> 임팩트 이벤트 브로드캐스트 및 속도 복구
            OnGroundSlamImpact.Broadcast(GroundSlamPlayerLock); // [Modified]

            // 속도 복구 및 상태 종료
            if (SavedNormalSpeed > 0.0f)
            {
                Speed = SavedNormalSpeed;
            }
            SavedNormalSpeed = 0.0f;
            bGroundSlamActive = false;
            GroundSlamPhase = 0;

            // GroundSlam 임팩트 디버그 메시지 비활성화
            //if (GEngine)
            //{
            //    GEngine->AddOnScreenDebugMessage(
            //        -1, 2.0f, FColor::Red,
            //        TEXT("[GroundSlam] Impact! Speed restored and sequence finished."));
            //}
        }
    }

    // FireBreath 상태 머신 처리
    if (bFireBreathActive)
    {
        if (FireBreathPhase == 1)
        {
            // 단계 전환: 웨이포인트 도착 -> 브레스 추적 모드 돌입
            FireBreathSavedSpeed = (FireBreathSavedSpeed <= 0.0f) ? Speed : FireBreathSavedSpeed; // 최초 진입 시 저장
            Speed = FireBreathSpeed;                  // 브레스 중 속도 (에디터에서 튜닝)
            StartTrackPlayer(true);                   // 플레이어 추적 시작(공격 사거리 모드로 설정)
            // MoveToTarget에서 FireBreathPhase==2 동안 도착 판정 무시
            FireBreathPhase = 2;

            // 행동 트리 연동: BT 태스크에서 "브레스 시작" 타이밍으로 사용할 이벤트
            OnFireBreathStarted.Broadcast();          // 애니메이션, 이펙트 시작은 블루프린트에서 처리

            // 코드 내부에서 브레스 지속시간 타이머를 관리한다 (기존 FireBreathDuration 사용)
            GetWorld()->GetTimerManager().ClearTimer(FireBreathTimerHandle);
            GetWorld()->GetTimerManager().SetTimer(
                FireBreathTimerHandle,
                [this]()
                {
                    // 타이머 만료: 속도 복원, 추적 종료, 이벤트 브로드캐스트
                    if (FireBreathSavedSpeed > 0.0f)
                    {
                        Speed = FireBreathSavedSpeed;
                    }
                    FireBreathSavedSpeed = 0.0f;

                    // 파이어볼 발사 타이머 정리
                    GetWorld()->GetTimerManager().ClearTimer(FireBreathFireballTimerHandle);

                    StopTrackPlayer();

                    // StartTrackPlayer(true) 경로에서는 자동 종료 신호가 발생하지 않으므로 직접 브로드캐스트
                    OnHeadingModeEnded.Broadcast();

                    // 행동 트리 연동: BT 태스크에서 "브레스 종료" 시점으로 사용할 이벤트
                    OnFireBreathEnded.Broadcast();

                    // 상태 종료
                    bFireBreathActive = false;
                    FireBreathPhase = 0;
                },
                FireBreathDuration, false
            );

            // FireBreathDuration 동안 FireballInterval 간격으로 SpawnFireBreathProjectile 실행
            GetWorld()->GetTimerManager().ClearTimer(FireBreathFireballTimerHandle);
            GetWorld()->GetTimerManager().SetTimer(
                FireBreathFireballTimerHandle,
                this,
                &UDragonFlightComponent::SpawnFireBreathProjectile,
                FireballInterval,
                true
            );

            // FireBreath Phase 2 디버그 메시지 비활성화
            //if (GEngine)
            //{
            //    GEngine->AddOnScreenDebugMessage(
            //        -1, 2.0f, FColor::Orange,
            //        FString::Printf(TEXT("[FireBreath] Phase 2: Tracking player for %.2fs (Speed=%.1f)"),
            //            FireBreathDuration, Speed));
            //}
        }
    }
}


void UDragonFlightComponent::GoToRandomFly(const FVector& CurrentLocation)
{
    // 랜덤 후보 선택 시 "전방 대비 과도한 후방 각도" 후보를 걸러낸다.
    //           너무 많은 시도 후에도 만족하지 못하면 Fallback으로 아무 웨이포인트나 선택한다.

    const int32 MaxTries = 10;                      // 기존 시도 횟수 유지
    const float MinDistance = RandomMinDistance;    // 기존 최소 거리 조건 유지
    const float BehindAngleRejectDeg = 170.0f;      // [Added] 전방 기준 허용 각도(이보다 크면 '너무 뒤쪽'으로 간주)

    const FVector Forward = Owner ? Owner->GetActorForwardVector() : FVector::ForwardVector; // [Added] 드래곤 전방

    for (int32 Try = 0; Try < MaxTries; ++Try)
    {
        if (Waypoints.Num() <= 0) break;

        const int32 Index = FMath::RandRange(0, Waypoints.Num() - 1);
        AActor* WP = Waypoints[Index];
        if (!WP) continue;

        const FVector Candidate = WP->GetActorLocation();
        const float   D = FVector::Dist(CurrentLocation, Candidate);

        // 전방-후보 방향 각도 계산
        const FVector ToCand = (Candidate - CurrentLocation).GetSafeNormal();
        const float Dot = FVector::DotProduct(Forward, ToCand);
        const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)));

        // 랜덤 선택 디버그 메시지 비활성화
        //if (GEngine)
        //{
        //    // 디버그: 각도와 거리 출력
        //    GEngine->AddOnScreenDebugMessage(
        //        -1, 0.6f, FColor::Silver,
        //        FString::Printf(TEXT("[GoToRandomFly] Try=%d  WP=%s  Dist=%.1f  Angle=%.1f"),
        //            Try, *WP->GetName(), D, AngleDeg));
        //}

        // 최소 거리 + 전방 각도 조건을 모두 만족해야 채택
        if (D >= MinDistance && AngleDeg <= BehindAngleRejectDeg)
        {
            // 순수 이동 모드로 전환하여 타깃 설정
            bTrackLivePlayer = false;
            bFireballMode = false;
            bArrived = false;
            CurrentTarget = Candidate;

            // 선택 결과 디버그 메시지/도형 비활성화
            //if (GEngine)
            //{
            //    GEngine->AddOnScreenDebugMessage(
            //        -1, 2.0f, FColor::Cyan,
            //        FString::Printf(TEXT("[GoToRandomFly] PICKED: %s  Dist=%.1f  Angle=%.1f"),
            //            *WP->GetName(), D, AngleDeg));
            //}
            //DrawDebugSphere(GetWorld(), CurrentTarget, 120.f, 16, FColor::Cyan, false, 5.0f, 0, 2.0f);
            return;
        }
    }

    // Fallback: 조건을 만족하는 지점을 찾지 못했을 때 임의로 하나 선택
    if (Waypoints.Num() > 0)
    {
        AActor* WP = Waypoints[FMath::RandRange(0, Waypoints.Num() - 1)];
        if (WP)
        {
            bTrackLivePlayer = false;
            bFireballMode = false;
            bArrived = false;
            CurrentTarget = WP->GetActorLocation();

            // Fallback 디버그 메시지/도형 비활성화
            //if (GEngine)
            //{
            //    const float D = FVector::Dist(CurrentLocation, CurrentTarget);
            //    GEngine->AddOnScreenDebugMessage(
            //        -1, 2.0f, FColor::Purple,
            //        FString::Printf(TEXT("[GoToRandomFly] FALLBACK PICK: %s  Dist=%.1f"), *WP->GetName(), D));
            //}
            //DrawDebugSphere(GetWorld(), CurrentTarget, 120.f, 16, FColor::Purple, false, 2.0f, 0, 2.0f);
        }
    }
}


void UDragonFlightComponent::FindDragonMousePoint()
{
    // 소유 액터 하위 컴포넌트들을 순회하면서 이름으로 DragonMousePoint 찾기

    if (!Owner) return;

    TArray<USceneComponent*> Children;
    Owner->GetComponents<USceneComponent>(Children);

    DragonMousePoint = nullptr;
    for (USceneComponent* Comp : Children)
    {
        if (Comp && Comp->GetName() == DragonMousePointName.ToString())
        {
            DragonMousePoint = Comp;
            break;
        }
    }
}

void UDragonFlightComponent::GroundSlam()
{
    // 배열 사용부 변경: GroundSlamWaypoints → HighWaypoints
    if (!Owner || HighWaypoints.Num() == 0)
    {
        //if (GEngine)
        //{
        //    GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("[GroundSlam] No Owner or HighWaypoints."));
        //}
        return;
    }

    // 어떤 모드든 정리(인트로/추적 종료 후 순수 이동으로)
    bDoingIntroMove = false;
    StopTrackPlayer();

    // FireBreath가 진행 중이었다면 정리
    if (bFireBreathActive)
    {
        GetWorld()->GetTimerManager().ClearTimer(FireBreathTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(FireBreathFireballTimerHandle); // [Added for FireBreath fireballs]
        bFireBreathActive = false;
        FireBreathPhase = 0;
        if (FireBreathSavedSpeed > 0.0f) { Speed = FireBreathSavedSpeed; FireBreathSavedSpeed = 0.0f; }
        OnFireBreathEnded.Broadcast();
        OnHeadingModeEnded.Broadcast();
    }

    // 기준 위치: 드래곤 마우스포인트가 아니라 "플레이어 위치"를 기준으로 가장 먼 웨이포인트를 찾는다.
    const FVector From = (Player) ? Player->GetActorLocation()
        : Owner->GetActorLocation();

    // 1. 가장 먼 웨이포인트 선택 (HighWaypoints 배열을 사용)
    const FVector Farthest = FindFarthestWaypointFrom(From);

    // 시퀀스 상태 초기화
    bGroundSlamActive = true;
    GroundSlamPhase = 1;
    SavedNormalSpeed = Speed; // 원래 속도 저장

    // 2. 해당 포인트로 비행(기존 이동 로직 활용)
    SetMoveTarget(Farthest);

    // GroundSlam Phase 1 디버그 메시지/도형 비활성화
    //if (GEngine)
    //{
    //    GEngine->AddOnScreenDebugMessage(
    //        -1, 2.0f, FColor::Yellow,
    //        FString::Printf(TEXT("[GroundSlam] Phase 1: Fly to farthest WP %s (SavedSpeed=%.1f)"),
    //            *Farthest.ToString(), SavedNormalSpeed));
    //}
    //DrawDebugSphere(GetWorld(), Farthest, 150.f, 20, FColor::Orange, false, 3.0f, 0, 3.0f);
}

// GroundSlam 보조: 기준점에서 가장 먼 웨이포인트를 선택할 때 HighWaypoints 배열을 사용
FVector UDragonFlightComponent::FindFarthestWaypointFrom(const FVector& From) const
{
    float MaxDist = -1.0f;
    FVector Best = From;

    for (AActor* WP : HighWaypoints)  // 'GroundSlamWaypoints' → 'HighWaypoints'
    {
        if (!WP) continue;
        const FVector Loc = WP->GetActorLocation();
        const float D = FVector::DistSquared(From, Loc); // 제곱 거리로 비교(성능)
        if (D > MaxDist)
        {
            MaxDist = D;
            Best = Loc;
        }
    }
    return Best;
}

// FireBreath 본체: 요구사항 전 과정 구현
void UDragonFlightComponent::FireBreath()
{
    // 배열 사용부 변경: GroundSlamWaypoints → HighWaypoints
    if (!Owner || HighWaypoints.Num() == 0)
    {
        //if (GEngine)
        //{
        //    GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("[FireBreath] No Owner or HighWaypoints."));
        //}
        return;
    }

    // 다른 모드/상태 정리
    bDoingIntroMove = false;
    StopTrackPlayer();

    // 진행 중인 GroundSlam 종료 정리
    if (bGroundSlamActive)
    {
        bGroundSlamActive = false;
        GroundSlamPhase = 0;
        if (SavedNormalSpeed > 0.0f) { Speed = SavedNormalSpeed; SavedNormalSpeed = 0.0f; }
    }

    // 진행 중인 FireBreath가 있다면 초기화
    if (bFireBreathActive)
    {
        GetWorld()->GetTimerManager().ClearTimer(FireBreathTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(FireBreathFireballTimerHandle); // [Added for FireBreath fireballs]
        bFireBreathActive = false;
        FireBreathPhase = 0;
        if (FireBreathSavedSpeed > 0.0f) { Speed = FireBreathSavedSpeed; FireBreathSavedSpeed = 0.0f; }
        OnFireBreathEnded.Broadcast();
        OnHeadingModeEnded.Broadcast();
    }

    // 기준 위치: 드래곤 마우스포인트가 아니라 "플레이어 위치"를 기준으로 가장 먼 웨이포인트를 찾는다.
    const FVector From = (Player) ? Player->GetActorLocation()
        : Owner->GetActorLocation();

    // 1. 가장 먼 웨이포인트 선택
    const FVector Farthest = FindFarthestWaypointFrom(From);

    // 상태 초기화
    bFireBreathActive = true;
    FireBreathPhase = 1;
    FireBreathSavedSpeed = Speed; // 원래 속도 저장

    // 해당 포인트로 이동 시작
    SetMoveTarget(Farthest);

    // FireBreath Phase 1 디버그 메시지
    //if (GEngine)
    //{
    //    GEngine->AddOnScreenDebugMessage(
    //        -1, 2.0f, FColor::Orange,
    //        FString::Printf(TEXT("[FireBreath] Phase 1: Fly to farthest WP %s (SavedSpeed=%.1f)"),
    //            *Farthest.ToString(), FireBreathSavedSpeed));
    //}
    //DrawDebugSphere(GetWorld(), Farthest, 150.f, 20, FColor::Orange, false, 3.0f, 0, 3.0f);
}

// FireBreath 중 일정 간격으로 호출되어 파이어볼을 발사하는 함수
void UDragonFlightComponent::SpawnFireBreathProjectile()
{
    // FireBreath 상태가 아니거나, 실제 브레스 발사 단계(Phase 2)가 아니면 발사하지 않는다.
    if (!bFireBreathActive || FireBreathPhase != 2) return;
    if (bIsDead) return;
    if (!FireballProjectileClass) return;

    // 발사 위치: DragonMousePoint 기준
    if (!DragonMousePoint)
    {
        FindDragonMousePoint(); // 누락 시 한 번 더 탐색
    }
    if (!DragonMousePoint) return;

    if (!Player) return;

    UWorld* World = GetWorld();
    if (!World) return;

    const FVector Start = DragonMousePoint->GetComponentLocation();
    FVector Target = Player->GetActorLocation();
    Target.Z += 40.0f; // 한글 주석: 살짝 위를 노려서 시각적으로 자연스러운 궤적 생성

    const FVector Direction = (Target - Start).GetSafeNormal();
    const FRotator Rotation = Direction.Rotation();
    const FTransform SpawnTM(Rotation, Start);

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* Projectile = World->SpawnActor<AActor>(FireballProjectileClass, SpawnTM, Params);
    if (!Projectile) return;

    UProjectileMovementComponent* MoveComp = Projectile->FindComponentByClass<UProjectileMovementComponent>();
    if (!MoveComp)
    {
        // 투사체에 이동 컴포넌트가 없으면 동적으로 추가
        MoveComp = NewObject<UProjectileMovementComponent>(Projectile);
        if (MoveComp)
        {
            MoveComp->RegisterComponent();
            MoveComp->UpdatedComponent = Projectile->GetRootComponent();
            MoveComp->InitialSpeed = FireballSpeed;
            MoveComp->MaxSpeed = FireballSpeed;
            MoveComp->bRotationFollowsVelocity = true;
            MoveComp->bShouldBounce = false;
            MoveComp->Velocity = Direction * FireballSpeed;
        }
    }
    else
    {
        // 이미 존재하는 경우 속도만 갱신
        MoveComp->Velocity = Direction * FireballSpeed;
    }
}
