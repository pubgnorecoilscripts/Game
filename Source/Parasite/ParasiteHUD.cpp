#include "ParasiteHUD.h"
#include "ParasitePlayerController.h"
#include "ParasitePlayerState.h"
#include "ParasiteGameState.h"
#include "ParasiteCharacter.h"
#include "ParasiteAudio.h"
#include "Core/ParasiteRules.h"
#include "Engine/World.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerInput.h"

namespace
{
	const FLinearColor ColPanel(0.02f, 0.05f, 0.07f, 0.72f);
	const FLinearColor ColTeamA(0.15f, 0.75f, 1.f, 1.f);
	const FLinearColor ColTeamB(1.f, 0.45f, 0.1f, 1.f);
	const FLinearColor ColText(0.85f, 0.95f, 0.95f, 1.f);
	const FLinearColor ColDim(0.5f, 0.6f, 0.62f, 1.f);
	const FLinearColor ColAccent(0.4f, 1.f, 0.65f, 1.f);
	const FLinearColor ColWarn(1.f, 0.25f, 0.35f, 1.f);

	enum EButtonId
	{
		Btn_Play = 1,
		Btn_Host,
		Btn_Join,
		Btn_JoinConfirm,
		Btn_Settings,
		Btn_Quit,
		Btn_Back,
		Btn_SensUp,
		Btn_SensDown,
		Btn_Rematch,
		Btn_Resume
	};

	FString FormatTime(float Seconds)
	{
		const int32 Total = FMath::Max(0, FMath::CeilToInt(Seconds));
		return FString::Printf(TEXT("%02d:%02d"), Total / 60, Total % 60);
	}
}

AParasiteHUD::AParasiteHUD()
{
	HUDFont = GEngine ? GEngine->GetMediumFont() : nullptr;
}

void AParasiteHUD::BeginPlay()
{
	Super::BeginPlay();
	if (!HUDFont && GEngine)
	{
		HUDFont = GEngine->GetMediumFont();
	}
}

AParasitePlayerController* AParasiteHUD::GetOwningParasiteController() const
{
	return Cast<AParasitePlayerController>(GetOwningPlayerController());
}

AParasitePlayerState* AParasiteHUD::GetOwningState() const
{
	const AParasitePlayerController* PC = GetOwningParasiteController();
	return PC ? PC->GetPlayerState<AParasitePlayerState>() : nullptr;
}

AParasiteGameState* AParasiteHUD::GetParasiteGameState() const
{
	return GetWorld() ? GetWorld()->GetGameState<AParasiteGameState>() : nullptr;
}

bool AParasiteHUD::IsMenuActive() const
{
	if (bMainMenuOpen)
	{
		return true;
	}
	const AParasitePlayerController* PC = GetOwningParasiteController();
	if (PC && PC->bMenuOpen)
	{
		return true;
	}
	const AParasiteGameState* GS = GetParasiteGameState();
	return GS && GS->Phase == EMatchPhase::PostMatch;
}

// ---------------------------------------------------------------------------
// Drawing primitives
// ---------------------------------------------------------------------------

void AParasiteHUD::DrawPanel(float X, float Y, float Width, float Height, const FLinearColor& Colour)
{
	DrawRect(Colour, X, Y, Width, Height);
	DrawRect(FLinearColor(0.3f, 0.9f, 0.9f, 0.35f), X, Y, Width, 2.f);
	DrawRect(FLinearColor(0.3f, 0.9f, 0.9f, 0.15f), X, Y + Height - 2.f, Width, 2.f);
}

void AParasiteHUD::DrawLabel(const FString& Text, float X, float Y, const FLinearColor& Colour, float Scale, bool bCentre)
{
	if (!Canvas)
	{
		return;
	}
	UFont* Font = HUDFont ? HUDFont.Get() : (GEngine ? GEngine->GetMediumFont() : nullptr);
	float DrawX = X;
	if (bCentre && Font)
	{
		float TextWidth = 0.f, TextHeight = 0.f;
		Canvas->TextSize(Font, Text, TextWidth, TextHeight, Scale, Scale);
		DrawX = X - TextWidth * 0.5f;
	}
	DrawText(Text, Colour, DrawX, Y, Font, Scale, false);
}

