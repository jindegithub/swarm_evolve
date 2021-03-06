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
#include <iomanip>
#include <chrono>
#include <ctime>
#include <signal.h>

#include <cstdlib>
#include <string>
#include <sstream>
#include <cstdlib>
#include <memory>
#include <scrimmage/parse/MissionParse.h>
#include <scrimmage/common/Utilities.h>

#include <scrimmage/entity/Entity.h>
#include <scrimmage/autonomy/Autonomy.h>
#include <scrimmage/entity/Contact.h>

#include <scrimmage/simcontrol/SimControl.h>
#include <scrimmage/viewer/Viewer.h>

#include <scrimmage/log/Log.h>
#include <scrimmage/log/Frame.h>
#include <boost/filesystem.hpp>

using std::cout;
using std::endl;

namespace fs = boost::filesystem;
namespace sc = scrimmage;

// Handle kill signal
std::mutex mutex_;
bool quit = false;
void HandleSignal(int s)
{
    cout << endl << "Exiting gracefully" << endl;
    mutex_.lock();
    quit = true;
    mutex_.unlock();
}

// Setup SimControl (interface between log file and Viewer)
sc::SimControl simcontrol;

void playback_loop(std::list<sc::Frame> frames, sc::MissionParsePtr mp)
{
    double time_warp = 5;

    simcontrol.setup_timer(1.0 / mp->dt(), time_warp);
    simcontrol.start_overall_timer();
    simcontrol.single_step(false);
    simcontrol.pause(true);

    std::map<int, sc::ContactVisualPtr> contact_visuals;
    
    bool exit_loop = false;
    sc::FileSearch file_search;
    for (sc::Frame &frame : frames) {
        simcontrol.start_loop_timer();
        
        for (auto &kv : *frame.contacts_) {
            // Are there any new contacts?
            if (contact_visuals.count(kv.first) == 0) {
                // Get the entity ID from the frame
                sc::ID id = kv.second.id();                
            
                sc::Entity ent;
                ent.set_id(id);
                ent.parse_visual(mp->entity_descriptions()[id.sub_swarm_id()], mp, file_search);
                contact_visuals[kv.first] = ent.contact_visual();
                simcontrol.set_contact_visuals(contact_visuals);               
            }            
        }
        
        cout << frame.time_ << endl;
        
        simcontrol.set_time(frame.time_);
        simcontrol.set_contacts(frame.contacts_);
        
        // Wait loop timer.
        // Stay in loop if currently paused.
        do {
            simcontrol.loop_wait();
        
            mutex_.lock();
            bool quit_test = quit;
            mutex_.unlock();
        
            if (simcontrol.single_step()) {
                simcontrol.single_step(false);
                break;
            }
        
            if (quit_test || simcontrol.external_exit()) {
                exit_loop = true;
            }
        } while(simcontrol.paused() && !exit_loop);
        if (exit_loop) break;
    }
    simcontrol.set_finished(true);
}

int main(int argc, char *argv[])
{
    // Handle kill signals
    struct sigaction sa;
    memset( &sa, 0, sizeof(sa) );
    sa.sa_handler = HandleSignal;
    sigfillset(&sa.sa_mask);
    sigaction(SIGINT,&sa,NULL);
    sigaction(SIGTERM,&sa,NULL);

    if (optind >= argc || argc < 2) {
        cout << "usage: " << argv[0] << " /path/to/log-dir" << endl;
        return -1;
    }

    std::string log_dir = argv[1];
    if (!fs::is_directory(log_dir)) {
        cout << "Log directory doesn't exist: " << log_dir << endl;
        return -1;
    }

    std::string bin_file = log_dir + "/frames.bin";
    std::string mission_file = log_dir + "/mission.xml";
    // Search for frames.bin and mission.xml file.
    if (!fs::exists(fs::path(bin_file))) {
        cout << "Bin file doesn't exist: " << bin_file << endl;
        return -1;
    }

    if (!fs::exists(fs::path(mission_file))) {
        cout << "Mission file doesn't exist: " << mission_file << endl;
        return -1;
    }

    // Parse mission config file:
    sc::MissionParsePtr mp = std::make_shared<sc::MissionParse>();
    if (!mp->parse(mission_file)) {
        cout << "Failed to parse file: " << mission_file << endl;
        return -1;
    }

    // Setup Logger
    scrimmage::Log log;
    log.init(log_dir, scrimmage::Log::READ);
    //log.parse(bin_file, scrimmage::Log::FRAMES);
    cout << "Frames parsed: " << log.scrimmage_frames().size() << endl;

    // Extract the entity visuals from the mission parser before kicking
    // off the simcontrol thread, look at the first frame only
    simcontrol.set_mission_parse(mp);
    //simcontrol.generate_entities(true, mp->t0()-1);        

    // Kick off playback thread
    std::thread thread(&playback_loop, log.scrimmage_frames(), mp);

    // Start up visualization (in main thread - VTK reasons)
    scrimmage::Viewer viewer;
    //viewer.set_mission_parser(mp);
    //viewer.set_simcontrol(&simcontrol);
    //viewer.run();
    viewer.init();
    viewer.run();

    simcontrol.force_exit();
    thread.join();

    // Close the log file
    log.close_log();

    cout << "Playback Complete" << endl;
    return 0;
}
