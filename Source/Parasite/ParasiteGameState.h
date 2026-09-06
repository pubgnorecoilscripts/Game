#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "ParasiteTypes.h"
#include "ParasiteGameState.generated.h"

class AParasiteNest;

/** Replicated match-wide state: phase, timer, infection, nests. */
UCLASS()
class PARASITE_API AParasiteGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AParasiteGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** Builds the mall locally on every machine - the scenery is never replicated. */
	virtual void BeginPlay() override;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	EMatchPhase Phase = EMatchPhase::Lobby;

	/** Seconds left in the current phase. Server ticks it down. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	float PhaseTimeRemaining = 0.f;

	/** Infection of Team A's nest, i.e. progress made BY Team B. 0-100. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	float NestAInfection = 0.f;

	/** Infection of Team B's nest, i.e. progress made BY Team A. 0-100. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	float NestBInfection = 0.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	EParasiteTeam WinningTeam = EParasiteTeam::None;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	FString ResultReason;

	/** Progress a team has made against the enemy nest. */
	UFUNCTION(BlueprintPure, Category = "Parasite")
	float GetInfectionByTeam(EParasiteTeam Team) const;

	UFUNCTION(BlueprintPure, Category = "Parasite")
	int32 GetTeamDNA(EParasiteTeam Team) const;

	UFUNCTION(BlueprintPure, Category = "Parasite")
	int32 GetTeamPlayerCount(EParasiteTeam Team) const;
};
