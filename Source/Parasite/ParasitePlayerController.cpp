#include "ParasitePlayerController.h"
#include "ParasiteCharacter.h"
#include "ParasitePlayerState.h"
#include "ParasiteGameState.h"
#include "ParasiteGameMode.h"
#include "ParasiteHUD.h"
#include "ParasiteNPC.h"
#include "ParasiteNest.h"
#include "PossessablePawn.h"
#include "PossessableComponent.h"
#include "ParasiteAudio.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
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
		bShowMouseCursor = false;
		SetInputMode(FInputModeGameOnly());
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

bool AParasitePlayerController::IsPossessing() const
{
	const AParasitePlayerState* PS = GetParasitePlayerState();
	return PS && PS->CurrentHost != nullptr;
}

AActor* AParasitePlayerController::GetBodyActor() const
{
	if (const AParasitePlayerState* PS = GetParasitePlayerState())
	{
		if (PS->CurrentHost)
		{
			return PS->CurrentHost.Get();
		}
	}
	return ParasiteBody ? Cast<AActor>(ParasiteBody.Get()) : Cast<AActor>(GetPawn());
}

void AParasitePlayerController::SetParasiteBody(AParasiteCharacter* Body)
{
	if (HasAuthority())
	{
		ParasiteBody = Body;
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
	InputComponent->BindAction(TEXT("Possess"), IE_Pressed, this, &AParasitePlayerController::OnPossessPressed);
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

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
bool AParasitePlayerController::InputKey(const FInputKeyEventArgs& Params)
#else
bool AParasitePlayerController::InputKey(const FInputKeyParams& Params)
#endif
{
	// While the join field is open the keyboard belongs to it.
	if (Params.Event == IE_Pressed)
	{
		if (AParasiteHUD* HUD = GetParasiteHUD())
		{
			if (HUD->HandleTextInput(Params.Key))
			{
				return true;
			}
		}
	}
	return Super::InputKey(Params);
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
	if (!IsInputBlocked())
	{
		const float Delta = Value * 120.f * GetWorld()->GetDeltaSeconds();
		AddYawInput(Delta);
		CachedYawDelta += Delta;
	}
}

void AParasitePlayerController::OnLookUpRate(float Value)
{
	if (!IsInputBlocked())
	{
		AddPitchInput(Value * 90.f * GetWorld()->GetDeltaSeconds());
	}
}

void AParasitePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	const bool bBlocked = IsInputBlocked();
	const float Forward = bBlocked ? 0.f : CachedForward;
	const float Right = bBlocked ? 0.f : CachedRight;

	if (IsLocalController())
	{
		if (HijackVictim)
		{
			// Driving somebody else's parasite: pure server authority.
			ServerHijackInput(Forward, Right, CachedYawDelta * 2.f);
		}
		else if (APossessablePawn* Prop = Cast<APossessablePawn>(GetPawn()))
		{
			// Props have no client prediction, so the server moves them.
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
				const FRotator Yaw(0.f, GetControlRotation().Yaw, 0.f);
				Parasite->AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Forward);
				Parasite->AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Right);
			}
		}
	}
	CachedYawDelta = 0.f;

}

// ---------------------------------------------------------------------------
// Simple actions
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
	else if (ACharacter* Char = Cast<ACharacter>(GetPawn()))
	{
		Char->Jump();
	}
	else if (APossessablePawn* Prop = Cast<APossessablePawn>(GetPawn()))
	{
		// Doors swing, everything else gives a little hop of shame.
		Prop->ToggleHinge();
	}
}

void AParasitePlayerController::OnJumpReleased()
{
	if (ACharacter* Char = Cast<ACharacter>(GetPawn()))
	{
		Char->StopJumping();
	}
}

void AParasitePlayerController::OnSprintPressed()
{
	bSprinting = true;
	if (AParasiteCharacter* Parasite = Cast<AParasiteCharacter>(GetPawn()))
	{
		Parasite->SetSprinting(true);
	}
}

void AParasitePlayerController::OnSprintReleased()
{
	bSprinting = false;
	if (AParasiteCharacter* Parasite = Cast<AParasiteCharacter>(GetPawn()))
	{
		Parasite->SetSprinting(false);
	}
}

void AParasitePlayerController::OnCrouchToggle()
{
	if (ACharacter* Char = Cast<ACharacter>(GetPawn()))
	{
		if (Char->bIsCrouched)
		{
			Char->UnCrouch();
		}
		else
		{
			Char->Crouch();
		}
	}
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
			// Escape backs out of settings but never out of the front end itself.
			HUD->bSettingsOpen = false;
			return;
		}
	}
	bMenuOpen = !bMenuOpen;
	bShowMouseCursor = bMenuOpen;
	if (bMenuOpen)
	{
		SetInputMode(FInputModeGameAndUI());
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
	}
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
// Possession
// ---------------------------------------------------------------------------

