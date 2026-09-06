#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ParasiteTypes.h"
#include "ParasiteRevealable.h"
#include "PossessablePawn.generated.h"

class UStaticMeshComponent;
class UFloatingPawnMovement;
class USpringArmComponent;
class UCameraComponent;
class UPossessableComponent;
class UMaterialInstanceDynamic;

/**
 * Every possessable mall object is one of these. Adding a new kind of host is a
 * single Configure() call - the mobility profile decides how it drives.
 */
UCLASS()
class PARASITE_API APossessablePawn : public APawn, public IParasiteRevealable
{
	GENERATED_BODY()

public:
	APossessablePawn();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnRevealChanged(bool bRevealed) override;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Optional second block: a chair back, a cart basket, a car roof. */
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

	/** Sets up size, colour and host behaviour. Server side; the look replicates. */
	void Configure(const FString& DisplayName, EHostType InHostType, EHostMobility InMobility,
		const FVector& BoxExtent, const FLinearColor& Colour,
		const FVector& DetailOffset = FVector::ZeroVector, const FVector& DetailExtent = FVector::ZeroVector);

	/** Driven by the player controller; behaviour depends on the mobility profile. */
	void DriveForward(float Value);
	void DriveRight(float Value);

	/** Hinge props (doors) swing instead of driving. */
	void ToggleHinge();

	// --- Replicated description, so clients build the same prop ----------
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

private:
	void ApplyVisuals();

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> MeshMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> DetailMaterial;

	bool bHingeOpen = false;
	float HingeAlpha = 0.f;
	float HingeClosedYaw = 0.f;
	float WobbleTime = 0.f;
};
