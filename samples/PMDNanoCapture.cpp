/**
 * @file PMDNanoCapture.cpp
 * @author Yutaka Kondo <yutaka.kondo@youtalk.jp>
 * @date Jul 30, 2013
 */

#include <memory>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <pcl/visualization/cloud_viewer.h>
#include "rgbd/camera/PMDNano.h"

using namespace rgbd;

int main(int argc, char *argv[]) {
    if (argc < 3)
        return -1;

    std::shared_ptr<DepthCamera> camera(new PMDNano(argv[1], argv[2]));
    camera->start();

    cv::Mat depth = cv::Mat::zeros(camera->depthSize(), CV_32F);
    cv::Mat amplitude = cv::Mat::zeros(camera->depthSize(), CV_32F);
    std::shared_ptr<pcl::visualization::CloudViewer> viewer(
            new pcl::visualization::CloudViewer("Vertex"));
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>());
    cloud->points.resize(camera->depthSize().width * camera->depthSize().height);

    cv::namedWindow("Depth", CV_WINDOW_AUTOSIZE | CV_WINDOW_FREERATIO);
    cv::namedWindow("Amplitude", CV_WINDOW_AUTOSIZE | CV_WINDOW_FREERATIO);

    while (cv::waitKey(10) != 0x1b) {
        camera->captureDepth(depth);
        camera->captureAmplitude(amplitude);
        camera->captureVertex(cloud->points);

        cv::flip(depth, depth, 0);
        cv::flip(amplitude, amplitude, 0);

        cv::Mat a;
        amplitude.convertTo(a, CV_8U, 255.0 / 1000.0);

        cv::imshow("Depth", depth);
        cv::imshow("Amplitude", a);
        viewer->showCloud(cloud);
    }

    return 0;
}