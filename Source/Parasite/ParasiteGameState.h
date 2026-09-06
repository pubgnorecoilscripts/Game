#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "ParasiteTypes.h"
#include "ParasiteGameState.generated.h"

/** Replicated match-wide state: phase, timer, infection, result. */
UCLASS()
class PARASITE_API AParasiteGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Builds the mall locally on every machine; scenery is never replicated. */
	virtual void BeginPlay() override;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	EMatchPhase Phase = EMatchPhase::Lobby;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	float PhaseTimeRemaining = 0.f;

	/** Infection of team A's nest, i.e. progress made BY team B. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	float NestAInfection = 0.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	float NestBInfection = 0.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	EParasiteTeam WinningTeam = EParasiteTeam::None;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	FString ResultReason;

	/** Progress the given team has made against the enemy nest. */
	UFUNCTION(BlueprintPure, Category = "Parasite")
	float GetInfectionByTeam(EParasiteTeam Team) const;
};
