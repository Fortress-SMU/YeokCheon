// DamageCalculatingSystem.cpp

#include "DamageCalculatingSystem.h"

/// <summary>
/// 데미지 관련 연산 후, DamageResponse에 저장
/// </summary>
FDamageResponse UDamageCalculatingSystem::CalculateDamage(const FDamageRequest& Req, const FDamageReceiverState& ReceiverState)
{
	FDamageResponse Response;

	// 데미지 공격자 받기
	Response.DamageCauser = Req.DamageCauser;
	
	// 데미지 계산 (차후 방어력 등 적용)
	Response.FinalDamage = Req.Damage;

	// 경직
	const bool bStaggerBlocked = (ReceiverState.bResistStagger && !Req.bBreakStaggerResistance);
	Response.bAppliedStagger = (Req.bHasStagger && !bStaggerBlocked);

	// 넉백
	const bool bKnockbackBlocked = (ReceiverState.bResistKnockback && !Req.bBreakKnockbackResistance);
	Response.bAppliedKnockback = (Req.bHasKnockback && !bKnockbackBlocked);

	// 최종 넉백 세기: 적용될 때만 요청값을 반영, 아니면 0
	Response.FinalKnockbackPower = Response.bAppliedKnockback ? Req.KnockbackPower : 0.f;

	return Response;
}

