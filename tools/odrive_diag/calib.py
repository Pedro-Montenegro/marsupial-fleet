import odrive,time,sys,collections
from odrive.enums import AXIS_STATE_FULL_CALIBRATION_SEQUENCE as FC
axi=int(sys.argv[1])
o=odrive.find_any(serial_number='3678387D3333',timeout=5); ax=getattr(o,f'axis{axi}')
ax.error=0; ax.encoder.error=0; ax.motor.error=0; ax.controller.error=0
for i in (3,2,1): print(f'{i}...',flush=True); time.sleep(1)
ax.requested_state=FC; t0=time.time(); time.sleep(0.3); hc=collections.Counter(); tp=0
while ax.current_state!=1 and time.time()-t0<150:
    h=ax.encoder.hall_state; hc[h]+=1
    if time.time()-tp>10: print(f' t={time.time()-t0:5.1f} state={ax.current_state} hall={h:03b} err={ax.error}',flush=True); tp=time.time()
    time.sleep(0.05)
if ax.current_state!=1: ax.requested_state=1; print('TIMEOUT -> IDLE')
m=ax.motor;e=ax.encoder
print(f'fim t={time.time()-t0:.0f}s err={ax.error} mot.err={m.error} enc.err={e.error} R={m.config.phase_resistance:.4f} L={m.config.phase_inductance:.6f} motor_cal={m.is_calibrated} enc_ready={e.is_ready} offset={e.config.offset} dir={m.config.direction}')
n=sum(hc.values()) or 1; print('halls durante cal:',{f'{k:03b}':f'{100*v/n:.0f}%' for k,v in sorted(hc.items())})
