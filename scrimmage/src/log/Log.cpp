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
#include <fcntl.h>
#include <unistd.h>

#include <scrimmage/proto/Frame.pb.h>
#include <scrimmage/proto/Shape.pb.h>
#include <scrimmage/proto/Visual.pb.h>
#include <scrimmage/math/State.h>
#include <scrimmage/log/Log.h>
#include <scrimmage/proto/ProtoConversions.h>
#include <scrimmage/plugin_manager/Plugin.h>

#include <scrimmage/pubsub/Message.h>
#include <scrimmage/msgs/Collision.pb.h>

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

using std::cout;
using std::endl;

namespace sp = scrimmage_proto;

namespace scrimmage {

Log::Log() : frames_output_(), shapes_output_(), utm_terrain_output_(), contact_visual_output_()
{
    // Verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;      

    frames_fd_ = -1;
    shapes_fd_ = -1;
    utm_terrain_fd_ = -1;
    contact_visual_fd_ = -1;
    msgs_fd_ = -1;
    
    ascii_filename_ = "log.txt";
    
    frames_name_ = "frames.bin";
    shapes_name_ = "shapes.bin";
    utm_terrain_name_ = "utm_terrain.bin";
    contact_visual_name_ = "contact_visual.bin";
    msgs_name_ = "msgs.bin";

    enable_log_ = true;
}

bool Log::open_file(std::string filename, int &fd)
{
    fd = open(filename.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd == -1) {
        cout << "Failed to open file for writing: "
             << filename << endl;
        return false;
    }        
    return true;
}
    
bool Log::init(std::string dir, Log::Mode mode)
{        
    mode_ = mode;
    log_dir_ = dir;   

    ascii_filename_ = log_dir_ + "/" + ascii_filename_;

    frames_name_ = log_dir_ + "/" + frames_name_;
    shapes_name_ = log_dir_ + "/" + shapes_name_;
    utm_terrain_name_ = log_dir_ + "/" + utm_terrain_name_;
    contact_visual_name_ = log_dir_ + "/" + contact_visual_name_;
    msgs_name_ = log_dir_ + "/" + msgs_name_;
    
    if (mode_ == WRITE) {
        if (open_file(frames_name_, frames_fd_)) {
            frames_output_ = std::make_shared<google::protobuf::io::FileOutputStream>(frames_fd_);
        }
        if (open_file(shapes_name_, shapes_fd_)) {
            shapes_output_ = std::make_shared<google::protobuf::io::FileOutputStream>(shapes_fd_);
        }
        if (open_file(utm_terrain_name_, utm_terrain_fd_)) {
            utm_terrain_output_ = std::make_shared<google::protobuf::io::FileOutputStream>(utm_terrain_fd_);
        }
        if (open_file(contact_visual_name_, contact_visual_fd_)) {
            contact_visual_output_ = std::make_shared<google::protobuf::io::FileOutputStream>(contact_visual_fd_);
        }
        //if (open_file(msgs_name_, msgs_fd_)) {
        //    msgs_output_ = std::make_shared<google::protobuf::io::FileOutputStream>(msgs_fd_);
        //}    
    } else if (mode_ == READ) {
        parse(dir);
    }    

    return true;
}

void Log::init_network(NetworkPtr network)
{
    pubsub_ = std::make_shared<Plugin>();
    pubsub_->set_network(network);
    sub_ent_collisions_ = pubsub_->create_subscriber("EntityCollision");
}

bool Log::write_ascii(std::string str)
{
    if (!enable_log_) return true;
    
    if (!ascii_output_.is_open()) {
        ascii_output_.open(ascii_filename_.c_str(),
                           std::ios::out | std::ios::in | std::ios::trunc);
    }

    if (!ascii_output_.is_open()) {
        cout << "Failed to open file for writing: " << ascii_filename_ << endl;
        return false;
    }

    ascii_output_ << str << endl;
    return true;
}

bool Log::save_frame(std::shared_ptr<scrimmage_proto::Frame> &frame)
{ return writeDelimitedTo(*frame, frames_output_); }

bool Log::save_shapes(scrimmage_proto::Shapes &shapes)
{ return writeDelimitedTo(shapes, shapes_output_); }

bool Log::save_utm_terrain(std::shared_ptr<scrimmage_proto::UTMTerrain> &utm_terrain)
{ return writeDelimitedTo(*utm_terrain, utm_terrain_output_); }

bool Log::save_contact_visual(std::shared_ptr<scrimmage_proto::ContactVisual> &contact_visual)
{ return writeDelimitedTo(*contact_visual, contact_visual_output_); }

std::string Log::frames_filename() { return frames_name_; }

std::string Log::shapes_filename() { return shapes_name_; }

std::string Log::utm_terrain_filename() { return utm_terrain_name_; }

std::string Log::contact_visual_filename() { return contact_visual_name_; }

std::string Log::msgs_filename() { return msgs_name_; }

void Log::set_enable_log(bool enable) { enable_log_ = enable; }

bool Log::parse(std::string dir)
{
    if (!fs::is_directory(dir)) {
        cout << "Log directory doesn't exist: " << dir << endl;
        return false;
    }
    
    if (!fs::exists(fs::path(frames_name_))) {
        cout << "Frames file doesn't exist: " << frames_name_ << endl;
    } else {
        parse(frames_name_, FRAMES);
    }

    if (!fs::exists(fs::path(shapes_name_))) {
        cout << "Shapes file doesn't exist: " << shapes_name_ << endl;
    } else {
        parse(shapes_name_, SHAPES);
    }

    if (!fs::exists(fs::path(utm_terrain_name_))) {
        cout << "UTM Terrain file doesn't exist: " << utm_terrain_name_ << endl;
    } else {
        parse(utm_terrain_name_, UTMTERRAIN);
    }

    if (!fs::exists(fs::path(contact_visual_name_))) {
        cout << "Contact Visual file doesn't exist: " << contact_visual_name_ << endl;
    } else {
        parse(contact_visual_name_, CONTACTVISUAL);
    }

    //if (!fs::exists(fs::path(msgs_name_))) {
    //    cout << "Messages file doesn't exist: " << msgs_name_ << endl;
    //} else {
    //    cout << "No parser for messages yet." << endl;
    //    //parse(msgs_name_, CONTACTVISUAL);
    //}
    
    return true;
}

bool Log::parse(std::string filename, Log::FileType type)
{
    int input_fd = open(filename.c_str(), O_RDONLY);
    if (input_fd == -1) {
        cout << "Failed to open file: " << filename.c_str() << endl;
        return false;
    }

    ZeroCopyInputStreamPtr input = std::make_shared<google::protobuf::io::FileInputStream>(input_fd);

    if (!input) {
        cout << "Failed to open FileInputStream. Filename: "
             << filename.c_str() << endl;
    }

    if (type == FRAMES) {
        parse_frames(filename, input);
    } else if (type == SHAPES) {
        parse_shapes(filename, input);
    } else if (type == UTMTERRAIN) {
        parse_utm_terrain(filename, input);
    } else if (type == CONTACTVISUAL) {
        parse_contact_visual(filename, input);
    } else if (type == MSG) {
        //parse_contact_visual(filename, input);
        cout << "No parser for messages yet. " << type << endl;
    } else {
        cout << "Log parse(): Invalid parse type: " << type << endl;
    }
    close(input_fd);

    return true;
}

bool Log::parse_frames(std::string filename, 
                      ZeroCopyInputStreamPtr input)
{
    frames_.clear();    
    scrimmage_frames_.clear();
    bool success = false, clean_eof = false;
    do {
        std::shared_ptr<scrimmage_proto::Frame> frame = std::make_shared<scrimmage_proto::Frame>();
        success = this->readDelimitedFrom(filename, input, frame, clean_eof);
        if (clean_eof || !success) break;
        frames_.push_back(frame);        
        scrimmage_frames_.push_back(proto_2_frame(*frame));
    } while(success);

    if (!clean_eof) {
        cout << "Frames - WARNING: Clean end-of-file not detected." << endl;
    }
    return true;
}

bool Log::parse_shapes(std::string filename, 
                      ZeroCopyInputStreamPtr input)
{
    shapes_.clear();    
    bool success = false, clean_eof = false;
    do {
        std::shared_ptr<scrimmage_proto::Shapes> shapes = std::make_shared<scrimmage_proto::Shapes>();
        success = this->readDelimitedFrom(filename, input, shapes, clean_eof);
        if (clean_eof || !success) break;
        shapes_.push_back(shapes);
    } while(success);

    if (!clean_eof) {
        cout << "Shapes - WARNING: Clean end-of-file not detected." << endl;
    }
    return true;
}

bool Log::parse_utm_terrain(std::string filename, 
                            ZeroCopyInputStreamPtr input)
{
    utm_terrain_.clear();    
    bool success = false, clean_eof = false;
    do {
        std::shared_ptr<scrimmage_proto::UTMTerrain> utm_terrain = std::make_shared<scrimmage_proto::UTMTerrain>();
        success = this->readDelimitedFrom(filename, input, utm_terrain, clean_eof);
        if (clean_eof || !success) break;
        utm_terrain_.push_back(utm_terrain);
    } while(success);

    if (!clean_eof) {
        cout << "UTMTerrain - WARNING: Clean end-of-file not detected." << endl;
    }
    return true;
}

bool Log::parse_contact_visual(std::string filename, 
                               ZeroCopyInputStreamPtr input)
{
    contact_visual_.clear();    
    bool success = false, clean_eof = false;
    do {
        std::shared_ptr<scrimmage_proto::ContactVisual> contact_visual = std::make_shared<scrimmage_proto::ContactVisual>();
        success = this->readDelimitedFrom(filename, input, contact_visual, clean_eof);
        if (clean_eof || !success) break;
        contact_visual_.push_back(contact_visual);
    } while(success);

    if (!clean_eof) {
        cout << "ContactVisual - WARNING: Clean end-of-file not detected." << endl;
    }
    return true;
}

//bool Log::save_messages()
//{
//    //for (auto msg : sub_ent_collisions_->pop_msgs<Message<scrimmage_msgs::EntityCollision>>()) {
//    //    writeDelimitedTo(msg->data, msgs_output_);
//    //}
//    return true;
//}

bool Log::writeDelimitedTo(const google::protobuf::MessageLite& message,
                           ZeroCopyOutputStreamPtr rawOutput)
{
    if (mode_ == READ || !enable_log_) return true;
    
    // We create a new coded stream for each message.  Don't worry, this is fast.
    google::protobuf::io::CodedOutputStream output(rawOutput.get());

    // Write the size.
    const int size = message.ByteSize();
    output.WriteVarint32(size);

    //cout << "Writing message of size: " << size << endl;

    uint8_t* buffer = output.GetDirectBufferForNBytesAndAdvance(size);
    if (buffer != NULL) {
        // Optimization:  The message fits in one buffer, so use the faster
        // direct-to-array serialization path.
        message.SerializeWithCachedSizesToArray(buffer);
    } else {
        // Slightly-slower path when the message is multiple buffers.
        message.SerializeWithCachedSizes(&output);
        if (output.HadError()) {
            return false;
        }
    }

    return true;
}

bool Log::readDelimitedFrom(std::string filename,
                            ZeroCopyInputStreamPtr rawInput,
                            MessageLitePtr message, bool& clean_eof)
{
    // see here for the implementation: http://stackoverflow.com/a/22927149

    // We create a new coded stream for each message.  Don't worry, this is fast,
    // and it makes sure the 64MB total size limit is imposed per-message rather
    // than on the whole stream.  (See the CodedInputStream interface for more
    // info on this limit.)
    google::protobuf::io::CodedInputStream input(rawInput.get());
    const int start = input.CurrentPosition();
    clean_eof = false;

    // Read the size.
    uint32_t size;
    if (!input.ReadVarint32(&size)) {
        clean_eof = (input.CurrentPosition() == start);
        return false;
    }
    // Tell the stream not to read beyond that size.
    google::protobuf::io::CodedInputStream::Limit limit = input.PushLimit(size);

    // Parse the message.
    if (!message->MergeFromCodedStream(&input)) {
        cout << "Error: MergeFromCodedStream failed: " << filename << endl;
        return false;
    }

    if (!input.ConsumedEntireMessage()) {
        cout << "Error: ConsumedEntireMessage() failed: " << filename << endl;
        return false;
    }

    // Release the limit.
    input.PopLimit(limit);

    return true;
}

bool Log::close_log()
{
    if (ascii_output_.is_open()) {
        ascii_output_.close();
    }

    google::protobuf::ShutdownProtobufLibrary();
    frames_output_.reset();
    shapes_output_.reset();
    utm_terrain_output_.reset();
    contact_visual_output_.reset();

    return true;
}

std::list<Frame> &Log::scrimmage_frames() { return scrimmage_frames_; }

std::list<std::shared_ptr<scrimmage_proto::Frame> > &Log::frames() { return frames_; }

std::list<std::shared_ptr<scrimmage_proto::Shapes> > &Log::shapes() { return shapes_; }

std::list<std::shared_ptr<scrimmage_proto::UTMTerrain> > &Log::utm_terrain() { return utm_terrain_; }

std::list<std::shared_ptr<scrimmage_proto::ContactVisual> > &Log::contact_visual() { return contact_visual_; }

std::string Log::log_dir() { return log_dir_; }

}
