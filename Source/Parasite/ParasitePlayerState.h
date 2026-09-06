#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "ParasiteTypes.h"
#include "ParasitePlayerState.generated.h"

class UPossessableComponent;

/**
 * Per-player match state. Everything here is written by the server only.
 */
UCLASS()
class PARASITE_API AParasitePlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AParasitePlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* NewPlayerState) override;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	EParasiteTeam Team = EParasiteTeam::None;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	int32 DNA = 0;

	/** Total DNA earned this match (used for the tie-break; never spent down). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	int32 LifetimeDNA = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	TArray<EParasiteUpgrade> Upgrades;

	/** World time when possession becomes available again. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	float PossessAvailableTime = 0.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	float ScanAvailableTime = 0.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	float LeapAvailableTime = 0.f;

	/** The host this player is currently riding, if any. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	TObjectPtr<AActor> CurrentHost = nullptr;

	/** Number of nest infection ticks contributed (stat / scoreboard). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	int32 InfectionTicks = 0;

	EParasiteTeam GetTeam() const { return Team; }
	EParasiteTeam GetEnemyTeam() const;

	bool HasUpgrade(EParasiteUpgrade Upgrade) const { return Upgrades.Contains(Upgrade); }

	/** Effective possession range including the Jumper upgrade. */
	float GetPossessRange() const;

	/** Radius multiplier applied to enemy scans looking for this player (Infiltrator). */
	float GetStealthScale() const;

	bool IsPossessCooldownReady() const;
	bool IsScanReady() const;
	bool IsLeapReady() const;

	/** Server only helpers. */
	void AwardDNA(int32 Amount);
	bool TryPurchaseUpgrade(EParasiteUpgrade Upgrade);
	void StartPossessCooldown();
	void StartScanCooldown();
	void StartLeapCooldown();
	void ResetForNewMatch();
};
