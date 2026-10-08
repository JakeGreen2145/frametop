#!/usr/bin/env python3
"""Decode private metadata records. No memory access or compositor interaction."""
import argparse, json, pathlib, re, struct
ROOT=pathlib.Path(__file__).resolve().parent
NAMES=re.findall(r'\{"([a-z_]+)",0x', (ROOT/'pins.h').read_text())
HEADER=struct.Struct('<4I6Q4I32Q')
SIZE=2384
CONTROLS={0:'prepared',1:'installed',2:'drained',10:'refuse-executable',11:'refuse-mutator-environment',12:'refuse-load-or-threads',13:'refuse-duration',14:'refuse-metadata-read',15:'refuse-pagesize',16:'refuse-islands',17:'refuse-signals',18:'refuse-precommit',19:'refuse-transaction',91:'fatal-rollback-rx',92:'fatal-install-rx',93:'fatal-word-postcheck',94:'fatal-page-postcheck',95:'fatal-signals',96:'fatal-log',97:'fatal-drainer'}
TAGS={1:'native_ubo_528',2:'same_draw_raw_to_standing',3:'packet_prefix_352',4:'recorded_descriptors_9x24',5:'hmd_pose_80',6:'stock_S_1632',7:'policy_shared_reread_24',8:'policy_owner_enable_2',9:'F_tracking_and_native_id',10:'F_validity_64',11:'layer_activity_cache',12:'K_resource_state_40'}
def records(f):
    while True:
        b=f.read(SIZE)
        if not b:return
        if len(b)!=SIZE:raise ValueError('truncated record')
        h=HEADER.unpack_from(b)
        if h[0]!=0x3154464e or h[1]!=SIZE:raise ValueError('unsupported magic/size')
        site,flags=h[2:4];xs=h[14:45];sp=h[45];used=h[13]
        if used>2048:raise ValueError('invalid payload length')
        e=dict(schema=1,site=site,flags=flags,seq=h[4],frame_context=h[5],thread_seq=h[6],tid=h[7],monotonic_ns=h[8],owner=hex(h[9]),depth=h[10],eye=h[11],eye_seen_mask=h[12],x=[hex(x) for x in xs],sp=hex(sp))
        if site==0xffffffff:
            e['control']=CONTROLS.get(flags,'unknown')
            e['elf_sha256']=b[HEADER.size:HEADER.size+64].decode('ascii')
            e['counters']=dict(zip(['pid','dropped','read_failures','filtered_non_camera','reentrant','scope_errors'],xs[:6]))
        else:
            if site>=len(NAMES):raise ValueError('unsupported site')
            e['event']=NAMES[site];e['chunks']=[];at=HEADER.size;end=at+used
            while at<end:
                if end-at<16:raise ValueError('truncated chunk header')
                tag,n,address=struct.unpack_from('<IIQ',b,at);at+=16
                if n>end-at:raise ValueError('truncated chunk')
                data=b[at:at+n];at+=n;valid=not bool(tag&0x80000000);tag&=0x7fffffff
                c=dict(tag=tag,name=TAGS.get(tag,'unknown'),address=hex(address),valid=valid,hex=data.hex())
                if valid:
                    if tag==2 and n==48:c['matrix_3x4']=struct.unpack('<12f',data)
                    if tag==3 and n==352:
                        c['vs']=struct.unpack_from('<I',data,0x20)[0];c['ps']=struct.unpack_from('<I',data,0x30)[0]
                        c['blend']=hex(struct.unpack_from('<Q',data,8)[0]);c['vs_ubo']=hex(struct.unpack_from('<Q',data,0x38)[0]);c['ps_ubo']=hex(struct.unpack_from('<Q',data,0x58)[0])
                    if tag==5 and n==80:
                        c['device_to_absolute_3x4']=struct.unpack_from('<12f',data)
                        c['tracking_result']=struct.unpack_from('<I',data,72)[0];c['pose_valid']=data[76];c['connected']=data[77]
                e['chunks'].append(c)
        yield e
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('file',type=pathlib.Path);a=p.parse_args()
    with a.file.open('rb') as f:
        for e in records(f):print(json.dumps(e,separators=(',',':')))
