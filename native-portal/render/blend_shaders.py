#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Close B4's missing manifest/raw-ID proof using the eight pinned originals.
Requires actual Khronos spirv-dis/as/val. No custom validator substitution.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import generate_shaders as g

def verify(source,receipt,output):
    inputs=json.loads(receipt.read_text())
    assert inputs['compositor_sha256']=='9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265'
    assert len(inputs['assets'])==8 and len({r['id'] for r in inputs['assets']})==8
    for item in inputs['assets']:
        pin=g.MANIFEST['shaders'][str(item['id'])];data=(source/pin['filename']).read_bytes()
        assert len(data)==item['size'] and hashlib.sha256(data).hexdigest()==item['sha256']==pin['sha256']
    manifest=g.generate(source,output/'generated')
    assert manifest['pairs']==[[57,125],[57,126],[57,127],[58,128],[58,129],[58,130]]
    mapping=[]
    for ident in ('57','58'):
        pin=g.MANIFEST['shaders'][ident]
        text=subprocess.check_output(['spirv-dis','--raw-id',str(source/pin['filename'])],text=True)
        lines=g.instructions(text)
        defs={s.split(' = ')[0]:s.split(' = ')[1].split() for s in lines if ' = ' in s}
        raw='%'+str(pin['raw_position_id']);inst=defs[raw]
        assert inst[0]=='OpMatrixTimesVector'
        matrix=defs[inst[2]];assert matrix[0]=='OpLoad'
        access=defs[matrix[2]];assert access[0]=='OpAccessChain' and len(access)==4
        ubo,index=access[2:];assert defs[index][0]=='OpConstant' and defs[index][-1]=='0'
        assert f'OpDecorate {ubo} DescriptorSet 0' in lines and f'OpDecorate {ubo} Binding 0' in lines
        block=defs[defs[ubo][1]][2]
        for member,offset in enumerate((0,64,128,192)):
            for decoration in ('RowMajor','MatrixStride 16',f'Offset {offset}'):
                assert f'OpMemberDecorate {block} {member} {decoration}' in lines
        # Prove the following eye and projection operations consume this result.
        previous=raw
        for member in (1,2):
            candidates=[]
            for result,operation in defs.items():
                if operation[0]!='OpMatrixTimesVector' or operation[3]!=previous:continue
                load=defs[operation[2]]
                if load[0]!='OpLoad':continue
                chain=defs[load[2]]
                if chain[0]=='OpAccessChain' and len(chain)==4 and chain[2]==ubo and defs[chain[3]][-1]==str(member):
                    candidates.append(result)
            assert len(candidates)==1
            previous=candidates[0]
        # M0 consumes the actual Location0 vec3 extended with W=1, not eye or clip.
        point=defs[inst[3]];assert point[0]=='OpCompositeConstruct'
        assert defs[point[-1]][0]=='OpConstant' and defs[point[-1]][-1]=='1'
        extracts=[defs[p] for p in point[2:-1]]
        assert [p[-1] for p in extracts]==['0','1','2'] and all(p[0]=='OpCompositeExtract' for p in extracts)
        assert len({p[2] for p in extracts})==1
        vertex=defs[extracts[0][2]];assert vertex[0]=='OpLoad'
        assert f'OpDecorate {vertex[2]} Location 0' in lines
        assert defs[vertex[2]][-1]=='Input'
        transformed=subprocess.check_output(['spirv-dis','--raw-id',str(output/'generated'/f'{ident}.portal.spv')],text=True)
        assert transformed.count(f'OpCompositeExtract %6 {raw}')==text.count(f'OpCompositeExtract %6 {raw}')+3
        mapping.append(dict(vertex=int(ident),raw_id=pin['raw_position_id'],matrix_member=0,ubo_offset=0,pre_eye=True))
    report=dict(assets=8,pairs=manifest['pairs'],raw_mapping=mapping,
        tool=subprocess.check_output(['spirv-val','--version'],text=True).splitlines()[0],
        variants='8 generated and spirv-val Vulkan1.0 validated',
        scope='binary/manifest/instruction proof, not native optical alignment or GPU pixels')
    (output/'shader-verification.json').write_text(json.dumps(report,indent=2)+'\n')
    return report

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('assets',type=Path);p.add_argument('receipt',type=Path)
    a=p.parse_args();print(json.dumps(verify(a.assets,a.receipt,ROOT/'render/build/blend'),indent=2))
