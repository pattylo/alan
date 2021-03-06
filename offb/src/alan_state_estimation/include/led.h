#include "essential.h"

#include <geometry_msgs/Point.h>
#include <std_msgs/Bool.h>
#include <geometry_msgs/PointStamped.h>
#include <geometry_msgs/PoseStamped.h>

#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/image_encodings.h>
#include <message_filters/subscriber.h>
#include <message_filters/synchronizer.h>
#include <message_filters/sync_policies/approximate_time.h>
#include <sensor_msgs/Image.h>

#include <opencv2/aruco.hpp>
#include <opencv2/calib3d.hpp>
#include <sophus/se3.hpp>

#include <pluginlib/class_list_macros.h>
#include <nodelet/nodelet.h>

#include <pthread.h>




namespace alan_pose_estimation
{
    typedef struct Match
    {
        int id;
        bool toofar;
    }Match;

    class LedNodelet : public nodelet::Nodelet
    {
        private:

            cv::Mat frame;
            double LANDING_DISTANCE = 0;
            Eigen::MatrixXd cameraMat = Eigen::MatrixXd::Zero(3,3);

            message_filters::Subscriber<sensor_msgs::CompressedImage> subimage;
            message_filters::Subscriber<sensor_msgs::Image> subdepth;
            typedef message_filters::sync_policies::ApproximateTime<sensor_msgs::CompressedImage, sensor_msgs::Image> MySyncPolicy;
            typedef message_filters::Synchronizer<MySyncPolicy> sync;//(MySyncPolicy(10), subimage, subdepth);
            boost::shared_ptr<sync> sync_;
            
            vector<int> id_tracked;

            void camera_callback(const sensor_msgs::CompressedImageConstPtr & rgbimage, const sensor_msgs::ImageConstPtr & depth);
            
            //solve pose
            void pose_w_LED_pnp(cv::Mat& frame, cv::Mat depth);            

            //LED extraction
            vector<Eigen::Vector2d> LED_extract_POI(cv::Mat& frame, cv::Mat depth);

            void LED_tracking();

            void LED_tracking_initialize(cv::Mat& frame, cv::Mat depth);

            void reject_outlier(vector<Eigen::Vector3d>& pts_3d_pcl_detect);

            void solveicp_for_initialize(vector<Eigen::Vector3d> pts_3d, vector<Eigen::Vector3d> body_frame_pts, Eigen::Matrix3d& R, Eigen::Vector3d& t);
                        
            void solveicp_svd(vector<Eigen::Vector3d> pts_3d, vector<Eigen::Vector3d> body_frame_pts, Eigen::Matrix3d& R, Eigen::Vector3d& t);

            vector<Eigen::Vector3d> pointcloud_generate(vector<Eigen::Vector2d> pts_2d_detected, cv::Mat depthimage);
            
            //Munkres
            void cost_generate(vector<cv::Point> detected, vector<cv::Point> previous);
            vector<Match> solution(vector<cv::Point> measured, vector<cv::Point> previous);//return the corresponding ids
            vector<Match> id_match;

            void stp1(int &step);//reduce with the minima of row and column
            void stp2(int &step);
            void stp3(int &step);
            void stp4(int &step);
            void stp5(int &step);
            void stp6(int &step);
            void stp7();

            void find_a_zero(int& row, int& col);
            bool star_in_row(int row);
            void find_star_in_row(int row, int& col);
            void find_min(double& minval);
            void find_star_in_col(int col, int& row);
            void find_prime_in_row(int row, int& col);
            void augment_path();
            void clear_covers();
            void erase_primes();

            int step = 1;
            Eigen::MatrixXd cost, mask, path, copy;
            vector<int> cover_row;
            vector<int> cover_col;
            int path_row_0, path_col_0, path_count;


            virtual void onInit()
            {
                ros::NodeHandle& nh = getNodeHandle();

                nh.getParam("/alan_pose/LANDING_DISTANCE", LANDING_DISTANCE);     
                cout<<"hi"<<LANDING_DISTANCE<<endl;   

                //load camera intrinsics
                Eigen::Vector4d intrinsics_value;
                XmlRpc::XmlRpcValue intrinsics_list;
                nh.getParam("/aruco/cam_intrinsics_455", intrinsics_list);                
                                
                for(int i = 0; i < 4; i++)
                {
                    intrinsics_value[i] = intrinsics_list[i];
                }

                cameraMat <<    
                    intrinsics_value[0], 0, intrinsics_value[2], 
                    0, intrinsics_value[1], intrinsics_value[3],
                    0, 0,  1;         
        
                //subscribe
                subimage.subscribe(nh, "/camera/color/image_raw/compressed", 1);
                subdepth.subscribe(nh, "/camera/aligned_depth_to_color/image_raw", 1);                
                sync_.reset(new sync( MySyncPolicy(10), subimage, subdepth));            
                sync_->registerCallback(boost::bind(&LedNodelet::camera_callback, this, _1, _2));               
            }

    };

    PLUGINLIB_EXPORT_CLASS(alan_pose_estimation::LedNodelet, nodelet::Nodelet)
}