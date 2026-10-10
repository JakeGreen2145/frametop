# SPDX-License-Identifier: MIT
"""Generate adverse original native observations, not invented queue results."""
def generate(g, destination):
    S=g['S']; Native=g['Native']; dll=g['dll'];events=[];calls=[]
    cases=[dict(connect_failure=7),dict(alias_failure=7,failure_queue=1)]
    for kwargs in cases:
        n=Native(dll);original=n.observed
        def observe(module,pc):
            e=dict(module=module,pc=pc,x=[n.x(i) for i in range(31)],sp=n.m.reg_read(S.A.UC_ARM64_REG_SP))
            n.read_log.clear();original(module,pc);e['reads']=n.read_log[:];events.append(e)
        n.observed=observe
        first=len(events);assert n.run(**kwargs)
        state=[n.get(n.camera+p,1 if p in (0x349,0x34a,0x360) else 8) for p in (0x349,0x34a,0x360,0x350,0x358)]
        events.append(dict(module=0,pc=0x15faa4,x=[n.result]+[0]*30,sp=n.stack,reads=[]))
        calls.append((first,len(events),n.result,state));n.close()
    lines=['static const SourceEvent admission_events[] = {']
    for e in events:
        reads=','.join('{%s,%s,%d}'%(hex(a),hex(v),size) for a,v,size in e['reads'])
        lines.append('{%d,%s,{%s},%s,%d,{%s}},'%(e['module'],hex(e['pc']),','.join(map(hex,e['x'])),hex(e['sp']),len(e['reads']),reads))
    lines += ['};','struct AdmissionCall {unsigned first,last;uint64_t result,state[5];};','static const AdmissionCall admission_calls[] = {']
    for first,last,result,state in calls:lines.append('{%d,%d,%d,{%s}},'%(first,last,result,','.join(map(hex,state))))
    lines+=['};'];destination.write_text('\n'.join(lines)+'\n')
    # Original final packet selection. The choose-camera result and selected slot
    # are explicit fixture inputs; selection arithmetic/loads and enqueue argument
    # assembly are the pinned instructions, including slot*3, not a Python formula.
    n=Native(dll);selection=[]
    for index,slot in [(1,3),(0,13)]:
        K=n.camera;R=K+0x30000;texture=K+0x32000;sp=n.stack-0x460
        n.put(K+8,R);n.put(R,R+0x1000);n.put(R+0x1000+0x228,0x29b278)
        n.put(K+0x2e8,1,4);n.put(K+0x343,0,1);n.put(K+0x38c+60*index,0,4)
        n.put(K+0x820+4*index,slot,4);n.put(K+0x520+384*index+24*slot,texture)
        n.put(sp+0xb0,0,4)
        for reg,value in [(27,K),(28,index),(24,slot),(22,sp+0x300),(1,K+0x20000)]:n.x(reg,value)
        n.m.reg_write(S.A.UC_ARM64_REG_SP,sp)
        n.m.emu_start(0x161e0c,0x161ed8,count=100)
        assert n.m.reg_read(S.A.UC_ARM64_REG_PC)==0x161ed8
        assert n.get(sp+0x380)==texture and n.x(21)==n.x(24)==slot*3
        assert [n.x(i) for i in (0,1,2,4,22,27)]==[R,sp+0x300,0x450650,0x29b278,sp+0x300,K]
        selection.append((n.x(28),n.x(24),n.get(K+0x820+4*index,4)))
    n.close()
    (destination.parent/'selection.private.hpp').write_text('static constexpr unsigned native_selection[][3] = {'+
        ','.join('{'+','.join(map(str,row))+'}' for row in selection)+'};\n')
