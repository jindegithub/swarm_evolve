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
#include <string>
#include <memory>
#include <future>

#include <scrimmage/common/Random.h>
//#include <scrimmage/common/Shape.h>
#include <scrimmage/parse/MissionParse.h>
#include <scrimmage/log/Log.h>
#include <scrimmage/metrics/Metrics.h>
#include <scrimmage/plugin_manager/PluginManager.h>
#include <scrimmage/sensor/Sensor.h>
#include <scrimmage/sensor/Sensable.h>
#include <scrimmage/common/Utilities.h>
#include <scrimmage/entity/Contact.h>
#include <scrimmage/common/RTree.h>
#include <scrimmage/entity/Entity.h>
#include <scrimmage/motion/MotionModel.h>
#include <scrimmage/motion/Controller.h>
#include <scrimmage/simcontrol/SimControl.h>
#include <scrimmage/simcontrol/EntityInteraction.h>
#include <scrimmage/parse/ConfigParse.h>
#include <scrimmage/parse/ParseUtils.h>
#include <scrimmage/autonomy/Autonomy.h>
#include <scrimmage/pubsub/Network.h>
#include <scrimmage/math/State.h>
#include <GeographicLib/LocalCartesian.hpp>


#include <scrimmage/proto/ProtoConversions.h>
#include <scrimmage/proto/Visual.pb.h>
#include <scrimmage/proto/Frame.pb.h>

#include <scrimmage/pubsub/Message.h>
#include <scrimmage/msgs/Event.pb.h>

namespace sc = scrimmage;
namespace sp = scrimmage_proto;
namespace sm = scrimmage_msgs;

using std::cout;
using std::endl;

namespace scrimmage {

    SimControl::SimControl() : mp_(NULL), display_progress_(false),
                               finished_(false), exit_(false)
    {
        pause(false);
        single_step(false);
        random_ = std::make_shared<Random>();

        plugin_manager_ = std::make_shared<scrimmage::PluginManager>();

        team_lookup_ = std::make_shared<std::unordered_map<int,int> >();

        contacts_mutex_.lock();
        contacts_ = std::make_shared<ContactMap>();
        contacts_mutex_.unlock();

        next_id_ = 1;

        send_shutdown_msg_ = true;
    }

