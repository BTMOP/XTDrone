import rospy
from geometry_msgs.msg import PoseStamped
from nav_msgs.msg import Odometry
import sys
import tf

vehicle_type = sys.argv[1]
vehicle_num = int(sys.argv[2])
multi_pose_pub = [None]*vehicle_num
multi_local_pose = [PoseStamped() for i in range(vehicle_num)]
multi_odom_groundtruth_sub = [None]*vehicle_num

def odm_groundtruth_callback(msg, i):
    multi_local_pose[i].header = msg.header
    multi_local_pose[i].pose = msg.pose.pose

if __name__ == '__main__':
    rospy.init_node(vehicle_type+'get_pose_groundtruth')
    for i in range(vehicle_num):
        multi_odom_groundtruth_sub[i] = rospy.Subscriber('/xtdrone/'+vehicle_type+'_'+str(i)+'/ground_truth/odom', Odometry, odm_groundtruth_callback, i)
        multi_pose_pub[i] = rospy.Publisher(vehicle_type+'_'+str(i)+'/mavros/vision_pose/pose', PoseStamped, queue_size=2)
    rate = rospy.Rate(100)
    while True:
        for i in range(vehicle_num):
            multi_pose_pub[i].publish(multi_local_pose[i])
        rate.sleep()

