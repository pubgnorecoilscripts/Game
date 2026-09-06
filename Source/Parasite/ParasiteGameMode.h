#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ParasiteTypes.h"
#include "ParasiteAudio.h"
#include "ParasiteGameMode.generated.h"

class AParasiteNest;
class AParasitePlayerController;
class APlayerStart;

/**
 * Runs the whole match: teams, phases, nests, scoring and the win condition.
 * Nothing in here ever trusts a client.
 */
UCLASS()
class PARASITE_API AParasiteGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AParasiteGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	/** Starts a fresh match with the same connected players. */
	UFUNCTION(Exec)
	void RestartMatch();

private:
	void EnterPhase(EMatchPhase NewPhase);
	void AssignTeam(AParasitePlayerState* PlayerState);
	void SpawnNests();
	void ClearNests();
	void SpawnAllPlayers();
	void TickInfiltrationRewards(float DeltaSeconds);
	void TickPossessionTimers();
	void EvaluateWinCondition();
	void FinishMatch(EParasiteTeam Winner, const FString& Reason);
	void BroadcastSound(EParasiteSound Sound);

	UPROPERTY()
	TArray<TObjectPtr<AParasiteNest>> Nests;

	UPROPERTY()
	TArray<TObjectPtr<APlayerStart>> TeamAStarts;

	UPROPERTY()
	TArray<TObjectPtr<APlayerStart>> TeamBStarts;

	int32 MatchSeed = 0;
	int32 StartIndexA = 0;
	int32 StartIndexB = 0;
	float InfiltrationTimer = 0.f;

	/** Players already rewarded for reaching the enemy back of house this match. */
	UPROPERTY()
	TSet<TObjectPtr<AParasitePlayerState>> InfiltrationCredited;
};
