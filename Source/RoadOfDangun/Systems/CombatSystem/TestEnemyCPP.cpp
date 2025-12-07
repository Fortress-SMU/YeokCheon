#include "TestEnemyCPP.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Damageable.h"     // UDamageable / IDamageable
#include "DamageSystemTypes.h"  // FDamageRequest 선언된 곳(실제 파일명)

ATestEnemyCPP::ATestEnemyCPP()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ATestEnemyCPP::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoStartTrace)
	{
		StartSphereTrace();
	}

	// 플레이 시작 시 에디터 프리뷰는 지워줌(원한다면 남겨도 됨)
	ClearEditorPreview();
}

void ATestEnemyCPP::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ATestEnemyCPP::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void ATestEnemyCPP::StartSphereTrace()
{
	if (TraceInterval < 0.01f)
	{
		TraceInterval = 0.01f;
	}

	if (GetWorld() != nullptr)
	{
		GetWorldTimerManager().SetTimer(
			TraceTimerHandle,
			this,
			&ATestEnemyCPP::DoSphereTrace,
			TraceInterval,
			true
		);
	}
}

void ATestEnemyCPP::StopSphereTrace()
{
	if (GetWorld() != nullptr)
	{
		GetWorldTimerManager().ClearTimer(TraceTimerHandle);
	}
}

bool ATestEnemyCPP::IsTracing() const
{
	if (GetWorld() == nullptr)
	{
		return false;
	}
	return GetWorldTimerManager().IsTimerActive(TraceTimerHandle);
}

void ATestEnemyCPP::DoSphereTrace()
{
	UWorld* World = GetWorld();
	if (World == nullptr) { return; }

	const FVector Center = GetActorLocation() + TraceOffset;
	const FVector Start = Center, End = Center;

	FCollisionShape SphereShape = FCollisionShape::MakeSphere(TraceRadius);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemySphereTrace), false, this);
	if (bIgnoreSelf) { QueryParams.AddIgnoredActor(this); }

	TArray<FHitResult> Hits;
	bool bHit = World->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, TraceChannel, SphereShape, QueryParams);
	LastTraceHits = Hits;

	FDamageRequest Request = TraceDamageRequest;
	if (!Request.DamageCauser.IsValid()) { Request.DamageCauser = this; }

	for (const FHitResult& HR : Hits)
	{
		AActor* HitActor = HR.GetActor();
		if (HitActor == nullptr) { continue; }

		// 액터가 가진 "모든 컴포넌트" 중 Damageable 인터페이스 구현 컴포넌트를 찾아서 실행
		TArray<UActorComponent*> DamageableComps;
		DamageableComps = HitActor->GetComponentsByInterface(UDamageable::StaticClass());

		for (UActorComponent* Comp : DamageableComps)
		{
			if (Comp == nullptr) { continue; }

			// 디버그 출력(선택)
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(
					-1, 2.0f, FColor::Green,
					FString::Printf(TEXT("HitActor: %s, DamageableComp: %s"),
						*HitActor->GetName(), *Comp->GetName())
				);
			}

			IDamageable::Execute_TakeDamage(Comp, Request);
			return; //중복 실행 방지
		}
	}

	// === 런타임 디버그 그리기 ===
	if (bDrawDebug)
	{
		FColor SphereColor = bHit ? FColor::Red : FColor::Green;
		DrawDebugSphere(World, Center, TraceRadius, 20, SphereColor, false, DebugDrawTime, 0, 1.5f);

		for (const FHitResult& HR : Hits)
		{
			DrawDebugPoint(World, HR.ImpactPoint, 10.0f, FColor::Yellow, false, DebugDrawTime);
			DrawDebugLine(World, Center, HR.ImpactPoint, FColor::Yellow, false, DebugDrawTime, 0, 1.5f);

			const FVector Tip = HR.ImpactPoint + HR.ImpactNormal * 30.0f;
			DrawDebugDirectionalArrow(World, HR.ImpactPoint, Tip, 20.0f, FColor::Cyan, false, DebugDrawTime, 0, 1.5f);
		}
	}
}


// ===== 에디터 프리뷰(플레이 전에도 보이게) =====
void ATestEnemyCPP::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshEditorPreview();
}

#if WITH_EDITOR
void ATestEnemyCPP::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RefreshEditorPreview();
}
#endif

void ATestEnemyCPP::RefreshEditorPreview()
{
#if WITH_EDITOR
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// 에디터 뷰에서만: 영구 디버그 라인으로 프리뷰
	if (bEditorPreview)
	{
		// 이전 프리뷰 지우고 다시 그림
		FlushPersistentDebugLines(World);

		const FVector Center = GetActorLocation() + TraceOffset;
		DrawDebugSphere(World, Center, TraceRadius, 24, FColor::Cyan, true /*persistent*/, 0.0f, 0, 1.5f);
	}
	else
	{
		ClearEditorPreview();
	}
#endif
}

void ATestEnemyCPP::ClearEditorPreview()
{
#if WITH_EDITOR
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	FlushPersistentDebugLines(World);
#endif
}
