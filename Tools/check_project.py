#!/usr/bin/env python3
"""Static consistency checks for the Parasite project.

Not a compiler, but it catches the wiring mistakes that a C++ build would:
replicated properties that were never registered, input bindings with no
mapping, and RPCs declared without an implementation.
"""
import re, sys, glob, os

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
SRC = os.path.join(ROOT, 'Source', 'Parasite')
INI = os.path.join(ROOT, 'Config', 'DefaultInput.ini')

errors, checks = [], 0

def check(cond, message):
    global checks
    checks += 1
    if not cond:
        errors.append(message)

# 1. Every UPROPERTY(Replicated*) is registered in GetLifetimeReplicatedProps.
for header in glob.glob(os.path.join(SRC, '*.h')):
    body = open(header).read()
    cpp = header[:-2] + '.cpp'
    impl = open(cpp).read() if os.path.exists(cpp) else ''
    for match in re.finditer(r'UPROPERTY\(([^)]*)\)\s*\n\s*[\w:<>\* ]+?\s+(\w+)\s*(?:=|;)', body):
        specifiers, name = match.group(1), match.group(2)
        if 'Replicated' not in specifiers:
            continue
        check(f'DOREPLIFETIME({os.path.basename(header)[:-2]}, {name})'.replace('.h', '') in impl.replace('DOREPLIFETIME_CONDITION', 'DOREPLIFETIME')
              or re.search(r'DOREPLIFETIME(?:_CONDITION)?\(\w+,\s*' + name + r'\s*[,)]', impl),
              f'{os.path.basename(header)}: replicated property "{name}" is never registered')
        if 'ReplicatedUsing' in specifiers:
            fn = re.search(r'ReplicatedUsing\s*=\s*(\w+)', specifiers).group(1)
            check(fn in body, f'{os.path.basename(header)}: OnRep function "{fn}" is not declared')

# 2. Every Bind{Axis,Action} name exists in DefaultInput.ini.
ini = open(INI).read()
mapped = set(re.findall(r'(?:ActionName|AxisName)="([^"]+)"', ini))
for cpp in glob.glob(os.path.join(SRC, '*.cpp')):
    for kind, name in re.findall(r'Bind(Axis|Action)\(TEXT\("([^"]+)"\)', open(cpp).read()):
        check(name in mapped, f'{os.path.basename(cpp)}: input "{name}" has no mapping in DefaultInput.ini')

# 3. Every UFUNCTION RPC has an _Implementation.
for header in glob.glob(os.path.join(SRC, '*.h')):
    body = open(header).read()
    cpp = header[:-2] + '.cpp'
    impl = open(cpp).read() if os.path.exists(cpp) else ''
    for specifiers, name in re.findall(r'UFUNCTION\(([^)]*)\)\s*\n\s*(?:virtual\s+)?[\w:<>\*&\s]+?\b(\w+)\s*\(', body):
        if not re.search(r'\b(Server|Client|NetMulticast)\b', specifiers):
            continue
        check(f'::{name}_Implementation(' in impl, f'{os.path.basename(header)}: RPC "{name}" has no _Implementation')

# 4. Asset paths referenced in code are engine content only (project ships no assets).
for cpp in glob.glob(os.path.join(SRC, '*.cpp')):
    for path in re.findall(r'TEXT\("(/[\w/\.]+)"\)', open(cpp).read()):
        check(path.startswith('/Engine/'), f'{os.path.basename(cpp)}: non-engine asset reference "{path}"')

# 5. Headers and their implementation files agree on class names.
for header in glob.glob(os.path.join(SRC, '*.h')):
    body = open(header).read()
    if 'UCLASS' not in body and 'USTRUCT' not in body:
        continue        # plain C++ helper header, no reflection needed
    check('.generated.h"' in body, f'{os.path.basename(header)}: missing generated header include')
    for cls in re.findall(r'class PARASITE_API (\w+)', body):
        check(f'UCLASS' in body, f'{os.path.basename(header)}: {cls} is not a UCLASS')

print(f'{checks} checks run')
for error in errors:
    print('FAIL: ' + error)
print('OK' if not errors else f'{len(errors)} problem(s)')
sys.exit(1 if errors else 0)
