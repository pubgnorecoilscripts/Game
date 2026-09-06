#include "PossessablePawn.h"
#include "PossessableComponent.h"
#include "ParasiteAudio.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"

namespace
{
	/** The engine cube is 100 uu on a side, so extent to scale is a divide. */
	FVector ExtentToScale(const FVector& Extent)
	{
		return FVector(FMath::Max(Extent.X, 1.f), FMath::Max(Extent.Y, 1.f), FMath::Max(Extent.Z, 1.f)) / 50.f;
	}
}

APossessablePawn::APossessablePawn()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	AutoPossessPlayer = EAutoReceiveInput::Disabled;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterial> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionProfileName(TEXT("Pawn"));
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
	if (BasicMaterial.Succeeded())
	{
		Mesh->SetMaterial(0, BasicMaterial.Object);
	}

	Detail = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Detail"));
	Detail->SetupAttachment(Mesh);
	Detail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Detail->SetVisibility(false);
	if (CubeMesh.Succeeded())
	{
		Detail->SetStaticMesh(CubeMesh.Object);
	}
	if (BasicMaterial.Succeeded())
	{
		Detail->SetMaterial(0, BasicMaterial.Object);
	}

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(Mesh);
	CameraBoom->TargetArmLength = 350.f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = true;
	CameraBoom->bInheritRoll = false;
	// The mesh is scaled per prop; the boom must not stretch with it.
	CameraBoom->SetUsingAbsoluteScale(true);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	Movement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
	Movement->UpdatedComponent = Mesh;
	Movement->MaxSpeed = 220.f;
	Movement->Acceleration = 900.f;
	Movement->Deceleration = 1600.f;

	Possessable = CreateDefaultSubobject<UPossessableComponent>(TEXT("Possessable"));
	Possessable->HostType = EHostType::Prop;
	Possessable->Mobility = EHostMobility::Slide;
}

void APossessablePawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APossessablePawn, VisualExtent);
	DOREPLIFETIME(APossessablePawn, VisualDetailOffset);
	DOREPLIFETIME(APossessablePawn, VisualDetailExtent);
	DOREPLIFETIME(APossessablePawn, VisualColour);
}

void APossessablePawn::BeginPlay()
{
	Super::BeginPlay();
	HingeClosedYaw = GetActorRotation().Yaw;
	ApplyVisuals();
}

void APossessablePawn::Configure(const FString& DisplayName, EHostType InHostType, EHostMobility InMobility,
	const FVector& BoxExtent, const FLinearColor& Colour, const FVector& DetailOffset, const FVector& DetailExtent)
{
	Possessable->HostDisplayName = DisplayName;
	Possessable->HostType = InHostType;
	Possessable->Mobility = InMobility;

	VisualExtent = BoxExtent;
	VisualColour = Colour;
	VisualDetailOffset = DetailOffset;
	VisualDetailExtent = DetailExtent;

	switch (InMobility)
	{
	case EHostMobility::Wheeled:
		Movement->MaxSpeed = (InHostType == EHostType::Vehicle) ? 700.f : 320.f;
		Possessable->CameraDistance = (InHostType == EHostType::Vehicle) ? 650.f : 320.f;
		break;
	case EHostMobility::Slide:
		Movement->MaxSpeed = 190.f;
		Possessable->CameraDistance = 300.f;
		break;
	default:
		// Static and hinge hosts shuffle at best. A vending machine does not sprint.
		Movement->MaxSpeed = 45.f;
		Possessable->CameraDistance = 280.f;
		break;
	}
	Possessable->CameraHeight = BoxExtent.Z + 40.f;
	ApplyVisuals();
}

void APossessablePawn::OnRep_Visual()
{
	ApplyVisuals();
}

