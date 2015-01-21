/**
 * @file DS325Calibration.cpp
 * @author Yutaka Kondo <yutaka.kondo@youtalk.jp>
 * @date Jun 20, 2014
 */

#include <iostream>
#include <string>
#include <sstream>
#include <opencv2/opencv.hpp>
#include <gflags/gflags.h>

DEFINE_string(intrinsics, "intrinsics.xml", "intrinsics file");
DEFINE_string(extrinsics, "extrinsics.xml", "extrinsics file");
DEFINE_string(dir, "/tmp/calib", "calibration data directory");
DEFINE_string(suffix, ".png", "file suffix");
DEFINE_int32(size, 1, "number of files");

void loadImages(std::vector<cv::Mat> &colors, std::vector<cv::Mat> &depths, const int &fileNum) {
    cv::namedWindow("color", cv::WINDOW_AUTOSIZE | cv::WINDOW_FREERATIO);
    cv::namedWindow("depth", cv::WINDOW_AUTOSIZE | cv::WINDOW_FREERATIO);

    colors.clear();
    depths.clear();

    for (int i = 0; i < fileNum; ++i) {
        std::stringstream colorFile, depthFile;
        colorFile << FLAGS_dir << "/color_" << i << FLAGS_suffix;
        depthFile << FLAGS_dir << "/depth_" << i << FLAGS_suffix;
        std::cout << colorFile.str() << ", " << depthFile.str() << ": load" << std::endl;

        cv::Mat color = cv::imread(colorFile.str(), 0);
        colors.push_back(color);

        //This used to be
        //cv::Mat depth = cv::imread(depthFile.str(), CV_LOAD_IMAGE_ANY_DEPTH);
        // but this is no longer supported in OpenCV???
        cv::Mat depth = cv::imread(depthFile.str(), CV_16SC1);
        depth.convertTo(depth, CV_8U, 255.0 / 1000.0);
        cv::resize(depth, depth, color.size());
        cv::Mat roi;
        cv::resize(depth(cv::Rect(40, 43, 498, 498 / 4 * 3)), roi, color.size()); // TODO
        depths.push_back(roi);

        cv::imshow("color", colors[i]);
        cv::imshow("depth", depths[i]);
        cv::waitKey(100);
    }
}

int findChessboards(
        std::vector<cv::Mat> &colors, std::vector<cv::Mat> &depths,
        std::vector<std::vector<std::vector<cv::Point2f>>> &imagePoints,
        const cv::Size patternSize, const int &fileNum) {
    for (int i = 0; i < colors.size(); ++i) {

        if (cv::findChessboardCorners(
                colors[i], patternSize, imagePoints[0][i],
                cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE) &&
            cv::findChessboardCorners(
                depths[i], patternSize, imagePoints[1][i],
                    cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE)) {

            cv::cornerSubPix(
                    colors[i], imagePoints[0][i], cv::Size(11, 11), cv::Size(-1, -1),
                    cv::TermCriteria(cv::TermCriteria::COUNT + cv::TermCriteria::EPS, 30, 0.01));
            cv::cornerSubPix(
                    depths[i], imagePoints[1][i], cv::Size(11, 11), cv::Size(-1, -1),
                    cv::TermCriteria(cv::TermCriteria::COUNT + cv::TermCriteria::EPS, 30, 0.01));

            cv::drawChessboardCorners(colors[i], patternSize,
                                      (cv::Mat)(imagePoints[0][i]), true);
            cv::drawChessboardCorners(depths[i], patternSize,
                                      (cv::Mat)(imagePoints[1][i]), true);
            cv::imshow("color", colors[i]);
            cv::imshow("depth", depths[i]);
        } else {
            std::cout << "cannot find all corners" << std::endl;
            colors.erase(colors.begin() + i);
            depths.erase(depths.begin() + i);
            imagePoints[0].erase(imagePoints[0].begin() + i);
            imagePoints[1].erase(imagePoints[1].begin() + i);
            std::cout << colors.size() << std::endl;
            i--;
        }

        cv::waitKey(100);
    }

    return colors.size();
}

void setWorldPoints(std::vector<std::vector<cv::Point3f>> &worldPoints,
                    const cv::Size patternSize, double squareSize,
                    const int &fileNum) {
    worldPoints.clear();
    worldPoints.resize(fileNum);

    for (int i = 0; i < fileNum; i++) {
        for (int j = 0; j < patternSize.height; j++)
            for (int k = 0; k < patternSize.width; k++)
                worldPoints[i].push_back(
                        cv::Point3f(k * squareSize, j * squareSize, 0));
    }
}

