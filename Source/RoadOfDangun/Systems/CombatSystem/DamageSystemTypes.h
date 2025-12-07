// DamageSystemTypes.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "DamageSystemTypes.generated.h"

// 전방 선언 (의존성 최소화)
class AActor;

/** 공격자가 보낸 데미지 요청 정보 */
USTRUCT(BlueprintType)
struct ROADOFDANGUN_API FDamageRequest
{
	GENERATED_BODY()

	/** 기본 피해량 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageSystem|Request")
	float Damage = 0.f;

	/** 넉백 세기 (bHasKnockback이 true일 때만 사용됨) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageSystem|Request", meta = (EditCondition = "bHasKnockback", ClampMin = "0.0"))
	float KnockbackPower = 500.0f;

	/** 경직 효과 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageSystem|Request")
	bool bHasStagger = false;

	/** 넉백 효과 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageSystem|Request")
	bool bHasKnockback = false;

	/** 경직 저항 무시(방어파괴) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageSystem|Request")
	bool bBreakStaggerResistance = false;

	/** 넉백 저항 무시(방어파괴) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageSystem|Request")
	bool bBreakKnockbackResistance = false;

	/** (선택) 데미지 유발자, TWeakObjectPtr는 약한 참조를 의미*/ 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageSystem|Request")
	TWeakObjectPtr<AActor> DamageCauser = nullptr;
};

/** 피격자의 현재 상태 (저항/방어 등) */
USTRUCT(BlueprintType)
struct ROADOFDANGUN_API FDamageReceiverState
{
	GENERATED_BODY()

	/** 경직 저항 상태 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageSystem|ReceiverState")
	bool bResistStagger = false;

	/** 넉백 저항 상태 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageSystem|ReceiverState")
	bool bResistKnockback = false;
};

/** 최종 데미지 처리 결과 */
USTRUCT(BlueprintType)
struct ROADOFDANGUN_API FDamageResponse
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageSystem|Request")
	TWeakObjectPtr<AActor> DamageCauser = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageSystem|Response")
	float FinalDamage = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageSystem|Response")
	bool bAppliedStagger = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageSystem|Response")
	bool bAppliedKnockback = false;

	/** 최종 적용된 넉백 세기 (저항/파괴 로직 반영) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageSystem|Response")
	float FinalKnockbackPower = 0.f; // ← NEW
};

