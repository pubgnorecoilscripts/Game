#include "ParasitePlayerController.h"
#include "ParasiteCharacter.h"
#include "ParasitePlayerState.h"
#include "ParasiteGameMode.h"
#include "ParasiteHUD.h"
#include "ParasiteNPC.h"
#include "PossessablePawn.h"
#include "ParasiteAudio.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

AParasitePlayerController::AParasitePlayerController()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
}

void AParasitePlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AParasitePlayerController, ParasiteBody, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AParasitePlayerController, HijackVictim, COND_OwnerOnly);
}

void AParasitePlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		// The front end takes the mouse until the player picks something; see
		// AParasiteHUD, which owns cursor state from here on.
		bMenuOpen = true;
	}
}

AParasitePlayerState* AParasitePlayerController::GetParasitePlayerState() const
{
	return GetPlayerState<AParasitePlayerState>();
}

AParasiteHUD* AParasitePlayerController::GetParasiteHUD() const
{
	return Cast<AParasiteHUD>(GetHUD());
}

bool AParasitePlayerController::IsInputBlocked() const
{
	if (bMenuOpen)
	{
		return true;
	}
	const AParasiteHUD* HUD = GetParasiteHUD();
	return HUD && HUD->IsMenuActive();
}

void AParasitePlayerController::SetParasiteBody(AParasiteCharacter* Body)
{
	if (HasAuthority())
	{
		ParasiteBody = Body;
	}
}

void AParasitePlayerController::SetHijackVictim(AParasiteCharacter* Victim)
{
	if (HasAuthority())
	{
		HijackVictim = Victim;
	}
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void AParasitePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (!InputComponent)
	{
		return;
	}

	InputComponent->BindAxis(TEXT("MoveForward"), this, &AParasitePlayerController::OnMoveForward);
	InputComponent->BindAxis(TEXT("MoveRight"), this, &AParasitePlayerController::OnMoveRight);
	InputComponent->BindAxis(TEXT("Turn"), this, &AParasitePlayerController::OnTurn);
	InputComponent->BindAxis(TEXT("LookUp"), this, &AParasitePlayerController::OnLookUp);
	InputComponent->BindAxis(TEXT("TurnRate"), this, &AParasitePlayerController::OnTurnRate);
	InputComponent->BindAxis(TEXT("LookUpRate"), this, &AParasitePlayerController::OnLookUpRate);

	InputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &AParasitePlayerController::OnJumpPressed);
	InputComponent->BindAction(TEXT("Jump"), IE_Released, this, &AParasitePlayerController::OnJumpReleased);
	InputComponent->BindAction(TEXT("Sprint"), IE_Pressed, this, &AParasitePlayerController::OnSprintPressed);
	InputComponent->BindAction(TEXT("Sprint"), IE_Released, this, &AParasitePlayerController::OnSprintReleased);
	InputComponent->BindAction(TEXT("Crouch"), IE_Pressed, this, &AParasitePlayerController::OnCrouchToggle);
	InputComponent->BindAction(TEXT("Interact"), IE_Pressed, this, &AParasitePlayerController::OnInteractPressed);
	InputComponent->BindAction(TEXT("ExitPossession"), IE_Pressed, this, &AParasitePlayerController::OnExitPressed);
	InputComponent->BindAction(TEXT("ParasiteLeap"), IE_Pressed, this, &AParasitePlayerController::OnLeapPressed);
	InputComponent->BindAction(TEXT("Scan"), IE_Pressed, this, &AParasitePlayerController::OnScanPressed);
	InputComponent->BindAction(TEXT("Ping"), IE_Pressed, this, &AParasitePlayerController::OnPingPressed);
	InputComponent->BindAction(TEXT("Resist"), IE_Pressed, this, &AParasitePlayerController::OnResistPressed);
	InputComponent->BindAction(TEXT("Scoreboard"), IE_Pressed, this, &AParasitePlayerController::OnScoreboardPressed);
	InputComponent->BindAction(TEXT("Scoreboard"), IE_Released, this, &AParasitePlayerController::OnScoreboardReleased);
	InputComponent->BindAction(TEXT("Upgrade1"), IE_Pressed, this, &AParasitePlayerController::OnUpgrade1);
	InputComponent->BindAction(TEXT("Upgrade2"), IE_Pressed, this, &AParasitePlayerController::OnUpgrade2);
	InputComponent->BindAction(TEXT("Upgrade3"), IE_Pressed, this, &AParasitePlayerController::OnUpgrade3);
	InputComponent->BindAction(TEXT("MenuToggle"), IE_Pressed, this, &AParasitePlayerController::OnMenuToggle);
	InputComponent->BindAction(TEXT("MenuClick"), IE_Pressed, this, &AParasitePlayerController::OnMenuClick);
}

