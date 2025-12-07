// Damageable.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DamageSystemTypes.h"  // ★ FDamageRequest 사용
#include "Damageable.generated.h"

// 블루프린트에서 구현 가능하도록 인터페이스 노출
UINTERFACE(BlueprintType)
class UDamageable : public UInterface
{
	GENERATED_BODY()
};

/**
 * 데미지 관련 공통 인터페이스
 */
class ROADOFDANGUN_API IDamageable
{
	GENERATED_BODY()

public:
	/** 현재 체력 가져오기 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "DamageSystem|Interface")
	float GetCurrentHealth();

	/** 최대 체력 가져오기 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "DamageSystem|Interface")
	float GetMaxHealth();

	/** 데미지 요청(구조체) 기반 피격 처리 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "DamageSystem|Interface")
	void TakeDamage(const FDamageRequest& Request);

	/** 회복 처리 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "DamageSystem|Interface")
	void Heal(float amount);

	/** 풀 회복 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "DamageSystem|Interface")
	void FullHeal();
};
