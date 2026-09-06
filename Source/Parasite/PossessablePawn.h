#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ParasiteTypes.h"
#include "PossessablePawn.generated.h"

class UStaticMeshComponent;
class UFloatingPawnMovement;
class USpringArmComponent;
class UCameraComponent;
class UPossessableComponent;

/**
 * Every possessable mall object is one of these. The mobility profile decides how
 * it behaves when driven, so a new possessable only needs a call to Configure().
 */
UCLASS()
class PARASITE_API APossessablePawn : public APawn
{
	GENERATED_BODY()

public:
	APossessablePawn();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Optional second block, e.g. a chair back or a cart basket. */
	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<UStaticMeshComponent> Detail;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<UFloatingPawnMovement> Movement;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<UPossessableComponent> Possessable;

	/**
	 * Sets up mesh, size, colour and host behaviour. Called by the mall builder on
	 * the server; the values that matter visually are replicated.
	 */
	void Configure(const FString& DisplayName, EHostType InHostType, EHostMobility InMobility,
		const FVector& BoxExtent, const FLinearColor& Colour, const FVector& DetailOffset = FVector::ZeroVector,
		const FVector& DetailExtent = FVector::ZeroVector);

	/** Replicated visual description so late joiners and clients build the same prop. */
	UPROPERTY(ReplicatedUsing = OnRep_Visual)
	FVector VisualExtent = FVector(50.f, 50.f, 50.f);

	UPROPERTY(ReplicatedUsing = OnRep_Visual)
	FVector VisualDetailOffset = FVector::ZeroVector;

	UPROPERTY(ReplicatedUsing = OnRep_Visual)
	FVector VisualDetailExtent = FVector::ZeroVector;

	UPROPERTY(ReplicatedUsing = OnRep_Visual)
	FLinearColor VisualColour = FLinearColor::Gray;

	UFUNCTION()
	void OnRep_Visual();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Highlight this host for a short time (scan reveal). Client-side visual. */
	void SetRevealed(float Seconds);

	/** Hinge props (doors) toggle instead of driving. */
	void ToggleHinge();

	/** Driven by the player controller; behaviour depends on the mobility profile. */
	void DriveForward(float Value);
	void DriveRight(float Value);

private:
	void ApplyVisuals();

	UPROPERTY()
	TObjectPtr<class UMaterialInstanceDynamic> MeshMaterial;

	UPROPERTY()
	TObjectPtr<class UMaterialInstanceDynamic> DetailMaterial;

	float RevealEndTime = 0.f;
	bool bRevealApplied = false;

	/** Hinge state. */
	bool bHingeOpen = false;
	float HingeAlpha = 0.f;
	float HingeClosedYaw = 0.f;

	/** Idle wobble accumulator for static hosts. */
	float WobbleTime = 0.f;
};
