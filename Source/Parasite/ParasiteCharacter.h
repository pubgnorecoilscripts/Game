#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ParasiteTypes.h"
#include "ParasiteRevealable.h"
#include "ParasiteCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UPossessableComponent;
class UMaterialInstanceDynamic;

/**
 * The little alien the player starts as, and a host for the enemy team.
 *
 * A hijacked parasite keeps its own controller and connection: it simply stops
 * listening to its owner for eight seconds and takes movement from the attacker
 * instead. Nobody ever loses their pawn to another player.
 */
UCLASS()
class PARASITE_API AParasiteCharacter : public ACharacter, public IParasiteRevealable
{
	GENERATED_BODY()

public:
	AParasiteCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnRevealChanged(bool bRevealed) override;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<UStaticMeshComponent> Eye;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<UPointLightComponent> Glow;

	/** Lets the enemy team ride this parasite. */
	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<UPossessableComponent> Possessable;

	/** True while an enemy is driving this pawn. */
	UPROPERTY(ReplicatedUsing = OnRep_Hijacked, BlueprintReadOnly, Category = "Parasite")
	bool bHijacked = false;

	/** 0..1 resist meter, filled by the victim mashing R. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	float ResistProgress = 0.f;

	/** Hidden and non-colliding while its owner is riding another host. */
	UPROPERTY(ReplicatedUsing = OnRep_Dormant, BlueprintReadOnly, Category = "Parasite")
	bool bDormant = false;

	UFUNCTION()
	void OnRep_Hijacked();

	UFUNCTION()
	void OnRep_Dormant();

	/** Server: sleep the parasite (owner entered a host) or wake it at a location. */
	void SetDormant(bool bNewDormant, const FVector& WakeLocation);

	/** Server: mirrors the simulation's hijack state onto this pawn. */
	void SetHijacked(bool bNewHijacked, float NewResistProgress);

	/** Server: movement forwarded from the parasite riding this body. */
	void ApplyHijackInput(float Forward, float Right, float YawDelta);

	void SetSprinting(bool bSprint);
	void ApplyTeamColour(EParasiteTeam Team);

private:
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;

	EParasiteTeam AppliedTeam = EParasiteTeam::None;
	float StepSoundTimer = 0.f;
};
