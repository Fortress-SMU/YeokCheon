#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DragonFlightComponent.generated.h"

// [Added for FireBreath fireballs] 파이어볼 투사체 타입 전방 선언
class AActor;

// ===== 이벤트 델리게이트 =====
// 인트로 경유점(마지막) 도착 후 호출
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnIntroFinished);
// 임의의 타깃 지점에 도착했을 때 호출 (랜덤/인트로/추적 등 공통)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnArrivedTarget);
// 플레이어 추적(Heading) 모드가 시작될 때 브로드캐스트
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHeadingEventDispatcher);
// 플레이어 추적(Heading) 모드가 타이머 등으로 종료될 때 브로드캐스트
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHeadingEndEventDispatcher);

// [Added] 지면 강타(GroundSlam) 충돌/피해 타이밍에 사용될 이벤트(블루프린트에서 처리)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGroundSlamImpact, FVector, ImpactCenter);

// [Added] FireBreath 시작/종료 알림 이벤트(공격 실행은 블루프린트에서 처리)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFireBreathStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFireBreathEnded);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RoadOfDangun_API UDragonFlightComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDragonFlightComponent();

protected:
    // 액터 시작 시점 초기화: 소유자 포인터/플레이어 참조 캐싱, 마우스 포인트 찾기 등
    virtual void BeginPlay() override;