int main(int argc, char *argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);

    std::vector<cv::Mat> colors, depths;
    const cv::Size patternSize(9, 6);
    std::vector<std::vector<cv::Point3f>> worldPoints;
    std::vector<std::vector<std::vector<cv::Point2f>>> imagePoints(2);
    cv::TermCriteria criteria(cv::TermCriteria::COUNT | cv::TermCriteria::EPS, 20, 0.001);

    for (size_t i = 0; i < 2; i++)
        imagePoints[i].resize(FLAGS_size);

    loadImages(colors, depths, FLAGS_size);
    FLAGS_size = findChessboards(colors, depths, imagePoints, patternSize, FLAGS_size);
    std::cout << "number of correct files = " << FLAGS_size << std::endl;
    setWorldPoints(worldPoints, patternSize, 24.0, FLAGS_size);

    std::cout << "calibrate stereo cameras" << std::endl;
    std::vector<cv::Mat> cameraMatrix(2);
    std::vector<cv::Mat> distCoeffs(2);
    cameraMatrix[0] = cv::Mat::eye(3, 3, CV_64FC1);
    cameraMatrix[1] = cv::Mat::eye(3, 3, CV_64FC1);
    distCoeffs[0] = cv::Mat(8, 1, CV_64FC1);
    distCoeffs[1] = cv::Mat(8, 1, CV_64FC1);
    cv::Mat R, T, E, F;

    std::vector<cv::Mat> rvecs;
    std::vector<cv::Mat> tvecs;
    double rms1 = calibrateCamera(worldPoints, imagePoints[0], colors[0].size(),
                                  cameraMatrix[0], distCoeffs[0], rvecs, tvecs, cv::CALIB_FIX_K4 | cv::CALIB_FIX_K5);
    double rms2 = calibrateCamera(worldPoints, imagePoints[1], colors[0].size(),
                                  cameraMatrix[1], distCoeffs[1], rvecs, tvecs, cv::CALIB_FIX_K4 | cv::CALIB_FIX_K5);

    double rms = stereoCalibrate(
            worldPoints, imagePoints[0], imagePoints[1], cameraMatrix[0],
            distCoeffs[0], cameraMatrix[1], distCoeffs[1], colors[0].size(),
            R, T, E, F,  cv::CALIB_USE_INTRINSIC_GUESS +
                    //CV_CALIB_FIX_INTRINSIC +
                            cv::CALIB_FIX_PRINCIPAL_POINT + cv::CALIB_FIX_ASPECT_RATIO + cv::CALIB_ZERO_TANGENT_DIST +
                    //CV_CALIB_SAME_FOCAL_LENGTH +
                            cv::CALIB_RATIONAL_MODEL + cv::CALIB_FIX_K3 + cv::CALIB_FIX_K4 + cv::CALIB_FIX_K5,
            cv::TermCriteria(cv::TermCriteria::COUNT + cv::TermCriteria::EPS, 100, 1e-5)
   );
    std::cout << "done with RMS error = " << rms << std::endl;

    double err = 0;
    int npoints = 0;
    std::vector<cv::Vec3f> lines[2];
    for (int i = 0; i < FLAGS_size; i++) {
        int size = (int) imagePoints[0][i].size();
        cv::Mat imgpt[2];
        for (int k = 0; k < 2; k++) {
            imgpt[k] = cv::Mat(imagePoints[k][i]);
            cv::undistortPoints(imgpt[k], imgpt[k], cameraMatrix[k], distCoeffs[k],
                                cv::Mat(), cameraMatrix[k]);
            cv::computeCorrespondEpilines(imgpt[k], k + 1, F, lines[k]);
        }

        for (int j = 0; j < size; j++) {
            double errij =
                    std::fabs(imagePoints[0][i][j].x * lines[1][j][0] +
                              imagePoints[0][i][j].y * lines[1][j][1] +
                              lines[1][j][2]) +
                    std::fabs(imagePoints[1][i][j].x * lines[0][j][0] +
                              imagePoints[1][i][j].y * lines[0][j][1] +
                              lines[0][j][2]);
            err += errij;
        }
        npoints += size;
    }
    std::cout << "average reprojection error = " << err / npoints << std::endl;

    cv::Mat R1, R2, P1, P2, Q;
    cv::Rect validROI[2];
    stereoRectify(cameraMatrix[0], distCoeffs[0], cameraMatrix[1],
                  distCoeffs[1], colors[0].size(), R, T, R1, R2, P1, P2, Q,
                  cv::CALIB_ZERO_DISPARITY, 1, colors[0].size(),
                  &validROI[0], &validROI[1]);

    {
        cv::FileStorage fs(FLAGS_intrinsics.c_str(), cv::FileStorage::WRITE);
        if (fs.isOpened()) {
            fs << "M1" << cameraMatrix[0] << "D1" << distCoeffs[0]
                << "M2" << cameraMatrix[1] << "D2" << distCoeffs[1];
            fs.release();
        }
    }

    cv::Mat rmap[2][2];
    cv::initUndistortRectifyMap(cameraMatrix[0], distCoeffs[0], R1, P1,
                                colors[0].size(),
                                CV_16SC2,
                                rmap[0][0], rmap[0][1]);
    cv::initUndistortRectifyMap(cameraMatrix[1], distCoeffs[1], R2, P2,
                                colors[0].size(),
                                CV_16SC2,
                                rmap[1][0], rmap[1][1]);

    cv::Mat_<int> validROIMat(2, 4);
    for (int i = 0; i < 2; ++i) {
        validROIMat.at<int>(i, 0) = validROI[i].x;
        validROIMat.at<int>(i, 1) = validROI[i].y;
        validROIMat.at<int>(i, 2) = validROI[i].width;
        validROIMat.at<int>(i, 3) = validROI[i].height;
    }

    cv::FileStorage fs("stereo-params.xml", cv::FileStorage::WRITE);
    if (fs.isOpened()) {
        fs << "M1" << cameraMatrix[0] << "D1" << distCoeffs[0] << "M2"
           << cameraMatrix[1] << "D2" << distCoeffs[1];
        fs << "R" << R << "T" << T << "R1" << R1 << "R2" << R2 << "P1" << P1
           << "P2" << P2 << "Q" << Q << "validROI" << validROIMat;
        fs.release();
    }

    double apertureWidth = 0, apertureHeight = 0;
    double fovx = 0, fovy = 0;
    double focalLength = 0;
    cv::Point2d principalPoint(0, 0);
    double aspectRatio = 0;

    {
        cv::FileStorage fs(FLAGS_extrinsics.c_str(), cv::FileStorage::WRITE);
        if (fs.isOpened()) {
            fs << "R" << R << "T" << T << "R1" << R1 << "R2" << R2
               << "P1" << P1 << "P2" << P2 << "Q" << Q
               << "V1" << validROI[0] << "V2" << validROI[1];
            fs.release();
        }
    }

    cv::Mat canvas;
    double sf;
    int w, h;

    sf = 600. / MAX(colors[0].size().width, colors[0].size().height);
    w = cvRound(colors[0].size().width * sf);
    h = cvRound(colors[0].size().height * sf);
    canvas.create(h, w * 2, CV_8UC3);

    cv::namedWindow("Rectified", cv::WINDOW_AUTOSIZE | cv::WINDOW_FREERATIO);

    for (int i = 0; i < FLAGS_size; i++) {
        for (int k = 0; k < 2; k++) {
            if (k == 0) {
                cv::Mat img = colors[i].clone(), rimg, cimg;
                cv::remap(img, rimg, rmap[k][0], rmap[k][1], cv::INTER_LINEAR);
                cv::cvtColor(rimg, cimg, cv::COLOR_GRAY2BGR);
                cv::Mat canvasPart = canvas(cv::Rect(w * k, 0, w, h));
                cv::resize(cimg, canvasPart, canvasPart.size(), 0, 0, cv::INTER_AREA);

                cv::Rect vroi(cvRound(validROI[k].x * sf),
                              cvRound(validROI[k].y * sf),
                              cvRound(validROI[k].width * sf),
                              cvRound(validROI[k].height * sf));
                cv::rectangle(canvasPart, vroi, cv::Scalar(0, 0, 255), 3, 8);
            } else {
                cv::Mat img = depths[i].clone(), rimg, cimg;
                cv::remap(img, rimg, rmap[k][0], rmap[k][1], cv::INTER_LINEAR);
                cvtColor(rimg, cimg, cv::COLOR_GRAY2BGR);
                cv::Mat canvasPart = canvas(cv::Rect(w * k, 0, w, h));
                cv::resize(cimg, canvasPart, canvasPart.size(), 0, 0, cv::INTER_AREA);

                cv::Rect vroi(cvRound(validROI[k].x * sf),
                              cvRound(validROI[k].y * sf),
                              cvRound(validROI[k].width * sf),
                              cvRound(validROI[k].height * sf));
                cv::rectangle(canvasPart, vroi, cv::Scalar(0, 0, 255), 3, 8);
            }
        }

        for (int j = 0; j < canvas.rows; j += 16)
            cv::line(canvas, cv::Point(0, j), cv::Point(canvas.cols, j),
                     cv::Scalar(0, 255, 0), 1, 8);

        cv::imshow("Rectified", canvas);

        if (cv::waitKey(0) == 'q')
            break;
    }

    cv::destroyAllWindows();
    return 0;
}