void AParasiteHUD::DrawBar(float X, float Y, float Width, float Height, float Fraction, const FLinearColor& Colour)
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), X, Y, Width, Height);
	DrawRect(Colour, X + 2.f, Y + 2.f, FMath::Clamp(Fraction, 0.f, 1.f) * (Width - 4.f), Height - 4.f);
}

void AParasiteHUD::AddButton(const FString& Label, float X, float Y, float Width, float Height, int32 Id)
{
	FParasiteButton Button;
	Button.Label = Label;
	Button.Position = FVector2D(X, Y);
	Button.Size = FVector2D(Width, Height);
	Button.Id = Id;
	Buttons.Add(Button);

	bool bHover = false;
	if (AParasitePlayerController* PC = GetOwningParasiteController())
	{
		float MouseX = 0.f, MouseY = 0.f;
		if (PC->GetMousePosition(MouseX, MouseY))
		{
			bHover = MouseX >= X && MouseX <= X + Width && MouseY >= Y && MouseY <= Y + Height;
		}
	}
	DrawPanel(X, Y, Width, Height, bHover ? FLinearColor(0.1f, 0.35f, 0.35f, 0.9f) : ColPanel);
	DrawLabel(Label, X + Width * 0.5f, Y + Height * 0.5f - 10.f, bHover ? ColAccent : ColText, 1.2f, true);
}

// ---------------------------------------------------------------------------
// Main draw
// ---------------------------------------------------------------------------

void AParasiteHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}
	Buttons.Reset();
	ViewX = Canvas->ClipX;
	ViewY = Canvas->ClipY;

	// One place owns the cursor: the front end, the pause menu, or the end screen.
	if (AParasitePlayerController* PC = GetOwningParasiteController())
	{
		const bool bWantsCursor = IsMenuActive();
		if (PC->bShowMouseCursor != bWantsCursor)
		{
			PC->bShowMouseCursor = bWantsCursor;
			if (bWantsCursor)
			{
				PC->SetInputMode(FInputModeGameAndUI());
			}
			else
			{
				PC->SetInputMode(FInputModeGameOnly());
			}
		}
	}

	if (bMainMenuOpen)
	{
		if (bSettingsOpen)
		{
			DrawSettings();
		}
		else
		{
			DrawMainMenu();
		}
		return;
	}

	DrawMatchBar();
	DrawPlayerBar();
	DrawMarkers();
	DrawMessage();
	DrawCrosshair();

	const AParasiteGameState* GS = GetParasiteGameState();
	if (GS && GS->Phase == EMatchPhase::PostMatch)
	{
		DrawEndScreen();
	}
	if (bShowScoreboard)
	{
		DrawScoreboard();
	}
	if (const AParasitePlayerController* PC = GetOwningParasiteController())
	{
		if (PC->bMenuOpen)
		{
			if (bSettingsOpen)
			{
				DrawSettings();
			}
			else
			{
				DrawPauseMenu();
			}
		}
	}
}

void AParasiteHUD::DrawMatchBar()
{
	const AParasiteGameState* GS = GetParasiteGameState();
	if (!GS)
	{
		return;
	}
	const float BarW = 620.f;
	const float BarX = ViewX * 0.5f - BarW * 0.5f;
	DrawPanel(BarX, 12.f, BarW, 62.f, ColPanel);

	const float InfectA = GS->GetInfectionByTeam(EParasiteTeam::TeamA);
	const float InfectB = GS->GetInfectionByTeam(EParasiteTeam::TeamB);

	DrawLabel(FString::Printf(TEXT("TEAM A  %3.0f%%"), InfectA), BarX + 20.f, 22.f, ColTeamA, 1.1f);
	DrawBar(BarX + 20.f, 48.f, 170.f, 12.f, InfectA / 100.f, ColTeamA);

	DrawLabel(FString::Printf(TEXT("%3.0f%%  TEAM B"), InfectB), BarX + BarW - 150.f, 22.f, ColTeamB, 1.1f);
	DrawBar(BarX + BarW - 190.f, 48.f, 170.f, 12.f, InfectB / 100.f, ColTeamB);

	FString Centre;
	switch (GS->Phase)
	{
	case EMatchPhase::Lobby:		Centre = TEXT("WAITING"); break;
	case EMatchPhase::Countdown:	Centre = FString::Printf(TEXT("STARTS IN %d"), FMath::CeilToInt(GS->PhaseTimeRemaining)); break;
	case EMatchPhase::PostMatch:	Centre = TEXT("MATCH OVER"); break;
	default:						Centre = FormatTime(GS->PhaseTimeRemaining); break;
	}
	DrawLabel(Centre, ViewX * 0.5f, 28.f, ColText, 1.6f, true);
}

