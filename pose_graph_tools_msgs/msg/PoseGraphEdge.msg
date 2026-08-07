std_msgs/Header header

uint64 key_from
uint64 key_to

int32 robot_from
int32 robot_to

int32 type

# Type enums
int32 UNKNOWN = -1
int32 ODOM = 0
int32 LOOPCLOSE = 1
int32 LANDMARK = 2
int32 REJECTED_LOOPCLOSE = 3
int32 MESH = 4
int32 POSE_MESH = 5
int32 MESH_POSE = 6
int32 PRIOR = 7
int32 REJECTED_PRIOR = 8

# Transforms in ede
geometry_msgs/Pose pose
float64[36] covariance

# Optional Sim(3) scale measurement. pose.position always stores the physical
# translation. Consumers reconstruct GTSAM Similarity3 as
# Similarity3(R, pose.position / scale, scale).
bool has_scale false
float64 scale 1.0
# Standard deviation in log-scale space. Negative means unset.
float64 scale_sigma -1.0
