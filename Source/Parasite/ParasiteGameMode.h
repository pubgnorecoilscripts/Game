#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ParasiteTypes.h"
#include "ParasiteAudio.h"
#include "Core/MatchSim.h"
#include "ParasiteGameMode.generated.h"

class UPossessableComponent;
class AParasitePlayerController;
class AParasitePlayerState;
class AParasiteNest;
class APlayerStart;

/**
 * The only class that owns match rules, and it delegates all of them to
 * Parasite::FMatchSim (see Core/, and Tests/CoreTests.cpp).
 *
 * Each tick it pushes actor positions into the simulation, ticks it, then
 * mirrors the results back onto actors and replicated properties. Clients never
 * decide anything.
 */
UCLASS()
class PARASITE_API AParasiteGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AParasiteGameMode();

	/** Convenience accessor; returns null on clients, which have no game mode. */
	static AParasiteGameMode* Get(const UObject* WorldContext);

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	// --- Host registry ---------------------------------------------------
	void RegisterHost(UPossessableComponent* Host);
	void UnregisterHost(UPossessableComponent* Host);

	// --- Player requests, already validated as coming from the server ----
	void RequestPossess(AParasitePlayerController* Controller);
	void RequestExit(AParasitePlayerController* Controller);
	void RequestScan(AParasitePlayerController* Controller);
	void RequestLeap(AParasitePlayerController* Controller, const FVector& Direction);
	void RequestResist(AParasitePlayerController* Controller);
	void RequestUpgrade(AParasitePlayerController* Controller, EParasiteUpgrade Upgrade);
	void RequestPing(AParasitePlayerController* Controller, const FVector& Location);
	void RequestRematch();

	/** Used by the door interaction; hinge props are opened, not possessed. */
	void RequestInteract(AParasitePlayerController* Controller);

	const Parasite::FMatchSim& GetSim() const { return Sim; }

private:
	void PushWorldIntoSim();
	void PullSimIntoWorld();
	void ApplyPossessionChanges();
	void EnterHostInWorld(AParasitePlayerController* Controller, UPossessableComponent* Host);
	void ExitHostInWorld(AParasitePlayerController* Controller, UPossessableComponent* Host);
	void SyncPhase();
	void SpawnNests();
	void SpawnAllPlayers();
	void BroadcastSound(EParasiteSound Sound);

	UPossessableComponent* FindHostComponent(Parasite::HostId HostId) const;
	AParasitePlayerController* FindController(Parasite::PlayerId PlayerId) const;

	Parasite::FMatchSim Sim;

	/** Live host components, by simulation id. */
	UPROPERTY()
	TMap<int32, TObjectPtr<UPossessableComponent>> HostComponents;

	/** Controllers, by simulation id. */
	UPROPERTY()
	TMap<int32, TObjectPtr<AParasitePlayerController>> ControllerByPlayer;

	/** Last host each player was riding, so changes can be detected. */
	TMap<int32, int32> LastKnownHost;

	UPROPERTY()
	TArray<TObjectPtr<AParasiteNest>> Nests;

	UPROPERTY()
	TArray<TObjectPtr<APlayerStart>> TeamAStarts;

	UPROPERTY()
	TArray<TObjectPtr<APlayerStart>> TeamBStarts;

	EMatchPhase LastPhase = EMatchPhase::Lobby;
	int32 StartIndexA = 0;
	int32 StartIndexB = 0;
	int32 MatchSeed = 0;
};