void AParasitePlayerController::OnPossessPressed()
{
	if (IsInputBlocked())
	{
		return;
	}
	ServerRequestPossess();
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
	if (IsInputBlocked())
	{
		return;
	}
	const FRotator Rot = GetControlRotation();
	ServerRequestLeap(FRotationMatrix(Rot).GetUnitAxis(EAxis::X));
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
	if (IsInputBlocked())
	{
		return;
	}
	// Ping whatever the player is looking at, or a point ahead of them.
	FHitResult Hit;
	const AActor* Body = GetBodyActor();
	const FVector Origin = Body ? Body->GetActorLocation() : GetFocalLocation();
	const FVector Forward = FRotationMatrix(GetControlRotation()).GetUnitAxis(EAxis::X);
	FVector PingAt = Origin + Forward * 1200.f;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Origin + FVector(0, 0, 40.f), PingAt, ECC_Visibility))
	{
		PingAt = Hit.ImpactPoint;
	}
	ServerRequestPing(PingAt);
}

void AParasitePlayerController::OnResistPressed()
{
	ServerRequestResist();
}

UPossessableComponent* AParasitePlayerController::FindBestTarget(float& OutDistance) const
{
	OutDistance = MAX_flt;
	const AParasitePlayerState* PS = GetParasitePlayerState();
	const AActor* Body = ParasiteBody ? Cast<AActor>(ParasiteBody.Get()) : nullptr;
	if (!PS || !Body || !GetWorld())
	{
		return nullptr;
	}

	const float Range = PS->GetPossessRange();
	const FVector Origin = Body->GetActorLocation();
	const FVector Forward = FRotationMatrix(GetControlRotation()).GetUnitAxis(EAxis::X);

	UPossessableComponent* Best = nullptr;
	float BestScore = -MAX_flt;

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor == Body || !IsValid(Actor))
		{
			continue;
		}
		UPossessableComponent* Comp = Actor->FindComponentByClass<UPossessableComponent>();
		if (!Comp || !Comp->CanBePossessedBy(PS))
		{
			continue;
		}
		// Never possess your own body or a team mate's parasite.
		if (const AParasiteCharacter* AsParasite = Cast<AParasiteCharacter>(Actor))
		{
			const AParasitePlayerState* OtherPS = AsParasite->GetParasitePlayerState();
			if (!OtherPS || OtherPS->GetTeam() == PS->GetTeam() || AsParasite->bDormant)
			{
				continue;
			}
		}

		const FVector ToTarget = Actor->GetActorLocation() - Origin;
		const float Distance = ToTarget.Size();
		if (Distance > Range)
		{
			continue;
		}
		// Prefer what the player is looking at.
		const float Score = FVector::DotProduct(ToTarget.GetSafeNormal(), Forward) * 100.f - Distance * 0.1f;
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Comp;
			OutDistance = Distance;
		}
	}
	return Best;
}

void AParasitePlayerController::ServerRequestPossess_Implementation()
{
	AParasitePlayerState* PS = GetParasitePlayerState();
	AParasiteGameState* GS = GetWorld() ? GetWorld()->GetGameState<AParasiteGameState>() : nullptr;
	if (!PS || !GS || GS->Phase != EMatchPhase::InProgress)
	{
		return;
	}
	if (PS->CurrentHost)
	{
		ClientNotify(TEXT("ALREADY POSSESSING"), 1.5f);
		ClientPlaySound(static_cast<uint8>(EParasiteSound::PossessFail));
		return;
	}
	if (!PS->IsPossessCooldownReady())
	{
		ClientNotify(TEXT("POSSESSION ON COOLDOWN"), 1.5f);
		ClientPlaySound(static_cast<uint8>(EParasiteSound::PossessFail));
		return;
	}
	if (ParasiteBody && ParasiteBody->bHijacked)
	{
		ClientNotify(TEXT("YOU ARE BEING RIDDEN"), 1.5f);
		return;
	}

	// Context sensitive: a door within reach is opened rather than possessed, so
	// disguised players can move through the mall without breaking character.
	if (AActor* Body = GetBodyActor())
	{
		const FVector Origin = Body->GetActorLocation();
		for (TActorIterator<APossessablePawn> It(GetWorld()); It; ++It)
		{
			APossessablePawn* Prop = *It;
			if (!IsValid(Prop) || Prop == Body || Prop->Possessable->Mobility != EHostMobility::Hinge)
			{
				continue;
			}
			if (FVector::DistSquared(Prop->GetActorLocation(), Origin) <= FMath::Square(260.f))
			{
				Prop->ToggleHinge();
				ClientNotify(TEXT("DOOR"), 1.f);
				return;
			}
		}
	}

	float Distance = 0.f;
	UPossessableComponent* Target = FindBestTarget(Distance);
	if (!Target)
	{
		ClientNotify(TEXT("NO HOST IN RANGE"), 1.5f);
		ClientPlaySound(static_cast<uint8>(EParasiteSound::PossessFail));
		return;
	}
	EnterHost(Target);
}

