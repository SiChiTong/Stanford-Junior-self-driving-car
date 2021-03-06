/*--------------------------------------------------------------------
 * project ....: Darpa Urban Challenge 2007
 * authors ....: Team AnnieWAY
 * organization: Transregional Collaborative Research Center 28 /
 *               Cognitive Automobiles
 * creation ...: xx/xx/2007
 * revisions ..: $Id:$
---------------------------------------------------------------------*/
#ifndef AW_CHSM_PLANNER_HPP
#define AW_CHSM_PLANNER_HPP

// TODO use boost::pool_alloc for all states
#include <iostream>
#include <deque>
#include <boost/statechart/state_machine.hpp>
#include <boost/statechart/state.hpp>
#include <boost/statechart/simple_state.hpp>
#include <boost/statechart/event.hpp>
#include <boost/statechart/transition.hpp>
#include <boost/statechart/custom_reaction.hpp>
#include <boost/statechart/exception_translator.hpp>
#include <boost/statechart/deep_history.hpp>
#include <boost/pool/pool_alloc.hpp>
#include <boost/mpl/list.hpp>
#include <ipp.h>
#include <ipc_interface.h>
#include <poseQueue.h>
#include <curveSmoother.h>
#include <clothoid.h>
#include <aw_geometry.h>
#include <aw_Vehicle.h>
#include <aw_VehicleManager.hpp>
#include <aw_Topology.hpp>
#include <aw_RouteSampler.hpp>
#include <aw_IntersectionManager.hpp>
#include <aw_BlockadeManager.hpp>
#include <aw_vehicle_interface.h>
#include <aw_navigator_interface.h>
#include <aw_chsm_planner_interface.h>
#include <perception_interface.h>
#include <traffic_light_interface.h>

#include <aw_graph_tools.hpp>
#include "aw_StLaneChangeTypes.hpp"
#include "aw_ChsmEvents.hpp"
#include "trajectoryEvaluator.h"


namespace mpl = boost::mpl;
namespace sc = boost::statechart;

namespace vlr {

#define C2_CURVEPOINTS_LEASTDISTANCE 0.0001 /* points have to be at least some distance apart */
//#define C2_CURVEPOINTS_MAXSPEED 10      /* [m/s] for safety */
//#define C2_CURVEPOINTS_MAXCURVATURE (1./5.) /* [1/m] */
//

// constants
#define STD_VEHICLE_LENGTH                         5.0

#define TRIGGER_DIST_STOPLINE                      50.0
#define TRIGGER_DIST_TRAFFIC_LIGHT                 50.0
#define TRIGGER_DIST_CROSSWALK                     50.0
#define TRIGGER_DIST_GOAL                          50.0
#define TRIGGER_DIST_NASTY_GOAL                     3.0
#define TRIGGER_DIST_LANECHANGE                    15.0
#define TRIGGER_DIST_FOLLOWING                     30.0 // 50 seemed far too much
#define TRIGGER_DIST_OBSTACLE                      20.0
#define TRIGGER_DIST_KTURN                         20.0
#define TRIGGER_DIST_BLINK                         30.0
#define TRIGGER_DIST_APPROACH_INTERSECTION         55.0
#define TRIGGER_DIST_APPROACH_TRAFFIC_LIGHT        55.0
#define TRIGGER_DIST_APPROACH_CROSSWALK            55.0
#define TRIGGER_DIST_INTERSECTION_RECOVER          10.0
#define TRIGGER_DIST_ENTERING_ZONE                 10.0

/*! if the car is moved in pause mode more than this distance in [m]
 *  the history of StActive is cleared.
 */
#define TRIGGER_DIST_CLEAR_HISTORY                 10.0

#define STOP_DIST_BEFORE_OBSTACLE                  STD_VEHICLE_LENGTH // sk changed for brake tests
#define RETURN_DIST_AFTER_OBSTACLE                 10.0

// Min wait times in seconds
#define MIN_WAIT_STOPLINE                          0.5
#define MIN_WAIT_OBSTACLE                          5.0
#define MIN_WAIT_PARKED                            5.0
#define MIN_WAIT_ACTIVATION                        0.0  // do not wait anymore
#define SWITCH_TO_LONGTERM_PAUSE                 180.0
#define CONGESTION_TIMEOUT                        15.0
#define MIN_WAIT_FOR_OBSTACLEMAP                   0.5

// Thresholds
#define STOP_DIST_THRESHOLD                        1.0
#define TRAFFIC_LIGHT_DIST_THRESHOLD               1.0
#define CROSSWALK_DIST_THRESHOLD                   1.0
#define STOP_SPEED_THRESHOLD                       0.1
#define ACCEL_HISTORY_SIZE                         10
#define AFTER_BURNER_DIST                          2.0

// recovery
#define RECOVERY_TIMEOUT                          100000
#define RECOVERY_TIMEOUT_DRIVEONLANE              100000
#define INTERSECTION_RECOVERY_TIMEOUT             100000
#define INTERSECTION_MAX_WAIT_TIME             (3*60.0)
#define GLOBAL_RECOVERY_TIMEOUT                (20*60.0) // was 4*60
#define RECOVERY_PROGRESS_INDICATOR                0.5   // [m]
#define GETBACKONTRACK_REPLAN_INTERVAL             5.0
const double FROG_MODE_LEAP_DIST               =  15.0;

// Lane Changing
const double SAFETY_DISTANCE                      = 1.0;
const double LANE_CHANGE_LENGTH                   = 2.*EGO_VEHICLE_LENGTH;
const double LANE_CHANGE_STD_MERGING_LENGTH       =    LANE_CHANGE_LENGTH +    EGO_VEHICLE_LENGTH + 2.*SAFETY_DISTANCE;
const double LANE_CHANGE_OPPOSITE_MERGING_LENGTH  = 2.*LANE_CHANGE_LENGTH + 2.*EGO_VEHICLE_LENGTH + 2.*SAFETY_DISTANCE;
const double VEHICLE_STAND_TILL_BLOCKADE_TIME     = 10.0;
const double VEHICLE_STAND_TILL_IGNORED           = 50.0;

class MissionContext : public BezierContext {

public:
  MissionContext();
  MissionContext(std::vector<CurvePoint>* points);
  virtual ~MissionContext();
  const std::vector<CurvePoint>& points() const {return *points_;}

private:
  void moveto(double x, double y, int is_open);
  void lineto(double x, double y);
  void curveto(double x1, double y1, double x2, double y2, double x3, double y3);

