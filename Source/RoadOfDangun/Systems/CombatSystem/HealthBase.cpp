#include "HealthBase.h"
#include "DamageCalculatingSystem.h"

UHealthBase::UHealthBase()
{
	PrimaryComponentTick.bCanEverTick = true;
	// 디폴트 테스트 값(원하면 변경)
	DefaultDamageRequest.Damage = 10.f;
}

void UHealthBase::BeginPlay()
{
	Super::BeginPlay();
	currentHealth = maxHealth;
}

void UHealthBase::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

float UHealthBase::GetCurrentHealth_Implementation()
{
	return currentHealth;
}

float UHealthBase::GetMaxHealth_Implementation()
{
	return maxHealth;
}

void UHealthBase::TakeDamage_Implementation(const FDamageRequest& Request)
{
	// 1) 피격자 상태 준비
	FDamageReceiverState Receiver;
	Receiver.bResistStagger = resistStagger;
	Receiver.bResistKnockback = resistKnockback;

	// 2) 계산
	const FDamageResponse Response = UDamageCalculatingSystem::CalculateDamage(Request, Receiver);

	// 3) 적용(이전 생존 상태 저장)
	const bool bWasDead = isDead;

	if (Response.FinalDamage > 0.f)
	{
		currentHealth = FMath::Clamp(currentHealth - Response.FinalDamage, 0.f, maxHealth);
	}

	// 사망 여부 갱신
	if (currentHealth <= 0.f)
	{
		isDead = true;
	}

	// 4) 이벤트 브로드캐스트(간단 규칙)
	// - Damaged: 실피해가 0보다 클 때만(스팸 방지)
	// - Death: 살아있던 상태에서 이번 타격으로 처음 사망했을 때만
	if (Response.FinalDamage > 0.f)
	{
		OnDamaged.Broadcast(Response);

		/*
		GEngine->AddOnScreenDebugMessage(
			-1, 1.5f, FColor::Yellow,
			FString::Printf(
				TEXT("bWasDead: %s | isDead: %s"),
				bWasDead ? TEXT("true") : TEXT("false"),
				isDead ? TEXT("true") : TEXT("false")
			)
		);*/

	}

	if (!bWasDead && isDead)
	{
		OnDeath.Broadcast(Response);
	}

	// 5) 뷰포트 디버그 출력(옵션)
	/*
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1, 1.5f, FColor::Yellow,
			FString::Printf(TEXT("DMG: %.1f | HP: %.1f / %.1f | Stagger:%s Knockback:%s (KB:%.1f)"),
				Response.FinalDamage,
				currentHealth, maxHealth,
				Response.bAppliedStagger ? TEXT("Y") : TEXT("N"),
				Response.bAppliedKnockback ? TEXT("Y") : TEXT("N"),
				Response.FinalKnockbackPower)
		);
	}
	*/
}

void UHealthBase::Heal_Implementation(float amount)
{
	if (amount <= 0.f) return;
	currentHealth = FMath::Clamp(currentHealth + amount, 0.f, maxHealth);
	if (currentHealth > 0.f) { isDead = false; }
}

void UHealthBase::FullHeal_Implementation()
{
	currentHealth = maxHealth;
	isDead = false;
}

void UHealthBase::ApplyDamageFromInspector()
{
	FDamageRequest Req = DefaultDamageRequest;
	if (!Req.DamageCauser.IsValid())
	{
		Req.DamageCauser = GetOwner(); // 편의: 비어있으면 자신으로
	}
	TakeDamage(Req); // 인터페이스 호출 → _Implementation 실행
}
