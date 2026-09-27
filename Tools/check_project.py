#!/usr/bin/env python3
"""Static consistency checks for the Unreal layer of PARASITE.

The gameplay rules are covered by Tests/run_tests.sh. This catches the wiring
mistakes a C++ build would otherwise be the first to find: replicated properties
that were never registered, input bindings with no mapping, RPCs with no
implementation, and stray asset references.
"""
import glob
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
SRC = os.path.join(ROOT, 'Source', 'Parasite')
CORE = os.path.join(SRC, 'Core')
INI = os.path.join(ROOT, 'Config', 'DefaultInput.ini')

errors = []
checks = 0


def check(condition, message):
    global checks
    checks += 1
    if not condition:
        errors.append(message)


def unreal_headers():
    return sorted(h for h in glob.glob(os.path.join(SRC, '*.h')))


# 1. Every UPROPERTY(Replicated*) is registered in GetLifetimeReplicatedProps.
for header in unreal_headers():
    name = os.path.basename(header)
    impl_path = header[:-2] + '.cpp'
    impl = open(impl_path).read() if os.path.exists(impl_path) else ''
    body = open(header).read()
    for match in re.finditer(r'UPROPERTY\(([^)]*)\)\s*\n\s*[\w:<>\* ]+?\s+(\w+)\s*(?:=|;)', body):
        specifiers, prop = match.group(1), match.group(2)
        if 'Replicated' not in specifiers:
            continue
        check(re.search(r'DOREPLIFETIME(?:_CONDITION)?\(\s*\w+\s*,\s*' + prop + r'\s*[,)]', impl),
              f'{name}: replicated property "{prop}" is never registered')
        if 'ReplicatedUsing' in specifiers:
            fn = re.search(r'ReplicatedUsing\s*=\s*(\w+)', specifiers).group(1)
            check(fn in body, f'{name}: OnRep function "{fn}" is not declared')

# 2. Every Bind{Axis,Action} name exists in DefaultInput.ini, and vice versa.
mapped = set(re.findall(r'(?:ActionName|AxisName)="([^"]+)"', open(INI).read()))
bound = set()
for cpp in glob.glob(os.path.join(SRC, '*.cpp')):
    for _, action in re.findall(r'Bind(Axis|Action)\(TEXT\("([^"]+)"\)', open(cpp).read()):
        bound.add(action)
        check(action in mapped, f'{os.path.basename(cpp)}: input "{action}" has no mapping in DefaultInput.ini')
for action in sorted(mapped - bound):
    check(False, f'DefaultInput.ini: mapping "{action}" is not bound by any code')

# 3. Every Server/Client/Multicast UFUNCTION has an _Implementation.
for header in unreal_headers():
    body = open(header).read()
    impl_path = header[:-2] + '.cpp'
    impl = open(impl_path).read() if os.path.exists(impl_path) else ''
    for specifiers, fn in re.findall(r'UFUNCTION\(([^)]*)\)\s*\n\s*(?:virtual\s+)?[\w:<>\*&\s]+?\b(\w+)\s*\(', body):
        if not re.search(r'\b(Server|Client|NetMulticast)\b', specifiers):
            continue
        check(f'::{fn}_Implementation(' in impl, f'{os.path.basename(header)}: RPC "{fn}" has no _Implementation')

# 4. The project ships no assets, so only engine content may be referenced.
for cpp in glob.glob(os.path.join(SRC, '*.cpp')):
    for path in re.findall(r'TEXT\("(/[\w/\.]+)"\)', open(cpp).read()):
        check(path.startswith('/Engine/'), f'{os.path.basename(cpp)}: non-engine asset reference "{path}"')

# 5. Reflected headers include their generated header.
for header in unreal_headers():
    body = open(header).read()
    if 'UCLASS' not in body and 'USTRUCT' not in body and 'UINTERFACE' not in body:
        continue        # plain C++ helper, no reflection
    check('.generated.h"' in body, f'{os.path.basename(header)}: missing generated header include')