    bool SimControl::init()
    {
        if (mp_ == NULL) {
            cout << "Mission Parse hasn't been set yet." << endl;
            return false;
        }

        proj_ = mp_->projection(); // get projection (origin) from mission

        if (get("show_plugins", mp_->params(), false)) {
            plugin_manager_->print_plugins("scrimmage::Autonomy", "Autonomy Plugins", file_search_);
            plugin_manager_->print_plugins("scrimmage::MotionModel", "Motion Plugins", file_search_);
            plugin_manager_->print_plugins("scrimmage::Controller", "Controller Plugins", file_search_);
            plugin_manager_->print_plugins("scrimmage::EntityInteraction", "Entity Interaction Plugins", file_search_);
            plugin_manager_->print_plugins("scrimmage::Sensor", "Sensor Plugins", file_search_);
            plugin_manager_->print_plugins("scrimmage::Sensable", "Sensable Plugins", file_search_);
        }

#if ENABLE_JSBSIM==1
        jsbsim_root_ = "./";
        if(const char* env_p = std::getenv("JSBSIM_ROOT")) {
            jsbsim_root_ = std::string(env_p);
        } else {
            cout << "Missing JSBSIM_ROOT env variable, using ./" << endl;
        }
#endif

        t0_ = mp_->t0();
        tend_ = mp_->tend();
        dt_ = mp_->dt();

        // What is the end condition?
        if (mp_->params().count("end_condition") > 0) {
            std::string cond = mp_->params()["end_condition"];

            if (cond.find("time") != std::string::npos) {
                end_conditions_ = end_conditions_ | EndConditionFlags::TIME;
            }

            if (cond.find("one_team") != std::string::npos) {
                end_conditions_ = end_conditions_ | EndConditionFlags::ONE_TEAM;
            }

            if (cond.find("none") != std::string::npos) {
                end_conditions_ = end_conditions_ | EndConditionFlags::NONE;
            }

            if (cond.find("all_dead") != std::string::npos) {
                end_conditions_ = end_conditions_ | EndConditionFlags::ALL_DEAD;
            }

        } else {
            end_conditions_ = EndConditionFlags::TIME;
        }

        // Start with the simulation paused?
        //if (mp_->start_paused() && mp_->enable_gui()) {
        if (mp_->start_paused()) {
            pause(true);
        }

        setup_timer(1.0 / dt_, mp_->time_warp());

        if (mp_->params().count("stream_port") > 0 &&
            mp_->params().count("stream_ip") > 0) {

            if (mp_->network_gui()) {
                outgoing_interface_->init_network(Interface::client,
                                                  mp_->params()["stream_ip"],
                                                  std::stoi(mp_->params()["stream_port"]));

                network_thread_ = std::thread(&Interface::init_network, &(*incoming_interface_),
                                              Interface::server,
                                              "localhost",
                                              std::stoi(mp_->params()["stream_port"])+1);
                network_thread_.detach();
            } else {
                outgoing_interface_->set_mode(Interface::shared);
                incoming_interface_->set_mode(Interface::shared);
            }

        } else {
            outgoing_interface_->set_mode(Interface::shared);
            incoming_interface_->set_mode(Interface::shared);
        }

        // Send initial gui information through GUI interface
        mp_->utm_terrain()->set_time(this->t());
        outgoing_interface_->send_utm_terrain(mp_->utm_terrain());

        // Create base shape objects
        for (auto &kv : mp_->team_info()) {
            int i = 0;
            for (Eigen::Vector3d &base_pos : kv.second.bases) {
                ShapePtr base = std::make_shared<scrimmage_proto::Shape>();
                base->set_type(scrimmage_proto::Shape::Sphere);
                sc::set(base->mutable_center(), base_pos);
                sc::set(base->mutable_color(), kv.second.color);
                base->set_radius(kv.second.radii[i]);
                base->set_persistent(true);
                shapes_[0].push_back(base);
                i++;
            }
        }

        if (mp_->params().count("seed") > 0) {
//            std::cout<<std::stoul(mp_->params()["seed"])<<"\n";
            random_->seed(std::stoul(mp_->params()["seed"]));
        } else {
            random_->seed();
        }
        log_->write_ascii("Seed: " + std::to_string(random_->get_seed()));

        int max_num_entities = 0;
        for (auto &kv : mp_->gen_info()) {
            max_num_entities += kv.second.total_count;
        }

        rtree_ = std::make_shared<scrimmage::RTree>();
        rtree_->init(max_num_entities);

        auto it_network = mp_->params().find("network");
        if (it_network == mp_->params().end()) {
            // use default perfect model
            network_ = std::make_shared<Network>();
        } else {
            ConfigParse config_parse;
            std::map<std::string, std::string> &overrides =
                mp_->attributes()["network"];
            network_ =
              std::dynamic_pointer_cast<Network>(
                  plugin_manager_->make_plugin(
                      "scrimmage::Network", it_network->second,
                      file_search_, config_parse, overrides));
            if (network_ == nullptr) {
                std::cout << "Could not initialize network: " << it_network->second << std::endl;
                return false;
            }

            network_->rtree() = rtree_;
            network_->init(config_parse.params());
        }

        // Setup simcontrol's pubsub plugin
        pubsub_ = std::make_shared<Plugin>();
        pubsub_->set_network(network_);
        pub_end_time_ = pubsub_->create_publisher("EndTime");
        pub_ent_gen_ = pubsub_->create_publisher("EntityGenerated");
        pub_ent_rm_ = pubsub_->create_publisher("EntityRemoved");
        pub_ent_pres_end_ = pubsub_->create_publisher("EntityPresentAtEnd");
        pub_ent_int_exit_ = pubsub_->create_publisher("EntityInteractionExit");
        pub_no_teams_ = pubsub_->create_publisher("NoTeamsPresent");
        pub_one_team_ = pubsub_->create_publisher("OneTeamPresent");

        log_->init_network(network_);

        // Get the list of "metrics" plugins, sorted by "order"
        for (std::string metrics_name : mp_->metrics()) {
            ConfigParse config_parse;
            std::map<std::string, std::string> &overrides =
                mp_->attributes()[metrics_name];
            MetricsPtr metrics =
                std::dynamic_pointer_cast<Metrics>(
                    plugin_manager_->make_plugin(
                        "scrimmage::Metrics", metrics_name,
                        file_search_, config_parse, overrides));

            if (metrics != nullptr) {
                metrics->set_team_lookup(team_lookup_);
                metrics->set_network(network_);
                metrics->init(config_parse.params());
                metrics_.push_back(metrics);
            } else {
                cout << "Failed to load metrics: " << metrics_name << endl;
                continue;
            }
        }

        // Get the list of "entity_interaction" plugins, sorted by "order"
        for (std::string ent_inter_name : mp_->entity_interactions()) {
            ConfigParse config_parse;
            std::map<std::string, std::string> &overrides =
                mp_->attributes()[ent_inter_name];
            EntityInteractionPtr ent_inter =
                std::dynamic_pointer_cast<EntityInteraction>(
                    plugin_manager_->make_plugin("scrimmage::EntityInteraction",
                                                 ent_inter_name, file_search_,
                                                 config_parse, overrides));

            if (ent_inter == nullptr) {
                cout << "Failed to load entity interaction plugin: ent_inter_name" << endl;
                continue;
            }

            ent_inter->set_random(random_);
            ent_inter->set_mission_parse(mp_);
            ent_inter->set_projection(proj_);
            ent_inter->set_network(network_);
            ent_inter->set_team_lookup(team_lookup_);
            ent_inter->init(mp_->params(), config_parse.params());

            // Get shapes from plugin
            shapes_[0].insert(shapes_[0].end(), ent_inter->shapes().begin(), ent_inter->shapes().end());
            ent_inter->shapes().clear();

            ent_inters_.push_back(ent_inter);
        }

        contacts_mutex_.lock();
        contacts_->reserve(max_num_entities+1);
        contacts_mutex_.unlock();

        generate_entities(0);

        if (get("show_plugins", mp_->params(), false)) {
            plugin_manager_->print_returned_plugins();
        }

        use_entity_threads_ = get("multi_threaded", mp_->params(), false);
        if (use_entity_threads_) {
            entity_pool_stop_ = false;
            num_entity_threads_ = get("num_threads", mp_->attributes()["multi_threaded"], 1);
            entity_worker_threads_.clear();
            entity_worker_threads_.reserve(num_entity_threads_);
            for (int i = 0; i < num_entity_threads_; i++) {
                entity_worker_threads_.push_back(std::thread(&SimControl::worker, this));
            }
        }

        run_send_shapes(); // draw any intial shapes

        return true;
    }

