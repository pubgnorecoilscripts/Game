#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ParasiteTypes.h"
#include "ParasiteCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UPossessableComponent;
class AParasitePlayerState;

/**
 * The little alien the player starts as. Also a valid possession host for the
 * enemy team: a hijacked parasite keeps its own controller (so the victim never
 * loses their connection/state) but ignores its owner's input for 8 seconds.
 */
UCLASS()
class PARASITE_API AParasiteCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AParasiteCharacter();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<UCameraComponent> Camera;

	/** Simple stylised body: a squashed sphere plus an eye. */
	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<UStaticMeshComponent> Eye;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<UPointLightComponent> Glow;

	/** Lets enemies hijack this parasite. */
	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<UPossessableComponent> Possessable;

	/** True while an enemy parasite is driving this pawn. */
	UPROPERTY(ReplicatedUsing = OnRep_Hijacked, BlueprintReadOnly, Category = "Parasite")
	bool bHijacked = false;

	/** 0..1 resist meter the victim fills by mashing Resist. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	float ResistProgress = 0.f;

	/** Hidden + non-colliding while its owner is riding another host. */
	UPROPERTY(ReplicatedUsing = OnRep_Dormant, BlueprintReadOnly, Category = "Parasite")
	bool bDormant = false;

	UFUNCTION()
	void OnRep_Hijacked();

	UFUNCTION()
	void OnRep_Dormant();

	/** Server: put the parasite to sleep (owner entered another host) or wake it up. */
	void SetDormant(bool bNewDormant, const FVector& WakeLocation);

	/** Server: input forwarded from the hijacking parasite's controller. */
	void ApplyHijackInput(float Forward, float Right, float YawDelta);

	/** Server: victim mashed resist. Returns true when the parasite is forced out. */
	bool AddResist(float Amount);

	void SetSprinting(bool bSprint);

	AParasitePlayerState* GetParasitePlayerState() const;

	/** Team colour applied to the body material. */
	void ApplyTeamColour(EParasiteTeam Team);

private:
	UPROPERTY()
	TObjectPtr<class UMaterialInstanceDynamic> BodyMaterial;

	float StepSoundTimer = 0.f;
};