# 6. Core/ stays engine free, so the tests can keep building it standalone.
for source in glob.glob(os.path.join(CORE, '*')):
    body = open(source).read()
    for banned in ('CoreMinimal.h', 'UCLASS', 'UPROPERTY', 'UFUNCTION', 'FString', 'UWorld'):
        check(banned not in body,
              f'Core/{os.path.basename(source)}: uses Unreal symbol "{banned}"; Core must stay engine free')

# 7. The Unreal mirrors of the core enums have to stay in the same order.
types = open(os.path.join(SRC, 'ParasiteTypes.h')).read()
rules = open(os.path.join(CORE, 'ParasiteRules.h')).read()
for ue_enum, core_enum in (('EHostMobility', 'EHostMobility'), ('EMatchPhase', 'EMatchPhase')):
    ue_values = re.findall(r'\n\t(\w+)\s+UMETA', types[types.index(f'enum class {ue_enum}'):])
    core_block = rules[rules.index(f'enum class {core_enum}'):]
    core_block = core_block[:core_block.index('};')]
    core_values = re.findall(r'\n\t\t(\w+)[,\s]', core_block)
    check(ue_values[:len(core_values)] == core_values,
          f'{ue_enum}: Unreal mirror does not match the core enum order ({ue_values} vs {core_values})')

# 8. A UFUNCTION must not reuse a name the parent class already reflects: UHT
#    rejects re-declaring an inherited UFUNCTION, which fails the whole build.
#    Curated rather than exhaustive - it lists the engine reflected names a game
#    class is plausibly tempted to reuse.
ENGINE_UFUNCTIONS = {
    'APlayerController': {
        'ClientPlaySound', 'ClientPlaySoundAtLocation', 'ClientMessage', 'ClientTeamMessage',
        'ClientReset', 'ClientRestart', 'ClientGameEnded', 'ClientWasKicked', 'ClientSetHUD',
        'ClientSetViewTarget', 'ClientSetCameraMode', 'ClientSetCameraFade', 'ClientCapBandwidth',
        'ClientPlayCameraShake', 'ClientStopCameraShake', 'ClientPlayForceFeedback',
        'ClientStopForceFeedback', 'ClientReturnToMainMenu', 'ClientTravelInternal',
        'ClientIgnoreMoveInput', 'ClientIgnoreLookInput', 'ClientGotoState', 'ClientPrestreamTextures',
        'ClientEnableNetworkVoice', 'ClientMutePlayer', 'ClientUnmutePlayer', 'ClientRepObjRef',
        'ServerPause', 'ServerRestartPlayer', 'ServerChangeName', 'ServerMutePlayer',
        'ServerUnmutePlayer', 'ServerUpdateCamera', 'ServerCamera', 'ServerExec',
        'ServerAcknowledgePossession', 'ServerVerifyViewTarget', 'ServerViewNextPlayer',
        'ServerViewPrevPlayer', 'ServerViewSelf', 'ServerSetSpectatorWaiting',
        'ServerNotifyLoadedWorld', 'ServerShortTimeout',
        'SetPause', 'Possess', 'UnPossess', 'ConsoleCommand', 'GetHUD', 'SetViewTargetWithBlend',
        'AddYawInput', 'AddPitchInput', 'AddRollInput', 'SetName',
    },
    'ACharacter': {
        'Jump', 'StopJumping', 'Crouch', 'UnCrouch', 'CanJump', 'LaunchCharacter',
        'ClientCheatWalk', 'ClientCheatFly', 'ClientCheatGhost', 'ClientAdjustPosition',
        'ClientVeryShortAdjustPosition', 'ClientAckGoodMove', 'OnLanded',
    },
    'APawn': {
        'AddMovementInput', 'AddControllerYawInput', 'AddControllerPitchInput',
        'SpawnDefaultController', 'GetMovementComponent', 'IsMoveInputIgnored',
    },
    'AHUD': {
        'DrawRect', 'DrawLine', 'DrawText', 'DrawTexture', 'DrawTextureSimple', 'DrawMaterial',
        'DrawMaterialSimple', 'Project', 'Deproject', 'GetTextSize', 'AddHitBox',
        'ShowHUD', 'ShowDebug', 'GetOwningPlayerController', 'GetOwningPawn',
    },
    'APlayerState': {'GetPlayerName', 'GetScore', 'GetPingInMilliseconds', 'OnRep_Score'},
    'AGameStateBase': {'GetServerWorldTimeSeconds', 'HasBegunPlay', 'HasMatchStarted'},
    'AActor': {
        'Destroy', 'SetOwner', 'SetLifeSpan', 'SetActorHiddenInGame', 'SetActorTickEnabled',
        'WasRecentlyRendered', 'SetReplicates', 'ForceNetUpdate',
    },
    'UActorComponent': {
        'SetActive', 'ToggleActive', 'SetComponentTickEnabled', 'DestroyComponent',
        'SetIsReplicated', 'ComponentHasTag',
    },
}
# Walk our declared parents so a subclass inherits its whole chain's names.
PARENT_CHAIN = {
    'APlayerController': ['APlayerController', 'AActor'],
    'ACharacter': ['ACharacter', 'APawn', 'AActor'],
    'APawn': ['APawn', 'AActor'],
    'AHUD': ['AHUD', 'AActor'],
    'APlayerState': ['APlayerState', 'AActor'],
    'AGameStateBase': ['AGameStateBase', 'AActor'],
    'AGameModeBase': ['AActor'],
    'AActor': ['AActor'],
    'UActorComponent': ['UActorComponent'],
}
for header in unreal_headers():
    body = open(header).read()
    for cls, parent in re.findall(r'class PARASITE_API (\w+)\s*:\s*public\s+(\w+)', body):
        reserved = set()
        for ancestor in PARENT_CHAIN.get(parent, []):
            reserved |= ENGINE_UFUNCTIONS.get(ancestor, set())
        if not reserved:
            continue
        class_body = body[body.index(f'class PARASITE_API {cls}'):]
        for fn in re.findall(r'UFUNCTION\([^)]*\)\s*\n\s*(?:virtual\s+)?[\w:<>\*&\s]+?\b(\w+)\s*\(', class_body):
            check(fn not in reserved,
                  f'{os.path.basename(header)}: {cls}::{fn} reuses a UFUNCTION name from {parent}; '
                  f'UHT rejects re-declaring an inherited UFUNCTION')