bool AParasitePlayerController::EnterHost(UPossessableComponent* Target)
{
	AParasitePlayerState* PS = GetParasitePlayerState();
	if (!HasAuthority() || !PS || !Target || !Target->GetOwner())
	{
		return false;
	}
	AActor* HostActor = Target->GetOwner();

	Target->BeginPossession(PS);
	PS->CurrentHost = HostActor;
	PS->AwardDNA(Target->GetDNAReward());

	if (AParasiteCharacter* Victim = Cast<AParasiteCharacter>(HostActor))
	{
		// Enemy player: the victim keeps their controller and their pawn. We only
		// take the wheel for a few seconds and watch through their eyes.
		Victim->bHijacked = true;
		Victim->ResistProgress = 0.f;
		Victim->OnRep_Hijacked();
		HijackVictim = Victim;

		if (ParasiteBody)
		{
			ParasiteBody->SetDormant(true, FVector::ZeroVector);
		}
		SetViewTargetWithBlend(Victim, 0.25f);

		if (AParasitePlayerController* VictimPC = Cast<AParasitePlayerController>(Victim->GetController()))
		{
			VictimPC->ClientNotify(TEXT("!! SOMETHING IS INSIDE YOU - MASH [R] TO RESIST !!"), 4.f);
			VictimPC->ClientPlaySound(static_cast<uint8>(EParasiteSound::Detected));
		}
	}
	else
	{
		// Objects, NPCs and vehicles: hand the pawn over for real.
		if (APawn* HostPawn = Cast<APawn>(HostActor))
		{
			if (ParasiteBody)
			{
				ParasiteBody->SetDormant(true, FVector::ZeroVector);
			}
			// An NPC is already driven by its AI controller: take the wheel from it.
			if (AController* Existing = HostPawn->GetController())
			{
				Existing->UnPossess();
			}
			UnPossess();
			Possess(HostPawn);
		}
	}

	ClientNotify(FString::Printf(TEXT("POSSESSED: %s"), *Target->HostDisplayName), 2.f);
	ClientPlaySound(static_cast<uint8>(EParasiteSound::Possess));
	FParasiteAudio::Play(this, EParasiteSound::Possess, HostActor->GetActorLocation());
	return true;
}

void AParasitePlayerController::ServerRequestExit_Implementation()
{
	ServerExitPossession(false);
}

