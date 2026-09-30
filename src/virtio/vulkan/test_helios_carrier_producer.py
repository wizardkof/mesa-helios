#!/usr/bin/env python3
"""Compile exact production producer/helper/cleanup bodies with fake OS/KMD seams.

No copied implementation: function bodies are extracted from vn_renderer_helios.c.
The mocks model failures and ownership only; native API calling conventions are
also compiled against Windows headers by supplying --cc for either MinGW ABI.
"""
import argparse
from pathlib import Path
import re
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--cc', default='cc')
p.add_argument('--output', type=Path, required=True)
p.add_argument('--compile-only', action='store_true')
a = p.parse_args()
root = Path(__file__).resolve().parent
source = (root / 'vn_renderer_helios.c').read_text()

def block(marker):
    start = source.index(marker)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

parts = []
for name in ['_OBJECT_ATTRIBUTES', 'helios_unicode_string', 'helios_escape_header',
             'helios_escape_p06_production_carrier']:
    parts.append(block('struct ' + name + ' {') + ';')
functions = [('bool', 'helios_carrier_id_nonzero'),
             ('bool', 'helios_carrier_win32_name_valid'),
             ('void', 'helios_carrier_drop_owned')]
if re.search(r'^helios_carrier_open_reader\(', source, re.M):
    functions.append(('HANDLE', 'helios_carrier_open_reader'))
functions.append(('VkResult', 'helios_carrier_create_producer'))
for ret, name in functions:
    parts.append('static ' + ret + '\n' + block(name + '('))
template = (root / 'test_helios_carrier_producer_mock.c').read_text()
assert template.count('/* PRODUCTION_STRUCTS */') == template.count('/* PRODUCTION_FUNCTIONS */') == 1
unit = template.replace('/* PRODUCTION_STRUCTS */', '\n'.join(parts[:4]))
unit = unit.replace('/* PRODUCTION_FUNCTIONS */', '\n'.join(parts[4:]))
a.output.parent.mkdir(parents=True, exist_ok=True)
generated = a.output.with_suffix('.generated.c')
generated.write_text(unit)
cmd = [a.cc, '-std=c11', '-fshort-wchar', '-Wall', '-Wextra', '-Wno-unused-function',
       '-I' + str(root), str(generated), '-o', str(a.output)]
print('BUILD', ' '.join(cmd), flush=True)
subprocess.run(cmd, check=True)
if not a.compile_only:
    subprocess.run([str(a.output.resolve())], check=True)
