#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ParasiteTypes.h"
#include "ParasiteNest.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

/**
 * An organic growth tucked into a corner of the mall. Purely a view: infection
 * and health are computed by the match simulation and pushed in by the game mode.
 */
UCLASS()
class PARASITE_API AParasiteNest : public AActor
{
	GENERATED_BODY()

public:
	AParasiteNest();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	EParasiteTeam OwningTeam = EParasiteTeam::TeamA;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	float Infection = 0.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	float Health = 100.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	bool bUnderAttack = false;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<USphereComponent> InfectionZone;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<UStaticMeshComponent> Core;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<UStaticMeshComponent> Growth1;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<UStaticMeshComponent> Growth2;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<UPointLightComponent> CoreLight;

private:
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> CoreMaterial;

	float InfectSoundTimer = 0.f;
};