    bool SimControl::generate_entities(double t)
    {
        // Initialize each entity
        for (EntityDesc_t::iterator it = mp_->entity_descriptions().begin();
             it != mp_->entity_descriptions().end(); it++) {

            // Determine if we have to generate entities for this entity
            // description
            if (mp_->gen_info().count(it->first) == 0) {
                continue;
            }

            // Generate entities if time has been reached
            int gen_count = 0;
            for (std::vector<double>::iterator time_it = mp_->next_gen_times()[it->first].begin();
                 time_it != mp_->next_gen_times()[it->first].end(); time_it++) {
                if (t >= *time_it && mp_->gen_info()[it->first].total_count > 0) {
                    double x_var = scrimmage::get("variance_x",it->second, 100.0);
                    double y_var = scrimmage::get("variance_y",it->second, 100.0);
                    double z_var = scrimmage::get("variance_z",it->second, 0.0);

#if ENABLE_JSBSIM==1
                    it->second["JSBSIM_ROOT"] = jsbsim_root_;
#endif

                    it->second["dt"] = std::to_string(dt_);
                    double motion_multiplier = mp_->motion_multiplier();
                    it->second["motion_multiplier"] = std::to_string(motion_multiplier);

                    double x0 = scrimmage::get("x0",it->second, 0);
                    double y0 = scrimmage::get("y0",it->second, 0);
                    double z0 = scrimmage::get("z0",it->second,0);

                    Eigen::Vector3d pos(x0,y0,z0);

                    // Use variance if not the first entity in this group, or if a
                    // collision exists (This happens when you place <entity> tags"
                    // at the same location).
                    if (!mp_->gen_info()[it->first].first_in_group ||
                        collision_exists(pos)) {
                        // Use the uniform distribution to place aircraft
                        // within the x/y variance
                        bool exists = false;
                        int collision_count = 0;
                        do {
                            pos(0) = x0 + (random_->rng_uniform())*x_var;
                            pos(1) = y0 + (random_->rng_uniform())*y_var;
                            pos(2) = z0 + (random_->rng_uniform())*z_var;
                            exists = collision_exists(pos);

                            // it is possible that the user has not added enough
                            // noise and this process takes a while
                            if (exit_) {
                                return false;
                            }

                            if (collision_count > 1e6) {
                                cout << "----------------------------------" << endl;
                                cout << "ERROR: Having difficulty finding collision-free location for entity at: "
                                     << "(" << x0 << "," << y0 << "," << z0 << ")" << endl
                                     << "With variance: (" << pos(0) << "," << pos(1) << "," << pos(2) << ")" << endl;
                                return false;
                            }
                            collision_count++;
                        } while (exists);

                        it->second["x"] = std::to_string(pos(0));
                        it->second["y"] = std::to_string(pos(1));
                        it->second["z"] = std::to_string(pos(2));
                    } else {
                        mp_->gen_info()[it->first].first_in_group = false;
                    }

                    // Fill in the ent's lat/lon/alt value
                    double lat, lon, alt;
                    proj_->Reverse(pos(0), pos(1), pos(2), lat, lon, alt);
                    it->second["latitude"] = std::to_string(lat);
                    it->second["longitude"] = std::to_string(lon);
                    it->second["altitude"] = std::to_string(alt);

                    std::shared_ptr<Entity> ent = std::make_shared<Entity>();
                    ent->set_random(random_);
                    ent->set_parameter_vector(parameter_vector_);
                    ent->set_nn_path(nn_path_);
                    ent->set_nn_path2(nn_path2_);

                    contacts_mutex_.lock();
                    AttributeMap &attr_map = mp_->entity_attributes()[it->first];
                    bool ent_status = ent->init(attr_map, it->second, contacts_,mp_,
                                                proj_, next_id_, it->first,
                                                plugin_manager_,
                                                network_, file_search_, rtree_);
                    contacts_mutex_.unlock();

                    if (!ent_status) {
                        cout << "Failed to parse entity at start position: "
                             << "x=" << x0 << ", y=" << y0 << endl;
                        return false;
                    }

                    (*team_lookup_)[ent->id().id()] = ent->id().team_id();

                    contact_visuals_[ent->id().id()] = ent->contact_visual();

                    // Send the visual information to the viewer
                    outgoing_interface_->send_contact_visual(ent->contact_visual());

                    ents_.push_back(ent);
                    rtree_->add(ent->state()->pos(), ent->id());
                    contacts_mutex_.lock();
                    (*contacts_)[ent->id().id()] = Contact(ent->id(), ent->state(), ent->type(), ent->contact_visual(), ent->sensables());
                    contacts_mutex_.unlock();

                    auto msg = std::make_shared<Message<sm::EntityGenerated>>();
                    msg->data.set_entity_id(ent->id().id());
                    pubsub_->publish(t, pub_ent_gen_, msg);

                    next_id_++;
                    mp_->gen_info()[it->first].total_count--;

                    // Is rate generation enabled?
                    if (mp_->gen_info()[it->first].rate > 0) {
                        *time_it = t + (1.0 / mp_->gen_info()[it->first].rate) + random_->rng_normal()*mp_->gen_info()[it->first].time_variance;
                        if (*time_it <= t) {
                            cout << "Next generation time less than current time. "
                                 << "generate_time_variance is too large." << endl;
                            *time_it = t + (1.0 / mp_->gen_info()[it->first].rate);
                        }
                    }
                    gen_count++;

                    if (gen_count >= mp_->gen_info()[it->first].gen_count) {
                        break;
                    }
                }
            }
        }

        // Delete gen_info's that don't have ents remaining (count==0:
        for (std::map<int, GenerateInfo>::iterator it = mp_->gen_info().begin();
             it != mp_->gen_info().end() ; /* No increment*/) {
            if (it->second.total_count <= 0) {
                mp_->gen_info().erase(it++);
            } else {
                ++it;
            }
        }
        return true;
    }

