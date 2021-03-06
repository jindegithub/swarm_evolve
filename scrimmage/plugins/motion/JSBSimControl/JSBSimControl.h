/// ---------------------------------------------------------------------------
/// @section LICENSE
///  
/// Copyright (c) 2016 Georgia Tech Research Institute (GTRI) 
///               All Rights Reserved
///  
/// The above copyright notice and this permission notice shall be included in 
/// all copies or substantial portions of the Software.
///  
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
/// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
/// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
/// DEALINGS IN THE SOFTWARE.
/// ---------------------------------------------------------------------------
/// @file filename.ext
/// @author Kevin DeMarco <kevin.demarco@gtri.gatech.edu> 
/// @author Eric Squires <eric.squires@gtri.gatech.edu>
/// @version 1.0
/// ---------------------------------------------------------------------------
/// @brief A brief description.
/// 
/// @section DESCRIPTION
/// A long description.
/// ---------------------------------------------------------------------------
#ifndef JSBSIMCONTROL_H_
#define JSBSIMCONTROL_H_
#include <scrimmage/math/Angles.h>
#include <scrimmage/motion/MotionModel.h>
#include <scrimmage/motion/Controller.h>
#include <scrimmage/common/PID.h>
#include <scrimmage/entity/Entity.h>
#include <Eigen/Dense>

#if ENABLE_JSBSIM==1
#include <FGFDMExec.h>
#include <models/FGAircraft.h>
#include <input_output/FGPropertyManager.h>
#include <initialization/FGInitialCondition.h>

typedef std::shared_ptr<JSBSim::FGFDMExec> FGFDMExecPtr;
#endif

class JSBSimControl : public scrimmage::MotionModel{
public:
     JSBSimControl();     

     virtual std::tuple<int,int,int> version();
     
     virtual bool init(std::map<std::string, std::string> &info,
                       std::map<std::string, std::string> &params);          
     virtual bool step(double time, double dt);          

    class Controller : public scrimmage::Controller {
     public:
        virtual Eigen::Vector3d &u() = 0; 
    };

protected:

#if ENABLE_JSBSIM==1 
     FGFDMExecPtr exec;
     
     JSBSim::FGPropertyNode *longitude_node_;
     JSBSim::FGPropertyNode *latitude_node_;
     JSBSim::FGPropertyNode *altitude_node_;
     
     JSBSim::FGPropertyNode *roll_node_;
     JSBSim::FGPropertyNode *pitch_node_;
     JSBSim::FGPropertyNode *yaw_node_;
     
     JSBSim::FGPropertyNode *ap_aileron_cmd_node_;     
     JSBSim::FGPropertyNode *ap_elevator_cmd_node_;
     JSBSim::FGPropertyNode *ap_rudder_cmd_node_;
     JSBSim::FGPropertyNode *ap_throttle_cmd_node_;
     
     JSBSim::FGPropertyNode *vel_north_node_;     
     JSBSim::FGPropertyNode *vel_east_node_;     
     JSBSim::FGPropertyNode *vel_down_node_;          
     JSBSim::FGPropertyNode *u_vel_node_;

     scrimmage::Angles angles_to_jsbsim_;
     scrimmage::Angles angles_from_jsbsim_;

     scrimmage::PID roll_pid_;
     scrimmage::PID pitch_pid_;
     scrimmage::PID yaw_pid_;
          
private:     
#endif 
};

#endif
