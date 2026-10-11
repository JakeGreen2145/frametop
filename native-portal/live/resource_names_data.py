# SPDX-License-Identifier: MIT
"""Parse delivered static data only. No native analysis runner or ELF access."""
import hashlib
import json
from pathlib import Path
PIN='c53cbde8c5da8aa3cb2bc74ac63ef8d1e7e7ca28e598bbaef4ff017399c9866f'
DEFAULT=Path('/home/jakeg/.hermes/work/desktop-compositor-re/retry-resource-639eace7fdb7/verified-payloads/STATIC-FACTS.json')
def header(path):
    data=Path(path).read_bytes();assert hashlib.sha256(data).hexdigest()==PIN
    rows=json.loads(data)['selected_resource_names']
    assert len(rows)==268
    rows=[r for r in rows if (r['table']=='main' and r['index'] in (57,58)) or
          (r['table']=='indexed-extra' and 125<=r['index']<=130) or
          r['table'] in ('vertex-extra','fragment-extra','fixed')]
    assert len(rows)==17
    out='struct ResourceFixtureName {uint64_t pointer,string;const char* text;};\nstatic constexpr ResourceFixtureName resource_fixture_names[]={\n'
    out+=''.join('{'+r.get('pointer_elf','0')+','+r['string_elf']+','+json.dumps(r['value'])+'},\n' for r in rows)
    return out+'};\n'