    void SimControl::set_mission_parse(MissionParsePtr mp) { mp_ = mp; }

    MissionParsePtr SimControl::mp() { return mp_; }

    void SimControl::set_log(std::shared_ptr<Log> &log) { log_ = log; }

    bool SimControl::enable_gui()
    {
        return mp_->enable_gui();
    }

    void SimControl::start()
    {
        thread_ = std::thread(&SimControl::run,this);
    }

    void SimControl::display_progress(bool enable) { display_progress_ = enable; }

    void SimControl::join()
    {
        thread_.join();
    }

    void SimControl::create_rtree() {
        rtree_->clear();
        for (EntityPtr &ent: ents_) {
            rtree_->add(ent->state()->pos(), ent->id());
        }
    }

    void SimControl::set_autonomy_contacts() {
        std::map<std::string, AutonomyPtr> autonomy_map;
        for (EntityPtr &ent : ents_) {
            for (AutonomyPtr &autonomy : ent->autonomies()) {
                if (autonomy->need_reset()) {
                    std::string type = autonomy->type();
                    auto it = autonomy_map.find(type);
                    if (it == autonomy_map.end()) {
                        contacts_mutex_.lock();
                        autonomy->set_contacts(contacts_);
                        contacts_mutex_.unlock();
                        autonomy_map[type] = autonomy;
                    } else {
                        contacts_mutex_.lock();
                        autonomy->set_contacts_from_plugin(it->second);
                        contacts_mutex_.unlock();
                    }
                }
            }
        }
    }

