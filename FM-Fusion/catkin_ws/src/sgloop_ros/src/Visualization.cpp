#include "Visualization.h"

namespace Visualization
{
    Visualizer::Visualizer(ros::NodeHandle &nh, ros::NodeHandle &nh_private, std::vector<std::string> remote_agents)
    {
        // Ref graph
        ref_graph = nh_private.advertise<sensor_msgs::PointCloud2>("ref/instance_map", 1000);
        ref_centroids = nh_private.advertise<visualization_msgs::Marker>("ref/centroids", 1000);
        ref_global_map = nh_private.advertise<sensor_msgs::PointCloud2>("ref/global_map", 1000);
        // ref_edges = nh_private.advertise<visualization_msgs::Marker>("ref/edges", 1000);

        // Local graph
        src_graph = nh_private.advertise<sensor_msgs::PointCloud2>("instance_map", 1000);
        src_centroids = nh_private.advertise<visualization_msgs::Marker>("centroids", 1000);
        src_edges = nh_private.advertise<visualization_msgs::Marker>("edges", 1000);
        src_global_map = nh_private.advertise<sensor_msgs::PointCloud2>("global_map", 1000);
        node_annotation = nh_private.advertise<visualization_msgs::MarkerArray>("node_annotation", 1000);
        node_dir_lines = nh_private.advertise<visualization_msgs::Marker>("node_dir_lines", 1000);

        // Loop
        instance_match = nh_private.advertise<visualization_msgs::Marker>("instance_match", 1000);
        point_match = nh_private.advertise<visualization_msgs::Marker>("point_match", 1000);
        src_map_aligned = nh_private.advertise<sensor_msgs::PointCloud2>("aligned_map", 1000); //aligned and render in reference frame
        path_aligned = nh_private.advertise<nav_msgs::Path>("aligned_path", 1000);

        rgb_image = nh_private.advertise<sensor_msgs::Image>("rgb_image", 1000);
        pred_image = nh_private.advertise<sensor_msgs::Image>("pred_image", 1000);
        camera_pose = nh_private.advertise<nav_msgs::Odometry>("camera_pose", 1000);
        path = nh_private.advertise<nav_msgs::Path>("path", 1000);


        path_msg.header.stamp = ros::Time::now();

        // Global viz setting
        param.edge_width = nh.param("viz/edge_width", 0.02);
        param.edge_color[0] = nh.param("viz/edge_color/r", 0.0);
        param.edge_color[1] = nh.param("viz/edge_color/g", 0.0);
        param.edge_color[2] = nh.param("viz/edge_color/b", 1.0);
        param.centroid_size = nh.param("viz/centroid_size", 0.1);
        param.centroid_color[0] = nh.param("viz/centroid_color/r", 0.0);
        param.centroid_color[1] = nh.param("viz/centroid_color/g", 0.0);
        param.centroid_color[2] = nh.param("viz/centroid_color/b", 0.0);
        param.centroid_voffset = nh.param("viz/centroid_v_offset", 0.0);
        param.annotation_size = nh.param("viz/annotation_size", 0.2);
        param.annotation_voffset = nh.param("viz/annotation_v_offset", 1.5);

        // Agents relative transform
        for(auto agent: remote_agents){
            Eigen::Vector3d t;
            Eigen::Matrix4d T = Eigen::Matrix4d::Identity();
            
            t[0] = nh.param("br/"+agent+"/x", 0.0);
            t[1] = nh.param("br/"+agent+"/y", 0.0);
            t[2] = nh.param("br/"+agent+"/z", 0.0);
            float yaw = nh.param("br/"+agent+"/yaw", 0.0);

            T.block<3,1>(0,3) = t;
            T.block<3,3>(0,0) = Eigen::AngleAxisd(yaw, Eigen::Vector3d::UnitZ()).toRotationMatrix();

            t_local_remote[agent] = t;
            Transfrom_local_remote[agent] = T;
        }

    }

    bool render_point_cloud(const std::shared_ptr<open3d::geometry::PointCloud> &pcd, ros::Publisher pub, std::string frame_id)
    {
        if(pub.getNumSubscribers()==0 || pcd->points_.size()<10) return false;
        // Publish point cloud
        sensor_msgs::PointCloud2 msg;
        open3d_conversions::open3dToRos(*pcd, msg, frame_id);

        pub.publish(msg);

        return true;
    }

