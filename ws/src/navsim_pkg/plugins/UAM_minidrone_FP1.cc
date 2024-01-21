#include "gazebo/gazebo.hh"
#include <gazebo/physics/physics.hh>

#include "rclcpp/rclcpp.hpp"

#include <Eigen/Core>
#include <Eigen/Geometry>
// #include <stdio.h>

#include "navsim_msgs/msg/telemetry.hpp"
#include "navsim_msgs/msg/remote_command.hpp"
#include "navsim_msgs/msg/waypoint.hpp"
#include "navsim_msgs/msg/flight_plan.hpp"




namespace gazebo {

class UAM_minidrone_FP1 : public ModelPlugin {

private:

////////////////////////////////////////////////////////////////////////
// Gazebo

physics::ModelPtr    model;
physics::LinkPtr     link;
event::ConnectionPtr updateConnector;
common::Time         currentTime;



////////////////////////////////////////////////////////////////////////
// ROS2

std::string UAVname;
rclcpp::Node::SharedPtr rosNode;

rclcpp::Publisher<navsim_msgs::msg::Telemetry>::SharedPtr rosPub_Telemetry;
common::Time prevTelemetryPubTime;
double TelemetryPeriod = 1.0;    // seconds

rclcpp::Subscription<navsim_msgs::msg::FlightPlan>::SharedPtr rosSub_FlightPlan;
common::Time prevRosCheckTime;
double RosCheckPeriod = 2.0;     // seconds




////////////////////////////////////////////////////////////////////////
// Navigation parameters

// Flight plan 
navsim_msgs::msg::FlightPlan::SharedPtr fp = nullptr;

// Waypoint currently flying to:
// -1: no waypoint
//  0: starting point
//  1: first waypoint
// numWPs-1: last waypoint

int currentWP = -1;  

// std::vector<navsim_msgs::msg::Waypoint> route;


// rotor engine status on/off
bool  rotors_on = false;

// AutoPilot navigation command
bool   cmd_on   = false;          // (bool) motores activos 
double cmd_velX = 0.0;            // (m/s)  velocidad lineal  deseada en eje X
double cmd_velY = 0.0;            // (m/s)  velocidad lineal  deseada en eje Y
double cmd_velZ = 0.0;            // (m/s)  velocidad lineal  deseada en eje Z
double cmd_rotZ = 0.0;            // (m/s)  velocidad angular deseada en eje Z

common::Time CommandExpTime;








////////////////////////////////////////////////////////////////////////
// quadcopter parameters

const double g    = 9.8;
const double mass = 0.595;


// Velocidad de rotación de los motores (rad/s)
double w_rotor_NE;
double w_rotor_NW;
double w_rotor_SE;
double w_rotor_SW;

// Max and minimum angular velocity of the motors
const double w_max = 628.3185;      // rad/s = 15000rpm
const double w_min = 0;             // rad/s =     0rpm

//Rotors position at distance 15cms from the center of mass, inclination 45º from the axes
ignition::math::Vector3<double> pos_CM = ignition::math::Vector3<double>(     0,      0, 0);   // center of mass
ignition::math::Vector3<double> pos_NE = ignition::math::Vector3<double>( 0.075, -0.075, 0);   // cosd(45º) * 0.15m
ignition::math::Vector3<double> pos_NW = ignition::math::Vector3<double>( 0.075,  0.075, 0);
ignition::math::Vector3<double> pos_SE = ignition::math::Vector3<double>(-0.075, -0.075, 0);
ignition::math::Vector3<double> pos_SW = ignition::math::Vector3<double>(-0.075,  0.075, 0);

// Aero-dynamic thrust force constant
// Force generated by the rotors is FT = kFT * w²
const double kFT = 1.7179e-05;                 // assuming that FT_max = 0.692kg
const double w_hov = sqrt(mass * g / 4 /kFT);  // 628.3185 rad/s

//Aero-dynamic drag force constant
//Moment generated by the rotors is MDR = kMDR * w²
const double kMDR = 3.6714e-08;


//Aero-dynamic drag force constant per axis
//Drag force generated by the air friction, opposite to the velocity is FD = -kFD * r_dot*|r_dot| (depends on the shape of the object in each axis).
// Horizontal axis:
const double kFDx = 1.1902e-04;
const double kFDy = 1.1902e-04;
// Vertical axis:
const double kFDz = 36.4437e-4;


//Aero-dynamic drag moment constant per axis
//Drag moment generated by the air friction, opposite to the angular velocity is MD = -kMD * rpy_dot*|rpy_dot| (depends on the shape of the object in each axis).

// Horizontal axis:
//Assuming similar drag in both axes (although the fuselage is not equal), no gravity and the drone is propulsed by two rotors of the same side at maximum speed the maximum angular velocity is Vrp_max = 2 * 2*pi;
//operating kMDxy =  2 * FT_max * sin(deg2rad(45))^2 / Vrp_max^2 we get that...
const double kMDx = 1.1078e-04;
const double kMDy = 1.1078e-04;

//Vertical axis:
//Must be verified at maximum yaw velocity
//Assuming tjat Vyaw_max = 4*pi rad/s (max yaw velocity of 2rev/s) and w_hov2 (rotor speed to maintain the hovering)
//And taking into account that MDR = kMDR * w² and MDz = kMDz * Vyaw²
//operating MDz  = MDR (the air friction compensates the effect of the rotors) and kMDz = kMDR* (2 * w_hov2²) / Vyaw_max² we get that...
const double kMDz = 7.8914e-05;



////////////////////////////////////////////////////////////////////////
// Control matrices variables
Eigen::Matrix<double, 8, 1> x;  // model state
Eigen::Matrix<double, 4, 1> y;  // model output
Eigen::Matrix<double, 4, 8> Kx; // state control matrix
Eigen::Matrix<double, 4, 4> Ky; // error control matrix
Eigen::Matrix<double, 4, 1> Hs; // hovering speed
Eigen::Matrix<double, 4, 1> u;  // input (rotors speeds)
Eigen::Matrix<double, 4, 1> r;  // model reference
Eigen::Matrix<double, 4, 1> e;  // model error
Eigen::Matrix<double, 4, 1> E;  // model acumulated error

int E_max = 5;                    // maximun model acumulated error
common::Time prevControlTime = 0; // Fecha de la ultima actualizacion del control de bajo nivel 
        




////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

public: 
void Load(physics::ModelPtr _parent, sdf::ElementPtr /*_sdf*/)
{
    // printf("DRONE CHALLENGE Drone plugin: loading\n");

    // Get information from the model
    model = _parent;
    UAVname = model->GetName();
    link = model->GetLink("dronelink");

    // Periodic event
    updateConnector = event::Events::ConnectWorldUpdateBegin(
        std::bind(&UAM_minidrone_FP1::OnWorldUpdateBegin, this));  

    // ROS2
    if (rclcpp::ok()) 
    {
        rosNode = rclcpp::Node::make_shared(this->UAVname);

        rosPub_Telemetry = rosNode->create_publisher<navsim_msgs::msg::Telemetry>(
            "UAV/" + UAVname + "/Telemetry", 10);

        // rosSub_RemoteCommand = rosNode->create_subscription<navsim_msgs::msg::RemoteCommand>(
        //     "UAV/" + UAVname + "/RemoteCommand", 10,
        //     std::bind(&UAM_minidrone_FP1::rosTopFn_RemoteCommand, this, 
        //             std::placeholders::_1));
            
        rosSub_FlightPlan = rosNode->create_subscription<navsim_msgs::msg::FlightPlan>(
            "UAV/" + UAVname + "/FlightPlan", 2,
            std::bind(&UAM_minidrone_FP1::rosTopFn_FlightPlan, this, 
                    std::placeholders::_1));
    
    }
    else
    {   
        std::cout << "\x1B[2J\x1B[H";       // Clear screen
        printf("\nERROR: NavSim world plugin is not running ROS2!\n\n");
    }


    //Initial control matrices
    Kx <<  -47.4820, -47.4820, -9.3626, -9.3626,  413.1508, -10.5091,  10.5091,  132.4440,
            47.4820, -47.4820,  9.3626, -9.3626, -413.1508, -10.5091, -10.5091,  132.4440,
           -47.4820,  47.4820, -9.3626,  9.3626, -413.1508,  10.5091,  10.5091,  132.4440,
            47.4820,  47.4820,  9.3626,  9.3626,  413.1508,  10.5091, -10.5091,  132.4440;

    Ky <<   -8.1889,  8.1889,  294.3201,  918.1130,
            -8.1889, -8.1889,  294.3201, -918.1130,
             8.1889,  8.1889,  294.3201, -918.1130,
             8.1889, -8.1889,  294.3201,  918.1130;
    
	// Linearization point
    Hs << w_hov, w_hov, w_hov, w_hov;
    //    std::cout  << Hs << " \n\n";

	//Reference to follow
	r << 0, 0, 0, 0;

	//Cumulative error in the reference following
	E << 0, 0, 0, 0;

	//Control input signal (rotor real speeds)
	u << w_hov, w_hov, w_hov, w_hov;

}



void Init()
{
    // printf("DC Navigation event: Init\n");

    currentTime = model->GetWorld()->SimTime();
    prevTelemetryPubTime = currentTime;
    prevRosCheckTime = currentTime;



////////////////////////////asumimos un comando!!!
    // cmd_on   =  1  ;

    // cmd_velX =  2.0;
    // cmd_velY =  0.0;
    // cmd_velZ =  0.0;
    // cmd_rotZ =  1.0; 
    // // printf("UAV command: ON: %.0f             velX: %.1f    velY: %.1f    velZ: %.1f              rotZ: %.1f \n\n", 
    //         cmd_on, cmd_velX, cmd_velY, cmd_velX, cmd_rotZ);
////////////////////////////

}



void OnWorldUpdateBegin()
{
    // Clear screen
    // std::cout << "\x1B[2J\x1B[H";
    // printf("DCdrone plugin: OnWorldUpdateBegin\n");
    
    // Get current simulation time
    currentTime = model->GetWorld()->SimTime();


    // UAV fligh plan navigation
    Navigation();
    
    // Platform low level control
    ServoControl();
    PlatformDynamics();

    // Telemetry communication
    Telemetry();

    // Check ROS2 subscriptions
    CheckROS();

}



void Navigation()
{
    if (fp == nullptr) return;

    int plannedWP = GetPlannedWP();
    if (currentWP == -1 && plannedWP != 0)
    {
        // This flight plan is obsolet
        printf("OBSOLETE FLIGHT PLAN IGNORED\n");
        fp = nullptr;
        return;
    }
    
    std::vector<navsim_msgs::msg::Waypoint> route = fp->route;
    int numWPs = route.size();

    // Print navigation information
    if (currentWP != plannedWP)
    {
        if (plannedWP == 0)
            printf("WAITING AT STARTING WAYPOINT %d \n",plannedWP);
        else if (plannedWP == 1)
            printf("STARTING FLIGHT TO WAYPOINT %d \n",plannedWP);
        else if (plannedWP < numWPs)
            printf("HEADING WAYPOINT %d \n",plannedWP);
        else
            printf("FLIGHT PLAN EXPIRED\n");
    }


    currentWP = plannedWP;
    if (currentWP == 0)
    {
        // drone waiting to start the flight
        return;
    }

    if (currentWP == numWPs)
    {
        // flight plan expired
        fp = nullptr;
        currentWP = -1;
        commandOff();
        return;
    }


    // COMPUTING PLANNED POSITION
    
    double x = route[currentWP-1].pos.x; 
    double y = route[currentWP-1].pos.y;
    double z = route[currentWP-1].pos.z;

    if (currentWP < numWPs)
    {
        
        double x2 = route[currentWP].pos.x; 
        double y2 = route[currentWP].pos.y;
        double z2 = route[currentWP].pos.z;
    
        common::Time t1;
        t1.sec  = route[currentWP-1].time.sec;
        t1.nsec = route[currentWP-1].time.nanosec;

        common::Time t2;
        t2.sec  = route[currentWP].time.sec;
        t2.nsec = route[currentWP].time.nanosec;

        double step = 0.1;
        common::Time t = currentTime + step;

        double interpol = (t-t1).Double() / (t2-t1).Double() ;
        x = interpol * (x2-x) + x ;
        y = interpol * (y2-y) + y ;
        z = interpol * (z2-z) + z ;

    }

    ignition::math::Vector3<double> plannedPos = ignition::math::Vector3d(x,y,z);
    // printf("(%d.%d)      %.2f  %.2f  %.2f \n",currentTime.sec, int(currentTime.nsec/1E6) ,x, y, z);


    // COMPUTING  DRONE COMMANDED VELOCITY (to achieve planned position in 1 second)

    ignition::math::Pose3<double> pose = model->WorldPose();
    ignition::math::Vector3<double> currentPos = pose.Pos();
    // ignition::math::Vector3<double> currentVel = model->RelativeLinearVel();
    
    ignition::math::Vector3<double> currentVel = plannedPos - currentPos;

    cmd_on   = true;
    cmd_velX = currentVel.X();
    cmd_velY = currentVel.Y();
    cmd_velZ = currentVel.Z();
    cmd_rotZ = 0; 

    common::Time duration;
    duration.sec  = 1;
    duration.nsec = 0;
    CommandExpTime = currentTime + duration;


}
    



int GetPlannedWP()
{
    std::vector<navsim_msgs::msg::Waypoint> route = fp->route;
    int numWPs = route.size();

    int i;
    for(i=0; i<numWPs; i++)
    {
        common::Time WPtime;
        WPtime.sec  = route[i].time.sec;
        WPtime.nsec = route[i].time.nanosec;
        if (currentTime < WPtime)
            break;
    } 
    
    return i;

}




void rosTopFn_FlightPlan(const std::shared_ptr<navsim_msgs::msg::FlightPlan> msg)
{
    // printf("Data received in topic Flight Plan\n");
    fp = msg;
    currentWP = -1;

    printf("plan_id: %d \n",fp->plan_id);
    std::vector<navsim_msgs::msg::Waypoint> route = fp->route;
    int numWPs = route.size();             
    for(int i=0; i<numWPs; i++)
    {
        //Next waypoint
        navsim_msgs::msg::Waypoint wp = fp->route[i];
        printf("[%d]  %.2f  %.2f  %.2f    ",i ,wp.pos.x, wp.pos.y, wp.pos.z);
        printf("(%d.%d) \n",wp.time.sec, int(wp.time.nanosec/1E7));
    }
   
}




void rosTopFn_RemoteCommand(const std::shared_ptr<navsim_msgs::msg::RemoteCommand> msg)
{
    // printf("DCdrone: data received in topic Remote Pilot\n");
    // printf("Received RemoteCommand: uav=%s, on=%d, cmd=[%f, %f, %f, %f], duration=(%d, %d)\n",
    //        msg->uav_id.c_str(), 
    //        msg->on, 
    //        msg->vel.linear.x, msg->vel.linear.y, msg->vel.linear.z,
    //        msg->vel.angular.z, 
    //        msg->duration.sec, msg->duration.nanosec);
    
    // This function listen and follow remote commands
    cmd_on   =  msg->on;
    cmd_velX =  msg->vel.linear.x;
    cmd_velY =  msg->vel.linear.y;
    cmd_velZ =  msg->vel.linear.z;
    cmd_rotZ =  msg->vel.angular.z; 

    common::Time duration;
    duration.sec  = msg->duration.sec;
    duration.nsec = msg->duration.nanosec;
    CommandExpTime = currentTime + duration;
    // printf("current control time: %.3f \n", currentTime.Double());
    // printf("command duration: %.3f \n", duration.Double());
    // printf("command expiration time: %.3f \n\n", CommandExpTime.Double());



    // Filtramos comandos fuera de rango

    int velMAX = 4;

    if (cmd_velX >  velMAX)
        cmd_velX =  velMAX;
    if (cmd_velX < -velMAX)
        cmd_velX = -velMAX;
    if (cmd_velY >  velMAX)
        cmd_velY =  velMAX;
    if (cmd_velY < -velMAX)
        cmd_velY = -velMAX;
    if (cmd_velZ >  velMAX)
        cmd_velZ =  velMAX;
    if (cmd_velZ < -velMAX)
        cmd_velZ = -velMAX;
    if (cmd_rotZ >  velMAX)
        cmd_rotZ =  velMAX;
    if (cmd_rotZ < -velMAX)
        cmd_rotZ = -velMAX;
}




void commandOff()
{
    cmd_on   = false;
    cmd_velX = 0;
    cmd_velY = 0;
    cmd_velZ = 0;
    cmd_rotZ = 0; 
}




void hover()
{
    commandOff();
    cmd_on = true;
}




void rotorsOff()
{

    // Apagamos motores
    w_rotor_NE = 0;
    w_rotor_NW = 0;
    w_rotor_SE = 0;
    w_rotor_SW = 0;
    rotors_on = false;

    // Reset del control
    E << 0, 0, 0, 0;
   
}




void ServoControl()
{
    // This fucntion converts 
    // a navigation command (desired velocity vector and rotation)
    // to speeds ot the for rotors

    // printf("DRONE CHALLENGE Drone plugin: ServoControl\n");


    if (cmd_on == false)
    {
        rotorsOff();
        return;
    }


    // Check if the simulation was reset
    if  (currentTime < prevControlTime)
    {
        prevControlTime = currentTime; 

        commandOff();
        rotorsOff();
        return;
        
    }


    // Check if the command has expired
    if (CommandExpTime < currentTime)
    {
        hover();
    }


    // Check if the fly starts
    if (rotors_on == false)
    {
        rotors_on = true;
        prevControlTime = currentTime; 
    }
    // printf("previous control time: %.3f \n", prevControlTime.Double());

    double interval = (currentTime - prevControlTime).Double();
    // printf("control interval (seconds): %.6f \n", interval);
    
    prevControlTime = currentTime;


    // Getting model status
    ignition::math::Pose3<double> pose = model->WorldPose();
    // printf("drone xyz =  %.2f  %.2f  %.2f \n", pose.X(), pose.Y(), pose.Z());
    // printf("drone YPR =  %.2f  %.2f  %.2f \n", pose.Yaw(), pose.Pitch(), pose.Roll());
    ignition::math::Vector3<double> linear_vel = model->RelativeLinearVel();
    ignition::math::Vector3<double> angular_vel = model->RelativeAngularVel();
    // printf("drone         vel xyz =  %.2f  %.2f  %.2f\n",  linear_vel.X(),  linear_vel.Y(),  linear_vel.Z());
    // printf("drone angular vel xyz =  %.2f  %.2f  %.2f\n", angular_vel.X(), angular_vel.Y(), angular_vel.Z());


    //Velocities commanded in horizon axes
    Eigen::Matrix<double, 3, 1> h_cmd;
    h_cmd(0, 0) = cmd_velX;     // eXdot
    h_cmd(1, 0) = cmd_velY;     // eYdot
    h_cmd(2, 0) = cmd_velZ;     // eZdot
    // std::cout  << "h_cmd:  " << h_cmd.transpose()  << " \n\n";

    //Matrix of transformation from horizon axes to body axes
    Eigen::Matrix<double, 3, 3> horizon2body;
    horizon2body = Eigen::AngleAxisd(-x(0, 0), Eigen::Vector3d::UnitX())    // roll
                    * Eigen::AngleAxisd(-x(1, 0), Eigen::Vector3d::UnitY()); // pitch

    // Transfrom the horizon command to body command
    Eigen::Matrix<double, 3, 1> b_cmd;
    b_cmd = horizon2body * h_cmd;
    // std::cout  << "b_cmd:  " << b_cmd.transpose()  << " \n\n";


	// Assign the model state
    x(0,0) = pose.Roll();     // ePhi
    x(1,0) = pose.Pitch();    // eTheta
    x(2,0) = angular_vel.X(); // bWx
    x(3,0) = angular_vel.Y(); // bWy
    x(4,0) = angular_vel.Z(); // bWz
    x(5,0) = linear_vel.X();  // bXdot
    x(6,0) = linear_vel.Y();  // bYdot
    x(7,0) = linear_vel.Z();  // bZdot
    // std::cout  << "x:  " << x.transpose()  << " \n\n";

	// Assign the model output
    y(0,0) = linear_vel.X();  // bXdot
    y(1,0) = linear_vel.Y();  // bYdot
    y(2,0) = linear_vel.Z();  // bZdot
    y(3,0) = angular_vel.Z(); // bWz
    // std::cout  << "y:  " << y.transpose()  << " \n\n";

	// Assign the model reference to be followed
    r(0, 0) = b_cmd(0, 0);    // bXdot
    r(1, 0) = b_cmd(1, 0);    // bYdot
    r(2, 0) = b_cmd(2, 0);    // bZdot
    r(3, 0) = cmd_rotZ;       // hZdot
    // std::cout  << "r:  " << r.transpose()  << " \n\n";


	// Error between the output and the reference (between the commanded velocity and the drone velocity)
	e = y - r;
    // std::cout  << "e:  " << e.transpose()  << " \n\n";

	// Cumulative error
    E = E + (e * interval);
    // std::cout  << "E:  " << E.transpose()  << " \n\n";

    if (E(0, 0) >  E_max)   E(0, 0) =  E_max;
    if (E(0, 0) < -E_max)   E(0, 0) = -E_max;
    if (E(1, 0) >  E_max)   E(1, 0) =  E_max;
    if (E(1, 0) < -E_max)   E(1, 0) = -E_max;
    if (E(2, 0) >  E_max)   E(2, 0) =  E_max;
    if (E(2, 0) < -E_max)   E(2, 0) = -E_max;
    if (E(3, 0) >  E_max)   E(3, 0) =  E_max;
    if (E(3, 0) < -E_max)   E(3, 0) = -E_max;
    // std::cout  << "E:  " << E.transpose()  << " \n\n";

    
    // control del sistema dinamico
    u = Hs - Kx * x - Ky * E;
    // std::cout  << "u: " << u.transpose()  << " \n\n";
   
	// Saturating the rotors speed in case of exceeding the maximum or minimum rotations
	if (u(0, 0) > w_max) u(0, 0) = w_max;
	if (u(0, 0) < w_min) u(0, 0) = w_min;
	if (u(1, 0) > w_max) u(1, 0) = w_max;
	if (u(1, 0) < w_min) u(1, 0) = w_min;
	if (u(2, 0) > w_max) u(2, 0) = w_max;
	if (u(2, 0) < w_min) u(2, 0) = w_min;
	if (u(3, 0) > w_max) u(3, 0) = w_max;
	if (u(3, 0) < w_min) u(3, 0) = w_min;
    // std::cout  << "u: " << u.transpose()  << " \n\n";


    // Asignamos la rotación de los motores
    w_rotor_NE = u(0, 0);
    w_rotor_NW = u(1, 0);
    w_rotor_SE = u(2, 0);
    w_rotor_SW = u(3, 0);

}




void PlatformDynamics()
{
    // Esta funcion traduce 
    // la velocidad de rotacion de los 4 motores
    // a fuerzas y torques del solido libre

    // // con esto simulamos rotacion de sustentacion
    // w_rotor_NE = w_hov;
    // w_rotor_NW = w_rotor_NE;
    // w_rotor_SE = w_rotor_NE;
    // w_rotor_SW = w_rotor_NE;

    // // con esto simulamos rotacion de giro
    // w_rotor_NE = 411.9621;
    // w_rotor_NW = 0;
    // w_rotor_SE = 0;
    // w_rotor_SW = 411.9621;


	//Apply the thrust force to the drone
    ignition::math::Vector3<double> FT_NE = ignition::math::Vector3<double>(0, 0, kFT * pow(w_rotor_NE, 2));
    ignition::math::Vector3<double> FT_NW = ignition::math::Vector3<double>(0, 0, kFT * pow(w_rotor_NW, 2));
    ignition::math::Vector3<double> FT_SE = ignition::math::Vector3<double>(0, 0, kFT * pow(w_rotor_SE, 2));
    ignition::math::Vector3<double> FT_SW = ignition::math::Vector3<double>(0, 0, kFT * pow(w_rotor_SW, 2));
    link->AddLinkForce(FT_NE, pos_NE);
    link->AddLinkForce(FT_NW, pos_NW);
    link->AddLinkForce(FT_SE, pos_SE);
    link->AddLinkForce(FT_SW, pos_SW);


	//Apply the drag moment to the drone
    ignition::math::Vector3<double> MDR_NE = ignition::math::Vector3<double>(0, 0, kMDR * pow(w_rotor_NE, 2));
    ignition::math::Vector3<double> MDR_NW = ignition::math::Vector3<double>(0, 0, kMDR * pow(w_rotor_NW, 2));
    ignition::math::Vector3<double> MDR_SE = ignition::math::Vector3<double>(0, 0, kMDR * pow(w_rotor_SE, 2));
    ignition::math::Vector3<double> MDR_SW = ignition::math::Vector3<double>(0, 0, kMDR * pow(w_rotor_SW, 2));
    //    printf("MDR  = %.15f\n",MDR_NE.Z() - MDR_NW.Z() - MDR_SE.Z() + MDR_SW.Z());
    link->AddRelativeTorque(MDR_NE - MDR_NW - MDR_SE + MDR_SW);


	// Apply the air friction force to the drone
    ignition::math::Vector3<double> linear_vel = model->RelativeLinearVel();
    ignition::math::Vector3<double> FD = ignition::math::Vector3<double>(
        -kFDx * linear_vel.X() * fabs(linear_vel.X()),
        -kFDy * linear_vel.Y() * fabs(linear_vel.Y()),
        -kFDz * linear_vel.Z() * fabs(linear_vel.Z()));
    // printf("drone relative vel \nbZ  %.2f\n|Z| %.2f\nFDz %.2f \n\n",
    //      linear_vel.Z(), fabs(linear_vel.Z()), -kFDz * linear_vel.Z() * fabs(linear_vel.Z()) );
    link->AddLinkForce(FD, pos_CM);


	// Apply the air friction moment to the drone
    ignition::math::Vector3<double> angular_vel = model->RelativeAngularVel();
    ignition::math::Vector3<double> MD = ignition::math::Vector3<double>(
        -kMDx * angular_vel.X() * fabs(angular_vel.X()),
        -kMDy * angular_vel.Y() * fabs(angular_vel.Y()),
        -kMDz * angular_vel.Z() * fabs(angular_vel.Z()));
    link->AddRelativeTorque(MD);

}




void CheckROS()
{
    
    // Check if the simulation was reset
    if (currentTime < prevRosCheckTime)
        prevRosCheckTime = currentTime; // The simulation was reset

    double interval = (currentTime - prevRosCheckTime).Double();
    if (interval < RosCheckPeriod) return;

    // printf("UAV %s checking ROS2 subscriptions \n", UAVname.c_str());
    // printf("current time: %.3f \n\n", currentTime.Double());

    prevRosCheckTime = currentTime;

    // ROS2 events proceessing
    rclcpp::spin_some(rosNode);

}




void Telemetry()
{
    // printf("UAV Telemetry \n");

    // Check if the simulation was reset
    if (currentTime < prevTelemetryPubTime)
        prevTelemetryPubTime = currentTime; // The simulation was reset

    double interval = (currentTime - prevTelemetryPubTime).Double();
    if (interval < TelemetryPeriod) return;

    // printf("UAV %s transmitting telemetry \n", UAVname.c_str());
    // printf("current time: %.3f \n\n", currentTime.Double());

    prevTelemetryPubTime = currentTime;


    // Getting model status
    ignition::math::Pose3<double> pose = model->WorldPose();
    ignition::math::Vector3<double> linear_vel = model->RelativeLinearVel();
    ignition::math::Vector3<double> angular_vel = model->RelativeAngularVel();
    // printf("drone xyz =  %.2f  %.2f  %.2f \n", pose.X(), pose.Y(), pose.Z());
    // printf("drone YPR =  %.2f  %.2f  %.2f \n", pose.Yaw(), pose.Pitch(), pose.Roll());
    // printf("drone         vel xyz =  %.2f  %.2f  %.2f\n",  linear_vel.X(),  linear_vel.Y(),  linear_vel.Z());
    // printf("drone angular vel xyz =  %.2f  %.2f  %.2f\n", angular_vel.X(), angular_vel.Y(), angular_vel.Z());


    navsim_msgs::msg::Telemetry msg;

    msg.uav_id = UAVname;

    msg.pose.position.x    = pose.X();
    msg.pose.position.y    = pose.Y();
    msg.pose.position.z    = pose.Z();
    msg.pose.orientation.x = pose.Roll();
    msg.pose.orientation.y = pose.Pitch();
    msg.pose.orientation.z = pose.Yaw();
    msg.pose.orientation.w = 0;

    msg.velocity.linear.x  = linear_vel.X();
    msg.velocity.linear.y  = linear_vel.Y();
    msg.velocity.linear.z  = linear_vel.Z();
    msg.velocity.angular.x = angular_vel.X();
    msg.velocity.angular.y = angular_vel.Y();
    msg.velocity.angular.z = angular_vel.Z();
    
    // msg.wip = 42;
    // msg.fpip = true;
    msg.time.sec = currentTime.sec;
    msg.time.nanosec = currentTime.nsec;

    rosPub_Telemetry->publish(msg);


}




}; // class

// Register this plugin with the simulator
GZ_REGISTER_MODEL_PLUGIN(UAM_minidrone_FP1)
} // namespace gazebo