    bool SimControl::run_interaction_detection() {
        bool any_false = false;
        for (EntityInteractionPtr ent_inter : ent_inters_) {
            bool result = ent_inter->step_entity_interaction(ents_, t_, dt_);
            if (!result) {
                cout << "Entity interaction requested simulation termination: "
                     << ent_inter->name() << endl;
            }
            any_false |= !result;
            shapes_[0].insert(shapes_[0].end(), ent_inter->shapes().begin(),
                              ent_inter->shapes().end());
        }

        // Determine if entities need to be removed
        for (auto it : ents_) {
            if (!it->is_alive() && it->posthumous(this->t())) {
                int id = it->id().id();

                auto msg = std::make_shared<Message<sm::EntityRemoved>>();
                msg->data.set_entity_id(id);
                pubsub_->publish_immediate(t_, pub_ent_rm_, msg);

                // Set the entity and contact to inactive to remove from
                // simulation
                it->set_active(false);
                auto it_cnt = contacts_->find(id);
                if (it_cnt != contacts_->end()) {
                    it_cnt->second.set_active(false);
                } else {
                    cout << "Failed to find contact to set inactive." << endl;
                }
            }
        }
        return any_false;
    }

    void SimControl::run_logging() {
        contacts_mutex_.lock();
        outgoing_interface_->send_frame(t_, contacts_);

        for (MetricsPtr metrics : metrics_) {
            metrics->step_metrics(t(), dt_);
        }
        contacts_mutex_.unlock();
    }

    void SimControl::run_remove_inactive()
    {
        auto it = ents_.begin();
        while (it != ents_.end()) {
            if (!(*it)->active()) {
                int id = (*it)->id().id();
                it = ents_.erase(it);
                contacts_mutex_.lock();
                contacts_->erase(id);
                contacts_mutex_.unlock();
            } else {
                ++it;
            }
        }
    }

    void SimControl::run()
    {
        start_overall_timer();
        // Simulate over the time range
        int loop_number = 0;
        bool exit_loop = false;
        set_time(t0_);
        bool end_condition_interaction;
        do {
            double t = this->t();
            start_loop_timer();
            if (!generate_entities(t)) {
                cout << "Failed to generate entity" << endl;
            }
            create_rtree();
            set_autonomy_contacts();
            run_entities();
            // Distribute messages from entities
            network_->clear_subscriber_msgs();
            network_->distribute();
            end_condition_interaction = run_interaction_detection();
            if (end_condition_interaction) {
                auto msg = std::make_shared<Message<sm::EntityInteractionExit>>();
                pubsub_->publish_immediate(t_, pub_ent_int_exit_, msg);
            }
            // Interaction plugins use publish_immediate, so subs will have
            // newest messages
            run_logging();

            run_remove_inactive();
            run_send_shapes();
            run_send_contact_visuals(); // send updated visuals
            if (display_progress_) {
                if (loop_number % 100 == 0) {
                    sc::display_progress((tend_ == 0) ? 1.0 : t / tend_);
                }
            }

            // Wait loop timer.
            // Stay in loop if currently paused.
            do {
                loop_wait();
                // Were we told to exit, externally?
                exit_mutex_.lock();
                if (exit_) {
                    exit_loop = true;
                }
                exit_mutex_.unlock();
                if (single_step()) {
                    single_step(false);
                    take_step_mutex_.lock();
                    take_step_ = true;
                    take_step_mutex_.unlock();
                    break;
                }
                run_check_network_msgs();
                scrimmage_proto::SimInfo info;
                info.set_time(this->t());
                info.set_desired_warp(this->time_warp());
                info.set_actual_warp(this->actual_time_warp());
                info.set_shutting_down(false);
                outgoing_interface_->send_sim_info(info);
            } while(paused() && !exit_loop);
            // Increment time and loop counter
            set_time(t + dt_);
            loop_number++;

        } while (!end_condition_interaction && !end_condition_reached(t(), dt_) && !exit_loop);

        if (use_entity_threads_) {
            entity_pool_stop_ = true;
            entity_pool_condition_var_.notify_all();
            for (std::thread &t : entity_worker_threads_) {
                t.join();
            }
        }
        // account for last step
        set_time(t() - dt_);
        loop_number--;

        // Save EntityPresentAtEnd messages
        for (EntityPtr &ent : ents_) {
            auto msg = std::make_shared<Message<sm::EntityPresentAtEnd>>();
            msg->data.set_entity_id(ent->id().id());
            pubsub_->publish_immediate(t_, pub_ent_pres_end_, msg);
        }

        //run entity interaction again so that interactions can finalize things
        run_interaction_detection();

        run_logging();

        if (display_progress_) cout << endl;

        set_finished(true);
    }

