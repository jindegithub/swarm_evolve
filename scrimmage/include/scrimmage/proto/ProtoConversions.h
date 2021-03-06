#ifndef _PROTO_CONVERSIONS_H_
#define _PROTO_CONVERSIONS_H_

#include <scrimmage/common/ColorMaps.h>
#include <scrimmage/proto/Vector3d.pb.h>
#include <scrimmage/proto/Shape.pb.h>

#include <scrimmage/proto/ID.pb.h>
#include <scrimmage/common/ID.h>

#include <scrimmage/math/Quaternion.h>
#include <scrimmage/proto/Quaternion.pb.h>

#include <scrimmage/proto/State.pb.h>

#include <scrimmage/entity/Contact.h>
#include <scrimmage/proto/Contact.pb.h>

#include <scrimmage/log/Frame.h>
#include <scrimmage/proto/Frame.pb.h>

#include <scrimmage/math/State.h>

#include <Eigen/Dense>

namespace sp = scrimmage_proto;

namespace scrimmage {

typedef std::shared_ptr<scrimmage_proto::Shape> ShapePtr;

void set(scrimmage_proto::Vector3d *dst, Eigen::Vector3d &src);
void set(scrimmage_proto::Color *dst, std::vector<int> &src);
void set(scrimmage_proto::Color &dst, const scrimmage_proto::Color &src);
void set(Eigen::Vector3d &dst, scrimmage_proto::Vector3d *src);
void set(scrimmage_proto::Color &dst, scrimmage_proto::Color &src);
void set(scrimmage_proto::Color &dst, int r, int g, int b);
void set(scrimmage_proto::Color *dst, scrimmage_proto::Color *src);
void set(scrimmage_proto::Color *dst, scrimmage_proto::Color src);
void set(scrimmage_proto::Color *dst, scrimmage::Color_t src);
void set(scrimmage_proto::Color *color, int r, int g, int b);
void set(scrimmage_proto::Color *color, int grayscale);

Eigen::Vector3d eigen(const scrimmage_proto::Vector3d src);

void add_point(std::shared_ptr<scrimmage_proto::Shape> s, Eigen::Vector3d src);

void add_point_color(std::shared_ptr<scrimmage_proto::Shape> s, scrimmage::Color_t c);
void add_point_color(ShapePtr s, int r, int g, int b);
void add_point_color(ShapePtr s, int grayscale);

ID proto_2_id(scrimmage_proto::ID proto_id);
Quaternion proto_2_quat(scrimmage_proto::Quaternion proto_quat);
Eigen::Vector3d proto_2_vector3d(scrimmage_proto::Vector3d proto_vector3d);

StatePtr proto_2_state(scrimmage_proto::State proto_state);

void path_to_lines(std::vector<Eigen::Vector3d> &path,
                          scrimmage_proto::Shape &sample_line, 
                          std::list<ShapePtr> &shapes);

Contact proto_2_contact(scrimmage_proto::Contact proto_contact);

Frame proto_2_frame(scrimmage_proto::Frame &proto_frame);

std::shared_ptr<scrimmage_proto::Frame>
create_frame(double time, std::shared_ptr<ContactMap> &contacts);


}
#endif
