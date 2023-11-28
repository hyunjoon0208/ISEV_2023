#include "header.h"
#include "dbscan.h"

using namespace std;

int minPoints = 10; //10          //Core Point 기준 필요 인접점 최소 개수
double epsilon = 0.3; //0.3        //Core Point 기준 주변 탐색 반경 

int minClusterSize = 10; //10     //Cluster 최소 사이즈
int maxClusterSize = 10000; //10000  //Cluster 최대 사이즈

double xMinROI = -10;
double xMaxROI = 10; 
double yMinROI = -10; 
double yMaxROI = 10; 
double zMinROI = -10; 
double zMaxROI = 10;

double xMinBoundingBox = 0.5;
double xMaxBoundingBox = 3;
double yMinBoundingBox = 0.5;
double yMaxBoundingBox = 3;
double zMinBoundingBox = 0.1;
double zMaxBoundingBox = 3; // BoundingBox 크기 범위 지정 변수 

typedef pcl::PointXYZ PointT;

ros::Publisher roiPub; //ROI Publishser
ros::Publisher clusterPub; //Cluster Publishser
ros::Publisher boundingBoxPub; //Bounding Box Visualization Publisher

void cloud_cb(const sensor_msgs::PointCloud2ConstPtr& inputcloud) {
  //ROS message 변환
  //PointXYZI가 아닌 PointXYZ로 선언하는 이유 -> 각각의 Cluster를 다른 색으로 표현해주기 위해서. Clustering 이후 각각 구별되는 intensity value를 넣어줄 예정.
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>);  //cloud는 PointCloud객체를 동적할당해서 가리키는 스마트 포인터
  pcl::fromROSMsg(*inputcloud, *cloud); //sensor_msgs::PointCloud2(ROS통신을 할때 사용하는 Type) -> pcl::PointCloud(데이터 후처리를 위한 Type) 로 바꾸어주는 작업. 

  //Visualizing에 필요한 Marker 선언
  visualization_msgs::Marker boundingBox; // Marker는 rviz상에서 사용하기 위한 일종의 메세지. publish해주면 rviz에서 받아 표현해준다.
  visualization_msgs::MarkerArray boundingBoxArray; // Marker의 집항을 MarkerArray라고 표현한다.

  pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_xf(new pcl::PointCloud<pcl::PointXYZ>); 
  pcl::PassThrough<pcl::PointXYZ> xfilter;
  xfilter.setInputCloud(cloud);
  xfilter.setFilterFieldName("x"); // filedname 을 x로 지정
  xfilter.setFilterLimits(xMinROI, xMaxROI); // 원하는 범위 설정
  xfilter.setFilterLimitsNegative(false); // true 이면 -Inf ~ xMinROI & xMaxROI ~ Inf 점들을 보여줌.
  xfilter.filter(*cloud_xf);

  pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_xyf(new pcl::PointCloud<pcl::PointXYZ>);
  pcl::PassThrough<pcl::PointXYZ> yfilter;
  yfilter.setInputCloud(cloud_xf);
  yfilter.setFilterFieldName("y");
  yfilter.setFilterLimits(yMinROI, yMaxROI);
  yfilter.setFilterLimitsNegative(false);
  yfilter.filter(*cloud_xyf);

  pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_xyzf(new pcl::PointCloud<pcl::PointXYZ>);
  pcl::PassThrough<pcl::PointXYZ> zfilter;
  zfilter.setInputCloud(cloud_xyf);
  zfilter.setFilterFieldName("z");
  zfilter.setFilterLimits(zMinROI, zMaxROI); // -0.62, 0.0
  zfilter.setFilterLimitsNegative(false);
  zfilter.filter(*cloud_xyzf);

  // //Voxel Grid를 이용한 DownSampling
  // pcl::VoxelGrid<pcl::PointXYZ> vg;    // VoxelGrid 선언
  // pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>); //Filtering 된 Data를 담을 PointCloud 선언
  // vg.setInputCloud(cloud);             // Raw Data 입력
  // vg.setLeafSize(0.5f, 0.5f, 0.5f); // 사이즈를 너무 작게 하면 샘플링 에러 발생
  // vg.filter(*cloud);          // Filtering 된 Data를 cloud PointCloud에 삽입

  //KD-Tree
  pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>);
  if (cloud_xyzf->size() > 0) {
    tree->setInputCloud(cloud_xyzf);
  }
  //Segmentation
  vector<pcl::PointIndices> cluster_indices;
  
  //DBSCAN with Kdtree for accelerating
  DBSCANKdtreeCluster<pcl::PointXYZ> dc;
  dc.setCorePointMinPts(minPoints);   //Set minimum number of neighbor points
  dc.setClusterTolerance(epsilon); //Set Epsilon 
  dc.setMinClusterSize(minClusterSize);
  dc.setMaxClusterSize(maxClusterSize);
  dc.setSearchMethod(tree);
  dc.setInputCloud(cloud_xyzf);
  dc.extract(cluster_indices);

  pcl::PointCloud<pcl::PointXYZI> totalcloud_clustered;
  int cluster_id = 0;

  //각 Cluster 접근
  for (vector<pcl::PointIndices>::const_iterator it = cluster_indices.begin(); it != cluster_indices.end(); it++, cluster_id++) {
    pcl::PointCloud<pcl::PointXYZI> eachcloud_clustered;
    float cluster_counts = cluster_indices.size();
    //각 Cluster내 각 Point 접근
    for(vector<int>::const_iterator pit = it->indices.begin(); pit != it->indices.end(); ++pit) {
        pcl::PointXYZI tmp;
        tmp.x = cloud_xyzf->points[*pit].x;
        tmp.y = cloud_xyzf->points[*pit].y;
        tmp.z = cloud_xyzf->points[*pit].z;
        tmp.intensity = cluster_id%8;
        eachcloud_clustered.push_back(tmp);
        totalcloud_clustered.push_back(tmp);
    }

    //minPoint와 maxPoint 받아오기
    pcl::PointXYZI minPoint, maxPoint;
    pcl::getMinMax3D(eachcloud_clustered, minPoint, maxPoint);

    float x_len = abs(maxPoint.x - minPoint.x);   //직육면체 x 모서리 크기
    float y_len = abs(maxPoint.y - minPoint.y);   //직육면체 y 모서리 크기
    float z_len = abs(maxPoint.z - minPoint.z);   //직육면체 z 모서리 크기 
    float volume = x_len * y_len * z_len;         //직육면체 부피

    float center_x = (minPoint.x + maxPoint.x)/2; //직육면체 중심 x 좌표
    float center_y = (minPoint.y + maxPoint.y)/2; //직육면체 중심 y 좌표
    float center_z = (minPoint.z + maxPoint.z)/2; //직육면체 중심 z 좌표 

    float distance = sqrt(center_x * center_x + center_y * center_y); //장애물 <-> 차량 거리

    if ( (xMinBoundingBox < x_len && x_len < xMaxBoundingBox) && (yMinBoundingBox < y_len && y_len < yMaxBoundingBox) && (zMinBoundingBox < z_len && z_len < zMaxBoundingBox) ) {
      boundingBox.header.frame_id = "velodyne";
      boundingBox.header.stamp = ros::Time();
      boundingBox.ns = cluster_counts; //ns = namespace
      boundingBox.id = cluster_id; 
      boundingBox.type = visualization_msgs::Marker::CUBE; //직육면체로 표시
      boundingBox.action = visualization_msgs::Marker::ADD;

      boundingBox.pose.position.x = center_x; 
      boundingBox.pose.position.y = center_y;
      boundingBox.pose.position.z = center_z;

      boundingBox.pose.orientation.x = 0.0;
      boundingBox.pose.orientation.y = 0.0;
      boundingBox.pose.orientation.z = 0.0;
      boundingBox.pose.orientation.w = 1.0;

      boundingBox.scale.x = x_len;
      boundingBox.scale.y = y_len;
      boundingBox.scale.z = z_len;

      boundingBox.color.a = 0.5; //직육면체 투명도, a = alpha
      boundingBox.color.r = 1.0; //직육면체 색상 RGB값
      boundingBox.color.g = 1.0;
      boundingBox.color.b = 1.0;

      boundingBox.lifetime = ros::Duration(0.1); //box 지속시간
      boundingBoxArray.markers.emplace_back(boundingBox);
    }

    cluster_id++; //intensity 증가

  }

  //Convert To ROS data type
  pcl::PCLPointCloud2 cloud_p;
  pcl::toPCLPointCloud2(totalcloud_clustered, cloud_p);

  sensor_msgs::PointCloud2 cluster;
  pcl_conversions::fromPCL(cloud_p, cluster);
  cluster.header.frame_id = "velodyne";


  pcl::PCLPointCloud2 cloud_cropbox;
  pcl::toPCLPointCloud2(*cloud_xyzf, cloud_cropbox);

  sensor_msgs::PointCloud2 ROI;
  pcl_conversions::fromPCL(cloud_cropbox, ROI);
  ROI.header.frame_id = "velodyne";


  roiPub.publish(ROI);
  clusterPub.publish(cluster);
  boundingBoxPub.publish(boundingBoxArray);
}


int main(int argc, char **argv) {
  ros::init(argc, argv, "bounding_box");
  ros::NodeHandle nh;

  ros::Subscriber rawDataSub = nh.subscribe("/velodyne_points", 1, cloud_cb);  // velodyne_points 토픽 구독. velodyne_points = 라이다 raw data

  roiPub = nh.advertise<sensor_msgs::PointCloud2>("/roi", 0.001); 
  clusterPub = nh.advertise<sensor_msgs::PointCloud2>("/cluster", 0.001);                  
  boundingBoxPub = nh.advertise<visualization_msgs::MarkerArray>("/boundingBox", 0.001);     

  ros::spin();

  return 0;
}
