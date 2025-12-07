// DragonCombatComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DragonCombatComponent.generated.h"

class UProjectileMovementComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RoadOfDangun_API UDragonCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDragonCombatComponent();

protected:
	virtual void BeginPlay() override;

public:
	// [Renamed Param] FireballSpawnPoint -> DragonMousePoint
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void FireBreathOnceAtPlayer(AActor* Player, USceneComponent* DragonMousePoint);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void StartGroundSlamAt(const FVector& Center); // [Skeleton only]

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void StartBarrelRoll(); // [Skeleton only]

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void StartEvadeAfterHit(); // [Skeleton only]

	void OnOwnerDied() { /* 나중에 타이머/이펙트 정리 */ } // [Added]

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	TSubclassOf<AActor> ProjectileClass; // [Added]

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float FireballSpeed = 1800.0f; // [Added] 원본 constexpr → 변수화

private:
	AActor* OwnerActor = nullptr;
};
