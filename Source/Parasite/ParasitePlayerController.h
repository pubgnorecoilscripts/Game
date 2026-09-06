#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ParasiteTypes.h"
#include "Runtime/Launch/Resources/Version.h"
#include "ParasitePlayerController.generated.h"

class AParasiteCharacter;
class AParasitePlayerState;
class UPossessableComponent;
class AParasiteHUD;

/**
 * Owns all player intent. Clients only ever *ask* - every state change below
 * happens on the server and replicates back.
 */
UCLASS()
class PARASITE_API AParasitePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AParasitePlayerController();

	virtual void SetupInputComponent() override;
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;
#else
	virtual bool InputKey(const FInputKeyParams& Params) override;
#endif
	virtual void PlayerTick(float DeltaTime) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	/** The player's own parasite body. Kept alive (dormant) while riding a host. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	TObjectPtr<AParasiteCharacter> ParasiteBody = nullptr;

	/** The enemy parasite this player is currently hijacking, if any. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	TObjectPtr<AParasiteCharacter> HijackVictim = nullptr;

	/** Server: register the parasite body spawned for this player. */
	void SetParasiteBody(AParasiteCharacter* Body);

	/** Server: forcibly return this player to their own body (timeout, resist, death, match end). */
	void ServerExitPossession(bool bWasExpelled);

	/** Server: drop everything and get ready for a fresh match. */
	void ResetForNewMatch();

	AParasitePlayerState* GetParasitePlayerState() const;
	AParasiteHUD* GetParasiteHUD() const;

	/** Client feedback. */
	UFUNCTION(Client, Reliable)
	void ClientNotify(const FString& Message, float Duration);

	UFUNCTION(Client, Reliable)
	void ClientPlaySound(uint8 Sound);

	UFUNCTION(Client, Reliable)
	void ClientAddMarker(FVector Location, FColor Colour, const FString& Label, float Duration);

	/** True when this player is currently riding anything. */
	bool IsPossessing() const;

	/** Menu state (client only). */
	bool bMenuOpen = false;

	/** True while a menu owns the input, so gameplay keys are ignored. */
	bool IsInputBlocked() const;

protected:
	// --- Input handlers -------------------------------------------------
	void OnMoveForward(float Value);
	void OnMoveRight(float Value);
	void OnTurn(float Value);
	void OnLookUp(float Value);
	void OnTurnRate(float Value);
	void OnLookUpRate(float Value);
	void OnJumpPressed();
	void OnJumpReleased();
	void OnSprintPressed();
	void OnSprintReleased();
	void OnCrouchToggle();
	void OnPossessPressed();
	void OnExitPressed();
	void OnLeapPressed();
	void OnScanPressed();
	void OnPingPressed();
	void OnResistPressed();
	void OnScoreboardPressed();
	void OnScoreboardReleased();
	void OnUpgrade1();
	void OnUpgrade2();
	void OnUpgrade3();
	void OnMenuToggle();
	void OnMenuClick();

	// --- Server RPCs ----------------------------------------------------
	UFUNCTION(Server, Reliable)
	void ServerRequestPossess();

	UFUNCTION(Server, Reliable)
	void ServerRequestExit();

	UFUNCTION(Server, Reliable)
	void ServerRequestLeap(FVector_NetQuantize Direction);

	UFUNCTION(Server, Reliable)
	void ServerRequestScan();

	UFUNCTION(Server, Reliable)
	void ServerRequestPing(FVector_NetQuantize Location);

	UFUNCTION(Server, Reliable)
	void ServerRequestResist();

	UFUNCTION(Server, Reliable)
	void ServerRequestUpgrade(EParasiteUpgrade Upgrade);

public:
	/** Asks the server for an immediate rematch (post-match screen button). */
	UFUNCTION(Server, Reliable)
	void ServerRequestRematch();

protected:

	/** Prop hosts have no client prediction, so the server drives them. */
	UFUNCTION(Server, Unreliable)
	void ServerDriveHost(float Forward, float Right);

	/** Hijack driving: the client sends intent, the server moves the victim. */
	UFUNCTION(Server, Unreliable)
	void ServerHijackInput(float Forward, float Right, float YawDelta);

private:
	/** Server: pick the best host in range of the player's current body. */
	UPossessableComponent* FindBestTarget(float& OutDistance) const;

	/** Server: enter a host. Handles both real possession and enemy hijack. */
	bool EnterHost(UPossessableComponent* Target);

	/** The actor that represents this player in the world right now. */
	AActor* GetBodyActor() const;

	float CachedForward = 0.f;
	float CachedRight = 0.f;
	float CachedYawDelta = 0.f;
	bool bSprinting = false;
};
