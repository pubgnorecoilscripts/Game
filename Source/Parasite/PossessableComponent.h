#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ParasiteTypes.h"
#include "PossessableComponent.generated.h"

class AParasitePlayerState;

/**
 * Put this on any actor to make it a possession host.
 *
 * The component holds no rules of its own: on BeginPlay the server registers the
 * owning actor with the match simulation and keeps the returned host id. What
 * you see below is the replicated *view* of that host, refreshed by the game
 * mode each tick so clients can draw it.
 */
UCLASS(ClassGroup = (Parasite), meta = (BlueprintSpawnableComponent))
class PARASITE_API UPossessableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPossessableComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// --- Configuration, set before or at spawn --------------------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Parasite|Host")
	EHostType HostType = EHostType::Prop;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Parasite|Host")
	EHostMobility Mobility = EHostMobility::Slide;

	/** Shown as "POSSESSED: <name>" on the rider's HUD. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Parasite|Host")
	FString HostDisplayName = TEXT("OBJECT");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Parasite|Host")
	float CameraDistance = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Parasite|Host")
	float CameraHeight = 80.f;

	// --- Replicated view of the simulation ------------------------------
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Parasite|Host")
	bool bPossessed = false;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Parasite|Host")
	TObjectPtr<AParasitePlayerState> Rider = nullptr;

	/** Seconds of possession left. Counted down by the server. */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Parasite|Host")
	float PossessionTimeRemaining = 0.f;

	/** Set for ~1.5 s after an enemy scan catches this host. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Revealed, Category = "Parasite|Host")
	bool bRevealed = false;

	UFUNCTION()
	void OnRep_Revealed();

	/**
	 * True when the game mode links this component to an existing simulation host
	 * instead of creating one. Player parasite bodies are already hosts, created
	 * by FMatchSim::AddPlayer, so their component must not register a second time.
	 */
	UPROPERTY(EditAnywhere, Category = "Parasite|Host")
	bool bManagedExternally = false;

	/** The simulation's id for this actor. Server only; invalid until registered. */
	Parasite::HostId GetHostId() const { return HostId; }
	void SetHostId(Parasite::HostId InHostId) { HostId = InHostId; }

private:
	Parasite::HostId HostId = Parasite::InvalidHost;
};
