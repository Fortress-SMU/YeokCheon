// DragonCombatComponent.cpp
#include "DragonCombatComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

UDragonCombatComponent::UDragonCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDragonCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerActor = GetOwner();
}

void UDragonCombatComponent::FireBreathOnceAtPlayer(AActor* Player, USceneComponent* DragonMousePoint) // [Renamed]
{
	if (!ProjectileClass || !Player || !DragonMousePoint || !GetWorld()) return;

	const FVector Start = DragonMousePoint->GetComponentLocation(); // [Renamed]
	FVector Target = Player->GetActorLocation();
	Target.Z += 40.0f; // 원본 보정

	const FVector Direction = (Target - Start).GetSafeNormal();
	const FRotator Rotation = Direction.Rotation();
	const FTransform SpawnTM = FTransform(Rotation, Start);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* Projectile = GetWorld()->SpawnActor<AActor>(ProjectileClass, SpawnTM, Params);
	if (!Projectile) return;

	UProjectileMovementComponent* MoveComp = Projectile->FindComponentByClass<UProjectileMovementComponent>();
	if (!MoveComp)
	{
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
		MoveComp->Velocity = Direction * FireballSpeed;
	}
}

void UDragonCombatComponent::StartGroundSlamAt(const FVector& Center)
{
	// [Skeleton only] 임시 디버그 표시
	DrawDebugCylinder(GetWorld(), Center + FVector(0, 0, -50.f), Center + FVector(0, 0, 50.f), 600.f, 32, FColor::Red, false, 3.0f, 0, 3.0f);
}

void UDragonCombatComponent::StartBarrelRoll()
{
	// [Skeleton only]
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Cyan, TEXT("Barrel Roll (TODO)"));
}

void UDragonCombatComponent::StartEvadeAfterHit()
{
	// [Skeleton only]
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Yellow, TEXT("Evade After Hit (TODO)"));
}
