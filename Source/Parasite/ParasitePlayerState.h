#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "ParasiteTypes.h"
#include "ParasitePlayerState.generated.h"

/**
 * A replicated read-only view of one player's simulation state. Nothing here is
 * ever written by a client, and nothing here is authoritative: the game mode
 * copies these values out of Parasite::FMatchSim every tick.
 */
UCLASS()
class PARASITE_API AParasitePlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* NewPlayerState) override;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	EParasiteTeam Team = EParasiteTeam::None;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	int32 DNA = 0;

	/** Total DNA earned this match; the timer tie break uses the team total. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	int32 LifetimeDNA = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	int32 InfectionTicks = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	TArray<EParasiteUpgrade> Upgrades;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	int32 NumUpgrades = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	float PossessCooldownRemaining = 0.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	float ScanCooldownRemaining = 0.f;

	/** The actor being ridden, if any. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	TObjectPtr<AActor> CurrentHost = nullptr;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	FString CurrentHostName;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	float HostTimeRemaining = 0.f;

	bool HasUpgrade(EParasiteUpgrade Upgrade) const { return Upgrades.Contains(Upgrade); }
};
