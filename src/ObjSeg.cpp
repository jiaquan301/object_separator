#include "ObjSeg.h"

ObjSeg::ObjSeg()
{
    pub = new Publisher();
    pcl::PointCloud<PointXYZRGBA>::Ptr init_cloud_ptr (new pcl::PointCloud<PointXYZRGBA>);
    viewer = initViewer(init_cloud_ptr);
}


pcl::visualization::PCLVisualizer::Ptr ObjSeg::initViewer(pcl::PointCloud<PointXYZRGBA>::ConstPtr cloud)
{
    pcl::visualization::PCLVisualizer::Ptr viewer(new pcl::visualization::PCLVisualizer("Viewer"));
    viewer->setBackgroundColor (0, 0, 0);
    pcl::visualization::PointCloudColorHandlerCustom<PointXYZRGBA> single_color(cloud, 0, 255, 0);
    viewer->addPointCloud<PointXYZRGBA>(cloud, single_color, "cloud");
    viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 1, "cloud");
    viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_OPACITY, 0.15, "cloud");
    viewer->addCoordinateSystem(1.0);
    viewer->initCameraParameters();
    
    return viewer;
}


void ObjSeg::callback(const PointCloud<PointXYZRGBA>::ConstPtr& input)
{
    PointCloud<PointXYZRGBA>::ConstPtr cloudCopy(new PointCloud<PointXYZRGBA>);
    PointCloud<PointXYZRGBA>::Ptr result(new PointCloud<PointXYZRGBA>);
    cloudCopy = input;
    copyPointCloud(*cloudCopy, *result);
    
    //--------------------------------------------------------
    
    unsigned char red[6] = {255,   0,   0, 255, 255,   0};
    unsigned char grn[6] = {  0, 255,   0, 255,   0, 255};
    unsigned char blu[6] = {  0,   0, 255,   0, 255, 255};
    
    pcl::IntegralImageNormalEstimation<PointXYZRGBA, pcl::Normal> ne;
    ne.setNormalEstimationMethod(ne.COVARIANCE_MATRIX);
    ne.setMaxDepthChangeFactor(0.03f);
    ne.setNormalSmoothingSize(20.0f);
    
    pcl::OrganizedMultiPlaneSegmentation<PointXYZRGBA, pcl::Normal, pcl::Label> mps;
    mps.setMinInliers(10000);
    mps.setAngularThreshold(0.017453 * 2.0); //3 degrees
    mps.setDistanceThreshold(0.02); //2cm
    
    std::vector<pcl::PlanarRegion<PointXYZRGBA>, Eigen::aligned_allocator<pcl::PlanarRegion<PointXYZRGBA> > > regions;
    pcl::PointCloud<PointXYZRGBA>::Ptr contour(new pcl::PointCloud<PointXYZRGBA>);
    size_t prev_models_size = 0;
    char name[1024];
    
    //compute normalization
    regions.clear();
    pcl::PointCloud<pcl::Normal>::Ptr normal_cloud(new pcl::PointCloud<pcl::Normal>);
    m_normalCloud = normal_cloud;
    ne.setInputCloud(cloudCopy);
    ne.compute(*normal_cloud);
    cout << "Normal Estimation finished " << endl;
    
    //extract planes
    mps.setInputNormals(normal_cloud);
    mps.setInputCloud(cloudCopy);
    mps.segmentAndRefine(regions);
    cout << "Plane extraction completed " << endl;
    
    pcl::PointCloud<PointXYZRGBA>::Ptr cluster (new pcl::PointCloud<PointXYZRGBA>);
    
    cout << "------Results------" << endl;
    cout << "Number of regions: " << regions.size() << endl;
    
    pclCloud = cloudCopy;
    //cout << "calling lccp" << endl;
    lccpSeg();
    
    //Draw Visualization (is this even doing anything?)
    for (size_t i = 0; i < regions.size(); i++)
    {
        Eigen::Vector3f centroid = regions[i].getCentroid();
        Eigen::Vector4f model = regions[i].getCoefficients();
        pcl::PointXYZ pt1 = pcl::PointXYZ(centroid[0], centroid[1], centroid[2]);
        pcl::PointXYZ pt2 = pcl::PointXYZ(centroid[0] + (0.5f * model[0]),
                                               centroid[1] + (0.5f * model[1]),
                                               centroid[2] + (0.5f * model[2]));
        sprintf(name, "normal_%lu", i);

        contour->points = regions[i].getContour();
        
        //instead, map contour ---> original image?
        for(size_t j = 0; j < contour->size(); j++)
        {
            result->points[j].r = 255;
        }
        sprintf(name, "plane_%02zu", i);
    }
    
    pcl::PointCloud<PointXYZRGBA>::CloudVectorType clusters;
    
    if(regions.size() > 0)
    {
        std::vector<bool> plane_labels;
        //plane_labels.resize(label_indices.size(), false);
    }
    
    
    
    //--------------------------------------------------------
    //pclCloud = cloudCopy;
    showVisualizer();
    
    sensor_msgs::Image rosImage;
    pcl::toROSMsg(*result, rosImage);
    pub->publish(rosImage);
}


