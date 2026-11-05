import odrive, json, functools
d=json.load(open('cfg_dump.json')); R=d['right']; L=d['left']
SKIP=('can_node_id','encoder.config.offset','encoder.config.offset_float','phase_resistance','phase_inductance','pre_calibrated','motor.config.direction')
o=odrive.find_any(serial_number='3678387D3333',timeout=5)
for a in ('axis0','axis1'):
    assert getattr(o,a).current_state==1, f'{a} nao esta IDLE'
changed=0
for k in sorted(R):
    if R[k]==L.get(k) or any(s in k for s in SKIP): continue
    *parts,attr=k.split('.')
    obj=functools.reduce(getattr,parts,o)
    setattr(obj,attr,R[k]); changed+=1
for a in ('axis0','axis1'):
    ax=getattr(o,a)
    ax.motor.config.pre_calibrated=False; ax.encoder.config.pre_calibrated=False
    ax.config.startup_encoder_offset_calibration=False; ax.config.startup_closed_loop_control=False
    ax.error=0; ax.encoder.error=0; ax.motor.error=0; ax.controller.error=0
print('parametros alterados:',changed)
# verifica
o2=o; bad=[]
for k in R:
    if any(s in k for s in SKIP): continue
    *parts,attr=k.split('.')
    v=getattr(functools.reduce(getattr,parts,o2),attr)
    if v!=R[k] and not (isinstance(v,float) and abs(v-R[k])<1e-6): bad.append((k,R[k],v))
print('diferencas restantes (fora dos ignorados):',bad)
for a in ('axis0','axis1'):
    ax=getattr(o,a); e=ax.encoder; m=ax.motor
    print(f'{a}: pp={m.config.pole_pairs} enc.mode={e.config.mode} cpr={e.config.cpr} err={ax.error} enc.err={e.error} hall={e.hall_state:03b} startup_cl={ax.config.startup_closed_loop_control}')
