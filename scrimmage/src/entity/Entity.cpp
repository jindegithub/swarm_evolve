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
#include <memory>

#include <scrimmage/math/State.h>
#include <scrimmage/math/Angles.h>
#include <scrimmage/entity/Entity.h>
#include <scrimmage/motion/MotionModel.h>
#include <scrimmage/motion/Controller.h>
#include <scrimmage/sensor/Sensor.h>
#include <scrimmage/sensor/Sensable.h>
#include <scrimmage/autonomy/Autonomy.h>
#include <scrimmage/parse/MissionParse.h>
#include <scrimmage/plugin_manager/PluginManager.h>
#include <scrimmage/common/Utilities.h>
#include <scrimmage/parse/ConfigParse.h>
#include <scrimmage/parse/ParseUtils.h>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

#include <scrimmage/proto/ProtoConversions.h>

using std::cout;
using std::endl;
namespace fs = boost::filesystem;
namespace sp = scrimmage_proto;


namespace scrimmage {

Entity::Entity() : health_points_(1), state_(std::make_shared<State>()),
                   active_(true), visual_changed_(false)
{
    visual_ = std::make_shared<scrimmage_proto::ContactVisual>();
}

bool Entity::init(AttributeMap &overrides,
                  std::map<std::string, std::string> &info,
                  ContactMapPtr &contacts,
                  MissionParsePtr mp,
                  std::shared_ptr<GeographicLib::LocalCartesian> proj,
                  int id, int sub_swarm_id,
                  PluginManagerPtr plugin_manager,
                  NetworkPtr network,
                  FileSearch &file_search,
                  RTreePtr &rtree)
{
    id_.set_id(id);
    id_.set_sub_swarm_id(sub_swarm_id);
    id_.set_team_id(std::stoi(info["team_id"]));

    if (mp == nullptr) {
        mp_ = std::make_shared<MissionParse>();
    } else {
        mp_ = mp;
        parse_visual(info, mp_, file_search);
    }

    // Setup lat/long x/y converter
    proj_ = proj;

    if (info.count("health") > 0) {
        health_points_ = std::stoi(info["health"]);
    }

    ////////////////////////////////////////////////////////////
    // set state
    ////////////////////////////////////////////////////////////
    state_ = std::make_shared<State>();

    double x = get("x", info, 0.0);
    double y = get("y", info, 0.0);
    double z = get("z", info, 0.0);
    state_->pos() << x, y, z;

    double vx = get("vx", info, 0.0);
    double vy = get("vy", info, 0.0);
    double vz = get("vz", info, 0.0);
    state_->vel() << vx, vy, vz;

    double roll = Angles::deg2rad(get("roll", info, 0));
    double pitch = Angles::deg2rad(get("pitch", info, 0));
    double yaw = Angles::deg2rad(get("heading", info, 0));
    state_->quat().set(roll, pitch, yaw);

    EntityPtr parent = shared_from_this();

    ConfigParse config_parse;

    ////////////////////////////////////////////////////////////
    // motion model
    ////////////////////////////////////////////////////////////
    if (info.count("motion_model") == 0) {
        motion_model_ = std::make_shared<MotionModel>();
        motion_model_->set_state(state_);
        motion_model_->set_parent(parent);
        motion_model_->set_network(network);
        // cout << "Warning: Missing motion model tag, initializing with base class" << endl;
    } else {
        motion_model_ =
            std::dynamic_pointer_cast<MotionModel>(
                plugin_manager->make_plugin("scrimmage::MotionModel",
                    info["motion_model"], file_search, config_parse,
                    overrides["motion_model"])); 

        if (motion_model_ == nullptr) {
            cout << "Failed to open motion model plugin: " << info["motion_model"] << endl;
            return false;
        }

        motion_model_->set_state(state_);
        motion_model_->set_parent(parent);
        motion_model_->set_network(network);
        motion_model_->init(info, config_parse.params());
    } 

    ////////////////////////////////////////////////////////////
    // sensor
    ////////////////////////////////////////////////////////////
    int sensor_ct = 0;
    std::string sensor_name = std::string("sensor") + std::to_string(sensor_ct);

    while (info.count(sensor_name) > 0) {
        SensorPtr sensor = 
            std::dynamic_pointer_cast<Sensor>(
                plugin_manager->make_plugin("scrimmage::Sensor",
                    info[sensor_name], file_search, config_parse,
                    overrides[sensor_name]));

        if (sensor == nullptr) {
            std::cout << "Failed to open sensor plugin: " << info[sensor_name] << std::endl;
            return false;
        }

        sensor->contacts() = contacts;
        sensor->rtree() = rtree;
        sensor->parent() = parent;
        sensor->init(config_parse.params());
        sensors_[sensor->name()].push_back(sensor);

        sensor_name = std::string("sensor") + std::to_string(++sensor_ct);
    }

    ////////////////////////////////////////////////////////////
    // sensable
    ////////////////////////////////////////////////////////////
    int sensable_ct = 0;
    std::string sensable_name = std::string("sensable") + std::to_string(sensable_ct);

    while (info.count(sensable_name) > 0) {
        SensablePtr sensable = 
            std::dynamic_pointer_cast<Sensable>(
                plugin_manager->make_plugin("scrimmage::Sensable",
                    info[sensable_name], file_search, config_parse,
                    overrides[sensable_name]));

        if (sensable == nullptr) {
            std::cout << "Failed to open sensable plugin: " << info[sensable_name] << std::endl;
            return false;
        }

        sensable->parent() = parent;
        sensable->init(config_parse.params());
        sensables_[sensable->name()].push_back(sensable);

        sensable_name = std::string("sensable") + std::to_string(++sensable_ct);
    }

    ////////////////////////////////////////////////////////////
    // autonomy
    ////////////////////////////////////////////////////////////
    int autonomy_ct = 0;
    std::string autonomy_name = std::string("autonomy") + std::to_string(autonomy_ct);

    while (info.count(autonomy_name) > 0) {
        AutonomyPtr autonomy = 
            std::dynamic_pointer_cast<Autonomy>(
                plugin_manager->make_plugin("scrimmage::Autonomy",
                    info[autonomy_name], file_search, config_parse,
                    overrides[autonomy_name]));

        if (autonomy == nullptr) {
            cout << "Failed to open autonomy plugin: " << info[autonomy_name] << endl;
            return false;
        }

        autonomy->set_rtree(rtree);
        autonomy->set_parent(parent);
        autonomy->set_projection(proj_);        
        autonomy->set_network(network);
        autonomy->set_state(motion_model_->state());
        autonomy->set_contacts(contacts);
        autonomy->set_is_controlling(true);
        autonomy->init(config_parse.params());        

        autonomies_.push_back(autonomy);
        autonomy_name = std::string("autonomy") + std::to_string(++autonomy_ct);
    }

    if (autonomies_.empty()) {
        cout << "Failed to initialize autonomy" << std::endl;
        return false;
    }

    ////////////////////////////////////////////////////////////
    // controller
    ////////////////////////////////////////////////////////////
    int ctrl_ct = 0;
    std::string ctrl_name = std::string("controller") + std::to_string(ctrl_ct);
    if (info.count(ctrl_name) == 0) {
        info[ctrl_name] = "SimpleAircraftControllerPID";
    }

    while (info.count(ctrl_name) > 0) {

        ControllerPtr controller =
            std::static_pointer_cast<Controller>(
                plugin_manager->make_plugin("scrimmage::Controller",
                    info[ctrl_name], file_search, config_parse,
                    overrides[ctrl_name]));

        if (controller == nullptr) {
            std::cout << "Failed to open controller plugin: " << info[ctrl_name] << std::endl;
            return false;
        }

        controller->set_state(state_);
        if (ctrl_ct == 0) {
            controller->set_desired_state(autonomies_.front()->desired_state());
        }
        controller->set_parent(parent);
        controller->set_network(network);
        controller->init(config_parse.params());        

        controllers_.push_back(controller);

        ctrl_ct++;
        ctrl_name = std::string("controller") + std::to_string(ctrl_ct);
    }

    if (controllers_.empty()) {
        std::cout << "Error: no controllers specified" << std::endl;
        return false;
    }

    return true;
}

bool Entity::parse_visual(std::map<std::string, std::string> &info,
                          MissionParsePtr mp, FileSearch &file_search)
{
    visual_->set_id(id_.id());
    visual_->set_opacity(1.0);
    
    ConfigParse cv_parse;
    bool mesh_found, texture_found;    
    find_model_properties(info["visual_model"], cv_parse,
                          file_search, visual_, 
                          mesh_found, texture_found);
    
    set(visual_->mutable_color(), mp->team_info()[id_.team_id()].color);
    
    std::string visual_model = boost::to_upper_copy(info["visual_model"]);

    if (mesh_found) {
        type_ = Contact::Type::MESH;
        visual_->set_visual_mode(texture_found ? scrimmage_proto::ContactVisual::TEXTURE : scrimmage_proto::ContactVisual::COLOR);
    } else if (visual_model == std::string("QUADROTOR")) {
        type_ = Contact::Type::QUADROTOR;
        visual_->set_visual_mode(scrimmage_proto::ContactVisual::COLOR);
    } else if (visual_model == std::string("AIRCRAFT")) {
        type_ = Contact::Type::AIRCRAFT;
        visual_->set_visual_mode(scrimmage_proto::ContactVisual::COLOR);
    } else if (visual_model == std::string("SPHERE")) {
        type_ = Contact::Type::SPHERE;
        visual_->set_visual_mode(scrimmage_proto::ContactVisual::COLOR);
    } else {
        type_ = Contact::Type::SPHERE;
        visual_->set_visual_mode(scrimmage_proto::ContactVisual::COLOR);
    }

    return true;
}

StatePtr &Entity::state() {return state_;}

std::vector<AutonomyPtr> &Entity::autonomies() {return autonomies_;}

MotionModelPtr &Entity::motion() {return motion_model_;}

std::vector<ControllerPtr> &Entity::controllers() {return controllers_;}

void Entity::set_id(ID &id) { id_ = id; }

ID &Entity::id() { return id_; }

void Entity::collision() { health_points_ -= 1e9; }

void Entity::hit() { health_points_--; }

void Entity::set_health_points(int health_points)
{ health_points_ = health_points; }

int Entity::health_points() { return health_points_; }

bool Entity::is_alive()
{
    return (health_points_ > 0);
}

bool Entity::posthumous(double t)
{
    bool any_autonomies =
        std::any_of(autonomies_.begin(), autonomies_.end(),
                    [t](AutonomyPtr &a) {return a->posthumous(t);});
    return any_autonomies && motion_model_->posthumous(t);
}

std::shared_ptr<GeographicLib::LocalCartesian> Entity::projection()
{ return proj_; }

MissionParsePtr Entity::mp() { return mp_; }

void Entity::set_random(RandomPtr random) { random_ = random; }

RandomPtr Entity::random() { return random_; }

Contact::Type Entity::type() { return type_; }

void Entity::set_visual_changed(bool visual_changed)
{ visual_changed_ = visual_changed; }

bool Entity::visual_changed() { return visual_changed_; }

scrimmage_proto::ContactVisualPtr &Entity::contact_visual()
{ return visual_; }

std::unordered_map<std::string, std::list<SensablePtr> > &Entity::sensables() {
    return sensables_;
}

std::unordered_map<std::string, std::list<SensorPtr> > &Entity::sensors() {
    return sensors_;
}

void Entity::set_active(bool active) { active_ = active; }

bool Entity::active() { return active_; }

void Entity::setup_desired_state() {
    if (controllers_.empty()) {
        return;
    }

    for (AutonomyPtr &autonomy : autonomies_) {
        if (autonomy->get_is_controlling()) {
            controllers_.front()->set_desired_state(autonomy->desired_state());
            break;
        }
    }
}

std::unordered_map<std::string, Service> &Entity::services() {return services_;}

} // scrimmage
