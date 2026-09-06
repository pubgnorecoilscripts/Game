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

print(f'{checks} checks run')
for error in errors:
    print('FAIL: ' + error)
print('OK' if not errors else f'{len(errors)} problem(s)')
sys.exit(1 if errors else 0)
