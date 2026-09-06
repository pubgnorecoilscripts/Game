#include "ParasiteHUD.h"
#include "ParasitePlayerController.h"
#include "ParasitePlayerState.h"
#include "ParasiteGameState.h"
#include "ParasiteCharacter.h"
#include "PossessableComponent.h"
#include "ParasiteAudio.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerInput.h"
#include "Kismet/GameplayStatics.h"

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
		Btn_Resume,
		Btn_Upgrade1,
		Btn_Upgrade2,
		Btn_Upgrade3
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
	// The front end owns the mouse until the player picks something.
	if (AParasitePlayerController* PC = GetOwningController())
	{
		PC->bMenuOpen = true;
		PC->bShowMouseCursor = true;
		PC->SetInputMode(FInputModeGameAndUI());
	}
}

AParasitePlayerController* AParasiteHUD::GetOwningController() const
{
	return Cast<AParasitePlayerController>(GetOwningPlayerController());
}

AParasitePlayerState* AParasiteHUD::GetOwningState() const
{
	const AParasitePlayerController* PC = GetOwningController();
	return PC ? PC->GetPlayerState<AParasitePlayerState>() : nullptr;
}

AParasiteGameState* AParasiteHUD::GetParasiteGameState() const
{
	return GetWorld() ? GetWorld()->GetGameState<AParasiteGameState>() : nullptr;
}

// ---------------------------------------------------------------------------
// Drawing primitives
// ---------------------------------------------------------------------------

void AParasiteHUD::DrawPanel(float X, float Y, float Width, float Height, const FLinearColor& Colour)
{
	DrawRect(Colour, X, Y, Width, Height);
	// Thin sci-fi edge.
	DrawRect(FLinearColor(0.3f, 0.9f, 0.9f, 0.35f), X, Y, Width, 2.f);
	DrawRect(FLinearColor(0.3f, 0.9f, 0.9f, 0.15f), X, Y + Height - 2.f, Width, 2.f);
}

void AParasiteHUD::DrawLabel(const FString& Text, float X, float Y, const FLinearColor& Colour, float Scale, bool bCentre)
{
	if (!Canvas)
	{
		return;
	}
	float DrawX = X;
	if (bCentre)
	{
		float TextWidth = 0.f, TextHeight = 0.f;
		Canvas->TextSize(HUDFont ? HUDFont : GEngine->GetMediumFont(), Text, TextWidth, TextHeight, Scale, Scale);
		DrawX = X - TextWidth * 0.5f;
	}
	DrawText(Text, Colour, DrawX, Y, HUDFont, Scale, false);
}

void AParasiteHUD::AddButton(const FString& Label, float X, float Y, float Width, float Height, int32 Id)
{
	FParasiteButton Button;
	Button.Label = Label;
	Button.Position = FVector2D(X, Y);
	Button.Size = FVector2D(Width, Height);
	Button.Id = Id;
	Buttons.Add(Button);

	// Hover highlight.
	float MouseX = 0.f, MouseY = 0.f;
	bool bHover = false;
	if (AParasitePlayerController* PC = GetOwningController())
	{
		if (PC->GetMousePosition(MouseX, MouseY))
		{
			bHover = MouseX >= X && MouseX <= X + Width && MouseY >= Y && MouseY <= Y + Height;
		}
	}
	DrawPanel(X, Y, Width, Height, bHover ? FLinearColor(0.1f, 0.35f, 0.35f, 0.9f) : ColPanel);
	DrawLabel(Label, X + Width * 0.5f, Y + Height * 0.5f - 10.f, bHover ? ColAccent : ColText, 1.2f, true);
}

