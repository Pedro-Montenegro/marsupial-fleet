import odrive, json, sys
def walk(obj, path, out, depth=0):
    if depth>6: return
    for name in dir(obj):
        if name.startswith('_'): continue
        try: v=getattr(obj,name)
        except Exception: continue
        p=f"{path}.{name}"
        if isinstance(v,(int,float,bool)): out[p]=v
        elif callable(v): continue
        elif hasattr(v,'__dir__') and type(v).__name__.startswith('anonymous'): walk(v,p,out,depth+1)
def dump(serial):
    o=odrive.find_any(serial_number=serial,timeout=5); out={}
    walk(o.config,'config',out)
    for a in ('axis0','axis1'):
        ax=getattr(o,a)
        for sub,obj in (('config',ax.config),('motor.config',ax.motor.config),('encoder.config',ax.encoder.config),('controller.config',ax.controller.config),('trap_traj.config',ax.trap_traj.config)):
            walk(obj,f"{a}.{sub}",out)
    return out
R=dump('3553346E3034'); L=dump('3678387D3333')
json.dump({'right':R,'left':L},open('cfg_dump.json','w'),indent=1)
diff=[k for k in sorted(set(R)|set(L)) if R.get(k)!=L.get(k)]
print(f"{len(R)} params right, {len(L)} left, {len(diff)} diferentes")
for k in diff: print(f"  {k:55s} right={R.get(k)!r:22} left={L.get(k)!r}")
