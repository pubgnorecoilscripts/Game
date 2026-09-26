#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ParasiteTypes.h"
#include "ParasiteHUD.generated.h"

class AParasitePlayerController;
class AParasitePlayerState;
class AParasiteGameState;
class UFont;

/** One clickable rectangle, rebuilt every frame. */
struct FParasiteButton
{
	FString Label;
	FVector2D Position = FVector2D::ZeroVector;
	FVector2D Size = FVector2D::ZeroVector;
	int32 Id = 0;
};

/** Canvas HUD, so the project needs no UMG assets. */
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

	/** Called by the controller on a left click while a menu is up. */
	void HandleMenuClick();

	/** Feeds typed characters into the join address field. True if consumed. */
	bool HandleTextInput(const FKey& Key);

	bool bShowScoreboard = false;

	/** The front end is up until the player picks PLAY, HOST or JOIN. */
	bool bMainMenuOpen = true;
	bool bSettingsOpen = false;
	bool bJoinEditing = false;

	FString JoinAddress = TEXT("127.0.0.1");
	float MouseSensitivity = 1.f;

private:
	void DrawMatchBar();
	void DrawPlayerBar();
	void DrawMarkers();
	void DrawMessage();
	void DrawCrosshair();
	void DrawScoreboard();
	void DrawMainMenu();
	void DrawSettings();
	void DrawEndScreen();
	void DrawPauseMenu();

	void DrawPanel(float X, float Y, float Width, float Height, const FLinearColor& Colour);
	void DrawLabel(const FString& Text, float X, float Y, const FLinearColor& Colour, float Scale = 1.f, bool bCentre = false);
	void DrawBar(float X, float Y, float Width, float Height, float Fraction, const FLinearColor& Colour);
	void AddButton(const FString& Label, float X, float Y, float Width, float Height, int32 Id);

	void LeaveMenus();

	AParasitePlayerController* GetOwningParasiteController() const;
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
