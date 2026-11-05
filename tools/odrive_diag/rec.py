import rclpy,time
from geometry_msgs.msg import Twist
from sensor_msgs.msg import Joy
rclpy.init(); n=rclpy.create_node('rec'); t0=time.time(); rows=[]; lb=[0]
n.create_subscription(Joy,'/joy',lambda m: lb.__setitem__(0,m.buttons[4] if len(m.buttons)>4 else 0),10)
n.create_subscription(Twist,'/cmd_vel',lambda m: rows.append((time.time()-t0,lb[0],m.linear.x,m.angular.z)),10)
while time.time()-t0<60: rclpy.spin_once(n,timeout_sec=0.1)
last=None
for t,b,v,w in rows:
    key=(b,round(v,2),round(w,2))
    if key!=last and (abs(v)>0.02 or abs(w)>0.02 or (last and (abs(last[1])>0.02 or abs(last[2])>0.02)) or b!=(last[0] if last else 0)):
        print(f't={t:5.1f} LB={b} v={v:+.3f} w={w:+.3f}')
    last=key
print('total msgs',len(rows),'max v',max((r[2] for r in rows),default=0),'min v',min((r[2] for r in rows),default=0),'max w',max((r[3] for r in rows),default=0),'min w',min((r[3] for r in rows),default=0))
print('movimento sem LB:',sum(1 for r in rows if r[1]==0 and (abs(r[2])>1e-3 or abs(r[3])>1e-3)))