# 9. Reflected object pointers name a type the header actually declares.
#    A missing forward declaration is a compile error UHT will not warn about.
BASE_DECLARED = {
    # Provided by the engine headers these classes already inherit from.
    'AActor', 'APawn', 'ACharacter', 'AController', 'APlayerController',
    'UObject', 'USceneComponent', 'UWorld', 'APlayerState',
}
for header in unreal_headers():
    body = open(header).read()
    declared = set(re.findall(r'^class\s+(\w+)\s*;', body, re.M))
    declared |= set(re.findall(r'class\s+PARASITE_API\s+(\w+)', body))
    for pointee in set(re.findall(r'TObjectPtr<\s*(\w+)\s*>', body)):
        check(pointee in declared or pointee in BASE_DECLARED,
              f'{os.path.basename(header)}: TObjectPtr<{pointee}> but "{pointee}" is never declared here')

# 10. The generated header has to be the last include in a reflected header.
for header in unreal_headers():
    body = open(header).read()
    includes = re.findall(r'^#include\s+(".+?")', body, re.M)
    if not any('generated.h' in inc for inc in includes):
        continue
    check('generated.h' in includes[-1],
          f'{os.path.basename(header)}: generated header is not the last include ({includes[-1]} follows it)')

# 11. A .cpp that uses an engine symbol includes the header defining it.
#     Headers are skipped on purpose: they use forward declarations. Curated to
#     the symbols this project touches.
ENGINE_SYMBOL_HEADERS = {
    r'\bUGameplayStatics::':            'Kismet/GameplayStatics.h',
    r'\bGEngine\b':                     'Engine/Engine.h',
    r'\bTActorIterator<':               'EngineUtils.h',
    r'\bUMaterialInstanceDynamic\b':    'Materials/MaterialInstanceDynamic.h',
    r'\bConstructorHelpers::':          'UObject/ConstructorHelpers.h',
    r'\bDOREPLIFETIME':                 'Net/UnrealNetwork.h',
    r'\bUSpringArmComponent\b':         'GameFramework/SpringArmComponent.h',
    r'\bUCameraComponent\b':            'Camera/CameraComponent.h',
    r'\bUCharacterMovementComponent\b': 'GameFramework/CharacterMovementComponent.h',
    r'GetCapsuleComponent\(\)':          'Components/CapsuleComponent.h',
    r'\bUStaticMeshComponent\b':        'Components/StaticMeshComponent.h',
    r'\bUPointLightComponent\b':        'Components/PointLightComponent.h',
    r'\bUSphereComponent\b':            'Components/SphereComponent.h',
    r'\bUFloatingPawnMovement\b':       'GameFramework/FloatingPawnMovement.h',
    r'\bAAIController\b':               'AIController.h',
    r'\bAPlayerStart\b':                'GameFramework/PlayerStart.h',
    r'\bGameSession\b':                 'GameFramework/GameSession.h',
    r'Canvas->':                         'Engine/Canvas.h',
    r'->PlayerInput\b':                  'GameFramework/PlayerInput.h',
    r'\bUSoundWaveProcedural\b':        'Sound/SoundWaveProcedural.h',
    r'\bUAudioComponent\b':             'Components/AudioComponent.h',
    r'GetTimerManager\(\)':              'TimerManager.h',
    r'\bUDirectionalLightComponent\b':  'Components/DirectionalLightComponent.h',
    r'\bUSkyLightComponent\b':          'Components/SkyLightComponent.h',
    r'InputComponent->Bind':             'Components/InputComponent.h',
    r'\bUStaticMesh\b(?!Component)':    'Engine/StaticMesh.h',
    r'\bUMaterial\b(?!Instance)':       'Materials/Material.h',
    r'TCHAR_TO_UTF8|UTF8_TO_TCHAR':      'Containers/StringConv.h',
    r'\bEKeys::':                        'InputCoreTypes.h',
    r'SpawnActor<|GetGameState<|GetPlayerControllerIterator': 'Engine/World.h',
}
for cpp in sorted(glob.glob(os.path.join(SRC, '*.cpp'))):
    body = open(cpp).read()
    includes = set(re.findall(r'#include\s+"(.+?)"', body))
    paired = cpp[:-4] + '.h'
    if os.path.exists(paired):
        includes |= set(re.findall(r'#include\s+"(.+?)"', open(paired).read()))
    for pattern, header in ENGINE_SYMBOL_HEADERS.items():
        if re.search(pattern, body):
            check(header in includes,
                  f'{os.path.basename(cpp)}: uses {pattern} but includes no "{header}"')

