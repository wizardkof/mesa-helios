#!/usr/bin/env python3
"""Test exact production negotiation bodies against controlled OS/escape seams."""
from pathlib import Path
import argparse,subprocess
p=argparse.ArgumentParser();p.add_argument('--cc',default='cc');p.add_argument('--output',type=Path,required=True);p.add_argument('--compile-only',action='store_true');a=p.parse_args()
r=Path(__file__).resolve().parent;s=(r/'vn_renderer_helios.c').read_text(encoding='utf-8')
def body(name):
 start=s.index(name+'(');brace=s.index('{',start);end=brace+1;depth=1
 while depth:
  depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return 'static bool\n'+s[start:end]
f='\n'.join(body(n) for n in ['helios_attest_exchange','helios_carrier_attest_negotiated'])
t=(r/'test_helios_attest_flow_mock.c').read_text(encoding='utf-8')
t=t.replace('/* FUNCTIONS */',f+'\n'+f.replace('helios_attest_exchange','second_exchange').replace('helios_carrier_attest_negotiated','second_negotiated'))
a.output.parent.mkdir(parents=True,exist_ok=True);g=a.output.with_suffix('.generated.c');g.write_text(t,encoding='utf-8')
subprocess.run([a.cc,'-std=c11','-Wall','-Wextra','-Wno-unused-function','-I'+str(r),str(g),'-o',str(a.output)],check=True)
if not a.compile_only:subprocess.run([str(a.output.resolve())],check=True)
