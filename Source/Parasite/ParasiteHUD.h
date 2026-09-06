#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ParasiteTypes.h"
#include "ParasiteHUD.generated.h"

class AParasitePlayerController;
class AParasitePlayerState;
class AParasiteGameState;

/** One clickable rectangle in the menu, rebuilt every frame. */
struct FParasiteButton
{
	FString Label;
	FVector2D Position = FVector2D::ZeroVector;
	FVector2D Size = FVector2D::ZeroVector;
	int32 Id = 0;
};

/**
 * Canvas HUD. Everything is drawn in code so the project needs no UMG assets.
 */
UCLASS()
class PARASITE_API AParasiteHUD : public AHUD
{
	GENERATED_BODY()

public:
	AParasiteHUD();

	virtual void DrawHUD() override;
	virtual void BeginPlay() override;

	void ShowMessage(const FString& Message, float Duration);
	void AddMarker(const FVector& Location, const FColor& Colour, const FString& Label, float Duration);

	/** True while any menu wants the mouse cursor. */
	bool IsMenuActive() const;

	/** Called by the controller on left click while a menu is open. */
	void HandleMenuClick();

	/** Feeds typed characters into the join-address field. Returns true if consumed. */
	bool HandleTextInput(const FKey& Key);

	bool bShowScoreboard = false;

	/** The front end is up until the player picks PLAY / HOST / JOIN. */
	bool bMainMenuOpen = true;

	bool bSettingsOpen = false;
	bool bJoinEditing = false;

	FString JoinAddress = TEXT("127.0.0.1");

	float MouseSensitivity = 1.f;

private:
	void DrawMatchBar();
	void DrawPlayerBar();
	void DrawMarkers();
	void DrawMessages();
	void DrawScoreboard();
	void DrawMainMenu();
	void DrawSettings();
	void DrawEndScreen();
	void DrawCrosshair();

	void DrawPanel(float X, float Y, float Width, float Height, const FLinearColor& Colour);
	void DrawLabel(const FString& Text, float X, float Y, const FLinearColor& Colour, float Scale = 1.f, bool bCentre = false);
	void AddButton(const FString& Label, float X, float Y, float Width, float Height, int32 Id);
	void DrawBar(float X, float Y, float Width, float Height, float Fraction, const FLinearColor& Colour);

	AParasitePlayerController* GetOwningController() const;
	AParasitePlayerState* GetOwningState() const;
	AParasiteGameState* GetParasiteGameState() const;

	TArray<FParasiteButton> Buttons;
	TArray<FParasiteMarker> Markers;

	FString CurrentMessage;
	float MessageExpiry = 0.f;

	UPROPERTY()
	TObjectPtr<UFont> HUDFont;

	float ViewX = 1280.f;
	float ViewY = 720.f;
};
