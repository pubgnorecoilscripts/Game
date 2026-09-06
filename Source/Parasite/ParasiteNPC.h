#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ParasiteTypes.h"
#include "ParasiteNPC.generated.h"

class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class UPossessableComponent;

/**
 * A mall shopper. Deliberately dumb: it walks a short list of waypoints and idles.
 * When a parasite takes over, the controller possesses it directly and the same
 * mesh keeps walking - which is the whole point, nobody can tell.
 */
UCLASS()
class PARASITE_API AParasiteNPC : public ACharacter
{
	GENERATED_BODY()

public:
	AParasiteNPC();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

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

	void SetRevealed(float Seconds);

	/** Driven by the player controller while possessed. */
	void DriveForward(float Value);
	void DriveRight(float Value);

private:
	void TickIdleBrain(float DeltaSeconds);

	UPROPERTY()
	TObjectPtr<class UMaterialInstanceDynamic> BodyMaterial;

	int32 WaypointIndex = 0;
	float IdleTimer = 0.f;
	float StuckTimer = 0.f;
	float RevealEndTime = 0.f;
	bool bRevealApplied = false;
};
