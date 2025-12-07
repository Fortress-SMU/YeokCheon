// HealthBase.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DamageSystemTypes.h"   // FDamageRequest / FDamageReceiverState / FDamageResponse
#include "Damageable.h"
#include "HealthBase.generated.h"

// === 이벤트 델리게이트(블루프린트에서도 바인딩 가능) 선언===
// FDamageResponse 하나만 넘겨준다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDamaged, const FDamageResponse&, Response);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeath, const FDamageResponse&, Response);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ROADOFDANGUN_API UHealthBase : public UActorComponent, public IDamageable
{
	GENERATED_BODY()

public:
	UHealthBase();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/* === 상태 값들 === */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageSystem|Health")
	float currentHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageSystem|Health")
	float maxHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageSystem|Health")
	bool isDead = false;

	// 피격자 저항 상태(간단 버전)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageSystem|ReceiverState")
	bool resistStagger = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageSystem|ReceiverState")
	bool resistKnockback = false;

	// 인스펙터에서 바로 때려볼 기본 요청값(옵션)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageSystem|Request")
	FDamageRequest DefaultDamageRequest;

	/* === 인터페이스 구현 === */
	virtual float GetCurrentHealth_Implementation() override;
	virtual float GetMaxHealth_Implementation() override;
	virtual void TakeDamage_Implementation(const FDamageRequest& Request) override;
	virtual void Heal_Implementation(float amount) override;
	virtual void FullHeal_Implementation() override;

	// 인스펙터 기본 요청값으로 즉시 적용(디버그용)
	UFUNCTION(BlueprintCallable, Category = "DamageSystem|Debug")
	void ApplyDamageFromInspector();

	/* === 이벤트(수신자들이 바인딩) === */
	UPROPERTY(BlueprintAssignable, Category = "DamageSystem|Events")
	FOnDamaged OnDamaged;

	UPROPERTY(BlueprintAssignable, Category = "DamageSystem|Events")
	FOnDeath OnDeath;
};
