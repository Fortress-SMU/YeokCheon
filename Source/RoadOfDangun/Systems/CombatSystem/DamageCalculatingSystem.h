// DamageCalculatingSystem.h
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "DamageSystemTypes.h"
#include "DamageCalculatingSystem.generated.h"

/**
 * 공격 요청(Req) + 피격자 상태(ReceiverState)를 입력받아
 * 최종 데미지 응답(Response)을 계산하는 정적 함수 모음
 */
UCLASS()
class ROADOFDANGUN_API UDamageCalculatingSystem : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	/** 요청 + 피격자 상태 -> 최종 응답 계산 */
	UFUNCTION(BlueprintPure, Category = "DamageSystem|Calculate")
	static FDamageResponse CalculateDamage(const FDamageRequest& Req, const FDamageReceiverState& ReceiverState);
};