void AParasiteHUD::DrawBar(float X, float Y, float Width, float Height, float Fraction, const FLinearColor& Colour)
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), X, Y, Width, Height);
	DrawRect(Colour, X + 2.f, Y + 2.f, FMath::Clamp(Fraction, 0.f, 1.f) * (Width - 4.f), Height - 4.f);
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

	// One place decides whether the mouse is free: front end, pause menu or the
	// end-of-match screen.
	if (AParasitePlayerController* PC = GetOwningController())
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
	DrawMessages();
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
	if (AParasitePlayerController* PC = GetOwningController())
	{
		if (PC->bMenuOpen)
		{
			// In-match pause menu.
			const float PanelW = 340.f;
			const float PanelX = ViewX * 0.5f - PanelW * 0.5f;
			DrawPanel(PanelX - 20.f, ViewY * 0.32f - 30.f, PanelW + 40.f, 260.f, FLinearColor(0.02f, 0.05f, 0.07f, 0.9f));
			DrawLabel(TEXT("PARASITE"), ViewX * 0.5f, ViewY * 0.32f - 20.f, ColAccent, 1.6f, true);
			AddButton(TEXT("RESUME"), PanelX, ViewY * 0.32f + 30.f, PanelW, 46.f, Btn_Resume);
			AddButton(TEXT("SETTINGS"), PanelX, ViewY * 0.32f + 86.f, PanelW, 46.f, Btn_Settings);
			AddButton(TEXT("QUIT"), PanelX, ViewY * 0.32f + 142.f, PanelW, 46.f, Btn_Quit);
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

	// Team A attacks nest B, so "TEAM A INFECTION" is progress made by team A.
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
	const AParasitePlayerController* PC = GetOwningController();
	if (!PS || !PC || !GetWorld())
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	const float PanelH = 118.f;
	const float PanelY = ViewY - PanelH - 16.f;
	DrawPanel(20.f, PanelY, 460.f, PanelH, ColPanel);

	// Team + DNA.
	const bool bTeamA = PS->GetTeam() == EParasiteTeam::TeamA;
	DrawLabel(bTeamA ? TEXT("TEAM A") : (PS->GetTeam() == EParasiteTeam::TeamB ? TEXT("TEAM B") : TEXT("NO TEAM")),
		36.f, PanelY + 10.f, bTeamA ? ColTeamA : ColTeamB, 1.2f);
	DrawLabel(FString::Printf(TEXT("DNA %d"), PS->DNA), 160.f, PanelY + 10.f, ColAccent, 1.2f);

	// Cooldowns.
	const float PossessLeft = FMath::Max(0.f, PS->PossessAvailableTime - Now);
	const float ScanLeft = FMath::Max(0.f, PS->ScanAvailableTime - Now);
	DrawLabel(TEXT("POSSESS [E]"), 36.f, PanelY + 40.f, PossessLeft > 0.f ? ColDim : ColText);
	DrawBar(180.f, PanelY + 42.f, 120.f, 10.f, PossessLeft > 0.f ? 1.f - PossessLeft / ParasiteRules::PossessCooldown : 1.f,
		PossessLeft > 0.f ? ColDim : ColAccent);

	DrawLabel(TEXT("SCAN [F]"), 36.f, PanelY + 62.f, ScanLeft > 0.f ? ColDim : ColText);
	DrawBar(180.f, PanelY + 64.f, 120.f, 10.f, ScanLeft > 0.f ? 1.f - ScanLeft / ParasiteRules::ScanCooldown : 1.f,
		ScanLeft > 0.f ? ColDim : ColAccent);

	// Host readout.
	FString HostLine = TEXT("HOST: NONE (PARASITE)");
	float HostFraction = 0.f;
	if (const AActor* Host = PS->CurrentHost.Get())
	{
		if (const UPossessableComponent* Comp = Host->FindComponentByClass<UPossessableComponent>())
		{
			const float Remaining = FMath::Max(0.f, Comp->PossessionEndTime - Now);
			const float MaxDuration = FMath::Max(1.f, Comp->GetMaxDuration(PS));
			HostFraction = Remaining / MaxDuration;
			HostLine = FString::Printf(TEXT("POSSESSED: %s  %.1fs"), *Comp->HostDisplayName, Remaining);
		}
	}
	DrawLabel(HostLine, 36.f, PanelY + 86.f, PS->CurrentHost ? ColWarn : ColDim, 1.1f);
	if (PS->CurrentHost)
	{
		DrawBar(320.f, PanelY + 88.f, 120.f, 10.f, HostFraction, ColWarn);
	}

	// Upgrades.
	const float UpgradeY = PanelY - 30.f;
	FString UpgradeLine = FString::Printf(TEXT("[1] JUMPER  [2] MIMIC  [3] INFILTRATOR   (%d DNA each, %d/%d used)"),
		ParasiteRules::UpgradeCost, PS->Upgrades.Num(), ParasiteRules::MaxUpgrades);
	DrawLabel(UpgradeLine, 24.f, UpgradeY, ColDim);

	// Being ridden warning.
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
	if (!Canvas || !GetWorld())
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

void AParasiteHUD::DrawMessages()
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
		const FLinearColor Colour = (PS->GetTeam() == EParasiteTeam::TeamA) ? ColTeamA : ColTeamB;
		DrawLabel(FString::Printf(TEXT("%-24s   %s    %4d    %4d"),
			*PS->GetPlayerName().Left(24),
			PS->GetTeam() == EParasiteTeam::TeamA ? TEXT("A") : TEXT("B"),
			PS->LifetimeDNA, PS->InfectionTicks), PanelX + 20.f, Row, Colour);
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

	FString Title;
	FLinearColor Colour = ColText;
	if (GS->WinningTeam == EParasiteTeam::None)
	{
		Title = TEXT("DRAW");
	}
	else
	{
		const AParasitePlayerState* PS = GetOwningState();
		const bool bWon = PS && PS->GetTeam() == GS->WinningTeam;
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

	AddButton(TEXT("PLAY"), ButtonX, Y, ButtonW, 48.f, Btn_Play);					Y += 58.f;
	AddButton(TEXT("HOST"), ButtonX, Y, ButtonW, 48.f, Btn_Host);					Y += 58.f;
	AddButton(TEXT("JOIN"), ButtonX, Y, ButtonW, 48.f, Btn_Join);					Y += 58.f;
	AddButton(TEXT("SETTINGS"), ButtonX, Y, ButtonW, 48.f, Btn_Settings);			Y += 58.f;
	AddButton(TEXT("QUIT"), ButtonX, Y, ButtonW, 48.f, Btn_Quit);					Y += 66.f;

	if (bJoinEditing)
	{
		DrawLabel(FString::Printf(TEXT("SERVER: %s_"), *JoinAddress), ViewX * 0.5f, Y, ColText, 1.4f, true);
		DrawLabel(TEXT("type an address, ENTER or CONNECT to join"), ViewX * 0.5f, Y + 26.f, ColDim, 1.f, true);
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

	const float ButtonW = 150.f;
	AddButton(TEXT("-"), ViewX * 0.5f - ButtonW - 10.f, ViewY * 0.4f, ButtonW, 44.f, Btn_SensDown);
	AddButton(TEXT("+"), ViewX * 0.5f + 10.f, ViewY * 0.4f, ButtonW, 44.f, Btn_SensUp);

	DrawLabel(TEXT("CONTROLS"), ViewX * 0.5f, ViewY * 0.52f, ColAccent, 1.4f, true);
	const TCHAR* Lines[] = {
		TEXT("WASD move   SHIFT sprint   CTRL crouch   SPACE jump / open door"),
		TEXT("E possess   Q exit host   LMB parasite leap   F scan   MMB ping"),
		TEXT("R resist a hijack   TAB scoreboard   1/2/3 evolve   ESC menu")
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Lines); ++Index)
	{
		DrawLabel(Lines[Index], ViewX * 0.5f, ViewY * 0.56f + Index * 22.f, ColDim, 1.1f, true);
	}

	AddButton(TEXT("BACK"), ViewX * 0.5f - 160.f, ViewY * 0.74f, 320.f, 46.f, Btn_Back);
}

// ---------------------------------------------------------------------------
// Interaction
// ---------------------------------------------------------------------------

bool AParasiteHUD::IsMenuActive() const
{
	if (bMainMenuOpen)
	{
		return true;
	}
	const AParasitePlayerController* PC = GetOwningController();
	if (PC && PC->bMenuOpen)
	{
		return true;
	}
	const AParasiteGameState* GS = GetParasiteGameState();
	return GS && GS->Phase == EMatchPhase::PostMatch;
}

void AParasiteHUD::HandleMenuClick()
{
	AParasitePlayerController* PC = GetOwningController();
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
			bMainMenuOpen = false;
			bJoinEditing = false;
			PC->bMenuOpen = false;
			PC->bShowMouseCursor = false;
			PC->SetInputMode(FInputModeGameOnly());
			break;

		case Btn_Host:
			// Listen server on the current map: everyone else uses JOIN.
			PC->ConsoleCommand(TEXT("open /Engine/Maps/Entry?listen"), true);
			bMainMenuOpen = false;
			PC->bMenuOpen = false;
			PC->bShowMouseCursor = false;
			PC->SetInputMode(FInputModeGameOnly());
			break;

		case Btn_Join:
			bJoinEditing = true;
			break;

		case Btn_JoinConfirm:
			PC->ClientTravel(JoinAddress, ETravelType::TRAVEL_Absolute);
			bMainMenuOpen = false;
			bJoinEditing = false;
			PC->bMenuOpen = false;
			PC->bShowMouseCursor = false;
			PC->SetInputMode(FInputModeGameOnly());
			break;

		case Btn_Settings:
			bSettingsOpen = true;
			bMainMenuOpen = true;
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
		if (AParasitePlayerController* PC = GetOwningController())
		{
			PC->ClientTravel(JoinAddress, ETravelType::TRAVEL_Absolute);
			bMainMenuOpen = false;
			bJoinEditing = false;
			PC->bMenuOpen = false;
			PC->bShowMouseCursor = false;
			PC->SetInputMode(FInputModeGameOnly());
		}
		return true;
	}

	const FString Name = Key.GetFName().ToString();
	if (Name.Len() == 1 && (FChar::IsDigit(Name[0]) || Name[0] == TEXT('.')))
	{
		JoinAddress.AppendChar(Name[0]);
		return true;
	}
	// Number keys report as "One", "Two", ... and the period as "Period"/"Decimal".
	static const TCHAR* Words[] = { TEXT("Zero"), TEXT("One"), TEXT("Two"), TEXT("Three"), TEXT("Four"),
		TEXT("Five"), TEXT("Six"), TEXT("Seven"), TEXT("Eight"), TEXT("Nine") };
	for (int32 Digit = 0; Digit < UE_ARRAY_COUNT(Words); ++Digit)
	{
		if (Name == Words[Digit] || Name == FString::Printf(TEXT("NumPad%s"), Words[Digit]))
		{
			JoinAddress.AppendChar(TEXT('0') + Digit);
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
