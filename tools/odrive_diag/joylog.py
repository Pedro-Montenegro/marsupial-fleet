import rclpy,time
from sensor_msgs.msg import Joy
rclpy.init(); n=rclpy.create_node('joylog'); t0=time.time(); prev=[None]
def cb(m):
    p=prev[0]; t=time.time()-t0
    if p:
        for i,(a,b) in enumerate(zip(p.buttons,m.buttons)):
            if a!=b: print(f't={t:5.1f} BOTAO {i}: {a}->{b}',flush=True)
        for i,(a,b) in enumerate(zip(p.axes,m.axes)):
            if abs(b)>0.6 and abs(a)<=0.6 or abs(a)>0.6 and abs(b)<=0.6: print(f't={t:5.1f} EIXO {i}: {a:+.2f}->{b:+.2f}',flush=True)
    prev[0]=m
n.create_subscription(Joy,'/joy',cb,50)
while time.time()-t0<60: rclpy.spin_once(n,timeout_sec=0.1)
