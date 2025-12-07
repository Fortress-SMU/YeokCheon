// BossBlueDragon.h
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "BossBlueDragon.generated.h"

class USkeletalMeshComponent;
class UDragonFlightComponent;
class UBehaviorTree;

UENUM(BlueprintType)
enum class EDragonState : uint8
{
	Intro, Combat, Evade, Dead
};

UCLASS(Blueprintable)
class RoadOfDangun_API ABossBlueDragon : public APawn
{
	GENERATED_BODY()
public:
	ABossBlueDragon();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Mesh")
	USkeletalMeshComponent* DragonMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Components")
	UDragonFlightComponent* Flight;

	// UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boss|Components")
	// class UDragonCombatComponent* Combat; // [Removed] Combat 컴포넌트는 사용하지 않음

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|AI")
	UBehaviorTree* BossBehaviorTree;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|State")
	EDragonState InitialState = EDragonState::Intro;

	UFUNCTION(BlueprintCallable, Category = "Boss|State")
	void HandleDeath();

	UFUNCTION(BlueprintCallable, Category = "Boss|State")
	bool IsDead() const { return bIsDead; }

	UFUNCTION(BlueprintCallable, Category = "Boss|AI")
	UBehaviorTree* GetBehaviorTree() const { return BossBehaviorTree; }

private:
	bool bIsDead = false;
};