    bool SimControl::end_condition_reached(double t, double dt)
    {
        if ((static_cast<int>(end_conditions_) & static_cast<int>(EndConditionFlags::TIME)) && t > mp_->tend() + dt/2.0) {
            auto msg = std::make_shared<Message<sm::EndTime>>();
            pubsub_->publish_immediate(t, pub_end_time_, msg);
            return true;
        }

        if (ents_.empty()) {
            auto msg = std::make_shared<Message<sm::NoTeamsPresent>>();
            pubsub_->publish_immediate(t, pub_no_teams_, msg);
            if (static_cast<int>(end_conditions_) & static_cast<int>(EndConditionFlags::ALL_DEAD | EndConditionFlags::ONE_TEAM)) {
//                std::cout << std::endl << "End of Simulation: No Entities Remaining" << std::endl;
                return true;
            }
        } else {

            // Count unique team IDs
            // note that ents_ cannot be empty from the above if statement
            int team1_id = ents_.front()->id().team_id();
            if (!std::any_of(ents_.rbegin(), ents_.rend(), [=](EntityPtr &ent) {return team1_id != ent->id().team_id();})) {
                auto msg = std::make_shared<Message<sm::OneTeamPresent>>();
                pubsub_->publish_immediate(t, pub_one_team_, msg);
                //                std::cout << std::endl << "End of Simulation: One Team (" << team1_id << ")" << std::endl;
                if (static_cast<int>(end_conditions_) & static_cast<int>(EndConditionFlags::ONE_TEAM))
                    return true;
            }
        }

        return false;
    }

    Timer &SimControl::timer() {return timer_;}

    std::list<MetricsPtr> &SimControl::metrics() { return metrics_; }

    PluginManagerPtr &SimControl::plugin_manager() {return plugin_manager_;}

    FileSearch &SimControl::file_search() {return file_search_;}

    bool SimControl::take_step()
    {
        take_step_mutex_.lock();
        bool value = take_step_;
        take_step_mutex_.unlock();
        return value;
    }

    void SimControl::step_taken()
    {
        take_step_mutex_.lock();
        take_step_ = false;
        take_step_mutex_.unlock();
    }

    void SimControl::set_incoming_interface(InterfacePtr &incoming_interface)
    { incoming_interface_ = incoming_interface; }

    void SimControl::set_outgoing_interface(InterfacePtr &outgoing_interface)
    { outgoing_interface_ = outgoing_interface; }

    void SimControl::run_check_network_msgs()
    {
        // Do we have any simcontrol message updates from GUI?
        if (incoming_interface_->gui_msg_update()) {
            incoming_interface_->gui_msg_mutex.lock();
            auto &control = incoming_interface_->gui_msg();
            auto it = control.begin();
            while (it != control.end()) {
                if (it->inc_warp()) {
                    this->inc_warp();
                } else if (it->dec_warp()) {
                    this->dec_warp();
                } else if (it->toggle_pause()) {
                    this->pause(!this->paused());
                } else if (it->single_step()) {
                    this->single_step(true);
                } else if (it->shutting_down()) {
                    send_shutdown_msg_ = false;
                    this->force_exit();
                } else if (it->request_cached()) {
                    outgoing_interface_->send_cached();
                }
                control.erase(it++);
            }
            incoming_interface_->gui_msg_mutex.unlock();
        }
    }

    bool SimControl::collision_exists(Eigen::Vector3d &p)
    {
        for (EntityInteractionPtr ent_inter : ent_inters_) {
            if (ent_inter->collision_exists(ents_, p)) {
                return true;
            }
        }
        return false;
    }

