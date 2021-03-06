#include <ros/ros.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <sepanta_msgs/upperbodymotors.h>
#include <sepanta_msgs/upperbodymotorsfeedback.h>
#include <sepanta_msgs/motorfeedback.h>
#include <sepanta_msgs/motor.h>
#include <sepanta_msgs/motorreset.h>
#include <sepanta_msgs/motorpid.h>
#include <sepanta_msgs/motortorque.h>
#include <sepanta_msgs/grip.h>
#include <std_msgs/String.h>
#include <boost/thread.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <sepanta_msgs/command.h>
#include <ik/simple_IK.h>
#include <ik/traj_IK.h>
#include <upperbody_core/sepanta_ik.h>

IKSystem::IKSystem(ros::NodeHandle n)
{
   node_handle = n;
   init();
}

void IKSystem::init_configs()
{
    motor_range m;

    m.max = 1023;
    m.min = 0;
    m.degree = 300;
    m.name = "RX-28";
    list_motor_configs.push_back(m);
    //===========================================
   
    m.max = 1023;
    m.min = 0;
    m.degree = 300;
    m.name = "RX-64";
    list_motor_configs.push_back(m);
    //===========================================
  
    m.max = 4095;
    m.min = 0;
    m.degree = 360;
    m.name = "MX-28";
    list_motor_configs.push_back(m);
    //===========================================
    
    m.max = 4095;
    m.min = 0;
    m.degree = 360;
    m.name = "MX-64";
    list_motor_configs.push_back(m);
    //===========================================
  
    m.max = 4095;
    m.min = 0;
    m.degree = 360;
    m.name = "MX-106";
    list_motor_configs.push_back(m);
}

float IKSystem::convert_motor_to_degree(string model_name,float value)
{
    if ( model_name == "MX-106")
    {
        int max_deg = list_motor_configs.at(4).degree;
        int min_pos = list_motor_configs.at(4).min;
        int max_pos = list_motor_configs.at(4).max;
        float result = roundf(((max_deg * float(value - min_pos)) / (max_pos - min_pos + 1)) - (max_deg / 2));
        return result;
    }
    else
    if ( model_name == "MX-64")
    {
        int max_deg = list_motor_configs.at(3).degree;
        int min_pos = list_motor_configs.at(3).min;
        int max_pos = list_motor_configs.at(3).max;
        float result = roundf(((max_deg * float(value - min_pos)) / (max_pos - min_pos + 1)) - (max_deg / 2));
        return result;
    }

    return -1;
}

float IKSystem::convert_degreeMotor_to_degreeIK(int index,float value)
{
    if ( index == 1) return (-1 * (value - 85) / 1.64); else
    if ( index == 2) return (-1 * (value - 27) / 1.64); else
    if ( index == 3) return (-1 * (value - 22) / 1.64); else 
    return -1;
}

float IKSystem::convert_degreeIK_to_degreeMotor(int index,float value)
{
    if ( index == 1) return (value * -1 * 1.64 + 85); else
    if ( index == 2) return (value * -1 * 1.64 + 27); else
    if ( index == 3) return (value * -1 * 1.64 + 22); else 

    return -1;
}

int IKSystem::convert_degree_to_motor(string model_name,float value)
{
    if ( model_name == "MX-106")
    {
        int max_deg = list_motor_configs.at(4).degree;
        int min_pos = list_motor_configs.at(4).min;
        int max_pos = list_motor_configs.at(4).max;
        int pos = 0;
        pos = (int)(roundf((max_pos - 1) * ((max_deg / 2 + float(value)) / max_deg)));
        pos = min(max(pos, 0), max_pos - 1);
        return pos;
    }
    else
    if ( model_name == "MX-64")
    {
        int max_deg = list_motor_configs.at(3).degree;
        int min_pos = list_motor_configs.at(3).min;
        int max_pos = list_motor_configs.at(3).max;
        int pos = 0;
        pos = (int)(roundf((max_pos - 1) * ((max_deg / 2 + float(value)) / max_deg)));
        pos = min(max(pos, 0), max_pos - 1);
        return pos;
    }

   
    return -1;
}

