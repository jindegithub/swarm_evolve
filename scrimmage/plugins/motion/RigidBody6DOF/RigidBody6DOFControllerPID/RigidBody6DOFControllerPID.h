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
#ifndef RigidBody6DOFControllerPID_H_
#define RigidBody6DOFControllerPID_H_
#include "../RigidBody6DOF.h"

class RigidBody6DOFControllerPID : public RigidBody6DOF::Controller {
 public: 
    virtual void init(std::map<std::string, std::string> &params);
    virtual bool step(double t, double dt);
    virtual std::shared_ptr<Eigen::Vector4d> u() {return u_;};
 protected:
    std::shared_ptr<Eigen::Vector4d> u_;
    scrimmage::PID heading_pid_;
    scrimmage::PID alt_pid_;
    scrimmage::PID vel_pid_;
};

#endif