void AParasiteHUD::DrawPlayerBar()
{
	const AParasitePlayerState* PS = GetOwningState();
	const AParasitePlayerController* PC = GetOwningParasiteController();
	if (!PS || !PC)
	{
		return;
	}
	const Parasite::FRules Rules;		// display only; the server owns the real ones
	const float PanelH = 118.f;
	const float PanelY = ViewY - PanelH - 16.f;
	DrawPanel(20.f, PanelY, 460.f, PanelH, ColPanel);

	const bool bTeamA = PS->Team == EParasiteTeam::TeamA;
	DrawLabel(bTeamA ? TEXT("TEAM A") : (PS->Team == EParasiteTeam::TeamB ? TEXT("TEAM B") : TEXT("NO TEAM")),
		36.f, PanelY + 10.f, bTeamA ? ColTeamA : ColTeamB, 1.2f);
	DrawLabel(FString::Printf(TEXT("DNA %d"), PS->DNA), 160.f, PanelY + 10.f, ColAccent, 1.2f);

	const bool bPossessReady = PS->PossessCooldownRemaining <= 0.f;
	DrawLabel(TEXT("POSSESS [E]"), 36.f, PanelY + 40.f, bPossessReady ? ColText : ColDim);
	DrawBar(180.f, PanelY + 42.f, 120.f, 10.f,
		bPossessReady ? 1.f : 1.f - PS->PossessCooldownRemaining / Rules.PossessCooldown,
		bPossessReady ? ColAccent : ColDim);

	const bool bScanReady = PS->ScanCooldownRemaining <= 0.f;
	DrawLabel(TEXT("SCAN [F]"), 36.f, PanelY + 62.f, bScanReady ? ColText : ColDim);
	DrawBar(180.f, PanelY + 64.f, 120.f, 10.f,
		bScanReady ? 1.f : 1.f - PS->ScanCooldownRemaining / Rules.ScanCooldown,
		bScanReady ? ColAccent : ColDim);

	if (PS->CurrentHost)
	{
		DrawLabel(FString::Printf(TEXT("POSSESSED: %s  %.1fs"), *PS->CurrentHostName, PS->HostTimeRemaining),
			36.f, PanelY + 86.f, ColWarn, 1.1f);
		// The bar is scaled against the longest possible stay for this host type.
		const float MaxDuration = (PS->CurrentHostName == TEXT("ENEMY")) ? Rules.PlayerDuration + Rules.MimicDurationBonus
			: (PS->CurrentHostName == TEXT("NPC")) ? Rules.NPCDuration : Rules.PropDuration;
		DrawBar(320.f, PanelY + 88.f, 120.f, 10.f, PS->HostTimeRemaining / MaxDuration, ColWarn);
	}
	else
	{
		DrawLabel(TEXT("HOST: NONE (PARASITE)"), 36.f, PanelY + 86.f, ColDim, 1.1f);
	}

	DrawLabel(FString::Printf(TEXT("[1] JUMPER  [2] MIMIC  [3] INFILTRATOR   (%d DNA each, %d/%d used)"),
		Rules.UpgradeCost, PS->NumUpgrades, Rules.MaxUpgrades), 24.f, PanelY - 30.f, ColDim);

	if (const AParasiteCharacter* Body = PC->ParasiteBody.Get())
	{
		if (Body->bHijacked)
		{
			DrawLabel(TEXT("!! HIJACKED - MASH [R] !!"), ViewX * 0.5f, ViewY * 0.62f, ColWarn, 1.8f, true);
			DrawBar(ViewX * 0.5f - 120.f, ViewY * 0.66f, 240.f, 16.f, Body->ResistProgress, ColWarn);
		}
	}
}

void AParasiteHUD::DrawMarkers()
{
	if (!GetWorld())
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	for (int32 Index = Markers.Num() - 1; Index >= 0; --Index)
	{
		if (Now > Markers[Index].ExpiryTime)
		{
			Markers.RemoveAtSwap(Index);
			continue;
		}
		const FVector Screen = Project(Markers[Index].Location);
		if (Screen.Z <= 0.f)
		{
			continue;		// behind the camera
		}
		const FLinearColor Colour(Markers[Index].Color);
		DrawRect(Colour, Screen.X - 8.f, Screen.Y - 8.f, 16.f, 16.f);
		DrawLabel(Markers[Index].Label, Screen.X, Screen.Y + 12.f, Colour, 1.f, true);
	}
}