void IKSystem::scale_velocity(double * v1,double * v2 , double * v3,int size)
{
   double max = 0;
   for ( int i = 0 ; i < size ; i++)
   {
   	  if ( abs(v1[i]) > max ) max = abs(v1[i]);
   	  if ( abs(v2[i]) > max ) max = abs(v2[i]);
   	  if ( abs(v3[i]) > max ) max = abs(v3[i]);
   }
   
   for  ( int i = 0 ; i < size ; i++)
   {
   	   v1[i] = motor_max_speed * v1[i] / max;
   	   v2[i] = motor_max_speed * v2[i] / max;
   	   v3[i] = motor_max_speed * v3[i] / max;
   }
}

bool IKSystem::convert_all_positions(double * positions,int size)
{

   for ( int i = 0 ; i < size ; i++)
   {
       positions[i] =  convert_degree_to_motor("MX-64",convert_degreeIK_to_degreeMotor(1,Rad2Deg(positions[i])));
       if ( positions[i] > current_data[0].max || positions[i] < current_data[0].min ) return false;
       
       positions[i + size] =  convert_degree_to_motor("MX-64",convert_degreeIK_to_degreeMotor(2,Rad2Deg(positions[i + size])));
       if ( positions[i + size] > current_data[1].max || positions[i + size] < current_data[1].min ) return false;
       
       positions[i + 2 * size] =  convert_degree_to_motor("MX-64",convert_degreeIK_to_degreeMotor(3,Rad2Deg(positions[i + 2 * size])));
       if ( positions[i + size * 2] > current_data[2].max || positions[i + 2 * size] < current_data[2].min ) return false;
   }

   return true;
}

int IKSystem::convert_radPerSecond_to_speedMotor(double value)
{
    int direction = 0;

    if (value < 0) 
         direction = 1024 ;
    
 
    double max_value = 1023 * 0.114 * 6;

    //value = min(max(value, -max_value), max_value);
    int speed = (int)(round(direction + abs(value) / (6 * 0.114)));

    return speed;
}

void IKSystem::motors_callback(const sepanta_msgs::upperbodymotorsfeedback::ConstPtr &msg)
{
   sepanta_msgs::motorfeedback m1 = msg->motorfeedbacks.at(0);
   sepanta_msgs::motorfeedback m2 = msg->motorfeedbacks.at(1);
   sepanta_msgs::motorfeedback m3 = msg->motorfeedbacks.at(2);
 
   ros::Time timeNow = ros::Time::now();

   current_data[0].position = m1.position;
   current_data[0].speed = m1.speed;
   current_data[0].min = m1.min;
   current_data[0].init = m1.init;
   current_data[0].max = m1.max;
   current_data[0].timestamp = timeNow;

   current_data[1].position = m2.position;
   current_data[1].speed = m2.speed;
   current_data[1].min = m2.min;
   current_data[1].init = m2.init;
   current_data[1].max = m2.max;
   current_data[1].timestamp = timeNow;

   current_data[2].position = m3.position;
   current_data[2].speed = m3.speed;
   current_data[2].min = m3.min;
   current_data[2].init = m3.init;
   current_data[2].max = m3.max;
   current_data[2].timestamp = timeNow;

   double p1  = Deg2Rad(convert_degreeMotor_to_degreeIK(1,convert_motor_to_degree("MX-64",current_data[0].position)));
   double p2  = Deg2Rad(convert_degreeMotor_to_degreeIK(2,convert_motor_to_degree("MX-64",current_data[1].position)));
   double p3  = Deg2Rad(convert_degreeMotor_to_degreeIK(3,convert_motor_to_degree("MX-64",current_data[2].position)));
  
   current_data[0].position_rad_ik = p1;
   current_data[1].position_rad_ik = p2;
   current_data[2].position_rad_ik = p3;
 
   double c1  = convert_degree_to_motor("MX-64", convert_degreeIK_to_degreeMotor(1,Rad2Deg(p1)));
   double c2  = convert_degree_to_motor("MX-64", convert_degreeIK_to_degreeMotor(2,Rad2Deg(p2)));
   double c3  = convert_degree_to_motor("MX-64", convert_degreeIK_to_degreeMotor(3,Rad2Deg(p3)));
  
   //std::cout<<"check 1 : "<<current_data[0].position<<" alireza 1 : "<<p1<<" motor 1 : "<<c1<<std::endl;
   //std::cout<<"check 2 : "<<current_data[1].position<<" alireza 2 : "<<p2<<" motor 2 : "<<c2<<std::endl;
   //std::cout<<"check 3 : "<<current_data[2].position<<" alireza 3 : "<<p3<<" motor 3 : "<<c3<<std::endl;
}

