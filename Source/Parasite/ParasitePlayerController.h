#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ParasiteTypes.h"
#include "Runtime/Launch/Resources/Version.h"
#include "ParasitePlayerController.generated.h"

class AParasiteCharacter;
class AParasitePlayerState;
class AParasiteHUD;

/**
 * Owns player intent and nothing else. Every action below is a request to the
 * server, which asks the match simulation and applies whatever it decides.
 */
UCLASS()
class PARASITE_API AParasitePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AParasitePlayerController();

	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;
#else
	virtual bool InputKey(const FInputKeyParams& Params) override;
#endif

	/** The player's own parasite body, kept alive while riding another host. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	TObjectPtr<AParasiteCharacter> ParasiteBody = nullptr;

	/** The enemy parasite this player is currently driving, if any. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Parasite")
	TObjectPtr<AParasiteCharacter> HijackVictim = nullptr;

	void SetParasiteBody(AParasiteCharacter* Body);
	void SetHijackVictim(AParasiteCharacter* Victim);

	/** This controller's id inside the match simulation. Server side. */
	int32 GetSimPlayerId() const { return SimPlayerId; }
	void SetSimPlayerId(int32 InId) { SimPlayerId = InId; }

	AParasitePlayerState* GetParasitePlayerState() const;
	AParasiteHUD* GetParasiteHUD() const;

	/** True while a menu owns the input, so gameplay keys are ignored. */
	bool IsInputBlocked() const;

	/** Menu state, client side only. */
	bool bMenuOpen = false;

	// --- Client feedback -------------------------------------------------
	UFUNCTION(Client, Reliable)
	void ClientNotify(const FString& Message, float Duration);

	UFUNCTION(Client, Reliable)
	void ClientPlaySound(uint8 Sound);

	UFUNCTION(Client, Reliable)
	void ClientAddMarker(FVector Location, FColor Colour, const FString& Label, float Duration);

	/** Asks the server for an immediate rematch (the post-match button). */
	UFUNCTION(Server, Reliable)
	void ServerRequestRematch();

protected:
	// --- Input handlers ---------------------------------------------------
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
	void OnInteractPressed();
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

	// --- Server RPCs -------------------------------------------------------
	UFUNCTION(Server, Reliable)
	void ServerRequestInteract();

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

	/** Props have no client prediction, so the server drives them. */
	UFUNCTION(Server, Unreliable)
	void ServerDriveHost(float Forward, float Right);

	/** Hijack steering: the client sends intent, the server moves the victim. */
	UFUNCTION(Server, Unreliable)
	void ServerHijackInput(float Forward, float Right, float YawDelta);

private:
	int32 SimPlayerId = -1;
	float CachedForward = 0.f;
	float CachedRight = 0.f;
	float CachedYawDelta = 0.f;
};