# 12. The target engine version is stated consistently everywhere.
TARGET_ENGINE = '5.4'
uproject = open(os.path.join(ROOT, 'Parasite.uproject')).read()
check(f'"EngineAssociation": "{TARGET_ENGINE}"' in uproject,
      f'Parasite.uproject: EngineAssociation is not "{TARGET_ENGINE}"')
for target in glob.glob(os.path.join(ROOT, 'Source', '*.Target.cs')):
    body = open(target).read()
    expected = 'EngineIncludeOrderVersion.Unreal' + TARGET_ENGINE.replace('.', '_')
    check(expected in body, f'{os.path.basename(target)}: IncludeOrderVersion is not {expected}')
readme = open(os.path.join(ROOT, 'README.md')).read()
check(f'UE_{TARGET_ENGINE}' in readme, f'README.md: build command does not reference UE_{TARGET_ENGINE}')
check(f'**{TARGET_ENGINE}**' in readme, f'README.md: does not state {TARGET_ENGINE} as the required engine')
# The classic input classes are pinned, since the project uses legacy mappings.
input_ini = open(INI).read()
check('DefaultPlayerInputClass=/Script/Engine.PlayerInput' in input_ini,
      'DefaultInput.ini: classic PlayerInput class is not pinned')
check('DefaultInputComponentClass=/Script/Engine.InputComponent' in input_ini,
      'DefaultInput.ini: classic InputComponent class is not pinned')

print(f'{checks} checks run')
for error in errors:
    print('FAIL: ' + error)
print('OK' if not errors else f'{len(errors)} problem(s)')
sys.exit(1 if errors else 0)
