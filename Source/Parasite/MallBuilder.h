#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ParasiteTypes.h"
#include "MallBuilder.generated.h"

class UStaticMeshComponent;

/** Plain coloured block. The whole mall is made of these. */
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

/** Sun + sky. Created at runtime, so both light components are movable. */
UCLASS()
class PARASITE_API AMallLighting : public AActor
{
	GENERATED_BODY()

public:
	AMallLighting();

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<class UDirectionalLightComponent> Sun;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<class USkyLightComponent> Sky;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	TObjectPtr<class UDirectionalLightComponent> Fill;
};

/** A lift platform. Server driven so every client sees the same ride. */
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
 * Builds the abandoned shopping mall.
 *
 * Static scenery is built identically on every machine (no replication cost);
 * gameplay actors - possessable props, NPCs, vehicles, nests - are spawned by
 * the server only and replicate down.
 */
UCLASS()
class PARASITE_API UMallBuilder : public UObject
{
	GENERATED_BODY()

public:
	/** Runs on server and clients. Idempotent per world. */
	static void BuildStaticGeometry(UWorld* World);

	/** Server only. Spawns everything that has gameplay meaning. */
	static void SpawnGameplayActors(UWorld* World);

	/** Spawn point for a player, spread around that team's home area. */
	static FVector GetTeamSpawn(EParasiteTeam Team, int32 PlayerIndex);

	/** One of several hiding spots, chosen per match by the game mode's seed. */
	static FVector GetNestLocation(EParasiteTeam Team, int32 Seed);

	/** True if the location sits inside the enemy's restricted back-of-house. */
	static bool IsRestrictedArea(const FVector& Location, EParasiteTeam ForTeam);
};