  std::vector<CurvePoint>* points_;
  CurvePoint cp_;
  bool own_point_vector_;
};


struct StPause;
struct StLaneChange;

//struct BlockadeInfo{
//      float thres_critical_zdif; //specify that value, you will get number of cells that violoate (are above) this threshold
//      double borderDist;      //specify that value, the width to each side of the track spanning the area to verify
//      int32_t thres_cirticalN;    //specify that value, if nAboveThreshold is above this threshold blocked will be set to true
//      int32_t nAboveThreshold;    //you receive that value, the number of cells that violoate @threshold_to_use
//      float maxValue;         //you recieve that value, the maximum value in the area
//      float averageValue;     //the average value in the area
//
//   /*says whether the path blocked*/
//   bool bBlocked;
//   /*says whether verification was successfull, only in this case @blocked can be trusted*/
//   bool bVerificationWasSuccessfull;
//};

#ifdef __DARWIN__
inline void pthread_yield() {pthread_yield_np();}
#endif

template<class T, void*(T::*mem_fn)(void*)>
void* threadCBWrapper(void* ptr) {
  return (static_cast<T*>(ptr)->*mem_fn)(NULL);
  }

class ObstaclePredictor;

class ChsmPlanner : public sc::state_machine< ChsmPlanner, StPause, boost::pool_allocator < char >, sc::exception_translator<> > {

  enum ReplanReason {
    OFF_TRACK,
    BLOCKED,
    LANE_CHANGE,
    STUCK,
    UNKNOWN
  };

public:
  class Parameters {
  public:
    Parameters();
    ~Parameters();

    // drive params
    double center_line_min_lookahead_dist;
    double center_line_back_sample_length;                  // in what distance are curvepoints sampled behind the car
    double center_line_sample_dist;                         // distance between resampled waypoints in m
    double center_line_post_smoothing_sample_dist;          // distance between sampled point after clothoid fitting
    uint32_t smoothing_range;                               // number of points used for each smoothing step

    // kturn params
    double kturn_delta;                  // kturn distance offset
    double kturn_radius;                 // kturn radius
    double kturn_switch_speed;           // switches to next phase if speed below this threshold
    double kturn_switch_distance;        // switches to next phase if position difference below this threshold

    // message buffer
    uint32_t message_buffer_size;   // size of the internal message buffer

    // distances
    double safety_stop_infront_vehicle_distance; // [m] stops x meter infront of vehicle

    int32_t enable_recovery;                  // if we want recover modes to be active (disable only for testing) (defaults to true)
    int32_t enable_blockade_detection;        // wether we want to enable blocakde detection (disable only for testing) (defaults to true)

