#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ParasiteTypes.h"
#include "ParasiteNest.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UPointLightComponent;
class AParasitePlayerState;

/**
 * An organic growth tucked into a corner of the mall. Enemy parasites standing
 * inside it for 20 uninterrupted seconds infect it. All progress is server owned.
 */
UCLASS()
class PARASITE_API AParasiteNest : public AActor
{
	GENERATED_BODY()

public:
	AParasiteNest();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** The team this nest belongs to (and therefore defends). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	EParasiteTeam OwningTeam = EParasiteTeam::TeamA;

	/** 0-100. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	float Infection = 0.f;

	/** Cosmetic durability, drained by the defensive pulse fight. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	float Health = 100.f;

	/** True while at least one enemy is inside. Used for defender feedback. */
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

	/** Server: a defender expelled the attackers - knock progress back. */
	void ApplyExpulsion();

private:
	/** Returns every enemy player currently inside, via their pawn or their host. */
	void GatherAttackers(TArray<AParasitePlayerState*>& OutAttackers) const;

	UPROPERTY()
	TObjectPtr<class UMaterialInstanceDynamic> CoreMaterial;

	float PulseTimer = 0.f;
	float DNATimer = 0.f;
	float SoundTimer = 0.f;
};
