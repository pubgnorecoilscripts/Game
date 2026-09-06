#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ParasiteTypes.h"
#include "PossessableComponent.generated.h"

class AParasitePlayerState;

/**
 * Add this component to any actor to make it a valid possession host.
 * The component owns the replicated possession state; the owning actor only
 * has to describe how it behaves (mobility profile, camera offset).
 *
 * Server authoritative: only the server ever calls BeginPossession/EndPossession.
 */
UCLASS(ClassGroup = (Parasite), meta = (BlueprintSpawnableComponent))
class PARASITE_API UPossessableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPossessableComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Category of host. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Parasite|Host")
	EHostType HostType = EHostType::Prop;

	/** How this host may move while possessed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Parasite|Host")
	EHostMobility Mobility = EHostMobility::Slide;

	/** Name shown in the "POSSESSED: X" HUD readout. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Parasite|Host")
	FString HostDisplayName = TEXT("OBJECT");

	/** Camera boom length used while possessing this host. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Parasite|Host")
	float CameraDistance = 350.f;

	/** Camera pivot height above the actor origin. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Parasite|Host")
	float CameraHeight = 80.f;

	/** If false the host cannot currently be entered (already taken, destroyed, ...). */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Parasite|Host")
	bool bPossessed = false;

	/** Player currently riding this host (server + replicated to all for scan/detection). */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Parasite|Host")
	TObjectPtr<AParasitePlayerState> Rider = nullptr;

	/** Server world time at which possession auto-expires. */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Parasite|Host")
	float PossessionEndTime = 0.f;

	/** Max seconds this host can be ridden, based on host type and rider upgrades. */
	float GetMaxDuration(const AParasitePlayerState* ForPlayer) const;

	/** True when a parasite may enter this host right now. */
	bool CanBePossessedBy(const AParasitePlayerState* ByPlayer) const;

	/** Server only. */
	void BeginPossession(AParasitePlayerState* ByPlayer);
	void EndPossession();

	/** DNA awarded on a successful entry. */
	int32 GetDNAReward() const;
};