    double max_speed_global;                 // global planner speed limit
    double curvature_slowdown_factor;        // slowdown factor for curvatures
    double surface_quality_slowdown_factor;  // slowdown factor for rough surface areas
    double max_speed_drive;
    double max_speed_kturn;
    double max_speed_intersection;
    double max_speed_intersection_approach;
    double max_speed_merge_intersection;
    double max_speed_traffic_light_approach;
    double max_speed_crosswalk_approach_empty;
    double max_speed_crosswalk_approach_occupied;
    double max_speed_goal;
    double max_speed_pass_obstacle;
    double max_speed_pass_switch_lane; // ?!?
    double max_speed_enter_zone;
    double max_speed_lane_merge_false; // ?!?
  };

  ChsmPlanner(std::string rndf_filename, std::string mdf_filename,
              double static_obstacle_map_width, double static_obstacle_map_height,
              double static_obstacle_map_resolution);

  ~ChsmPlanner();

  void initialize(std::string rndf_filename, std::string mdf_filename,
                  std::string name, bool recover_from_file, bool save_mission_progress_to_file,
                  double static_obstacle_map_width, double static_obstacle_map_height,
                  double static_obstacle_map_resolution);

  inline const Parameters& params() const {return params_;}
  inline void params(const Parameters& params) {params_ = params;}

  void offset_line( double x1, double y1, double x2, double y2, double& x1o, double& y1o, double& x2o, double& y2o, double offset );

  void initialise_perimeter_points();


  void start(); // start planner
  void process(); // plan one iteration
  void stop();// stop planner
  void activate(); // activate car
  void pause();// pause car

  inline bool emergencyStopInitiated() const {return emergency_stop_initiated_;}
  inline void resetEmergencyStopState() {emergency_stop_initiated_=false;}

  inline int32_t obstacleMapWidth() const {return static_obstacle_map_width_;}
  inline int32_t obstacleMapHeight() const {return static_obstacle_map_height_;}
  inline double obstacleMapCenterX() const {return static_obstacle_map_cx_;}
  inline double obstacleMapCenterY() const {return static_obstacle_map_cy_;}
  inline double obstacleMapResolution() const {return static_obstacle_map_res_;}
  inline const uint8_t* obstacleMapRaw() const {return static_obstacle_map_raw_;}
  inline const uint8_t* obstacleMapCS() const {return static_obstacle_map_;}
  inline double obstacleMapTimestamp() const {return static_obstacle_timestamp_;}
  inline const std::vector<CurvePoint>& missionPointsBezier() const {return mission_bezier_points_;}
  bool obstacleMapMsgData(int32_t& num_points, double& last_timestamp, PerceptionObstaclePoint& points, double& cx, double& cy);

  void updateRobot(double timestamp, double smooth_x, double smooth_y,  double offset_x, double offset_y, double yaw, double speed, double ax, double ay, double az, double yaw_rate);
  void updateVehicles(PerceptionDynamicObstacle* dyn_obstacles, uint32_t num_dyn_obstacles, double t0);
  void updateTrafficLightStates(TrafficLightList* tll);
  void updateStaticObstacleMapSize(double new_width, double new_height, double new_resolution);
  void updateStaticObstacleMap(PerceptionObstaclePoint* points, int32_t num_points, double timestamp);
  void updateBlockades(); // update the road blockages

  void generateCurvePoints(double stop_distance, double follow_distance, double maxSpeed);
//  void generateCurvePoints(double start_lateral_offset, double dist_to_step, double gap_length, double end_lateral_offset, double maxSpeed);
  void generateCurvePoints(double maxSpeed);
  void generateStopTrajectory();  // stop the car asap
  void getVehicleDistances(double& standing, double& moving);
  double distance(const Pose& pose); // distance to current robot pose

    // pose related functions
  inline bool poseAvailable() {return !pose_queue_.empty();}
  Pose currentPose();
  Pose pose(double timestamp);
  void currentLocalizeOffsets(double& offset_x, double& offset_y);