void AParasitePlayerController::PollAddressTyping()
{
	AParasiteHUD* HUD = GetParasiteHUD();
	if (!HUD || !HUD->bJoinEditing)
	{
		return;
	}
	// Only the handful of keys an address can contain. Polling rather than
	// overriding InputKey keeps this working on every engine version, since that
	// override's signature has changed between them.
	static const FKey AddressKeys[] = {
		EKeys::Zero, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four,
		EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine,
		EKeys::NumPadZero, EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour,
		EKeys::NumPadFive, EKeys::NumPadSix, EKeys::NumPadSeven, EKeys::NumPadEight, EKeys::NumPadNine,
		EKeys::Period, EKeys::Decimal, EKeys::Semicolon, EKeys::BackSpace, EKeys::Enter
	};
	for (const FKey& Key : AddressKeys)
	{
		if (WasInputKeyJustPressed(Key))
		{
			HUD->HandleTextInput(Key);
		}
	}
}

void AParasitePlayerController::OnMoveForward(float Value)
{
	CachedForward = Value;
}

void AParasitePlayerController::OnMoveRight(float Value)
{
	CachedRight = Value;
}

void AParasitePlayerController::OnTurn(float Value)
{
	if (!IsInputBlocked())
	{
		AddYawInput(Value);
		CachedYawDelta += Value;
	}
}

void AParasitePlayerController::OnLookUp(float Value)
{
	if (!IsInputBlocked())
	{
		AddPitchInput(Value);
	}
}

void AParasitePlayerController::OnTurnRate(float Value)
{
	if (!IsInputBlocked() && GetWorld())
	{
		const float Delta = Value * 120.f * GetWorld()->GetDeltaSeconds();
		AddYawInput(Delta);
		CachedYawDelta += Delta;
	}
}

void AParasitePlayerController::OnLookUpRate(float Value)
{
	if (!IsInputBlocked() && GetWorld())
	{
		AddPitchInput(Value * 90.f * GetWorld()->GetDeltaSeconds());
	}
}

void AParasitePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (!IsLocalController())
	{
		return;
	}
	PollAddressTyping();

	const bool bBlocked = IsInputBlocked();
	const float Forward = bBlocked ? 0.f : CachedForward;
	const float Right = bBlocked ? 0.f : CachedRight;

	if (HijackVictim)
	{
		// Driving somebody else's parasite: entirely server authoritative.
		ServerHijackInput(Forward, Right, CachedYawDelta * 2.f);
	}
	else if (Cast<APossessablePawn>(GetPawn()))
	{
		// Props use FloatingPawnMovement, which has no client prediction, so the
		// server moves them instead of trusting a local simulation.
		if (!FMath::IsNearlyZero(Forward) || !FMath::IsNearlyZero(Right))
		{
			ServerDriveHost(Forward, Right);
		}
	}
	else if (AParasiteNPC* NPC = Cast<AParasiteNPC>(GetPawn()))
	{
		NPC->DriveForward(Forward);
		NPC->DriveRight(Right);
	}
	else if (AParasiteCharacter* Parasite = Cast<AParasiteCharacter>(GetPawn()))
	{
		if (!Parasite->bHijacked)
		{
			const FRotationMatrix Yaw(FRotator(0.f, GetControlRotation().Yaw, 0.f));
			Parasite->AddMovementInput(Yaw.GetUnitAxis(EAxis::X), Forward);
			Parasite->AddMovementInput(Yaw.GetUnitAxis(EAxis::Y), Right);
		}
	}
	CachedYawDelta = 0.f;
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