    bool inter_graph_edges(const std::vector<Eigen::Vector3d> &centroids,
                            const std::vector<std::pair<int,int>> &edges,
                            ros::Publisher pub,
                            float width, 
                            std::array<float,3> color,
                            std::string frame_id)
    {
        visualization_msgs::Marker marker;
        marker.header.frame_id = frame_id;
        marker.header.stamp = ros::Time::now();
        marker.ns = "inter_graph_edges";
        marker.id = 0;
        marker.type = visualization_msgs::Marker::LINE_LIST;
        marker.action = visualization_msgs::Marker::ADD;
        marker.pose.orientation.w = 1.0;
        marker.scale.x = width; // line width
        marker.color.r = color[0];
        marker.color.g = color[1];
        marker.color.b = color[2];
        marker.color.a = 1.0;

        for (int i=0;i<edges.size();i++){
            geometry_msgs::Point p1, p2;
            p1.x = centroids[edges[i].first].x();
            p1.y = centroids[edges[i].first].y();
            p1.z = centroids[edges[i].first].z();
            p2.x = centroids[edges[i].second].x();
            p2.y = centroids[edges[i].second].y();
            p2.z = centroids[edges[i].second].z();
            marker.points.push_back(p1);
            marker.points.push_back(p2);
        }

        pub.publish(marker);

        return true;
    }

    bool correspondences(const std::vector<Eigen::Vector3d> &src_centroids,
                        const std::vector<Eigen::Vector3d> &ref_centroids,
                        ros::Publisher pub, 
                        std::string src_frame_id, 
                        std::vector<bool> pred_masks,
                        Eigen::Matrix4d T_local_remote,
                        float line_width,
                        float alpha)
    {
        if(pub.getNumSubscribers()==0) return false;
        visualization_msgs::Marker marker;
        marker.header.frame_id = src_frame_id;
        marker.header.stamp = ros::Time::now();
        marker.ns = "instance_match";
        marker.id = 0;
        marker.type = visualization_msgs::Marker::LINE_LIST;
        marker.action = visualization_msgs::Marker::ADD;
        marker.pose.orientation.w = 1.0;
        marker.scale.x = line_width; // line width
        marker.color.a = alpha;

        int n = src_centroids.size();
        assert(n==ref_centroids.size());

        for (int i=0;i<n;i++){
            geometry_msgs::Point p1, p2;
            Eigen::Vector3d aligned_ref_centroid = T_local_remote.block<3,3>(0,0)*ref_centroids[i] + T_local_remote.block<3,1>(0,3);

            p1.x = src_centroids[i].x();
            p1.y = src_centroids[i].y();
            p1.z = src_centroids[i].z();
            p2.x = aligned_ref_centroid.x(); // ref_centroids[i].x()+ t_local_remote[0];
            p2.y = aligned_ref_centroid.y(); //ref_centroids[i].y()+ t_local_remote[1];
            p2.z = aligned_ref_centroid.z(); //ref_centroids[i].z()+ t_local_remote[2];
            marker.points.push_back(p1);
            marker.points.push_back(p2);
            std_msgs::ColorRGBA line_color;
            line_color.a = 1;
            // line_color.r = 1;
            if(!pred_masks.empty()){
                if(pred_masks[i]) line_color.g = 1;
                else line_color.r = 1;
            }
            else line_color.g = 1;
            marker.colors.push_back(line_color);
            marker.colors.push_back(line_color);
        }

        pub.publish(marker);

        return true;

    }

    bool instance_centroids(const std::vector<Eigen::Vector3d> &centroids,
                            ros::Publisher pub, 
                            std::string frame_id, 
                            float scale,
                            std::array<float,3> color,
                            float offset_z)
    {
        if(pub.getNumSubscribers()==0) return false;

        visualization_msgs::Marker marker;
        marker.header.frame_id = frame_id;
        marker.header.stamp = ros::Time::now();
        marker.ns = "instance_centroid";
        marker.id = 0;
        marker.type = visualization_msgs::Marker::SPHERE_LIST;
        marker.action = visualization_msgs::Marker::ADD;
        marker.pose.orientation.w = 1.0;
        marker.scale.x = scale;
        marker.scale.y = scale;
        marker.scale.z = scale;
        marker.color.r = color[0];
        marker.color.g = color[1];
        marker.color.b = color[2];
        marker.color.a = 1.0;

        for (int i=0;i<centroids.size();i++){
            geometry_msgs::Point p;
            p.x = centroids[i].x();
            p.y = centroids[i].y();
            p.z = centroids[i].z() + offset_z;
            marker.points.push_back(p);
        }

        pub.publish(marker);
        return true;
    }