void AParasiteHUD::DrawMessage()
{
	if (!GetWorld() || CurrentMessage.IsEmpty() || GetWorld()->GetTimeSeconds() > MessageExpiry)
	{
		return;
	}
	DrawLabel(CurrentMessage, ViewX * 0.5f, ViewY * 0.32f, ColAccent, 1.6f, true);
}

void AParasiteHUD::DrawCrosshair()
{
	DrawRect(FLinearColor(0.6f, 1.f, 0.8f, 0.6f), ViewX * 0.5f - 1.f, ViewY * 0.5f - 6.f, 2.f, 12.f);
	DrawRect(FLinearColor(0.6f, 1.f, 0.8f, 0.6f), ViewX * 0.5f - 6.f, ViewY * 0.5f - 1.f, 12.f, 2.f);
}

void AParasiteHUD::DrawScoreboard()
{
	const AParasiteGameState* GS = GetParasiteGameState();
	if (!GS)
	{
		return;
	}
	const float PanelW = 560.f;
	const float PanelX = ViewX * 0.5f - PanelW * 0.5f;
	const float PanelY = 110.f;
	DrawPanel(PanelX, PanelY, PanelW, 60.f + GS->PlayerArray.Num() * 24.f, FLinearColor(0.02f, 0.05f, 0.07f, 0.9f));
	DrawLabel(TEXT("PLAYER                     TEAM     DNA    INFECT"), PanelX + 20.f, PanelY + 16.f, ColDim);

	float Row = PanelY + 44.f;
	for (const APlayerState* Base : GS->PlayerArray)
	{
		const AParasitePlayerState* PS = Cast<AParasitePlayerState>(Base);
		if (!PS)
		{
			continue;
		}
		DrawLabel(FString::Printf(TEXT("%-24s   %s    %4d    %4d"),
			*PS->GetPlayerName().Left(24),
			PS->Team == EParasiteTeam::TeamA ? TEXT("A") : TEXT("B"),
			PS->LifetimeDNA, PS->InfectionTicks),
			PanelX + 20.f, Row, PS->Team == EParasiteTeam::TeamA ? ColTeamA : ColTeamB);
		Row += 24.f;
	}
}

void AParasiteHUD::DrawEndScreen()
{
	const AParasiteGameState* GS = GetParasiteGameState();
	if (!GS)
	{
		return;
	}
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), 0.f, 0.f, ViewX, ViewY);

	FString Title = TEXT("DRAW");
	FLinearColor Colour = ColText;
	if (GS->WinningTeam != EParasiteTeam::None)
	{
		const AParasitePlayerState* PS = GetOwningState();
		const bool bWon = PS && PS->Team == GS->WinningTeam;
		Title = bWon ? TEXT("YOUR HIVE WINS") : TEXT("YOUR HIVE LOSES");
		Colour = bWon ? ColAccent : ColWarn;
	}
	DrawLabel(Title, ViewX * 0.5f, ViewY * 0.3f, Colour, 2.4f, true);
	DrawLabel(GS->ResultReason, ViewX * 0.5f, ViewY * 0.3f + 44.f, ColText, 1.2f, true);
	DrawLabel(FString::Printf(TEXT("NEXT MATCH IN %d"), FMath::CeilToInt(GS->PhaseTimeRemaining)),
		ViewX * 0.5f, ViewY * 0.3f + 76.f, ColDim, 1.2f, true);

	AddButton(TEXT("REMATCH NOW"), ViewX * 0.5f - 140.f, ViewY * 0.3f + 116.f, 280.f, 46.f, Btn_Rematch);
}