    void SimControl::force_exit()
    {
        exit_mutex_.lock();
        exit_ = true;
        exit_mutex_.unlock();
    }

    bool SimControl::external_exit()
    {
        bool exit;
        exit_mutex_.lock();
        exit = exit_;
        exit_mutex_.unlock();
        return exit;
    }

    void SimControl::set_finished(bool finished)
    {
        scrimmage_proto::SimInfo info;
        info.set_time(this->t());
        info.set_desired_warp(this->time_warp());
        info.set_actual_warp(this->actual_time_warp());
        info.set_shutting_down(true);
        if (send_shutdown_msg_) {
            outgoing_interface_->send_sim_info(info);
        }

        finished_mutex_.lock();
        finished_ = finished;
        finished_mutex_.unlock();
    }

    bool SimControl::finished()
    {
        bool status;
        finished_mutex_.lock();
        status = finished_;
        finished_mutex_.unlock();

        return status;
    }

    void SimControl::get_contacts(std::unordered_map<int, Contact> &contacts)
    {
        // The Viewer GUI also looks at the contacts
        contacts_mutex_.lock();
        contacts = *contacts_;
        for (auto &kv : contacts) {
            sc::State state = *kv.second.state();
            *kv.second.state() = state;
        }
        contacts_mutex_.unlock();
    }

    void SimControl::set_contacts(ContactMapPtr &contacts)
    {
        contacts_mutex_.lock();
        contacts_ = contacts;
        contacts_mutex_.unlock();
    }

    void SimControl::get_contact_visuals(std::map<int, ContactVisualPtr> &contact_visuals)
    {
        contact_visuals = contact_visuals_;
    }

    void SimControl::set_contact_visuals(std::map<int, ContactVisualPtr> &contact_visuals)
    {
        contact_visuals_ = contact_visuals;
    }

    void SimControl::inc_warp()
    {
        timer_mutex_.lock();
        timer_.inc_warp();
        timer_mutex_.unlock();
    }

    void SimControl::dec_warp()
    {
        timer_mutex_.lock();
        timer_.dec_warp();
        timer_mutex_.unlock();
    }

    void SimControl::pause(bool pause)
    {
        paused_mutex_.lock();
        paused_ = pause;
        paused_mutex_.unlock();
    }

    bool SimControl::paused()
    {
        bool result;
        paused_mutex_.lock();
        result = paused_;
        paused_mutex_.unlock();
        return result;
    }

    double SimControl::time_warp()
    {
        double warp;
        timer_mutex_.lock();
        warp = timer_.time_warp();
        timer_mutex_.unlock();
        return warp;
    }

    double SimControl::actual_time_warp() { return -1; }

    void SimControl::set_time(double t)
    {
        time_mutex_.lock();
        t_ = t;
        time_mutex_.unlock();
    }

    double SimControl::t()
    {
        double t;
        time_mutex_.lock();
        t = t_;
        time_mutex_.unlock();
        return t;
    }

    void SimControl::setup_timer(double rate, double time_warp)
    {
        // dt_ is a period, 1 / dt_ is a rate (Hz)
        timer_mutex_.lock();
        timer_.set_iterate_rate(rate);
        timer_.set_time_warp(time_warp);
        timer_.update_time_config();
        timer_mutex_.unlock();
    }

    void SimControl::start_overall_timer()
    {
        timer_mutex_.lock();
        timer_.start_overall_timer();
        timer_mutex_.unlock();
    }

    void SimControl::start_loop_timer()
    {
        timer_mutex_.lock();
        timer_.start_loop_timer();
        timer_mutex_.unlock();
    }

    void SimControl::loop_wait()
    {
        timer_mutex_.lock();
        timer_.loop_wait();
        timer_mutex_.unlock();
    }

    void SimControl::single_step(bool value)
    {
        single_step_mutex_.lock();
        single_step_ = value;
        single_step_mutex_.unlock();
    }

    bool SimControl::single_step()
    {
        single_step_mutex_.lock();
        bool value = single_step_;
        single_step_mutex_.unlock();
        return value;
    }