void APossessablePawn::ApplyVisuals()
{
	if (!Mesh)
	{
		return;
	}
	Mesh->SetWorldScale3D(ExtentToScale(VisualExtent));

	if (!MeshMaterial && Mesh->GetMaterial(0))
	{
		MeshMaterial = Mesh->CreateAndSetMaterialInstanceDynamic(0);
	}
	if (MeshMaterial)
	{
		MeshMaterial->SetVectorParameterValue(TEXT("Color"), VisualColour);
	}

	const bool bHasDetail = !VisualDetailExtent.IsNearlyZero();
	Detail->SetVisibility(bHasDetail);
	if (bHasDetail)
	{
		// The detail hangs off a scaled parent, so undo that scale.
		const FVector ParentScale = Mesh->GetComponentScale();
		Detail->SetWorldScale3D(ExtentToScale(VisualDetailExtent));
		Detail->SetRelativeLocation(FVector(
			VisualDetailOffset.X / FMath::Max(ParentScale.X, KINDA_SMALL_NUMBER),
			VisualDetailOffset.Y / FMath::Max(ParentScale.Y, KINDA_SMALL_NUMBER),
			VisualDetailOffset.Z / FMath::Max(ParentScale.Z, KINDA_SMALL_NUMBER)));

		if (!DetailMaterial && Detail->GetMaterial(0))
		{
			DetailMaterial = Detail->CreateAndSetMaterialInstanceDynamic(0);
		}
		if (DetailMaterial)
		{
			DetailMaterial->SetVectorParameterValue(TEXT("Color"), VisualColour * 0.7f);
		}
	}

	if (CameraBoom && Possessable)
	{
		CameraBoom->TargetArmLength = Possessable->CameraDistance;
		CameraBoom->SetWorldLocation(GetActorLocation() + FVector(0.f, 0.f, VisualExtent.Z + 40.f));
	}
}

void APossessablePawn::OnRevealChanged(bool bRevealed)
{
	if (MeshMaterial)
	{
		MeshMaterial->SetVectorParameterValue(TEXT("Color"), bRevealed ? FLinearColor(1.f, 0.1f, 0.6f) : VisualColour);
	}
}

void APossessablePawn::DriveForward(float Value)
{
	if (FMath::IsNearlyZero(Value))
	{
		return;
	}
	switch (Possessable->Mobility)
	{
	case EHostMobility::Wheeled:
		// Wheeled hosts drive along their own facing.
		AddMovementInput(GetActorForwardVector(), Value);
		break;
	case EHostMobility::Static:
	case EHostMobility::Hinge:
		AddMovementInput(GetActorForwardVector(), Value * 0.25f);
		break;
	default:
		AddMovementInput(FRotationMatrix(FRotator(0.f, GetControlRotation().Yaw, 0.f)).GetUnitAxis(EAxis::X), Value);
		break;
	}
}

void APossessablePawn::DriveRight(float Value)
{
	if (FMath::IsNearlyZero(Value))
	{
		return;
	}
	const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	switch (Possessable->Mobility)
	{
	case EHostMobility::Wheeled:
		// Steering, not strafing.
		AddActorWorldRotation(FRotator(0.f, Value * 90.f * DeltaSeconds, 0.f));
		break;
	case EHostMobility::Static:
	case EHostMobility::Hinge:
		AddActorWorldRotation(FRotator(0.f, Value * 45.f * DeltaSeconds, 0.f));
		break;
	default:
		AddMovementInput(FRotationMatrix(FRotator(0.f, GetControlRotation().Yaw, 0.f)).GetUnitAxis(EAxis::Y), Value);
		break;
	}
}

void APossessablePawn::ToggleHinge()
{
	if (Possessable->Mobility != EHostMobility::Hinge)
	{
		return;
	}
	bHingeOpen = !bHingeOpen;
	FParasiteAudio::Play(this, EParasiteSound::UIClick, GetActorLocation());
}

void APossessablePawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Possessable && Possessable->Mobility == EHostMobility::Hinge)
	{
		// Doors swing towards their target angle rather than driving.
		const float Target = bHingeOpen ? 1.f : 0.f;
		if (!FMath::IsNearlyEqual(HingeAlpha, Target, 0.005f))
		{
			HingeAlpha = FMath::FInterpConstantTo(HingeAlpha, Target, DeltaSeconds, 2.5f);
			FRotator Rotation = GetActorRotation();
			Rotation.Yaw = HingeClosedYaw + HingeAlpha * 95.f;
			SetActorRotation(Rotation);
		}
	}
	else if (Possessable && Possessable->Mobility == EHostMobility::Static && Possessable->bPossessed)
	{
		// A possessed static host breathes, which is subtly readable up close.
		WobbleTime += DeltaSeconds;
		SetActorRotation(FRotator(0.f, GetActorRotation().Yaw, FMath::Sin(WobbleTime * 6.f) * 1.5f));
	}
}
