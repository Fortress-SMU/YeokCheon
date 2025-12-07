#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DamageSystemTypes.h"
#include "TestEnemyCPP.generated.h"

UCLASS()
class ROADOFDANGUN_API ATestEnemyCPP : public ACharacter
{
	GENERATED_BODY()

public:
	ATestEnemyCPP();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** 트레이스로 줄 데미지 파라미터 (BP에서 세팅) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SphereTrace|Damage")
	FDamageRequest TraceDamageRequest;

	// === 트레이스 파라미터 ===
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SphereTrace")
	bool bAutoStartTrace = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SphereTrace", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float TraceInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SphereTrace", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float TraceRadius = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SphereTrace")
	FVector TraceOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SphereTrace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SphereTrace")
	bool bIgnoreSelf = true;

	// === 런타임 디버그 표시 ===
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SphereTrace|Debug")
	bool bDrawDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SphereTrace|Debug", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DebugDrawTime = 0.25f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SphereTrace")
	TArray<FHitResult> LastTraceHits;

	// === 에디터 프리뷰(플레이 전에도 보이게) ===
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SphereTrace|EditorPreview")
	bool bEditorPreview = true;   // 뷰포트에 프리뷰 구를 영구 표시

	// BP에서 시작/중지
	UFUNCTION(BlueprintCallable, Category = "SphereTrace")
	void StartSphereTrace();

	UFUNCTION(BlueprintCallable, Category = "SphereTrace")
	void StopSphereTrace();

	UFUNCTION(BlueprintCallable, Category = "SphereTrace")
	bool IsTracing() const;

	// 값 바뀔 때마다 프리뷰 다시 그림
	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	UFUNCTION()
	void DoSphereTrace();

	void RefreshEditorPreview();
	void ClearEditorPreview();

	FTimerHandle TraceTimerHandle;
};
