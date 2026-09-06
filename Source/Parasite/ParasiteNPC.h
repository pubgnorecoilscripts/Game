#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ParasiteTypes.h"
#include "ParasiteRevealable.h"
#include "ParasiteNPC.generated.h"

class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class UPossessableComponent;
class UMaterialInstanceDynamic;

/**
 * A mall shopper. Deliberately dumb: it walks a short waypoint loop and stops to
 * stare at nothing. When a parasite takes over it looks exactly the same, which
 * is the entire point.
 */
UCLASS()
class PARASITE_API AParasiteNPC : public ACharacter, public IParasiteRevealable
{
	GENERATED_BODY()

public:
	AParasiteNPC();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnRevealChanged(bool bRevealed) override;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<UPossessableComponent> Possessable;

	/** Patrol route, set by the mall builder on the server. */
	UPROPERTY(Replicated)
	TArray<FVector> Waypoints;

	UPROPERTY(ReplicatedUsing = OnRep_Colour)
	FLinearColor ShirtColour = FLinearColor::White;

	UFUNCTION()
	void OnRep_Colour();

	/** Driven by the player controller while possessed. */
	void DriveForward(float Value);
	void DriveRight(float Value);

private:
	void TickIdleBrain(float DeltaSeconds);

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;

	int32 WaypointIndex = 0;
	float IdleTimer = 0.f;
	float StuckTimer = 0.f;
};
