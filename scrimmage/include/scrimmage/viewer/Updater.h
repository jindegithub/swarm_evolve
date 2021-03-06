#ifndef UPDATER_H_
#define UPDATER_H_
/// ---------------------------------------------------------------------------
/// @file Updater.h
/// @author Kevin DeMarco <kevin.demarco@gmail.com>
///
/// Time-stamp: <2016-10-12 11:33:48 syllogismrxs>
///
/// @version 1.0
/// Created: 24 Feb 2017
///
/// ---------------------------------------------------------------------------
/// @section LICENSE
///
/// The MIT License (MIT)
/// Copyright (c) 2012 Kevin DeMarco
///
/// Permission is hereby granted, free of charge, to any person obtaining a
/// copy of this software and associated documentation files (the "Software"),
/// to deal in the Software without restriction, including without limitation
/// the rights to use, copy, modify, merge, publish, distribute, sublicense,
/// and/or sell copies of the Software, and to permit persons to whom the
/// Software is furnished to do so, subject to the following conditions:
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
/// @section DESCRIPTION
///
/// The Updater class ...
///
/// ---------------------------------------------------------------------------
#include <time.h>
#include <memory>

#include <vtkCommand.h>
#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkActor.h>
#include <vtkFollower.h>
#include <vtkTextActor.h>

#include <limits>

#include <scrimmage/viewer/Grid.h>
#include <scrimmage/viewer/OriginAxes.h>
#include <scrimmage/network/Interface.h>
#include <scrimmage/proto/Contact.pb.h>
#include <scrimmage/proto/Frame.pb.h>
#include <scrimmage/proto/Color.pb.h>
#include <scrimmage/proto/Visual.pb.h>

namespace sp = scrimmage_proto;

namespace scrimmage {

    class ActorContact
    {
    public:
        ActorContact() {}
        vtkSmartPointer<vtkActor> actor;
        vtkSmartPointer<vtkFollower> label;
        std::list<vtkSmartPointer<vtkActor> > trail;
        scrimmage_proto::Color color;
        scrimmage_proto::Contact contact;
        bool exists;
        bool remove;
    };

    class Updater : public vtkCommand {
    public:
        enum class ViewMode {
            FOLLOW = 0,
            FREE,
            OFFSET
        };

        static Updater *New();

        Updater();

        virtual void Execute(vtkObject *caller, unsigned long eventId,
                             void * vtkNotUsed(callData));

        void enable_fps();

        bool update();
        bool update_contacts(std::shared_ptr<scrimmage_proto::Frame> &frame);
        bool draw_shapes(scrimmage_proto::Shapes &shapes);
        bool update_scale();
        bool update_camera();
        bool update_text_display();
        bool update_shapes();

        bool update_utm_terrain(std::shared_ptr<scrimmage_proto::UTMTerrain> &utm);

        void set_max_update_rate(double max_update_rate);

        void set_renderer(vtkSmartPointer<vtkRenderer> &renderer);

        void set_rwi(vtkSmartPointer<vtkRenderWindowInteractor> &rwi);

        void set_incoming_interface(InterfacePtr &incoming_interface);

        void set_outgoing_interface(InterfacePtr &outgoing_interface);

        void init();

        void next_mode();

        void inc_follow();
        void dec_follow();

        void toggle_trails();

        void inc_warp();

        void dec_warp();

        void toggle_pause();

        void single_step();

        void request_cached();

        void shutting_down();

        void inc_scale();

        void dec_scale();

        void inc_follow_offset();
        void dec_follow_offset();

        void reset_scale();

        void reset_view();

        void create_text_display();

        void update_trail(std::shared_ptr<ActorContact> &actor_contact,
                          double &x_pos, double &y_pos, double &z_pos);

        void update_contact_visual(std::shared_ptr<ActorContact> &actor_contact,
                                   std::shared_ptr<scrimmage_proto::ContactVisual> &cv);

        bool draw_triangle(const scrimmage_proto::Shape &s,
                           vtkSmartPointer<vtkActor> &actor,
                           vtkSmartPointer<vtkPolyDataMapper> &mapper);
        bool draw_arrow(const scrimmage_proto::Shape &s,
                        vtkSmartPointer<vtkActor> &actor,
                        vtkSmartPointer<vtkPolyDataMapper> &mapper);
        bool draw_cone(const scrimmage_proto::Shape &s,
                       vtkSmartPointer<vtkActor> &actor,
                       vtkSmartPointer<vtkPolyDataMapper> &mapper);
        bool draw_line(const scrimmage_proto::Shape &s,
                       vtkSmartPointer<vtkActor> &actor,
                       vtkSmartPointer<vtkPolyDataMapper> &mapper);
        bool draw_polygon(const scrimmage_proto::Shape &s,
                          vtkSmartPointer<vtkActor> &actor,
                          vtkSmartPointer<vtkPolyDataMapper> &mapper);
        bool draw_polydata(const scrimmage_proto::Shape &s,
                           vtkSmartPointer<vtkActor> &actor,
                           vtkSmartPointer<vtkPolyDataMapper> &mapper);        
        bool draw_plane(const scrimmage_proto::Shape &s,
                        vtkSmartPointer<vtkActor> &actor,
                        vtkSmartPointer<vtkPolyDataMapper> &mapper);
        bool draw_pointcloud(const scrimmage_proto::Shape &s,
                             vtkSmartPointer<vtkActor> &actor,
                             vtkSmartPointer<vtkPolyDataMapper> &mapper);
        bool draw_sphere(const scrimmage_proto::Shape &s,
                         vtkSmartPointer<vtkActor> &actor,
                         vtkSmartPointer<vtkPolyDataMapper> &mapper);
        bool draw_circle(const scrimmage_proto::Shape &s,
                         vtkSmartPointer<vtkActor> &actor,
                         vtkSmartPointer<vtkPolyDataMapper> &mapper);

    protected:
        vtkSmartPointer<vtkRenderWindowInteractor> rwi_;
        vtkSmartPointer<vtkRenderer> renderer_;

        double frame_time_;

        int update_count;
        struct timespec prev_time;
        double max_update_rate_;
        std::shared_ptr<Grid> grid_;
        std::shared_ptr<OriginAxes> origin_axes_;

        InterfacePtr incoming_interface_;
        InterfacePtr outgoing_interface_;

        std::map<int, std::shared_ptr<ActorContact> > actor_contacts_;

        int follow_id_;
        bool inc_follow_;
        bool dec_follow_;

        double scale_;
        bool scale_required_;

        ViewMode view_mode_;
        bool enable_trails_;

        scrimmage_proto::GUIMsg gui_msg_;

        scrimmage_proto::SimInfo sim_info_;

        vtkSmartPointer<vtkTextActor> time_actor_;
        vtkSmartPointer<vtkTextActor> warp_actor_;
        vtkSmartPointer<vtkTextActor> heading_actor_;
        vtkSmartPointer<vtkTextActor> alt_actor_;
        vtkSmartPointer<vtkTextActor> fps_actor_;
        
        std::map<int, std::shared_ptr<scrimmage_proto::ContactVisual> > contact_visuals_;

        std::list<std::pair<scrimmage_proto::Shape, vtkSmartPointer<vtkActor> > > shapes_;

        std::map<std::string, std::shared_ptr<scrimmage_proto::UTMTerrain> > terrain_map_;
        std::map<std::string, std::shared_ptr<scrimmage_proto::ContactVisual> > contact_visual_map_;

        bool send_shutdown_msg_;

        vtkSmartPointer<vtkActor> terrain_actor_;

        double follow_offset_;

    private:
    };

}
#endif