void IKSystem::update_arm_motors(int positions[3],int speed[3])
{
    sepanta_msgs::motor _msg;

    _msg.position = positions[0];
    _msg.speed = abs(speed[0]);
   // cout<<"1 :"<<_msg.position<<" "<<_msg.speed<<endl;
    motor_pub[0].publish(_msg);

    _msg.position = positions[1];
    _msg.speed = abs(speed[1]);
  //  cout<<"2 :"<<_msg.position<<" "<<_msg.speed<<endl;
    motor_pub[1].publish(_msg);

    _msg.position = positions[2];
    _msg.speed = abs(speed[2]);
  //  cout<<"3 :"<<_msg.position<<" "<<_msg.speed<<endl;
    motor_pub[2].publish(_msg);
}

void IKSystem::update_pen_motor(int position,int speed)
{
    sepanta_msgs::motor _msg;
    _msg.position = position;
    _msg.speed = speed;
    motor_pub[4].publish(_msg);
}

bool IKSystem::SepantaIK(double x0, double y0, double (&q_goal)[3])
{
    double l1 = 220;
    double l2 = 246;
    double c2 = ((x0*x0)+(y0*y0)-(l1*l1)-(l2*l2))/(2*l1*l2);
    double q21 = atan2(sqrt(1-(c2*c2)),c2);
    double q22 = atan2(-sqrt(1-(c2*c2)),c2);
    double q2 = 0;
    if(q21<3.313 && q21>0.217) q2 = q21;
    else if(q22<3.313 && q22>0.217) q2 = q22;
    else return false;

    double k1 = l1+l2*cos(q2);
    double k2 = l2*sin(q2);
    double q1 = atan2(y0,x0)-atan2(k2,k1);    

    q_goal[0] = q1;
    q_goal[1] = q2;
    q_goal[2] = 1.5707963267948966 - (q1+q2);

    return true;
}

bool IKSystem::open_challange(double q0[3], double x0, double y0, double (&q_goal)[3])
{
  double T1[16] = {0};
  int i0;
  static const signed char iv0[4] = { 0, 0, -1, 0 };

  static const signed char iv1[4] = { 0, 0, 0, 1 };

  double E;
  double q2;
  double q3;
  double er1;
  double er2;
  double B;
  double x;

  //  --- get next point and give to IK ---
  //  -----------------------
  T1[12] = x0;
  T1[13] = y0;

  for (i0 = 0; i0 < 4; i0++)
  {
    T1[2 + (i0 << 2)] = iv0[i0];
    T1[3 + (i0 << 2)] = iv1[i0];
  }

  //  === newq = ik_sepanta(q0,T1,L) ===
  E = 100.0;

  //  ----------------------
  q2 = q0[0];
  q3 = q0[1];

  //  ----------------------
  //  ----------------------
  //  ----------------------
  int max_iteration = 100000;
  while (ros::ok()) 
  {
    //cout<<"Error :"<<E<<" "<<max_iteration<<endl;
    er1 = (22.0 * cos(q2) + 24.8 * cos(q2 + q3)) - T1[12] + 8.5;
    er2 = (22.0 * sin(q2) + 24.8 * sin(q2 + q3)) - T1[13] + 8;
    E = er1 * er1 + er2 * er2;
    B = q2 + q3;
    x = q2 + q3;
    q2 += -0.01 * (2.0 * er1 * (-22.0 * sin(q2) - 24.8 * sin(q2 + q3)) + 2.0 * er2 * (22.0 * cos(q2) + 24.8 * cos(q2 + q3)));
    q3 += -0.01 * (2.0 * er1 * (-24.8 * sin(B)) + 2.0 * er2 * (24.8 * cos(x)));
  
    if((int)E<20 || max_iteration < 0) break;
    max_iteration --;
  }

   // cout<<"Error :"<<E<<endl;

  if ( max_iteration <= 0 )
  {
    //problem
    return false;
  }

  //  ----------------------
  q_goal[0] = q2;
  q_goal[1] = q3;
  q_goal[2] = (-1.5707963267948966 - q2) - q3;

  
  return true;
}

