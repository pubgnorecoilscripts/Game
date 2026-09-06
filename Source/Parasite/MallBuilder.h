#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ParasiteTypes.h"
#include "MallBuilder.generated.h"

class UStaticMeshComponent;
class UDirectionalLightComponent;
class USkyLightComponent;

/** A plain coloured block. The whole mall is made of these. */
UCLASS()
class PARASITE_API AMallBlock : public AActor
{
	GENERATED_BODY()

public:
	AMallBlock();

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<UStaticMeshComponent> Mesh;

	void Build(const FVector& Extent, const FLinearColor& Colour, bool bUseSphere = false);

private:
	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereAsset;
};

/** Sun, sky and a fill light. Spawned at runtime, so all of it is movable. */
UCLASS()
class PARASITE_API AMallLighting : public AActor
{
	GENERATED_BODY()

public:
	AMallLighting();

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<UDirectionalLightComponent> Sun;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<USkyLightComponent> Sky;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<UDirectionalLightComponent> Fill;
};

/** The lift. Server driven, so every client sees the same ride. */
UCLASS()
class PARASITE_API AMallElevator : public AActor
{
	GENERATED_BODY()

public:
	AMallElevator();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<UStaticMeshComponent> Platform;

	UPROPERTY(EditAnywhere, Category = "Parasite")
	float BottomZ = 20.f;

	UPROPERTY(EditAnywhere, Category = "Parasite")
	float TopZ = 700.f;

private:
	float Direction = 1.f;
	float WaitTimer = 0.f;
};

/**
 * Builds the abandoned shopping mall, roughly 200 m x 200 m over two floors.
 *
 * Scenery is built identically on every machine and never replicated; anything
 * with gameplay meaning is spawned by the server only.
 */
UCLASS()
class PARASITE_API UMallBuilder : public UObject
{
	GENERATED_BODY()

public:
	/** Runs on the server and on every client. Idempotent per world. */
	static void BuildStaticGeometry(UWorld* World);

	/** Server only: props, NPCs, vehicles, doors, the lift. */
	static void SpawnGameplayActors(UWorld* World);

	static FVector GetTeamSpawn(EParasiteTeam Team, int32 PlayerIndex);

	/** One of four hiding places per side, picked by the match seed. */
	static FVector GetNestLocation(EParasiteTeam Team, int32 Seed);

	/** The enemy back of house that ForTeam earns an infiltration bonus in. */
	static void GetRestrictedZone(EParasiteTeam ForTeam, FVector& OutMin, FVector& OutMax);
};