void AParasiteHUD::DrawMainMenu()
{
	DrawRect(FLinearColor(0.01f, 0.03f, 0.04f, 0.94f), 0.f, 0.f, ViewX, ViewY);
	DrawLabel(TEXT("PARASITE"), ViewX * 0.5f, ViewY * 0.16f, ColAccent, 4.f, true);
	DrawLabel(TEXT("anything here could be them"), ViewX * 0.5f, ViewY * 0.16f + 60.f, ColDim, 1.2f, true);

	const float ButtonW = 320.f;
	const float ButtonX = ViewX * 0.5f - ButtonW * 0.5f;
	float Y = ViewY * 0.34f;

	AddButton(TEXT("PLAY"), ButtonX, Y, ButtonW, 48.f, Btn_Play);			Y += 58.f;
	AddButton(TEXT("HOST"), ButtonX, Y, ButtonW, 48.f, Btn_Host);			Y += 58.f;
	AddButton(TEXT("JOIN"), ButtonX, Y, ButtonW, 48.f, Btn_Join);			Y += 58.f;
	AddButton(TEXT("SETTINGS"), ButtonX, Y, ButtonW, 48.f, Btn_Settings);	Y += 58.f;
	AddButton(TEXT("QUIT"), ButtonX, Y, ButtonW, 48.f, Btn_Quit);			Y += 66.f;

	if (bJoinEditing)
	{
		DrawLabel(FString::Printf(TEXT("SERVER: %s_"), *JoinAddress), ViewX * 0.5f, Y, ColText, 1.4f, true);
		DrawLabel(TEXT("type an address, then ENTER or CONNECT"), ViewX * 0.5f, Y + 26.f, ColDim, 1.f, true);
		AddButton(TEXT("CONNECT"), ButtonX, Y + 52.f, ButtonW, 44.f, Btn_JoinConfirm);
	}
	else
	{
		DrawLabel(TEXT("PLAY starts a local match. HOST opens a listen server for up to 10 players."),
			ViewX * 0.5f, Y, ColDim, 1.f, true);
	}
}

void AParasiteHUD::DrawSettings()
{
	DrawRect(FLinearColor(0.01f, 0.03f, 0.04f, 0.94f), 0.f, 0.f, ViewX, ViewY);
	DrawLabel(TEXT("SETTINGS"), ViewX * 0.5f, ViewY * 0.2f, ColAccent, 2.6f, true);
	DrawLabel(FString::Printf(TEXT("MOUSE SENSITIVITY: %.2f"), MouseSensitivity), ViewX * 0.5f, ViewY * 0.34f, ColText, 1.4f, true);

	AddButton(TEXT("-"), ViewX * 0.5f - 160.f, ViewY * 0.4f, 150.f, 44.f, Btn_SensDown);
	AddButton(TEXT("+"), ViewX * 0.5f + 10.f, ViewY * 0.4f, 150.f, 44.f, Btn_SensUp);

	DrawLabel(TEXT("CONTROLS"), ViewX * 0.5f, ViewY * 0.52f, ColAccent, 1.4f, true);
	const TCHAR* Lines[] = {
		TEXT("WASD move   SHIFT sprint   CTRL crouch   SPACE jump / swing a door"),
		TEXT("E possess or interact   Q leave host   LMB leap   F scan   MMB ping"),
		TEXT("R resist a hijack   TAB scoreboard   1/2/3 evolve   ESC menu")
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Lines); ++Index)
	{
		DrawLabel(Lines[Index], ViewX * 0.5f, ViewY * 0.56f + Index * 22.f, ColDim, 1.1f, true);
	}

	AddButton(TEXT("BACK"), ViewX * 0.5f - 160.f, ViewY * 0.74f, 320.f, 46.f, Btn_Back);
}

void AParasiteHUD::DrawPauseMenu()
{
	const float PanelW = 340.f;
	const float PanelX = ViewX * 0.5f - PanelW * 0.5f;
	DrawPanel(PanelX - 20.f, ViewY * 0.32f - 30.f, PanelW + 40.f, 260.f, FLinearColor(0.02f, 0.05f, 0.07f, 0.9f));
	DrawLabel(TEXT("PARASITE"), ViewX * 0.5f, ViewY * 0.32f - 20.f, ColAccent, 1.6f, true);
	AddButton(TEXT("RESUME"), PanelX, ViewY * 0.32f + 30.f, PanelW, 46.f, Btn_Resume);
	AddButton(TEXT("SETTINGS"), PanelX, ViewY * 0.32f + 86.f, PanelW, 46.f, Btn_Settings);
	AddButton(TEXT("QUIT"), PanelX, ViewY * 0.32f + 142.f, PanelW, 46.f, Btn_Quit);
}

// ---------------------------------------------------------------------------
// Interaction
// ---------------------------------------------------------------------------