void AParasitePlayerController::ServerExitPossession(bool bWasExpelled)
{
	if (!HasAuthority())
	{
		return;
	}
	AParasitePlayerState* PS = GetParasitePlayerState();
	if (!PS)
	{
		return;
	}
	AActor* HostActor = PS->CurrentHost.Get();
	FVector WakeLocation = ParasiteBody ? ParasiteBody->GetActorLocation() : FVector::ZeroVector;

	if (IsValid(HostActor))
	{
		WakeLocation = HostActor->GetActorLocation() + FVector(0.f, 0.f, 60.f);
		if (UPossessableComponent* Comp = HostActor->FindComponentByClass<UPossessableComponent>())
		{
			Comp->EndPossession();
		}
		if (AParasiteCharacter* Victim = Cast<AParasiteCharacter>(HostActor))
		{
			Victim->bHijacked = false;
			Victim->ResistProgress = 0.f;
			Victim->OnRep_Hijacked();
			// Pop out beside the victim rather than inside them.
			WakeLocation = Victim->GetActorLocation() + Victim->GetActorForwardVector() * -120.f + FVector(0.f, 0.f, 40.f);
		}
	}
	HijackVictim = nullptr;
	PS->CurrentHost = nullptr;
	PS->StartPossessCooldown();

	// Hand NPCs back to their own dim little brain.
	if (AParasiteNPC* NPC = Cast<AParasiteNPC>(HostActor))
	{
		if (GetPawn() == NPC)
		{
			UnPossess();
		}
		if (!NPC->GetController())
		{
			NPC->SpawnDefaultController();
		}
	}

	if (ParasiteBody && IsValid(ParasiteBody))
	{
		ParasiteBody->SetDormant(false, WakeLocation);
		if (GetPawn() != ParasiteBody)
		{
			UnPossess();
			Possess(ParasiteBody);
		}
		SetViewTargetWithBlend(ParasiteBody, 0.2f);
	}

	ClientNotify(bWasExpelled ? TEXT("EXPELLED!") : TEXT("LEFT HOST"), 1.5f);
	ClientPlaySound(static_cast<uint8>(EParasiteSound::PossessExit));
	if (IsValid(HostActor))
	{
		FParasiteAudio::Play(this, EParasiteSound::PossessExit, HostActor->GetActorLocation());
	}

	// Expelled inside an enemy nest: the defenders claw progress back.
	if (bWasExpelled && GetWorld())
	{
		for (TActorIterator<AParasiteNest> It(GetWorld()); It; ++It)
		{
			AParasiteNest* Nest = *It;
			if (Nest->OwningTeam != PS->GetTeam() &&
				FVector::DistSquared(Nest->GetActorLocation(), WakeLocation) <= FMath::Square(ParasiteRules::NestRadius * 1.5f))
			{
				Nest->ApplyExpulsion();
			}
		}
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
	if (HijackVictim && IsValid(HijackVictim))
	{
		HijackVictim->ApplyHijackInput(Forward, Right, YawDelta);
	}
}

void AParasitePlayerController::ServerRequestResist_Implementation()
{
	if (AParasiteCharacter* Body = ParasiteBody.Get())
	{
		if (Body->bHijacked && Body->AddResist(0.12f))
		{
			// Find whoever is riding us and throw them out.
			if (UPossessableComponent* Comp = Body->FindComponentByClass<UPossessableComponent>())
			{
				if (AParasitePlayerState* RiderPS = Comp->Rider.Get())
				{
					if (AParasitePlayerController* RiderPC = Cast<AParasitePlayerController>(RiderPS->GetOwner()))
					{
						RiderPC->ServerExitPossession(true);
					}
				}
			}
			if (AParasitePlayerState* PS = GetParasitePlayerState())
			{
				PS->AwardDNA(ParasiteRules::DNA_ExpelParasite);
			}
			ClientNotify(TEXT("YOU FORCED IT OUT"), 2.f);
		}
	}
}

void AParasitePlayerController::ServerRequestLeap_Implementation(FVector_NetQuantize Direction)
{
	AParasitePlayerState* PS = GetParasitePlayerState();
	if (!PS || !PS->IsLeapReady() || PS->CurrentHost)
	{
		return;
	}
	AParasiteCharacter* Body = ParasiteBody.Get();
	if (!Body || Body->bDormant || Body->bHijacked)
	{
		return;
	}

	// Leap towards the nearest host in leap range, otherwise straight ahead.
	FVector LaunchDir = FVector(Direction).GetSafeNormal();
	float BestDistSq = FMath::Square(ParasiteRules::LeapRange);
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor == Body || !IsValid(Actor))
		{
			continue;
		}
		UPossessableComponent* Comp = Actor->FindComponentByClass<UPossessableComponent>();
		if (!Comp || !Comp->CanBePossessedBy(PS))
		{
			continue;
		}
		const FVector ToTarget = Actor->GetActorLocation() - Body->GetActorLocation();
		if (ToTarget.SizeSquared() < BestDistSq && FVector::DotProduct(ToTarget.GetSafeNormal(), LaunchDir) > 0.35f)
		{
			BestDistSq = ToTarget.SizeSquared();
			LaunchDir = ToTarget.GetSafeNormal();
		}
	}

	Body->LaunchCharacter(LaunchDir * 900.f + FVector(0.f, 0.f, 420.f), true, true);
	PS->StartLeapCooldown();
	FParasiteAudio::Play(this, EParasiteSound::ParasiteMove, Body->GetActorLocation());
}

