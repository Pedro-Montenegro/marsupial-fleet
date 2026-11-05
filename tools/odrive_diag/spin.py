import sys, time, collections, odrive
from odrive.enums import AXIS_STATE_CLOSED_LOOP_CONTROL as CL, AXIS_STATE_IDLE as IDLE
side=sys.argv[1]; axi=int(sys.argv[2]); vel=float(sys.argv[3]); dur=float(sys.argv[4])
o=odrive.find_any(serial_number={"left":"3678387D3333","right":"3553346E3034"}[side],timeout=5); ax=getattr(o,f"axis{axi}")
ax.error=0; ax.encoder.error=0; ax.motor.error=0; ax.controller.error=0; time.sleep(0.1)
print(f"{side}.axis{axi} pre: err={ax.error} enc.err={ax.encoder.error} hall={ax.encoder.hall_state:03b}",flush=True)
for i in (3,2,1): print(f"{i}...",flush=True); time.sleep(1)
hc=collections.Counter(); vels=[]
try:
    ax.requested_state=CL; time.sleep(0.3)
    if ax.current_state!=CL:
        print(f"NAO ENTROU em closed loop: state={ax.current_state} err={ax.error} enc.err={ax.encoder.error} mot.err={ax.motor.error} hall={ax.encoder.hall_state:03b}"); sys.exit()
    ax.controller.input_vel=vel; t0=time.time(); tp=0
    while time.time()-t0<dur:
        h=ax.encoder.hall_state; hc[h]+=1; v=ax.encoder.vel_estimate; vels.append(v)
        if time.time()-tp>1: print(f" t={time.time()-t0:4.1f}s vel={v:6.3f} hall={h:03b} err={ax.error}",flush=True); tp=time.time()
        if ax.error: print(f" ERRO em t={time.time()-t0:.1f}s: err={ax.error} enc.err={ax.encoder.error} mot.err={ax.motor.error} ctrl.err={ax.controller.error}"); break
        time.sleep(0.02)
finally:
    ax.controller.input_vel=0; time.sleep(0.1); ax.requested_state=IDLE
n=sum(hc.values()) or 1
print("halls: "+"  ".join(f"{s:03b}:{100*c/n:.0f}%" for s,c in sorted(hc.items())))
if vels: print(f"vel media(2a metade)={sum(vels[len(vels)//2:])/max(1,len(vels)-len(vels)//2):.3f} rev/s")
print(f"final: state={ax.current_state} err={ax.error} enc.err={ax.encoder.error} mot.err={ax.motor.error}")