void AParasitePlayerController::OnJumpPressed()
{
	if (IsInputBlocked())
	{
		return;
	}
	if (AParasiteCharacter* Parasite = Cast<AParasiteCharacter>(GetPawn()))
	{
		if (!Parasite->bHijacked)
		{
			Parasite->Jump();
		}
	}
	else if (ACharacter* Character = Cast<ACharacter>(GetPawn()))
	{
		Character->Jump();
	}
	else if (APossessablePawn* Prop = Cast<APossessablePawn>(GetPawn()))
	{
		Prop->ToggleHinge();		// a possessed door swings itself
	}
}

void AParasitePlayerController::OnJumpReleased()
{
	if (ACharacter* Character = Cast<ACharacter>(GetPawn()))
	{
		Character->StopJumping();
	}
}

void AParasitePlayerController::OnSprintPressed()
{
	if (AParasiteCharacter* Parasite = Cast<AParasiteCharacter>(GetPawn()))
	{
		Parasite->SetSprinting(true);
	}
}

void AParasitePlayerController::OnSprintReleased()
{
	if (AParasiteCharacter* Parasite = Cast<AParasiteCharacter>(GetPawn()))
	{
		Parasite->SetSprinting(false);
	}
}

void AParasitePlayerController::OnCrouchToggle()
{
	if (ACharacter* Character = Cast<ACharacter>(GetPawn()))
	{
		if (Character->bIsCrouched)
		{
			Character->UnCrouch();
		}
		else
		{
			Character->Crouch();
		}
	}
}

void AParasitePlayerController::OnInteractPressed()
{
	if (!IsInputBlocked())
	{
		ServerRequestInteract();
	}
}

void AParasitePlayerController::OnExitPressed()
{
	if (!IsInputBlocked())
	{
		ServerRequestExit();
	}
}

void AParasitePlayerController::OnLeapPressed()
{
	if (!IsInputBlocked())
	{
		ServerRequestLeap(FRotationMatrix(GetControlRotation()).GetUnitAxis(EAxis::X));
	}
}

void AParasitePlayerController::OnScanPressed()
{
	if (!IsInputBlocked())
	{
		ServerRequestScan();
	}
}

void AParasitePlayerController::OnPingPressed()
{
	if (IsInputBlocked() || !GetWorld())
	{
		return;
	}
	// Ping whatever the player is looking at, or a point out in front of them.
	const AActor* Body = GetPawn() ? Cast<AActor>(GetPawn()) : Cast<AActor>(ParasiteBody.Get());
	const FVector Origin = (Body ? Body->GetActorLocation() : GetFocalLocation()) + FVector(0.f, 0.f, 40.f);
	const FVector Forward = FRotationMatrix(GetControlRotation()).GetUnitAxis(EAxis::X);

	FVector Target = Origin + Forward * 1200.f;
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, Target, ECC_Visibility))
	{
		Target = Hit.ImpactPoint;
	}
	ServerRequestPing(Target);
}

void AParasitePlayerController::OnResistPressed()
{
	ServerRequestResist();
}

void AParasitePlayerController::OnScoreboardPressed()
{
	if (AParasiteHUD* HUD = GetParasiteHUD())
	{
		HUD->bShowScoreboard = true;
	}
}

void AParasitePlayerController::OnScoreboardReleased()
{
	if (AParasiteHUD* HUD = GetParasiteHUD())
	{
		HUD->bShowScoreboard = false;
	}
}

void AParasitePlayerController::OnUpgrade1() { ServerRequestUpgrade(EParasiteUpgrade::Jumper); }
void AParasitePlayerController::OnUpgrade2() { ServerRequestUpgrade(EParasiteUpgrade::Mimic); }
void AParasitePlayerController::OnUpgrade3() { ServerRequestUpgrade(EParasiteUpgrade::Infiltrator); }