string IKSystem::go_to_xy(int x,int y,int time)
{
     double q0[3] =  { current_data[0].position_rad_ik, 
                       current_data[1].position_rad_ik,
                       current_data[2].position_rad_ik};

     cout<<"IK start "<<q0[0]<<" "<<q0[1]<<" "<<q0[2]<<endl;

     double f[2] = { x / 10 , y / 10};
     int frq=40;
     int TE=time;
     int samples = frq*TE;
     emxArray_real_T v1;
     v1.data = new double[samples];
     v1.size = new int[2];
     v1.size[0]=1;
     v1.size[1]=samples;
     v1.allocatedSize=samples;
     v1.numDimensions=2;
     v1.canFreeData=false;
     
     emxArray_real_T v2;
     v2.data = new double[samples];
     v2.size = new int[2];
     v2.size[0]=1;
     v2.size[1]=samples;
     v2.allocatedSize=samples;
     v2.numDimensions=2;
     v2.canFreeData=false;

     emxArray_real_T v3;
     v3.data = new double[samples];
     v3.size = new int[2];
     v3.size[0]=1;
     v3.size[1]=samples;
     v3.allocatedSize=samples;
     v3.numDimensions=2;
     v3.canFreeData=false;

     emxArray_real_T q1;
     q1.data = new double[samples * 3];
     q1.size = new int[2];
     q1.size[0]=3;
     q1.size[1]=samples;
     q1.allocatedSize=samples * 3;
     q1.numDimensions=2;
     q1.canFreeData=false;

     bool result = traj_IK(q0, f,TE,frq,&q1, &v1, &v2,&v3);

    if ( result )
    {
        bool result2 = convert_all_positions(q1.data,samples);
        if ( result2 == false )
        {
           //cout<<"Motor Limitation error - go to cancled"<<endl;
           return "ERROR MOTOR LIMITATION";
        }
        scale_velocity(v1.data,v2.data,v3.data,samples);

        int speeds[3];
        int positions[3];

        for ( int i = 1 ; i < samples ; i++ )
        {
            positions[0] = (int)q1.data[i];
            positions[1] = (int)q1.data[i + samples];
            positions[2] = (int)q1.data[i + 2 * samples];
              
            speeds[0] = (int)v1.data[i];
            speeds[1] = (int)v2.data[i];
            speeds[2] = (int)v3.data[i];

        	  update_arm_motors(positions,speeds);

            //cout<<"step :"<<i<<endl;
        	  boost::this_thread::sleep(boost::posix_time::milliseconds(1000 / frq));
        }

        return "DONE";
    }
    else
    {
        return "ERROR IK";
    }

}

void IKSystem::init()
{
  init_configs();

  ros::NodeHandle n2;
  sub_handles = n2.subscribe("/upperbodycoreout_feedback", 10, &IKSystem::motors_callback,this);
  //============================================================================================
  motor_pub[0] = node_handle.advertise<sepanta_msgs::motor>("/upperbodycorein_right_1", 1);
  motor_pub[1] = node_handle.advertise<sepanta_msgs::motor>("/upperbodycorein_right_2", 1);
  motor_pub[2] = node_handle.advertise<sepanta_msgs::motor>("/upperbodycorein_right_3", 1);
  motor_pub[3] = node_handle.advertise<sepanta_msgs::motor>("/upperbodycorein_right_4", 1);
  motor_pub[4] = node_handle.advertise<sepanta_msgs::motor>("/upperbodycorein_right_5", 1);
  //============================================================================================
  cout<<"Init Done"<<endl;
}