public:
    // 프레임마다 호출: 추적 모드 갱신, MoveToTarget 실행 등 비행의 메인 루프
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ===== 외부(BT/AI/블루프린트)에서 호출하는 공개 API =====

    UFUNCTION(BlueprintCallable, Category = "Flight")
    // 인트로 비행 시작: IntroWaypoints[0]부터 순서대로 향한다. 마지막에 OnIntroFinished 호출.
    void BeginIntroFlight();

    UFUNCTION(BlueprintCallable, Category = "Flight")
    // 랜덤 비행 목표 선택: Waypoints 중 하나를 거리 조건에 맞게 뽑아 CurrentTarget 설정
    void SetRandomFlyTarget();

    UFUNCTION(BlueprintCallable, Category = "Flight")
    // 외부에서 임의의 목표를 지정해 이동 시작(추적/공격 모드 해제)
    void SetMoveTarget(const FVector& InTarget);

    UFUNCTION(BlueprintCallable, Category = "Flight")
    // 플레이어 추적 모드 시작. bFireballMode=true면 도착 판정 거리를 공격 사거리(FireballAttackDistance)로 사용.
    void StartTrackPlayer(bool bFireballMode);

    UFUNCTION(BlueprintCallable, Category = "Flight")
    // 플레이어 추적 모드 종료. 타이머 클리어 및 상태 플래그 초기화.
    void StopTrackPlayer();

    UFUNCTION(BlueprintPure, Category = "Flight")
    // 이동 목표에 도달했는지 반환(ArrivalDistance 또는 추적/공격 모드별 임계값으로 판정)
    bool HasArrived() const { return bArrived; }

    UFUNCTION(BlueprintPure, Category = "Flight")
    // 인트로 비행이 종료되었는지 반환
    bool IsIntroFinished() const { return !bDoingIntroMove; }

    // [Added] 지면 강타 공격(기존)
    UFUNCTION(BlueprintCallable, Category = "Flight|Attack")
    void GroundSlam();

    // [Added] 파이어 브레스 공격: 행동 트리 태스크에서 이 함수를 호출하여 시퀀스를 시작한다.
    // 단계:
    // 1) GroundSlamWaypoints 중 현재 위치에서 가장 먼 지점을 목적지로 비행한다.
    // [Modified] → HighWaypoints 중 '플레이어와 가장 먼' 지점을 목적지로 비행한다. (이름 및 기준 변경)
    // 2) 도착하면 Speed를 FireBreathSpeed(기본 값 유지)로 변경하고,
    // 3) 플레이어를 추적 대상으로 설정(StartTrackPlayer(true) 사용). 이때 도착 판정은 무시된다.
    // 4) OnFireBreathStarted 이벤트를 브로드캐스트한다. 공격 이펙트/사운드는 블루프린트에서 처리한다.
    // 5) FireBreathDuration 타이머가 만료되면 Speed를 원래 값으로 복원하고 StopTrackPlayer()를 호출한다.
    // 6) OnFireBreathEnded 이벤트를 브로드캐스트하고 시퀀스를 종료한다.
    UFUNCTION(BlueprintCallable, Category = "Flight|Attack")
    void FireBreath();

    // 외부에서 사망 상태를 마킹. 사망 시 틱 로직에서 이동을 중단한다.
    void MarkDead(bool bDead) { bIsDead = bDead; }

    // ===== 이벤트 바인딩용 프로퍼티 =====
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnIntroFinished OnIntroFinished;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnArrivedTarget OnArrivedTarget;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnHeadingEventDispatcher OnHeadingModeTriggered;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnHeadingEndEventDispatcher OnHeadingModeEnded;

    // [Added] 지면 강타 임팩트(피해 타이밍) 이벤트
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnGroundSlamImpact OnGroundSlamImpact;

    // [Added] 파이어 브레스 시작/종료 이벤트
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnFireBreathStarted OnFireBreathStarted;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnFireBreathEnded OnFireBreathEnded;

    // ===== 에디터 노출 데이터(디자인/튜닝용) =====

    UPROPERTY(EditAnywhere, Category = "Intro")
    // 인트로 비행 경유점. 배열 순서대로 이동
    TArray<AActor*> IntroWaypoints;

    UPROPERTY(EditInstanceOnly, Category = "Flight")
    // 랜덤 비행에 사용할 후보 지점들
    TArray<AActor*> Waypoints;

    // [Added] 그라운드 슬램 전용 웨이포인트 배열을 추가
    // [Renamed] GroundSlamWaypoints → HighWaypoints (여러 공격에서 공용 사용)
    UPROPERTY(EditAnywhere, Category = "Attack")
    TArray<AActor*> HighWaypoints;  // 그라운드 슬램/파이어브레스 시작점으로 활용 (이름 변경)

    UPROPERTY(EditAnywhere, Category = "Flight")
    // 소유 액터 하위에서 탐색할 기준 컴포넌트 이름(비행 기준 위치로 사용)
    FName DragonMousePointName = TEXT("DragonMousePoint");

    UPROPERTY(EditAnywhere, Category = "Flight")
    // 플레이어 Heading 모드의 지속 시간(초). 타이머 만료 시 자동 종료.
    float HeadingTimeoutDuration = 6.0f;

    UPROPERTY(EditAnywhere, Category = "Flight")
    // 추적 모드(일반)에서 도착으로 간주할 거리(짧은 값: 근접 추적 느낌)
    float TrackingDistanceThreshold = 50.0f;

    UPROPERTY(EditAnywhere, Category = "Flight")
    // 공격 모드(브레스 등)일 때 도착으로 간주할 거리(사거리 역할)
    float FireballAttackDistance = 1500.0f;

    UPROPERTY(EditAnywhere, Category = "Flight")
    // 랜덤 타깃을 고를 때 최소 보장 이동거리(너무 가까운 타깃 선택 방지)
    float RandomMinDistance = 4000.0f;

    UPROPERTY(EditAnywhere, Category = "Flight")
    // 일반 이동 속도(부동 소수점 단위: UU/s)
    float Speed = 1500.0f;

    UPROPERTY(EditAnywhere, Category = "Flight")
    // 인트로 이동 시 사용할 속도
    float IntroSpeed = 1800.0f;

    UPROPERTY(EditAnywhere, Category = "Flight")
    // 일반 이동에서 도착으로 간주하는 거리(추적/공격 모드 제외)
    float ArrivalDistance = 430.0f;

    // [Added] 지면 강타 비행 속도(웨이포인트 도착 후 플레이어 지점으로 돌진할 때 사용)
    UPROPERTY(EditAnywhere, Category = "Attack")
    float GroundSlamSpeed = 2500.0f;

    // [Added] 파이어 브레스 속도(웨이포인트 도달 후 브레스 중 전진 속도)
    UPROPERTY(EditAnywhere, Category = "Attack")
    float FireBreathSpeed = 150.0f;

    // [Added] 파이어 브레스 지속 시간(타이머)
    UPROPERTY(EditAnywhere, Category = "Attack")
    float FireBreathDuration = 5.0f;

    // [Added] 파이어 브레스 동안 파이어볼 발사 간격(초)
    UPROPERTY(EditAnywhere, Category = "Attack")
    float FireballInterval = 0.1f;

    // [Added] 파이어 브레스에서 사용할 파이어볼 속도
    UPROPERTY(EditAnywhere, Category = "Attack")
    float FireballSpeed = 1800.0f;

    // [Added] 파이어 브레스에서 사용할 투사체 클래스
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (AllowPrivateAccess = "true"))
    TSubclassOf<AActor> FireballProjectileClass;

    // ===== [Added] 샘플링 간격(초): 초마다 플레이어 위치를 스냅샷 =====
    UPROPERTY(EditAnywhere, Category = "Flight")
    float TrackSampleInterval = 0.6f; // 초 단위 갱신 (에디터에서 조정 가능)