void AParasitePlayerController::OnMenuToggle()
{
	if (AParasiteHUD* HUD = GetParasiteHUD())
	{
		if (HUD->bMainMenuOpen)
		{
			// Escape backs out of settings, but never out of the front end itself.
			HUD->bSettingsOpen = false;
			return;
		}
	}
	bMenuOpen = !bMenuOpen;
	FParasiteAudio::Play2D(this, EParasiteSound::UIClick);
}

void AParasitePlayerController::OnMenuClick()
{
	if (AParasiteHUD* HUD = GetParasiteHUD())
	{
		if (HUD->IsMenuActive())
		{
			HUD->HandleMenuClick();
		}
	}
}

// ---------------------------------------------------------------------------
// Server RPCs: each one hands straight to the game mode, which owns the rules
// ---------------------------------------------------------------------------

void AParasitePlayerController::ServerRequestInteract_Implementation()
{
	if (AParasiteGameMode* GameMode = AParasiteGameMode::Get(this))
	{
		GameMode->RequestInteract(this);
	}
}

void AParasitePlayerController::ServerRequestExit_Implementation()
{
	if (AParasiteGameMode* GameMode = AParasiteGameMode::Get(this))
	{
		GameMode->RequestExit(this);
	}
}

void AParasitePlayerController::ServerRequestLeap_Implementation(FVector_NetQuantize Direction)
{
	if (AParasiteGameMode* GameMode = AParasiteGameMode::Get(this))
	{
		GameMode->RequestLeap(this, Direction);
	}
}

void AParasitePlayerController::ServerRequestScan_Implementation()
{
	if (AParasiteGameMode* GameMode = AParasiteGameMode::Get(this))
	{
		GameMode->RequestScan(this);
	}
}

void AParasitePlayerController::ServerRequestPing_Implementation(FVector_NetQuantize Location)
{
	if (AParasiteGameMode* GameMode = AParasiteGameMode::Get(this))
	{
		GameMode->RequestPing(this, Location);
	}
}

void AParasitePlayerController::ServerRequestResist_Implementation()
{
	if (AParasiteGameMode* GameMode = AParasiteGameMode::Get(this))
	{
		GameMode->RequestResist(this);
	}
}

void AParasitePlayerController::ServerRequestUpgrade_Implementation(EParasiteUpgrade Upgrade)
{
	if (AParasiteGameMode* GameMode = AParasiteGameMode::Get(this))
	{
		GameMode->RequestUpgrade(this, Upgrade);
	}
}

void AParasitePlayerController::ServerRequestRematch_Implementation()
{
	if (AParasiteGameMode* GameMode = AParasiteGameMode::Get(this))
	{
		GameMode->RequestRematch();
	}
}

void AParasitePlayerController::ServerDriveHost_Implementation(float Forward, float Right)
{
	if (APossessablePawn* Prop = Cast<APossessablePawn>(GetPawn()))
	{
		Prop->DriveForward(Forward);
		Prop->DriveRight(Right);
	}
}

void AParasitePlayerController::ServerHijackInput_Implementation(float Forward, float Right, float YawDelta)
{
	if (AParasiteCharacter* Victim = HijackVictim.Get())
	{
		Victim->ApplyHijackInput(Forward, Right, YawDelta);
	}
}

// ---------------------------------------------------------------------------
// Client feedback
// ---------------------------------------------------------------------------

void AParasitePlayerController::ClientShowNotice_Implementation(const FString& Message, float Duration)
{
	if (AParasiteHUD* HUD = GetParasiteHUD())
	{
		HUD->ShowMessage(Message, Duration);
	}
}

void AParasitePlayerController::ClientPlayCue_Implementation(uint8 Sound)
{
	FParasiteAudio::Play2D(this, static_cast<EParasiteSound>(Sound));
}

void AParasitePlayerController::ClientAddMarker_Implementation(FVector Location, FColor Colour, const FString& Label, float Duration)
{
	if (AParasiteHUD* HUD = GetParasiteHUD())
	{
		HUD->AddMarker(Location, Colour, Label, Duration);
	}
}

void AParasitePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (AParasiteCharacter* Parasite = Cast<AParasiteCharacter>(InPawn))
	{
		SetParasiteBody(Parasite);
	}
}