    bool node_annotation(const std::vector<Eigen::Vector3d> &centroids,
                        const std::vector<std::string> &annotations,
                        ros::Publisher pub, 
                        std::string frame_id, 
                        float scale,
                        float z_offset,
                        std::array<float,3> color)    
{
        if(pub.getNumSubscribers()==0) return false;
        assert(centroids.size()==annotations.size());
        visualization_msgs::MarkerArray marker_array;
        int N = centroids.size();

        for (int i=0;i<N;i++){
            visualization_msgs::Marker marker;
            marker.header.frame_id = frame_id;
            marker.header.stamp = ros::Time::now();
            marker.ns = "node_annotation";
            marker.id = i;
            marker.type = visualization_msgs::Marker::TEXT_VIEW_FACING;
            marker.action = visualization_msgs::Marker::ADD;
            marker.pose.orientation.w = 1.0;
            marker.scale.z = scale;
            marker.color.a = 1.0;
            marker.color.r = color[0];
            marker.color.g = color[1];
            marker.color.b = color[2];
            marker.text = annotations[i];
            marker.pose.position.x = centroids[i].x();
            marker.pose.position.y = centroids[i].y();
            marker.pose.position.z = centroids[i].z() + z_offset;
            marker_array.markers.push_back(marker);
        }

        // std::cout<<"Publish "<<marker_array.markers.size()<<" annotations."<<std::endl;
        pub.publish(marker_array);

        return true;
    }

    bool render_image(const cv::Mat &image, ros::Publisher pub, std::string frame_id)
    {
        if(pub.getNumSubscribers()==0) return false;
        else{ // Publish image
            sensor_msgs::ImagePtr msg = cv_bridge::CvImage(std_msgs::Header(), "rgb8", image).toImageMsg();
            msg->header.frame_id = frame_id;
            pub.publish(msg);
            return true;
        }
    }

    bool render_rgb_detections(const open3d::geometry::Image &color,
                                const std::vector<fmfusion::DetectionPtr> &detections, 
                                ros::Publisher pub, 
                                std::string frame_id)
    {
        std::shared_ptr<cv::Mat> rgb_cv = std::make_shared<cv::Mat>(color.height_,color.width_,CV_8UC3);
        std::memcpy(rgb_cv->data,color.data_.data(),color.data_.size()*sizeof(uint8_t));

        for(auto det:detections){
            auto bbox = det->bbox_;
            cv::rectangle(*rgb_cv, 
                        cv::Point(bbox.u0,bbox.v0),
                        cv::Point(bbox.u1,bbox.v1), 
                        cv::Scalar(0,255,0), 2);
            cv::putText(*rgb_cv, 
                        det->extract_label_string(),
                        cv::Point(bbox.u0+5,bbox.v0+16), 
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, 
                        cv::Scalar(0,255,0), 2);
        }

        render_image(*rgb_cv, pub, frame_id);
        return true;
    }

    bool render_camera_pose(const Eigen::Matrix4d &pose, ros::Publisher pub, std::string frame_id, int sequence_id)
    {
        nav_msgs::Odometry msg;
        msg.header.frame_id = frame_id;
        msg.header.seq = sequence_id;
        msg.header.stamp = ros::Time::now();
        msg.child_frame_id = "camera_pose";
        msg.pose.pose.position.x = pose(0,3);
        msg.pose.pose.position.y = pose(1,3);
        msg.pose.pose.position.z = pose(2,3);
        Eigen::Quaterniond q(pose.block<3,3>(0,0));
        msg.pose.pose.orientation.x = q.x();
        msg.pose.pose.orientation.y = q.y();
        msg.pose.pose.orientation.z = q.z();
        msg.pose.pose.orientation.w = q.w();
        pub.publish(msg);
        return true;
        
    }

    bool render_path (const Eigen::Matrix4d &poses, nav_msgs::Path &path_msg,
                    ros::Publisher pub,std::string frame_id, int sequence_id)
    {
        path_msg.header.frame_id = frame_id;

        geometry_msgs::PoseStamped pose_stamped;
        pose_stamped.header.frame_id = frame_id;
        pose_stamped.header.seq = sequence_id;
        pose_stamped.header.stamp = ros::Time::now();
        pose_stamped.pose.position.x = poses(0,3);
        pose_stamped.pose.position.y = poses(1,3);
        pose_stamped.pose.position.z = poses(2,3);
        Eigen::Quaterniond q(poses.block<3,3>(0,0));
        pose_stamped.pose.orientation.x = q.x();
        pose_stamped.pose.orientation.y = q.y();
        pose_stamped.pose.orientation.z = q.z();
        pose_stamped.pose.orientation.w = q.w();
        path_msg.poses.push_back(pose_stamped);
        pub.publish(path_msg);
        return true;
    }