private:
    // ===== 내부 상태(캐싱/플래그/누적 값) =====

    // 이 컴포넌트를 보유한 액터(드래곤) 캐시
    AActor* Owner = nullptr;

    // 추적 대상(플레이어) 참조 캐시
    AActor* Player = nullptr;

    // 비행 기준이 되는 하위 씬 컴포넌트(본 위치 보간 기준)
    USceneComponent* DragonMousePoint = nullptr;

    // 인트로 경유점 인덱스 및 인트로 진행 여부
    int32 IntroIndex = 0;
    bool  bDoingIntroMove = false;

    // 현재 목표 위치와 본 위치의 보간 캐시(부드러운 방향/거리 계산용)
    FVector CurrentTarget = FVector::ZeroVector;
    //FVector PreviousLocation = FVector::ZeroVector; // 사용 중지: 고착 감지 잔재
    FVector LastBoneLocation = FVector::ZeroVector;

    // 추적 모드 여부, 공격 사거리 모드 여부, 도착 여부, 사망 여부
    bool bTrackLivePlayer = false;
    bool bFireballMode = false;
    bool bArrived = false;
    //bool bForceRandomFlyNext = false; // 사용 중지: 이전 구조의 잔재
    bool bIsDead = false;

    //float StuckTimer = 0.0f; // 사용 중지: 고착 감지 잔재
    FTimerHandle HeadingTimeoutHandle; // 추적 모드 타임아웃 관리

    // [Added] 지면 강타 시퀀스 상태 관리
    bool  bGroundSlamActive = false;   // 시퀀스 진행 중 여부
    int32 GroundSlamPhase = 0;        // 0=비활성, 1=웨이포인트로 이동, 2=플레이어 지점 돌진
    float SavedNormalSpeed = 0.0f;     // 원래 속도 복원용
    FVector GroundSlamPlayerLock = FVector::ZeroVector; // 2단계 목표(플레이어 좌표 스냅샷)

    // [Added] 파이어 브레스 시퀀스 상태 관리
    bool  bFireBreathActive = false;   // 시퀀스 진행 중 여부
    int32 FireBreathPhase = 0;         // 0=비활성, 1=가장 먼 WP로 이동, 2=플레이어 추적(도착 판정 없음)
    float FireBreathSavedSpeed = 0.0f; // 속도 복원용
    FTimerHandle FireBreathTimerHandle;

    // [Added] 파이어 브레스 중 파이어볼 발사 타이머
    FTimerHandle FireBreathFireballTimerHandle;

    // [Added] 플레이어 위치 스냅샷 (실시간이 아닌 타이머 기반)
    FVector TrackedPlayerLocation = FVector::ZeroVector;

    // [Added] 플레이어 위치를 주기적으로 샘플링하는 타이머 핸들
    FTimerHandle TrackSampleTimerHandle;

    // ===== 내부 동작 함수(외부 노출 없음) =====
    // 목표까지의 프레임 단위 이동과 회전을 수행하고, 도착 시 후속 처리
    void MoveToTarget(float DeltaTime);

    // 현재 위치 기준으로 랜덤 비행 타깃을 선정
    void GoToRandomFly(const FVector& CurrentLocation);

    // 추적 모드일 때 플레이어 위치를 읽어 CurrentTarget 갱신
    void UpdateTargetForTracking();

    // CurrentTarget 도착 시 공통적으로 호출되는 후속 처리(이벤트 브로드캐스트)
    void OnArrived();

    // 소유 액터 하위에서 DragonMousePointName과 동일한 이름의 컴포넌트를 탐색
    void FindDragonMousePoint();

    // [Added] GroundSlam/FireBreath 공용: 현재 기준 위치에서 가장 먼 웨이포인트 위치를 반환
    FVector FindFarthestWaypointFrom(const FVector& From) const;

    // [Added] 타이머 콜백: 플레이어 현재 위치를 TrackedPlayerLocation에 저장
    void SampleTrackedPlayerLocation();

    // [Added] FireBreath 중 파이어볼을 스폰하는 내부 함수
    void SpawnFireBreathProjectile();
};
