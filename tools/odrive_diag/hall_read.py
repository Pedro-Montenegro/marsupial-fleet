import sys, time, collections, odrive
SERIALS={"right":"3553346E3034","left":"3678387D3333"}
dur=float(sys.argv[1]) if len(sys.argv)>1 else 10
odrvs={}
for l,s in SERIALS.items():
    try:
        o=odrive.find_any(serial_number=s,timeout=5); odrvs[l]=o
        print(f"[{l}] vbus={o.vbus_voltage:.2f}V")
        for a in ("axis0","axis1"):
            ax=getattr(o,a)
            print(f"  {a} state={ax.current_state} err={ax.error} enc.err={ax.encoder.error} mot.err={ax.motor.error} hall={ax.encoder.hall_state:03b}")
    except Exception as e: print(f"[{l}] NAO ENCONTRADA {e!r}")
cnt={}; last={}; trans=collections.Counter(); t0=time.time()
while time.time()-t0<dur:
    for l,o in odrvs.items():
        for a in ("axis0","axis1"):
            k=f"{l}.{a}"; h=getattr(o,a).encoder.hall_state
            cnt.setdefault(k,collections.Counter())[h]+=1
            if k in last and last[k]!=h: trans[k]+=1
            last[k]=h
    time.sleep(0.02)
print(f"--- {dur:.0f}s de amostragem ---")
for k,c in cnt.items():
    n=sum(c.values())
    print(f"{k}: "+"  ".join(f"{s:03b}:{100*v/n:.0f}%" for s,v in sorted(c.items()))+f"  trans={trans[k]}  ilegal(000/111)={c[0]+c[7]}")
for l,o in odrvs.items():
    for a in ("axis0","axis1"):
        ax=getattr(o,a); print(f"[{l}.{a}] final err={ax.error} enc.err={ax.encoder.error}")
