// BossBlueDragon.cpp
#include "BossBlueDragon.h"
#include "Components/SkeletalMeshComponent.h"
#include "DragonFlightComponent.h"
// #include "DragonCombatComponent.h" // [Removed]
#include "AIController.h"

ABossBlueDragon::ABossBlueDragon()
{
	PrimaryActorTick.bCanEverTick = true;

	DragonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("DragonMesh"));
	SetRootComponent(DragonMesh);

	Flight = CreateDefaultSubobject<UDragonFlightComponent>(TEXT("FlightComponent"));

	// Combat = CreateDefaultSubobject<UDragonCombatComponent>(TEXT("CombatComponent")); // [Removed]

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned; // [Kept]
}

void ABossBlueDragon::BeginPlay()
{
	Super::BeginPlay();
}

void ABossBlueDragon::HandleDeath()
{
	bIsDead = true;

	if (DragonMesh)
	{
		DragonMesh->SetSimulatePhysics(true);
		DragonMesh->SetEnableGravity(true);
	}



}