  // messaging methods
  int32_t addMessage(const std::string& message);
  inline std::deque<std::string>& getMessages() {return message_buffer_;}
  inline double lastMessageTimestamp() const {return last_message_timestamp_;}
  inline std::string lastMessage() const {if(message_buffer_.empty()) {return "";} else {return message_buffer_.back();}}
  void enable_road_boundaries( bool enabled );

private:
  void createSmoothedMissionLine();
  void createVelocityProfile();
  bool checkAndReplaceVelocityForward(CurvePoint& p_ref, CurvePoint& p, double max_accel);
  bool checkAndReplaceVelocityBackward(CurvePoint& p_ref, CurvePoint& p, double max_decel);
  void cubicBezierFromClothoid(const std::vector<CurvePoint>& points, std::vector<CurvePoint>& bez_points);
  double calculateVelocity(double maxSpeed, double sample_length_front);
  double calculateCurvatureVelocity(double max_speed, double curvature_slowdown_factor, double sample_length_front);
  double calculateRoughSurfaceVelocity(double max_speed);
  double calculateFollowingSpeed(const double nextVehicleSpeed, const dgc_pose_t& nextVehiclePose, const double angleToEdge);
  void estimateKappa(CurvePoint& p1, CurvePoint& p2, CurvePoint& p3, double& kappa);
  void clearBlockages();

  const std::string& name() const;

  void* updateStaticObstacleMapThread(void*);



  // internal variables
private:
  Parameters params_; // planner parameters

  pthread_attr_t  def_thread_attr_;

public:
  pthread_mutex_t pose_mutex_;
  pthread_mutex_t trajectory_mutex_;
  pthread_mutex_t dyn_obstacles_mutex_;
  pthread_mutex_t static_obstacle_map_mutex_;
  pthread_mutex_t static_obstacle_points_mutex_;
  pthread_mutex_t mission_mutex_;
  pthread_mutex_t topology_mutex_;
  pthread_mutex_t center_line_mutex_;
  pthread_mutex_t intersection_predictor_mutex_;
  pthread_mutex_t traffic_light_states_mutex_;
  pthread_mutex_t traffic_light_poses_mutex_;
  pthread_mutex_t pedestrian_prediction_mutex_;

private:
  PoseQueue pose_queue_;
  double robot_yaw_rate_;

public:
  // state feedback
  double velocity_desired_;
  double velocity_following_;
  double follow_distance_;
  double stop_distance_;

  std::vector<CurvePoint> last_splinepoints_;
  std::map<double, CurvePoint> center_line_;
  CurvePoint stop_point_;    // used to store coordinates of stop points for use with trajectory planning

  std::vector<TrajectoryPoint2D> trajectory_points_;
  std::vector<ObstaclePrediction> predicted_obstacles_;
  std::vector<ObstaclePrediction> predicted_pedestrians_;

  // vehicle commands
  kogmo_rtdb_obj_sfb_vehiclecmd_t vehiclecmd;

  Pose robot_pose_in_pause_;     // vehicle pose in pause mode: for detecting movements while in pause mode

  VehicleManager* vehicle_manager;    // the vehicle manager takes car of other vehicles
  BlockadeManager* blockade_manager;  // the road blockage manager takes care of road blockages

  Topology* topology; // topology encapsulates rndf, mission and other graph stuff
  RouteSampler* route_sampler;

  bool publish_traffic_lights_;
  std::vector<TrafficLightPose> traffic_light_poses_;
  uint32_t current_index_on_mission_;

  navigator_feedback_t* navigator_feedback_;
  kogmo_rtdb_obj_uc_navigator_control_t navigator_control;

  // StZone writes perimeter points into this thing
  NavigatorPerimeter navigator_perimeter;

  uint transitionCounter;

  // flag for intersections
  int32_t bIntersection;
  // flag for offroad driving
  bool bOffroad;

  bool inPause;

  RoutePlanner::Route::RouteEdgeList::iterator kturn_approach_edge;


  TrajectoryEvaluator* traj_eval_;

  std::vector<CurvePoint> mission_points_;
  std::vector<CurvePoint> sampled_mission_points_;
  std::vector<CurvePoint> smoothed_mission_points_;
  std::vector<CurvePoint> mission_bezier_points_;

  struct CurvePointComp {
    bool operator() (const CurvePoint* cp1, const CurvePoint* cp2) {
      if(cp1->y < cp2->y) {return true;}
      if(cp1->y == cp2->y) {
        if(cp1->x < cp2->x) {return true;}
      }
      return false;
    }
  };
  std::multiset<CurvePoint*, CurvePointComp> mission_points_set_;  //used to quickly find s for given way point coordinates
  std::map<double, CurvePoint*> mission_time_map_;  //used to quickly find waypoints/velocities at a given time
  uint32_t last_start_idx_;
  double min_lookahead_s_;

  // ---- likely to be removed soon...


  StLaneChange* lane_change_data;   // Internal Data

  // replanning
  bool stop_before_replanning;        //
  RndfEdge* forced_start_edge;        //