void AParasiteHUD::LeaveMenus()
{
	bMainMenuOpen = false;
	bJoinEditing = false;
	bSettingsOpen = false;
	if (AParasitePlayerController* PC = GetOwningParasiteController())
	{
		PC->bMenuOpen = false;
	}
}

void AParasiteHUD::HandleMenuClick()
{
	AParasitePlayerController* PC = GetOwningParasiteController();
	if (!PC)
	{
		return;
	}
	float MouseX = 0.f, MouseY = 0.f;
	if (!PC->GetMousePosition(MouseX, MouseY))
	{
		return;
	}

	for (const FParasiteButton& Button : Buttons)
	{
		const bool bInside =
			MouseX >= Button.Position.X && MouseX <= Button.Position.X + Button.Size.X &&
			MouseY >= Button.Position.Y && MouseY <= Button.Position.Y + Button.Size.Y;
		if (!bInside)
		{
			continue;
		}
		FParasiteAudio::Play2D(this, EParasiteSound::UIClick);

		switch (Button.Id)
		{
		case Btn_Play:
		case Btn_Resume:
			LeaveMenus();
			break;

		case Btn_Host:
			// A listen server on the current map; everybody else uses JOIN.
			PC->ConsoleCommand(TEXT("open /Engine/Maps/Entry?listen"), true);
			LeaveMenus();
			break;

		case Btn_Join:
			bJoinEditing = true;
			break;

		case Btn_JoinConfirm:
			PC->ClientTravel(JoinAddress, ETravelType::TRAVEL_Absolute);
			LeaveMenus();
			break;

		case Btn_Settings:
			bSettingsOpen = true;
			break;

		case Btn_Back:
			bSettingsOpen = false;
			break;

		case Btn_SensUp:
		case Btn_SensDown:
			MouseSensitivity = FMath::Clamp(MouseSensitivity + ((Button.Id == Btn_SensUp) ? 0.1f : -0.1f), 0.1f, 4.f);
			if (PC->PlayerInput)
			{
				PC->PlayerInput->SetMouseSensitivity(MouseSensitivity);
			}
			break;

		case Btn_Rematch:
			PC->ServerRequestRematch();
			break;

		case Btn_Quit:
			PC->ConsoleCommand(TEXT("quit"), true);
			break;

		default:
			break;
		}
		return;
	}
}

bool AParasiteHUD::HandleTextInput(const FKey& Key)
{
	if (!bJoinEditing)
	{
		return false;
	}
	if (Key == EKeys::BackSpace)
	{
		JoinAddress.LeftChopInline(1);
		return true;
	}
	if (Key == EKeys::Enter)
	{
		if (AParasitePlayerController* PC = GetOwningParasiteController())
		{
			PC->ClientTravel(JoinAddress, ETravelType::TRAVEL_Absolute);
			LeaveMenus();
		}
		return true;
	}

	// Digits report as "One", "Two", ... and the separators by name.
	const FString Name = Key.GetFName().ToString();
	static const TCHAR* Words[] = { TEXT("Zero"), TEXT("One"), TEXT("Two"), TEXT("Three"), TEXT("Four"),
		TEXT("Five"), TEXT("Six"), TEXT("Seven"), TEXT("Eight"), TEXT("Nine") };
	for (int32 Digit = 0; Digit < UE_ARRAY_COUNT(Words); ++Digit)
	{
		if (Name == Words[Digit] || Name == FString::Printf(TEXT("NumPad%s"), Words[Digit]))
		{
			JoinAddress.AppendChar(static_cast<TCHAR>(TEXT('0') + Digit));
			return true;
		}
	}
	if (Name == TEXT("Period") || Name == TEXT("Decimal"))
	{
		JoinAddress.AppendChar(TEXT('.'));
		return true;
	}
	if (Name == TEXT("Colon") || Name == TEXT("Semicolon"))
	{
		JoinAddress.AppendChar(TEXT(':'));
		return true;
	}
	return false;
}

void AParasiteHUD::ShowMessage(const FString& Message, float Duration)
{
	CurrentMessage = Message;
	MessageExpiry = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f) + Duration;
}

void AParasiteHUD::AddMarker(const FVector& Location, const FColor& Colour, const FString& Label, float Duration)
{
	FParasiteMarker Marker;
	Marker.Location = Location;
	Marker.Color = Colour;
	Marker.Label = Label;
	Marker.ExpiryTime = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f) + Duration;
	Markers.Add(Marker);
}