    bool render_path (const std::vector<Eigen::Matrix4d> &T_a_camera,
                    const Eigen::Matrix4d & T_b_a,
                    const std::string &agnetB_frame_id,
                    nav_msgs::Path &path_msg,
                    ros::Publisher pub)
    {
        if(pub.getNumSubscribers()==0||T_a_camera.empty()) return false;

        path_msg.header.frame_id = agnetB_frame_id;

        int k=0;
        for(auto T_a_c: T_a_camera){
            Eigen::Matrix4d T_b_c = T_b_a * T_a_c;
            // render_path(T_b_c, path_msg, pub, agnetB_frame_id, k);

            geometry_msgs::PoseStamped pose_stamped;
            pose_stamped.header.frame_id = agnetB_frame_id;
            pose_stamped.header.seq = k;
            pose_stamped.header.stamp = ros::Time::now();
            pose_stamped.pose.position.x = T_b_c(0,3);
            pose_stamped.pose.position.y = T_b_c(1,3);
            pose_stamped.pose.position.z = T_b_c(2,3);
            Eigen::Quaterniond q(T_b_c.block<3,3>(0,0));
            pose_stamped.pose.orientation.x = q.x();
            pose_stamped.pose.orientation.y = q.y();
            pose_stamped.pose.orientation.z = q.z();
            pose_stamped.pose.orientation.w = q.w();
            path_msg.poses.push_back(pose_stamped);

            k++;
        }

        pub.publish(path_msg);
        return true;


    }



std::vector<std::pair<std::string, Eigen::Vector3d>> detected_once;
std::vector<Eigen::Vector3d> persistent_centroids;
std::vector<std::string> persistent_annotations;

int render_semantic_map(const std::shared_ptr<open3d::geometry::PointCloud> &cloud, 
                        const std::vector<Eigen::Vector3d> &instance_centroids, 
                        const std::vector<std::string> &instance_annotations,
                        const Visualization::Visualizer &viz,
                        const std::string &agent_name)
{
    double min_distance = 0.5;

    for (size_t i = 0; i < instance_centroids.size(); ++i) {
        bool already_detected = false;

        // Comprobamos si el objeto ya ha sido detectado
        for (const auto& [prev_label, prev_centroid] : detected_once) {
            if ((instance_centroids[i] - prev_centroid).norm() < min_distance &&
                instance_annotations[i] == prev_label) {
                already_detected = true;
                break;
            }
        }

        if (!already_detected) {
            // Nuevo objeto detectado, lo guardamos de forma persistente
            detected_once.emplace_back(instance_annotations[i], instance_centroids[i]);
            persistent_centroids.push_back(instance_centroids[i]);
            persistent_annotations.push_back(instance_annotations[i]);

            // Visualización solo para el primer objeto detectado
            std::vector<Eigen::Vector3d> single_centroid = {instance_centroids[i]};
            std::vector<std::string> single_annotation = {instance_annotations[i]};

            // Visualizamos solo el primer objeto detectado
            Visualization::instance_centroids(single_centroid, 
                                              viz.src_centroids, 
                                              agent_name,
                                              viz.param.centroid_size,
                                              viz.param.centroid_color);

            Visualization::node_annotation(single_centroid, 
                                           single_annotation, 
                                           viz.node_annotation, 
                                           agent_name,
                                           viz.param.annotation_size,
                                           viz.param.annotation_voffset,
                                           viz.param.annotation_color);
        }
    }


    std::vector<Eigen::Vector3d> viz_centroids = persistent_centroids;

    if (viz.param.centroid_voffset > 0.1) {
        ROS_WARN("Visualize semantic nodes with a vertical offset %.1fm", viz.param.centroid_voffset);
        std::vector<Eigen::Vector3d> offset_centroids;
        for (const auto& center : persistent_centroids) {
            offset_centroids.push_back(Eigen::Vector3d(center.x(), center.y(), viz.param.centroid_voffset));
        }

        correspondences(persistent_centroids, offset_centroids, 
                        viz.node_dir_lines, agent_name,
                        {}, Eigen::Matrix4d::Identity(), 
                        0.05, 0.5);

        viz_centroids = offset_centroids;
    }

    // Visualizamos la nube de puntos
    Visualization::render_point_cloud(cloud, viz.src_graph, agent_name);

    ROS_INFO("Rendered %ld persistent objects and %ld points", viz_centroids.size(), cloud->points_.size());

    return viz_centroids.size();
}


}
