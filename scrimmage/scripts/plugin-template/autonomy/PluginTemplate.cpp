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
#include <iostream>

#include <scrimmage/plugin_manager/RegisterPlugin.h>
#include <scrimmage/entity/Entity.h>
#include <scrimmage/math/State.h>
#include <scrimmage/parse/ParseUtils.h>

#include "(>>>PLUGIN_NAME<<<).h"

using std::cout;
using std::endl;

namespace sc = scrimmage;

REGISTER_PLUGIN(scrimmage::Autonomy, (>>>PLUGIN_NAME<<<), (>>>PLUGIN_NAME<<<)_plugin)

(>>>PLUGIN_NAME<<<)::(>>>PLUGIN_NAME<<<)()
{     
}

void (>>>PLUGIN_NAME<<<)::init(std::map<std::string,std::string> &params)
{
    //double my_param = sc::get<double>("my_param", params, 10);
    
    desired_state_->vel() = Eigen::Vector3d::UnitX()*21;
    desired_state_->quat().set(0,0,state_->quat().yaw());    
    desired_state_->pos() = Eigen::Vector3d::UnitZ()*state_->pos()(2);
}

bool (>>>PLUGIN_NAME<<<)::step_autonomy(double t, double dt)
{
    return true;
}