void AParasitePlayerController::ServerRequestScan_Implementation()
{
	AParasitePlayerState* PS = GetParasitePlayerState();
	if (!PS || !PS->IsScanReady())
	{
		return;
	}
	PS->StartScanCooldown();

	const AActor* Body = GetBodyActor();
	const FVector Origin = Body ? Body->GetActorLocation() : GetFocalLocation();
	FParasiteAudio::Play(this, EParasiteSound::ScanPulse, Origin);
	ClientPlaySound(static_cast<uint8>(EParasiteSound::ScanPulse));

	int32 Found = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		UPossessableComponent* Comp = Actor ? Actor->FindComponentByClass<UPossessableComponent>() : nullptr;
		if (!Comp || !Comp->bPossessed || !Comp->Rider)
		{
			continue;
		}
		if (Comp->Rider->GetTeam() == PS->GetTeam())
		{
			continue;		// only enemy infiltrators light up
		}
		// Infiltrator upgrade shrinks the radius they can be caught in.
		const float Radius = ParasiteRules::ScanRadius * Comp->Rider->GetStealthScale();
		if (FVector::DistSquared(Actor->GetActorLocation(), Origin) > FMath::Square(Radius))
		{
			continue;
		}

		++Found;
		ClientAddMarker(Actor->GetActorLocation(), FColor(255, 25, 150), TEXT("PARASITE"), ParasiteRules::ScanRevealTime);

		if (APossessablePawn* Prop = Cast<APossessablePawn>(Actor))
		{
			Prop->SetRevealed(ParasiteRules::ScanRevealTime);
		}
		else if (AParasiteNPC* NPC = Cast<AParasiteNPC>(Actor))
		{
			NPC->SetRevealed(ParasiteRules::ScanRevealTime);
		}

		// The hunted feel the ping too - that is the mind game.
		if (AParasitePlayerController* PreyPC = Cast<AParasitePlayerController>(Comp->Rider->GetOwner()))
		{
			PreyPC->ClientNotify(TEXT("YOU WERE SCANNED"), 1.5f);
			PreyPC->ClientPlaySound(static_cast<uint8>(EParasiteSound::Detected));
		}
	}
	ClientNotify(Found > 0 ? FString::Printf(TEXT("SCAN: %d CONTACT(S)"), Found) : TEXT("SCAN: CLEAR"), 1.8f);
}

void AParasitePlayerController::ServerRequestPing_Implementation(FVector_NetQuantize Location)
{
	const AParasitePlayerState* PS = GetParasitePlayerState();
	if (!PS || !GetWorld())
	{
		return;
	}
	// A player controller is only relevant to its own owner, so the server sends
	// the marker to each team mate individually.
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AParasitePlayerController* Other = Cast<AParasitePlayerController>(It->Get());
		const AParasitePlayerState* OtherPS = Other ? Other->GetParasitePlayerState() : nullptr;
		if (OtherPS && OtherPS->GetTeam() == PS->GetTeam())
		{
			Other->ClientAddMarker(Location, FColor(255, 220, 60), TEXT("PING"), 8.f);
			Other->ClientPlaySound(static_cast<uint8>(EParasiteSound::UIClick));
		}
	}
}

void AParasitePlayerController::ServerRequestUpgrade_Implementation(EParasiteUpgrade Upgrade)
{
	AParasitePlayerState* PS = GetParasitePlayerState();
	if (!PS)
	{
		return;
	}
	if (PS->TryPurchaseUpgrade(Upgrade))
	{
		ClientNotify(TEXT("EVOLVED"), 2.f);
		ClientPlaySound(static_cast<uint8>(EParasiteSound::Possess));
	}
	else
	{
		ClientNotify(TEXT("CANNOT EVOLVE (DNA / LIMIT)"), 2.f);
		ClientPlaySound(static_cast<uint8>(EParasiteSound::PossessFail));
	}
}

void AParasitePlayerController::ServerRequestRematch_Implementation()
{
	if (AParasiteGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AParasiteGameMode>() : nullptr)
	{
		GM->RestartMatch();
	}
}

// ---------------------------------------------------------------------------
// Client feedback
// ---------------------------------------------------------------------------

void AParasitePlayerController::ClientNotify_Implementation(const FString& Message, float Duration)
{
	if (AParasiteHUD* HUD = GetParasiteHUD())
	{
		HUD->ShowMessage(Message, Duration);
	}
}

void AParasitePlayerController::ClientPlaySound_Implementation(uint8 Sound)
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

// ---------------------------------------------------------------------------
// Pawn lifecycle
// ---------------------------------------------------------------------------

void AParasitePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (AParasiteCharacter* Parasite = Cast<AParasiteCharacter>(InPawn))
	{
		SetParasiteBody(Parasite);
	}
}

void AParasitePlayerController::OnUnPossess()
{
	Super::OnUnPossess();
}

void AParasitePlayerController::ResetForNewMatch()
{
	if (!HasAuthority())
	{
		return;
	}
	if (GetParasitePlayerState() && GetParasitePlayerState()->CurrentHost)
	{
		ServerExitPossession(false);
	}
	HijackVictim = nullptr;
	if (AParasitePlayerState* PS = GetParasitePlayerState())
	{
		PS->ResetForNewMatch();
	}
}