    void SimControl::worker() {
        while (true) {
            entity_pool_mutex_.lock();
            entity_pool_condition_var_.wait(entity_pool_mutex_);
            entity_pool_mutex_.unlock();

            if (entity_pool_stop_) {
                break;
            }

            while (true) {
                entity_pool_mutex_.lock();
                if (entity_pool_queue_.empty()) {
                    entity_pool_mutex_.unlock();
                    break;
                }
                std::shared_ptr<Task> task = entity_pool_queue_.front();
                entity_pool_queue_.pop_front();
                entity_pool_mutex_.unlock();

                bool success = true;

                for (auto &kv : task->ent->sensables()) {
                    for (SensablePtr &sensable : kv.second) {
                        success &= sensable->update(t_, dt_);
                    }
                }

                for (auto &kv : task->ent->sensors()) {
                    for (SensorPtr &sensor : kv.second) {
                        success &= sensor->sense(t_, dt_);
                    }
                }

                for (AutonomyPtr &autonomy : task->ent->autonomies()) {
                    success &= autonomy->step_autonomy(t_, dt_);
                }

                double motion_dt = dt_ / mp_->motion_multiplier();
                double temp_t = t_;
                for (int i = 0; i < mp_->motion_multiplier(); i++) {
                    for (ControllerPtr &ctrl : task->ent->controllers()) {
                        success &= ctrl->step(temp_t, motion_dt);
                    }
                    success &= task->ent->motion()->step(temp_t, motion_dt);
                    temp_t += motion_dt;
                }

                entity_pool_mutex_.lock();
                task->prom.set_value(success);
                entity_pool_mutex_.unlock();
            }
        }
    }

    void SimControl::run_entities() {
        contacts_mutex_.lock();
        if (use_entity_threads_) {
            // put tasks on queue
            std::vector<std::future<bool>> futures;
            futures.reserve(ents_.size());

            entity_pool_mutex_.lock();
            for (EntityPtr &ent : ents_) {
                std::shared_ptr<Task> task = std::make_shared<Task>();
                task->ent = ent;
                entity_pool_queue_.push_back(task);
                futures.push_back(task->prom.get_future());
            }
            entity_pool_mutex_.unlock();

            // tell the threads to run
            entity_pool_condition_var_.notify_all();

            // wait for results
            for (std::future<bool> &future : futures) {
                future.get();
            }
        } else {
            for (EntityPtr ent : ents_) {
                for (auto &kv : ent->sensables()) {
                    for (SensablePtr &sensable : kv.second) {
                        sensable->update(t_, dt_);
                    }
                }
                for (auto &kv : ent->sensors()) {
                    for (SensorPtr &sensor : kv.second) {
                        sensor->sense(t_, dt_);
                    }
                }
                for (AutonomyPtr &autonomy : ent->autonomies()) {
                    autonomy->step_autonomy(t_, dt_);
                }
                ent->setup_desired_state();
            }
            double motion_dt = dt_ / mp_->motion_multiplier();
            double temp_t = t_;
            for (int i = 0; i < mp_->motion_multiplier(); i++) {
                for (EntityPtr ent : ents_) {
                    for (ControllerPtr &ctrl : ent->controllers()) {
                        ctrl->step(temp_t, motion_dt);
                    }
                }

                for (EntityPtr ent : ents_) {
                    ent->motion()->step(temp_t, motion_dt);
                }
                temp_t += motion_dt;
            }
            for (EntityPtr ent : ents_) {
                for (AutonomyPtr &autonomy : ent->autonomies()) {
                    if (autonomy->need_reset()) {
                        autonomy->set_state(ent->motion()->state());
                    }
                }
            }
        }
        contacts_mutex_.unlock();
        for (EntityPtr &ent : ents_) {
            std::list<ShapePtr> &shapes = shapes_[ent->id().id()];
            for (AutonomyPtr &autonomy : ent->autonomies()) {
                shapes.insert(shapes.end(), autonomy->shapes().begin(), autonomy->shapes().end());
                autonomy->shapes().clear();
            }
        }
    }

    void SimControl::run_send_shapes()
    {
        // Convert map of shapes to sp::Shapes type
        scrimmage_proto::Shapes shapes;
        shapes.set_time(this->t());
        for (auto &kv : shapes_) {
            for (auto &shape : kv.second) {
                scrimmage_proto::Shape *s = shapes.add_shape();
                *s = *shape;
            }
        }
        outgoing_interface_->send_shapes(shapes);
        shapes_.clear();
    }

    void SimControl::run_send_contact_visuals()
    {
        for (EntityPtr &ent : ents_) {
            if (ent->visual_changed()) {
                outgoing_interface_->send_contact_visual(ent->contact_visual());
                ent->set_visual_changed(false);
            }
        }
    }
}