void ObjSeg::updateVisualizer(pcl::visualization::PCLVisualizer::Ptr viewer, pcl::PointCloud<pcl::PointXYZRGBA>::ConstPtr cloud)
{
    pcl::visualization::PointCloudColorHandlerRGBField<pcl::PointXYZRGBA> rgb(cloud);
    viewer->removePointCloud();
    viewer->addPointCloud<pcl::PointXYZRGBA>(cloud, rgb);
    viewer->spinOnce(/*100*/);
}


void ObjSeg::showVisualizer()
{
    updateVisualizer(viewer, pclCloud);
}


void ObjSeg::lccpSeg()
{
    uint k_factor = 0; //if you want to use extended convexity, set to 1

    // Default values of parameters before parsing
    // Supervoxel Stuff
    float voxel_resolution = 0.0075f;
    float seed_resolution = 0.03f;
    float color_importance = 0.0f;
    float spatial_importance = 1.0f;
    float normal_importance = 4.0f;
    bool use_single_cam_transform = false;
    bool use_supervoxel_refinement = false;

    // LCCPSegmentation Stuff
    float concavity_tolerance_threshold = 10;
    float smoothness_threshold = 0.1;
    uint32_t min_segment_size = 0;
    bool use_extended_convexity = false;
    bool use_sanity_criterion = false;

    pcl::SupervoxelClustering<PointXYZRGBA> superVox(voxel_resolution, seed_resolution);

    superVox.setUseSingleCameraTransform(use_single_cam_transform); //!!!Not available in verion 1.7; requires >= 1.8.0!!!

    superVox.setInputCloud(pclCloud);
    //if(has_normals)
        superVox.setNormalCloud(m_normalCloud);
    superVox.setColorImportance(color_importance);
    superVox.setSpatialImportance(spatial_importance);
    superVox.setNormalImportance(normal_importance);
    std::map<uint32_t, pcl::Supervoxel<PointXYZRGBA>::Ptr> supervoxel_clusters;
    
    PCL_INFO("About to extract supervoxels\n");
    superVox.extract(supervoxel_clusters);
    
    if(use_supervoxel_refinement)
    {
        PCL_INFO("About to refine supervoxels\n");
        superVox.refineSupervoxels(2, supervoxel_clusters);
    }
    
    std::stringstream temp;
    temp << " Number of supervoxels: " << supervoxel_clusters.size() << "\n";
    PCL_INFO(temp.str().c_str() );
    
    PCL_INFO("About to process supervoxel adjacency\n");
    std::multimap<uint32_t, uint32_t> supervoxel_adjacency;
    superVox.getSupervoxelAdjacency(supervoxel_adjacency);
    
    // Get the cloud of supervoxel centroid with normals and the colored cloud with supervoxel coloring (this is used for visulization)
    pcl::PointCloud<pcl::PointNormal>::Ptr sv_centroid_normal_cloud = pcl::SupervoxelClustering<PointXYZRGBA>::makeSupervoxelNormalCloud (supervoxel_clusters);

    // The Main Step: Perform LCCPSegmentation
    PCL_INFO("About to run LCCPSegmentation\n");
    pcl::LCCPSegmentation<PointXYZRGBA> lccp;
    lccp.setConcavityToleranceThreshold(concavity_tolerance_threshold);
    lccp.setSanityCheck(use_sanity_criterion);
    lccp.setSmoothnessCheck(true, voxel_resolution, seed_resolution, smoothness_threshold);
    lccp.setKFactor(k_factor);
    lccp.segment(supervoxel_clusters, supervoxel_adjacency);
    
    if(min_segment_size > 0)
    {
        PCL_INFO("About to merge small segments\n");
        lccp.mergeSmallSegments(min_segment_size);
    }
    
    PCL_INFO ("Interpolation voxel cloud -> input cloud and relabeling\n");
    pcl::PointCloud<pcl::PointXYZL>::Ptr sv_labeled_cloud = superVox.getLabeledCloud();
    pcl::PointCloud<pcl::PointXYZL>::Ptr lccp_labeled_cloud = sv_labeled_cloud->makeShared();
    lccp.relabelCloud(*lccp_labeled_cloud);
    pcl::LCCPSegmentation<PointXYZRGBA>::SupervoxelAdjacencyList sv_adjacency_list;
    lccp.getSVAdjacencyList(sv_adjacency_list);  // Needed for visualization
    
    
}


void ObjSeg::llcpViewSetup()
{
    ;//TODO implement this!!!
}


Publisher* ObjSeg::getPublisher()
{
    return pub;
}


ObjSeg::~ObjSeg()
{
    ;
}
