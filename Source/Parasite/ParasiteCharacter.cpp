#include "ParasiteCharacter.h"
#include "ParasitePlayerState.h"
#include "PossessableComponent.h"
#include "ParasiteAudio.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"

AParasiteCharacter::AParasiteCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	GetCapsuleComponent()->InitCapsuleSize(28.f, 34.f);

	// The project ships no skeletal assets, so the character mesh stays empty and
	// the parasite is built from engine basic shapes.
	GetMesh()->SetVisibility(false);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterial> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	Body->SetRelativeScale3D(FVector(0.55f, 0.45f, 0.35f));
	Body->SetRelativeLocation(FVector(0.f, 0.f, -18.f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (SphereMesh.Succeeded())
	{
		Body->SetStaticMesh(SphereMesh.Object);
	}
	if (BasicMaterial.Succeeded())
	{
		Body->SetMaterial(0, BasicMaterial.Object);
	}

	Eye = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Eye"));
	Eye->SetupAttachment(Body);
	Eye->SetRelativeScale3D(FVector(0.35f, 0.35f, 0.45f));
	Eye->SetRelativeLocation(FVector(0.8f, 0.f, 0.5f));
	Eye->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (SphereMesh.Succeeded())
	{
		Eye->SetStaticMesh(SphereMesh.Object);
	}

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Body);
	Glow->SetIntensity(900.f);
	Glow->SetAttenuationRadius(220.f);
	Glow->SetLightColor(FLinearColor(0.4f, 1.f, 0.5f));
	Glow->SetCastShadows(false);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 220.f;
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 40.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	Possessable = CreateDefaultSubobject<UPossessableComponent>(TEXT("Possessable"));
	Possessable->HostType = EHostType::Player;
	Possessable->Mobility = EHostMobility::Walk;
	Possessable->HostDisplayName = TEXT("ENEMY");
	Possessable->CameraDistance = 220.f;
	// The simulation already owns a host for this body; the game mode links it.
	Possessable->bManagedExternally = true;

	bUseControllerRotationYaw = false;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 640.f, 0.f);
	Move->MaxWalkSpeed = 330.f;
	Move->MaxWalkSpeedCrouched = 160.f;
	Move->JumpZVelocity = 480.f;
	Move->AirControl = 0.6f;
	Move->GetNavAgentPropertiesRef().bCanCrouch = true;
	Move->CrouchedHalfHeight = 22.f;
}

void AParasiteCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (Body && Body->GetMaterial(0))
	{
		BodyMaterial = Body->CreateAndSetMaterialInstanceDynamic(0);
	}
	if (const AParasitePlayerState* PS = GetPlayerState<AParasitePlayerState>())
	{
		ApplyTeamColour(PS->Team);
	}
}

void AParasiteCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AParasiteCharacter, bHijacked);
	DOREPLIFETIME(AParasiteCharacter, ResistProgress);
	DOREPLIFETIME(AParasiteCharacter, bDormant);
}

void AParasiteCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The player state can arrive after BeginPlay on clients.
	if (const AParasitePlayerState* PS = GetPlayerState<AParasitePlayerState>())
	{
		if (PS->Team != AppliedTeam)
		{
			ApplyTeamColour(PS->Team);
		}
	}

	// Wet little footsteps, so a parasite close by is audible.
	if (IsLocallyControlled() && !bDormant && !GetCharacterMovement()->IsFalling() && GetVelocity().SizeSquared2D() > 100.f)
	{
		StepSoundTimer -= DeltaSeconds;
		if (StepSoundTimer <= 0.f)
		{
			StepSoundTimer = 0.32f;
			FParasiteAudio::Play(this, EParasiteSound::ParasiteMove, GetActorLocation());
		}
	}
}

void AParasiteCharacter::OnRep_Hijacked()
{
	if (Glow)
	{
		Glow->SetLightColor(bHijacked ? FLinearColor(1.f, 0.2f, 0.15f) : FLinearColor(0.4f, 1.f, 0.5f));
	}
	if (bHijacked && IsLocallyControlled())
	{
		FParasiteAudio::Play2D(this, EParasiteSound::Detected);
	}
}

void AParasiteCharacter::OnRep_Dormant()
{
	SetActorHiddenInGame(bDormant);
	SetActorEnableCollision(!bDormant);
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetMovementMode(bDormant ? MOVE_None : MOVE_Walking);
	}
}

void AParasiteCharacter::OnRevealChanged(bool bRevealed)
{
	if (Glow)
	{
		Glow->SetLightColor(bRevealed ? FLinearColor(1.f, 0.1f, 0.6f) : FLinearColor(0.4f, 1.f, 0.5f));
	}
}

void AParasiteCharacter::SetDormant(bool bNewDormant, const FVector& WakeLocation)
{
	if (!HasAuthority())
	{
		return;
	}
	if (!bNewDormant && !WakeLocation.IsNearlyZero())
	{
		SetActorLocation(WakeLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}
	bDormant = bNewDormant;
	OnRep_Dormant();		// apply on the server as well
}

void AParasiteCharacter::SetHijacked(bool bNewHijacked, float NewResistProgress)
{
	if (!HasAuthority())
	{
		return;
	}
	ResistProgress = NewResistProgress;
	if (bHijacked != bNewHijacked)
	{
		bHijacked = bNewHijacked;
		OnRep_Hijacked();
	}
}

void AParasiteCharacter::ApplyHijackInput(float Forward, float Right, float YawDelta)
{
	if (!HasAuthority() || !bHijacked)
	{
		return;
	}
	AddActorWorldRotation(FRotator(0.f, YawDelta, 0.f));

	const FRotator YawOnly(0.f, GetActorRotation().Yaw, 0.f);
	const FRotationMatrix Rotation(YawOnly);
	AddMovementInput(Rotation.GetUnitAxis(EAxis::X), FMath::Clamp(Forward, -1.f, 1.f));
	AddMovementInput(Rotation.GetUnitAxis(EAxis::Y), FMath::Clamp(Right, -1.f, 1.f));
}

void AParasiteCharacter::SetSprinting(bool bSprint)
{
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = bSprint ? 560.f : 330.f;
	}
}

void AParasiteCharacter::ApplyTeamColour(EParasiteTeam Team)
{
	AppliedTeam = Team;
	if (!BodyMaterial)
	{
		return;
	}
	const FLinearColor Colour =
		(Team == EParasiteTeam::TeamA) ? FLinearColor(0.15f, 0.75f, 1.f) :
		(Team == EParasiteTeam::TeamB) ? FLinearColor(1.f, 0.45f, 0.1f) : FLinearColor(0.6f, 0.6f, 0.6f);
	BodyMaterial->SetVectorParameterValue(TEXT("Color"), Colour);
}