  bool has_pause_lanechange;
  GraphTools::PlaceOnGraph pause_lanechange_change_point;
  GraphPlace pause_lanechange_merge_point;
  GraphPlace pause_lanechange_merge_end_point;
  StLaneChangeLaneChangeReason pause_lanechange_change_reason;
  StLaneChangeLaneChangeType pause_lanechange_change_type;
  double pause_lanechange_lateral_offset;               // offset between the two lanes
  double pause_lanechange_change_length;                // length of the change
  double pause_lanechange_merge_length;                 // length of the merging zone
  bool pause_lanechange_merge_allowed;
  bool pause_lanechange_has_to_stop;
  StLaneChangeRecoverType pause_lanechange_recover_type;

  std::map<Vehicle*, Timestamp> pause_lanechange_obstacles_in_merge_zone;
  std::map<Vehicle*, Timestamp> pause_lanechange_merging_suckers;

    // boundaries that are drawn to obstacle map in case recovery (A*) navigation is used
    // disabled for now
  bool road_boundaries_enabled;
  bool leftBoundaryIsObstacle;          // map left lane boundary as obstacle
  bool rightBoundaryIsObstacle;         // map right lane boundary as obstacle
  std::vector<kogmo_line_2Df_t> nogoBoundaries; // vector with nogo boundary line segments
  std::vector<kogmo_line_2Df_t> road_boundaries;

private:
  void detectBlockades();
  void generateCurvePoints(double length, double maxSpeed);

  void setCellIfValid(int32_t x, int32_t y, float* cell, int32_t width, int32_t height, float value);
  void dumpMissionLines(std::string& ml_name, std::string& ml_raw_name, std::string& ml_rndf_name);
  void createGaussFilterMask();
  void staticObstacleMessage2Map(double cx, double cy);
  uint32_t missionIndex(double x, double y, uint32_t last_idx);
  uint32_t missionIndex(CurvePoint& cp, uint32_t last_idx);
  double minLookaheadLaneWidth(Route::RouteEdgeList::iterator edge_it, double front_sample_dist);
  void extendCenterLineAtMissionStart(double s0);
  void extendCenterLineAtMissionEnd(double s0, double front_sample_dist);
  double replanDistance(double velocity);
  RoutePlanner::RndfEdge* bestPredictedEdge(const RoutePlanner::TRndfEdgeSet& edges);
  RoutePlanner::RndfEdge* bestPredictedEdge(const std::map<RoutePlanner::RndfEdge*, double>& edges, double& dist_to_end);

   // message queue
  std::deque<std::string> message_buffer_;
  double last_message_timestamp_;

  // disable copy-constructor: we don't want to have copies of it, use references instead
  ChsmPlanner(const ChsmPlanner& src);

  // for simulation if we have multiple Topologies
  std::string name_;

  CurveSmoother smoother;

  ReplanReason replan_reason;

  uint8_t* static_obstacle_map_raw_;    // the raw static map is kept for visualization
  uint8_t* static_obstacle_map_tmp_;
  int16_t* static_obstacle_map_tmp_raw_;
  int16_t* static_obstacle_map_tmp_h_;
  int16_t* static_obstacle_map_tmp_v_;
  uint8_t* static_obstacle_map_;        // this is the configuration space regarding static obstacles
  int32_t* gauss_filter_buf_;
  uint32_t gauss_filter_coeff_sum_;
  int32_t static_obstacle_map_width_;
  int32_t static_obstacle_map_height_;
  double static_obstacle_map_res_;
  double static_obstacle_map_cx_;
  double static_obstacle_map_cy_;
  PerceptionObstaclePoint* static_obstacle_points_;
  int32_t static_obstacle_num_points_;
  double static_obstacle_timestamp_;
  pthread_t obstacle_map_update_thread_id_;

  IppiSize static_obstacle_map_region_size_;
  uint32_t static_obstacle_map_cs_mask_size_;
  uint32_t static_obstacle_map_cs_anchor_;

  //ObstaclePredictor* predictor_;
  bool emergency_map_clear_;
  bool emergency_stop_initiated_;

  bool quit_chsm_planner_;

  bool new_static_obstacle_map_ready_;
  pthread_mutex_t static_obstacle_map_cycle_mutex_;
  pthread_cond_t static_obstacle_map_cycle_cv_;

  std::map<double, double> replan_distance_map_;

public:
  std::map<std::string, TrafficLightState*> traffic_light_states_;

private:
  static const uint32_t pose_queue_size_;
};

} // namespace vlr

#endif
