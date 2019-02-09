#include <cstdio>
#include <iostream>
#include <vector>
#include <numeric>
#include <chrono>
#include <atomic>
#include <locale>
#include <future>	// C++11: async(); feature<>;
#include <iostream>
#include <fstream>  // std::ofstream
#include <algorithm> // std::unique

#include <opencv2/opencv.hpp>			// C++
#include <opencv2/core/version.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

#ifdef _DEBUG
#define LIB_SUFFIX "d.lib"
#else
#define LIB_SUFFIX ".lib"
#endif // DEBUG

#ifndef CV_VERSION_EPOCH
#include "opencv2/videoio/videoio.hpp"
#define OPENCV_VERSION CVAUX_STR(CV_VERSION_MAJOR)"" CVAUX_STR(CV_VERSION_MINOR)"" CVAUX_STR(CV_VERSION_REVISION)
#pragma comment(lib, "opencv_world" OPENCV_VERSION LIB_SUFFIX)
#else
#define OPENCV_VERSION CVAUX_STR(CV_VERSION_EPOCH)"" CVAUX_STR(CV_VERSION_MAJOR)"" CVAUX_STR(CV_VERSION_MINOR)
#pragma comment(lib, "opencv_core" OPENCV_VERSION LIB_SUFFIX)
#pragma comment(lib, "opencv_imgproc" OPENCV_VERSION LIB_SUFFIX)
#pragma comment(lib, "opencv_highgui" OPENCV_VERSION LIB_SUFFIX)
#endif

using namespace cv;

#include "main.h"

//-------------*-------------
//    MAX WINDOW SIZE IN PX
const int ScreenWidth = 1680;
const int ScreenHeight = 1050;

const int PreviewHeight = 100;
const int PreviewWidth = 100;
//-------------*-------------

std::atomic<bool> right_button_click;
std::atomic<bool> clear_marks;

std::atomic<bool> show_help;
std::atomic<bool> exit_flag(false);

std::atomic<int> mark_line_width(2); // default mark line width is 2 pixels.
const int MAX_MARK_LINE_WIDTH = 3;
std::atomic<bool> show_mark_class(true);

std::atomic<int> x_delete, y_delete;
std::atomic<int> x_start, y_start;
std::atomic<int> x_end, y_end;
std::atomic<int> x_size, y_size;
std::atomic<bool> draw_select, selected, undo;
std::atomic<bool> drag_bbox;
std::atomic<int> x_mouseMove_start, y_mouseMove_start;
std::atomic<int> selectedBoxId;

std::vector<coord_t> current_coord_vec;
bool mousePanning = false;

std::atomic<int> add_id_img;
Rect prev_img_rect(0, 0, 50, 100);
Rect next_img_rect(1280 - 50, 0, 50, 100);

Size current_img_size;
Size original_img_size;

Rect full_rect_dst;
float mouseScroll = 0;
int scrollHeightPad = 0;
int scrollWidthPad = 0;
bool zooming = false;
std::atomic<int> imgHOrig, imgWOrig;
int topPad, botPad, leftPad, rightPad;
float scaleWidth, scaleHeight;
std::string images_path;

std::string previousImagePath = "";

void callback_mouse_click(int event, int x, int y, int flags, void* user_data)
{
    bool holdingCtrl = flags & cv::EVENT_FLAG_CTRLKEY; // ctrl is pressed
    
    if (event == cv::EVENT_LBUTTONDBLCLK)
    {
        std::cout << "cv::EVENT_LBUTTONDBLCLK \n";
    }
    else if (event == cv::EVENT_LBUTTONDOWN)
    {
        x_start = x;
        y_start = y;
        
        if(holdingCtrl){
            drag_bbox = true;
            x_mouseMove_start = x;
            y_mouseMove_start = y;
        }else{
            draw_select = true;
            selected = false;
            if (prev_img_rect.contains(Point2i(x, y))) add_id_img = -1;
            else if (next_img_rect.contains(Point2i(x, y))) add_id_img = 1;
            else add_id_img = 0;
        }
                
        // get selected
        selectedBoxId = getClickedBoxId(current_coord_vec, x, y);
        //std::cout << "cv::EVENT_LBUTTONDOWN \n";
    }
    else if (event == cv::EVENT_LBUTTONUP)
    {
        selectedBoxId = -1;
        
        x_size = abs(x - x_start);
        y_size = abs(y - y_start);
        x_end = max(x, 0);
        y_end = max(y, 0);
       
        //std::cout << "cv::EVENT_LBUTTONUP \n";
        
        if(drag_bbox){
            drag_bbox = false;
        }else{
            draw_select = false;
            selected = true;
        }
    }
    else if (event == cv::EVENT_RBUTTONDOWN)
    {
        x_mouseMove_start = x;
        y_mouseMove_start = y;
        mousePanning = true;
    }
    else if (event == cv::EVENT_RBUTTONUP)
    {
        mousePanning = false;
    }
    if (event == cv::EVENT_RBUTTONDBLCLK) // right mouse button double click
    {
        // get selected
        selectedBoxId = getClickedBoxId(current_coord_vec, x, y);
        right_button_click = true;
        std::cout << "cv::EVENT_RBUTTONDOWN \n";
        x_delete = x;
        y_delete = y;
        //std::cout << prev_img_rect.height;
        y_delete -= prev_img_rect.height; // remove top bar height from coordinates
        y_delete = max((int)y_delete, 0);
        std::cout << "cv::EVENT_RBUTTONDBLCLK \n";
    }
    else if (event == cv::EVENT_MOUSEMOVE)
    {
        if(mousePanning){
            scrollHeightPad += (y - y_mouseMove_start);
            scrollWidthPad += (x - x_mouseMove_start);
            // clamp min to above or equal to zero
            scrollHeightPad = max(scrollHeightPad, 0);
            scrollWidthPad = max(scrollWidthPad, 0);
            // clamp scroll max:
            //scrollHeightPad = min(scrollHeightPad, original_img_size.height - current_img_size.height);
            //scrollWidthPad = min(scrollWidthPad, original_img_size.width - current_img_size.width);
            x_mouseMove_start = x;
            y_mouseMove_start = y;
        }else{
            x_end = max(x, 0);
            y_end = max(y, 0);
        }
    }
    
    // if mouse scroll:
    if (event==EVENT_MOUSEWHEEL)
    {
        if (getMouseWheelDelta(flags) > 0 && mouseScroll > 0.1){
            mouseScroll -= (float)0.1;
            zooming = true;
            //std::min(mouseScroll, 0.9f);
            std::cout << "Zooming out " << mouseScroll << "\n";
        }
        else if(getMouseWheelDelta(flags) < 0 && mouseScroll < 0.9){
            mouseScroll += (float)0.1;
            zooming = true;
            //std::max(mouseScroll, 0.0f);
            std::cout << "Zooming in " << mouseScroll << "\n";
        }else{
            if(mouseScroll >= 0 && mouseScroll < 0.1){
                std::cout << "fully zoomed out!!, to zoom in please change scroll direction, ";
            }else{
                std::cout << "fully zoomed in!!, to zoom out please change scroll direction, ";
            }
            std::cout << "event = "<< event << ", mouse delta: " << getMouseWheelDelta(flags) << " mouse scroll: " << mouseScroll << "\n";
        }
    }
    
    // for older opencv versions and Ubuntu vertical scroll somehow uses horizontal scroll events
    if(event == EVENT_MOUSEHWHEEL)
    {
        if (getMouseWheelDelta(flags) > 0 && mouseScroll > 0.1){
            mouseScroll -= (float)0.1;
            zooming = true;
            //std::min(mouseScroll, 0.9f);
            std::cout << "Zooming out " << mouseScroll << "\n";
        }
        else if(getMouseWheelDelta(flags) < 0 && mouseScroll < 0.9){
            mouseScroll += (float)0.1;
            zooming = true;
            //std::max(mouseScroll, 0.0f);
            std::cout << "Zooming in " << mouseScroll << "\n";
        }else{
            if(mouseScroll >= 0 && mouseScroll < 0.1){
                std::cout << "fully zoomed out!!, to zoom in please change scroll direction, ";
            }else{
                std::cout << "fully zoomed in!!, to zoom out please change scroll direction, ";
            }
            std::cout << "event = "<< event << ", mouse delta: " << getMouseWheelDelta(flags) << " mouse scroll: " << mouseScroll << "\n";
        }
    }
}

class comma : public std::numpunct<char> {
public:
	comma() : std::numpunct<char>() {}
protected:
	char do_decimal_point() const { return '.';	}
};

int main(int argc, char *argv[])
{

	try
	{
		std::locale loccomma(std::locale::classic(), new comma);
		std::locale::global(loccomma);

		images_path = "./";

		if (argc >= 2) {
			images_path = std::string(argv[1]);         // path to images, train and synset
		}
		else {
			std::cout << "Usage: [path_to_images] [train.txt] [obj.names] \n";
			return 0;
		}

		std::string train_filename = images_path + "train.txt";
		std::string synset_filename = images_path + "obj.names";

		if (argc >= 3) {
			train_filename = std::string(argv[2]);		// file containing: list of images
		}

		if (argc >= 4) {
			synset_filename = std::string(argv[3]);		// file containing: object names
		}

		// capture frames from video file - 1 frame per 3 seconds of video
		if (argc >= 4 && train_filename == "cap_video") {
			const std::string videofile = synset_filename;
			cv::VideoCapture cap(videofile);
			//const int fps = cap.get(CV_CAP_PROP_FPS);
			const int fps = cap.get(CAP_PROP_FPS);
			int frame_counter = 0, image_counter = 0;
			float save_each_frames = 50;
			if (argc >= 5) save_each_frames = std::stoul(std::string(argv[4]));

			int pos_filename = 0;
			if ((1 + videofile.find_last_of("\\")) < videofile.length()) pos_filename = 1 + videofile.find_last_of("\\");
			if ((1 + videofile.find_last_of("/")) < videofile.length()) pos_filename = std::max(pos_filename, 1 + (int)videofile.find_last_of("/"));
			std::string const filename = videofile.substr(pos_filename);
			std::string const filename_without_ext = filename.substr(0, filename.find_last_of("."));

			for (cv::Mat frame; cap >> frame, cap.isOpened() && !frame.empty();) {
				cv::imshow("video cap to frames", frame);
#ifndef CV_VERSION_EPOCH
				int pressed_key = cv::waitKeyEx(20);	// OpenCV 3.x
#else
				int pressed_key = cv::waitKey(20);		// OpenCV 2.x
#endif
				if (pressed_key == 27 || pressed_key == 1048603) break;  // ESC - exit (OpenCV 2.x / 3.x)
				if (frame_counter++ >= save_each_frames) {		// save frame for each 3 second
					frame_counter = 0;
					std::string img_name = images_path + "/" + filename_without_ext + "_" + std::to_string(image_counter++) + ".jpg";
					std::cout << "saved " << img_name << std::endl;
					cv::imwrite(img_name, frame);
				}
			}
			exit(0);
		}

        selectedBoxId = -1;
		bool show_mouse_coords = false;
		std::vector<std::string> filenames_in_folder;
		//glob(images_path, filenames_in_folder); // void glob(String pattern, std::vector<String>& result, bool recursive = false);
		cv::String images_path_cv = images_path;
		std::vector<cv::String> filenames_in_folder_cv;
		glob(images_path_cv, filenames_in_folder_cv); // void glob(String pattern, std::vector<String>& result, bool recursive = false);
		for (auto &i : filenames_in_folder_cv) 
			filenames_in_folder.push_back(i);

		std::vector<std::string> jpg_filenames_path;
		std::vector<std::string> jpg_filenames;
		std::vector<std::string> jpg_filenames_without_ext;
		std::vector<std::string> image_ext;
		std::vector<std::string> txt_filenames;
		std::vector<std::string> jpg_in_train;
		std::vector<std::string> synset_txt;

		// image-paths to txt-paths
		for (auto &i : filenames_in_folder)
		{
			int pos_filename = 0;
			if ((1 + i.find_last_of("\\")) < i.length()) pos_filename = 1 + i.find_last_of("\\");
			if ((1 + i.find_last_of("/")) < i.length()) pos_filename = std::max(pos_filename, 1 + (int)i.find_last_of("/"));


			std::string const filename = i.substr(pos_filename);
			std::string const ext = i.substr(i.find_last_of(".") + 1);
			std::string const filename_without_ext = filename.substr(0, filename.find_last_of("."));

			if (ext == "jpg" || ext == "JPG" || 
				ext == "jpeg" || ext == "JPEG" ||
				ext == "bmp" || ext == "BMP" ||
				ext == "png" || ext == "PNG" || 
				ext == "ppm" || ext == "PPM")
			{
				jpg_filenames_without_ext.push_back(filename_without_ext);
				image_ext.push_back(ext);
				jpg_filenames.push_back(filename);
				jpg_filenames_path.push_back(i);
			}
			if (ext == "txt") {
				txt_filenames.push_back(filename_without_ext);
			}
		}
		std::sort(jpg_filenames.begin(), jpg_filenames.end());
		std::sort(jpg_filenames_path.begin(), jpg_filenames_path.end());
		std::sort(txt_filenames.begin(), txt_filenames.end());

		if (jpg_filenames.size() == 0) {
			std::cout << "Error: Image files not found by path: " << images_path << std::endl;
			return 0;
		}

		// check whether there are files with the same names (but different extensions)
		{
			auto sorted_names_without_ext = jpg_filenames_without_ext;
			std::sort(sorted_names_without_ext.begin(), sorted_names_without_ext.end());
			for (size_t i = 1; i < sorted_names_without_ext.size(); ++i) {
				if (sorted_names_without_ext[i - 1] == sorted_names_without_ext[i]) {
					std::cout << "Error: Can't create " << sorted_names_without_ext[i] << 
						".txt file for several images with different extensions but with the same filename: "
						<< sorted_names_without_ext[i] << std::endl;
					// print duplicate images
					for (size_t k = 0; k < jpg_filenames_without_ext.size(); ++k) {
						if (jpg_filenames_without_ext[k] == sorted_names_without_ext[i]) {
							std::cout << jpg_filenames_without_ext[k] << "." << image_ext[k] << std::endl;
						}
					}
					return 0;
				}
			}
		}

		// intersect jpg & txt
		std::vector<std::string> intersect_filenames(jpg_filenames.size());
		std::vector<std::string> difference_filenames(jpg_filenames.size());
		std::vector<std::string> intersect_ext;
		std::vector<std::string> difference_ext;

		auto dif_it_end = std::set_difference(jpg_filenames_without_ext.begin(), jpg_filenames_without_ext.end(),
			txt_filenames.begin(), txt_filenames.end(),
			difference_filenames.begin());
		difference_filenames.resize(dif_it_end - difference_filenames.begin());
				
		auto inter_it_end = std::set_intersection(jpg_filenames_without_ext.begin(), jpg_filenames_without_ext.end(),
			txt_filenames.begin(), txt_filenames.end(),
			intersect_filenames.begin());
		intersect_filenames.resize(inter_it_end - intersect_filenames.begin());

		// get intersect extensions for intersect_filenames
		for (auto &i : intersect_filenames) {
			size_t ext_index = find(jpg_filenames_without_ext.begin(), jpg_filenames_without_ext.end(), i) - jpg_filenames_without_ext.begin();
			intersect_ext.push_back(image_ext[ext_index]);
		}

		// get difference extensions for intersect_filenames
		for (auto &i : difference_filenames) {
			size_t ext_index = find(jpg_filenames_without_ext.begin(), jpg_filenames_without_ext.end(), i) - jpg_filenames_without_ext.begin();
			difference_ext.push_back(image_ext[ext_index]);
		}

		txt_filenames.clear();
		for (auto &i : intersect_filenames) {
			txt_filenames.push_back(i + ".txt");
		}

		int image_list_count = max(1, (int)jpg_filenames_path.size() - 1);

		// store train.txt
		std::ofstream ofs_train(train_filename, std::ios::out | std::ios::trunc);
		if (!ofs_train.is_open()) {
			throw(std::runtime_error("Can't open file: " + train_filename));
		}

		for (size_t i = 0; i < intersect_filenames.size(); ++i) {
			ofs_train << images_path << "/" << intersect_filenames[i] << "." << intersect_ext[i] << std::endl;
		}
		ofs_train.flush();
		std::cout << "File opened for output: " << train_filename << std::endl;


		// load synset.txt
		{
			std::ifstream ifs(synset_filename);
			if (!ifs.is_open()) {
				throw(std::runtime_error("Can't open file: " + synset_filename));
			}

			for (std::string line; getline(ifs, line);)
				synset_txt.push_back(line);
		}
		std::cout << "File loaded: " << synset_filename << std::endl;

		Mat preview(Size(PreviewWidth, PreviewHeight), CV_8UC3);
		//Mat full_image(Size(1680, 1050), CV_8UC3);
		Mat full_image(Size(ScreenWidth, ScreenHeight), CV_8UC3);
		Mat frame(Size(full_image.cols, full_image.rows + preview.rows), CV_8UC3); // 8bit U Channels 3

		full_rect_dst = Rect(Point2i(0, preview.rows), Size(frame.cols, frame.rows - preview.rows));
		Mat full_image_roi = frame(full_rect_dst);

		size_t const preview_number = frame.cols / preview.cols;
        std::vector<Mat> previewImagesCache;
        float scaleFactor = 1.0;
        float resizeFactorImage = 0.0;
        
		std::string const window_name = "Marking images";
		namedWindow(window_name, WINDOW_NORMAL);
        
        resizeWindow(window_name, 1280, 720);
		
        imshow(window_name, frame);
		moveWindow(window_name, 0, 0);
		setMouseCallback(window_name, callback_mouse_click);

		bool next_by_click = false;
		bool marks_changed = false;

		int old_trackbar_value = -1, trackbar_value = 0;
		std::string const trackbar_name = "image num";
		int tb_res = createTrackbar(trackbar_name, window_name, &trackbar_value, image_list_count);

		int old_current_obj_id = -1, current_obj_id = 0;
		std::string const trackbar_name_2 = "object id";
		int const max_object_id = (synset_txt.size() > 0) ? synset_txt.size() : 20;
		int tb_res_2 = createTrackbar(trackbar_name_2, window_name, &current_obj_id, max_object_id);
        Mat originalImage;
        int originalImaleLoadedId = -1;
        Mat dst_roi;

		do {
			//trackbar_value = min(max(0, trackbar_value), (int)jpg_filenames_path.size() - 1);
			if (old_trackbar_value != trackbar_value || exit_flag || zooming || mousePanning || drag_bbox)
			{
				trackbar_value = min(max(0, trackbar_value), (int)jpg_filenames_path.size() - 1);
				setTrackbarPos(trackbar_name, window_name, trackbar_value);
				frame(Rect(0, 0, frame.cols, preview.rows)) = Scalar::all(0);

				// save current coords
				if (old_trackbar_value >= 0 && !zooming && !mousePanning && !drag_bbox) // && current_coord_vec.size() > 0) // Yolo v2 can processes background-image without objects
				{
					try
					{
						std::string const jpg_filename = jpg_filenames[old_trackbar_value];
						std::string const filename_without_ext = jpg_filename.substr(0, jpg_filename.find_last_of("."));
						std::string const txt_filename = filename_without_ext + ".txt";
						std::string const txt_filename_path = images_path + "/" + txt_filename;

						std::cout << "txt_filename_path = " << txt_filename_path << std::endl;
                        previousImagePath = txt_filename_path;

						std::ofstream ofs(txt_filename_path, std::ios::out | std::ios::trunc);
						ofs << std::fixed;

						// store coords to [image name].txt
						for (auto &i : current_coord_vec)
						{
							float const relative_center_x = (float)(i.abs_rect.x + i.abs_rect.width / 2);// / full_image_roi.cols;
							float const relative_center_y = (float)(i.abs_rect.y + i.abs_rect.height / 2);// / full_image_roi.rows;
							float const relative_width = (float)i.abs_rect.width;// / full_image_roi.cols;
							float const relative_height = (float)i.abs_rect.height;// / full_image_roi.rows;

							if (relative_width <= 0) continue;
							if (relative_height <= 0) continue;
							if (relative_center_x <= 0) continue;
							if (relative_center_y <= 0) continue;

							ofs << i.id << " " <<
								relative_center_x << " " << relative_center_y << " " <<
								relative_width << " " << relative_height << std::endl;
						}
						
						// store [path/image name.jpg] to train.txt
						auto it = std::find(difference_filenames.begin(), difference_filenames.end(), filename_without_ext);
						if (it != difference_filenames.end())
						{
							ofs_train << images_path << "/" << jpg_filename << std::endl;
							ofs_train.flush();

							size_t new_size = std::remove(difference_filenames.begin(), difference_filenames.end(), filename_without_ext) -
								difference_filenames.begin();
							difference_filenames.resize(new_size);
						}
					}
					catch (...) { std::cout << " Exception when try to write txt-file \n"; }
				}
                
                bool loadPreviewImagesToCache = false;
				// show preview images
				for (size_t i = 0; i < preview_number && (i + trackbar_value) < jpg_filenames_path.size(); ++i)
				{
                    if(originalImaleLoadedId != trackbar_value && !zooming && !mousePanning && !drag_bbox){
                        originalImaleLoadedId = trackbar_value;
                        originalImage = imread(jpg_filenames_path[trackbar_value + i]);
                        // check if the image has been loaded successful to prevent crash 
                        if (originalImage.cols == 0)
                        {
                            continue;
                        }
                        original_img_size = originalImage.size();
                        resize(originalImage, preview, preview.size());
                        previewImagesCache.clear();
                        loadPreviewImagesToCache = true;
                        previewImagesCache.push_back(preview.clone());
                    }else{
                        if(loadPreviewImagesToCache){
                            Mat previewImage = imread(jpg_filenames_path[trackbar_value + i]);
                            // check if the image has been loaded successful to prevent crash 
                            if (previewImage.cols == 0)
                            {
                                continue;
                            }
                            resize(previewImage, preview, preview.size());
                            previewImagesCache.push_back(preview.clone());
                        }else{
                            if(i < previewImagesCache.size()){
                                preview = previewImagesCache.at(i);
                                //resize(previewImage, preview, preview.size());
                            }
                        }
                    }
                    
                    int const x_shift = i*preview.cols + prev_img_rect.width;
                    Rect rect_dst(Point2i(x_shift, 0), preview.size());
                    dst_roi = frame(rect_dst);
                    preview.copyTo(dst_roi);
                    //rectangle(frame, rect_dst, Scalar(200, 150, 200), 2);
                    putText(dst_roi, jpg_filenames[trackbar_value + i], Point2i(0, 10), FONT_HERSHEY_COMPLEX_SMALL, 0.5, Scalar::all(255));
                        
					if (i == 0) // for current image, show bellow
					{ 
                        imgHOrig = originalImage.rows;
                        imgWOrig = originalImage.cols;
                        
                        Mat img = originalImage;
                        full_image = resizeKeepAspectRatio(img, full_rect_dst.size(), cv::Scalar(0,0,0));
                        full_image.copyTo(full_image_roi);
						current_img_size = img.size();
                        scaleFactor = (float)original_img_size.width / current_img_size.width;
                        resizeFactorImage = (float) full_rect_dst.width / original_img_size.width;
                        
                        // pad to cursor location:
                        int const x_inside = std::min((int)x_end, full_image_roi.cols);
                        int const y_inside = std::min(std::max(0, y_end - (int)prev_img_rect.height), full_image_roi.rows);
                        float const relative_center_x = ((float)(x_inside) / full_image_roi.cols);
                        float const relative_center_y = ((float)(y_inside) / full_image_roi.rows);
                        
                        // read bounding boxes from file
                        if(!zooming && !mousePanning && !drag_bbox){
                            readCoordsFromFile(jpg_filenames[trackbar_value], current_coord_vec, true);
                        }
					}

					std::string const jpg_filename = jpg_filenames[trackbar_value + i];
					std::string const filename_without_ext = jpg_filename.substr(0, jpg_filename.find_last_of("."));
					if (!std::binary_search(difference_filenames.begin(), difference_filenames.end(), filename_without_ext))
					{
						line(dst_roi, Point2i(80, 88), Point2i(85, 93), Scalar(20, 70, 20), 5);
						line(dst_roi, Point2i(85, 93), Point2i(93, 85), Scalar(20, 70, 20), 5);

						line(dst_roi, Point2i(80, 88), Point2i(85, 93), Scalar(50, 200, 100), 2);
						line(dst_roi, Point2i(85, 93), Point2i(93, 85), Scalar(50, 200, 100), 2);
					}
				}
                if(!zooming && !mousePanning && !drag_bbox)
                    std::cout << " trackbar_value = " << trackbar_value << std::endl;

				old_trackbar_value = trackbar_value;

				marks_changed = false;
                zooming = false;

                
				//rectangle(frame, prev_img_rect, Scalar(100, 100, 100), CV_FILLED);
				//rectangle(frame, next_img_rect, Scalar(100, 100, 100), CV_FILLED);

				rectangle(frame, prev_img_rect, Scalar(100, 100, 100), FILLED);
				rectangle(frame, next_img_rect, Scalar(100, 100, 100), FILLED);
			}

			trackbar_value = min(max(0, trackbar_value), (int)jpg_filenames_path.size() - 1);

			// highlight prev img
			for (size_t i = 0; i < preview_number && (i + trackbar_value) < jpg_filenames_path.size(); ++i)
			{
				int const x_shift = i*preview.cols + prev_img_rect.width;
				Rect rect_dst(Point2i(x_shift, 0), Size(preview.cols - 2, preview.rows));
				Scalar color(100, 70, 100);
				if (i == 0) color = Scalar(250, 120, 150);
				if (y_end < preview.rows && i == (x_end - prev_img_rect.width) / preview.cols) color = Scalar(250, 200, 200);
				rectangle(frame, rect_dst, color, 2);
			}

			if (undo) {
				undo = false;
				if(current_coord_vec.size() > 0) {
					full_image.copyTo(full_image_roi);
					current_coord_vec.pop_back();
				}
			}

			if (selected) // mouse box draw
			{
				selected = false;
				full_image.copyTo(full_image_roi);

				if (y_end < preview.rows && x_end > prev_img_rect.width && x_end < (full_image.cols - prev_img_rect.width) &&
					y_start < preview.rows)
				{
					int const i = (x_end - prev_img_rect.width) / preview.cols;
					trackbar_value += i;
				}
				else if (y_end >= preview.rows)
				{
					if (next_by_click) {
						++trackbar_value;
						current_coord_vec.clear();
					}
                    
					Rect selected_rect(
						Point2i((int)min(x_start, x_end), (int)min(y_start, y_end)),
						Size(x_size, y_size));

					selected_rect &= full_rect_dst;
					selected_rect.y -= (int)prev_img_rect.height;
                    
                    coord_t coord;
                    // rescale selected_rect
                    int imgWidth = full_rect_dst.width - rightPad - leftPad;
                    int imgHeight = full_rect_dst.height - topPad - botPad;
                    coord.abs_rect.x = (float)selected_rect.x / scaleWidth - leftPad + scrollWidthPad;
                    coord.abs_rect.y = (float)selected_rect.y / scaleHeight - topPad + scrollHeightPad;
                
                    coord.abs_rect.x /= (float)imgWidth;
                    coord.abs_rect.y /= (float)imgHeight;
                    // clamp values between 0 and 1
                    coord.abs_rect.x = std::max((double)coord.abs_rect.x, 0.0);
                    coord.abs_rect.x = std::min((double)coord.abs_rect.x, 1.0);
                    coord.abs_rect.y = std::max((double)coord.abs_rect.y, 0.0);
                    coord.abs_rect.y = std::min((double)coord.abs_rect.y, 1.0);

                    coord.abs_rect.width = (float)selected_rect.width / scaleWidth / imgWidth;
                    coord.abs_rect.height = (float)selected_rect.height / scaleHeight / imgHeight;
                    
					//coord.abs_rect = selected_rect;
					coord.id = current_obj_id;
					current_coord_vec.push_back(coord);

					marks_changed = true;
				}
			}

			std::string current_synset_name;
			if (current_obj_id < synset_txt.size()) current_synset_name = "   - " + synset_txt[current_obj_id];
            
			if (show_mouse_coords) {
				full_image.copyTo(full_image_roi);
				int const x_inside = std::min((int)x_end, full_image_roi.cols);
				int const y_inside = std::min(std::max(0, y_end - (int)prev_img_rect.height), full_image_roi.rows);
				float const relative_center_x = (float)(x_inside) / full_image_roi.cols;
				float const relative_center_y = (float)(y_inside) / full_image_roi.rows;
				int const abs_x = relative_center_x*current_img_size.width;
				int const abs_y = relative_center_y*current_img_size.height;
				char buff[100];
				snprintf(buff, 100, "Abs: %d x %d    Rel: %.3f x %.3f", abs_x, abs_y, relative_center_x, relative_center_y);
				//putText(full_image_roi, buff, Point2i(800, 20), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(50, 10, 10), 3);
				putText(full_image_roi, buff, Point2i(800, 20), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(100, 50, 50), 2);
				putText(full_image_roi, buff, Point2i(800, 20), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(220, 120, 120), 1);
			}
			else
			{
				full_image.copyTo(full_image_roi);
				std::string text = "Show mouse coordinates - press M";
				putText(full_image_roi, text, Point2i(800, 20), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(100, 50, 50), 2);
				//putText(full_image_roi, text, Point2i(800, 20), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(220, 120, 120), 1);
			}


			if (draw_select)
			{
				if (add_id_img != 0) trackbar_value += add_id_img;

				if (y_start >= preview.rows)
				{
					//full_image.copyTo(full_image_roi);
					Rect selected_rect(
						Point2i(max(0, (int)min(x_start, x_end)), max(preview.rows, (int)min(y_start, y_end))),
						Point2i(max(x_start, x_end), max(y_start, y_end)));
					rectangle(frame, selected_rect, Scalar(150, 200, 150));

					if (show_mark_class)
					{
						putText(frame, std::to_string(current_obj_id) + current_synset_name,
							selected_rect.tl() + Point2i(2, 22), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(150, 200, 150), 2);
					}
				}
			}
            
            // Draw crosshair
			{
				const int offset = preview.rows; // Vertical offset

				// Only draw crosshair, if mouse is over image area
				if (y_end >= offset)
				{
					const bool bit_high = true;
					const bool bit_low = false;
					const int mouse_offset = 25;
					const int ver_min = draw_select ? std::min(x_end - mouse_offset, x_start - mouse_offset) : x_end - mouse_offset;
					const int ver_max = draw_select ? std::max(x_end + mouse_offset, x_start + mouse_offset) : x_end + mouse_offset;
					const int hor_min = draw_select ? std::min(y_end - mouse_offset, y_start - mouse_offset) : y_end - mouse_offset;
					const int hor_max = draw_select ? std::max(y_end + mouse_offset, y_start + mouse_offset) : y_end + mouse_offset;

					// Draw crosshair onto empty canvas (draws high bits on low-bit-canvas)
					cv::Mat crosshair_mask(frame.size(), CV_8UC1, cv::Scalar(bit_low));
					cv::line(crosshair_mask, cv::Point(0, y_end), cv::Point(ver_min, y_end), cv::Scalar(bit_high)); // Horizontal, left to mouse
					cv::line(crosshair_mask, cv::Point(ver_max, y_end), cv::Point(crosshair_mask.size().width, y_end), cv::Scalar(bit_high)); // Horizontal, mouse to right
					cv::line(crosshair_mask, cv::Point(x_end, offset), cv::Point(x_end, std::max(offset, hor_min)), cv::Scalar(bit_high)); // Vertical, top to mouse
					cv::line(crosshair_mask, cv::Point(x_end, hor_max), cv::Point(x_end, crosshair_mask.size().height), cv::Scalar(bit_high)); // Vertical, mouse to bottom

					// Draw crosshair onto frame copy
					cv::Mat crosshair_frame(frame.size(), frame.type());
					frame.copyTo(crosshair_frame);
					cv::bitwise_not(crosshair_frame, crosshair_frame, crosshair_mask);

					// Fade-in frame copy with crosshair into original frame (for alpha)
					const double alpha = 0.7;
					cv::addWeighted(crosshair_frame, alpha, frame, 1 - alpha, 0.0, frame);
				}
			}
			// remove all labels from this image
			if (clear_marks == true)
			{
				clear_marks = false;
				marks_changed = true;
				full_image.copyTo(full_image_roi);
				current_coord_vec.clear();
			}

			if (right_button_click == true)
			{
				right_button_click = false;
                //int id = getClickedBoxId(current_coord_vec, full_rect_dst);
                if(selectedBoxId > -1)
                {
                    std::cout << "deleting box at location x: " << x_delete << " y: " << y_delete << "\n";
                    current_coord_vec.erase(current_coord_vec.begin() + selectedBoxId);
                }else{
                    std::cout << "no bbox found at cursor location\n";
                }
                
				/*if (next_by_click)
				{
					++trackbar_value;
				}
				else
				{
					full_image.copyTo(full_image_roi);
					current_coord_vec.clear();
				}*/
			}else if(drag_bbox){
                int xEnd = x_end;
                int yEnd = y_end;
                
                if(selectedBoxId > -1 && selectedBoxId < current_coord_vec.size()){
                    Rect_<float>* bbox = &current_coord_vec[selectedBoxId].abs_rect;
                    
                    int imgWidth = full_rect_dst.width - rightPad - leftPad;
                    int imgHeight = full_rect_dst.height - topPad - botPad;
                    float xDelta = (float)(xEnd - x_mouseMove_start) / (imgWidth * scaleWidth);
                    float yDelta = (float)(yEnd - y_mouseMove_start) / (imgHeight * scaleHeight);
                    //std::cout << xDelta << " " << yDelta << "\n";
                    
                    // check if reached edge of image
                    float xMax = 1.0f - (float)bbox->width;
                    if(xDelta < 0){
                        bbox->x = std::max((float)bbox->x + xDelta, 0.0f);
                    }else{
                        bbox->x = std::min((float)bbox->x + xDelta, xMax);
                    }
                    
                    float yMax = 1.0f - (float)bbox->height;
                    if(yDelta < 0){
                        bbox->y = std::max((float)bbox->y + yDelta, 0.0f);
                    }else{
                        bbox->y = std::min((float)bbox->y + yDelta, yMax);
                    }
                }
                
                // reset:
                x_mouseMove_start = xEnd;
                y_mouseMove_start = yEnd;
            }


			if (old_current_obj_id != current_obj_id)
			{
				full_image.copyTo(full_image_roi);
				old_current_obj_id = current_obj_id;
				setTrackbarPos(trackbar_name_2, window_name, current_obj_id);
			}

            // draw bounding boxes around objects
			for (auto &i : current_coord_vec)
			{
				std::string synset_name;
				if (i.id < synset_txt.size()) synset_name = " - " + synset_txt[i.id];

				int offset = i.id * 25;
				int red = (offset + 0) % 255 * ((i.id + 2) % 3);
				int green = (offset + 70) % 255 * ((i.id + 1) % 3);
				int blue = (offset + 140) % 255 * ((i.id + 0) % 3);
				Scalar color_rect(red, green, blue);    // Scalar color_rect(100, 200, 100);

                Rect_<float> abs_rect = i.abs_rect; // move and rescale box for scaling
                
                //int topPad, botPad, leftPad, rightPad;
                int imgWidth = full_rect_dst.width - rightPad - leftPad;
                int imgHeight = full_rect_dst.height - topPad - botPad;
                abs_rect.x = (leftPad + (float)imgWidth * abs_rect.x) * scaleWidth - scrollWidthPad * scaleWidth;
                abs_rect.y = (botPad + (float)imgHeight * abs_rect.y) * scaleHeight - scrollHeightPad * scaleHeight;
                
                abs_rect.width *= (float)imgWidth * scaleWidth;
                abs_rect.height *= (float)imgHeight * scaleHeight;
                
                rectangle(full_image_roi, abs_rect, color_rect, mark_line_width);

				if (show_mark_class)
				{
					putText(full_image_roi, std::to_string(i.id) + synset_name,
                        abs_rect.tl() + Point2f(2, 22), FONT_HERSHEY_SIMPLEX, 0.8, color_rect, 2);
						//i.abs_rect.tl() + Point2f(2, 22), FONT_HERSHEY_SIMPLEX, 0.8, color_rect, 2);
				}
			}


			if (next_by_click)
				putText(full_image_roi, "Mode: 1 mark per image (next by click)",
					Point2i(850, 20), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(50, 170, 100), 2);

			{
				std::string const obj_str = "Object id: " + std::to_string(current_obj_id) + current_synset_name;

				putText(full_image_roi, obj_str, Point2i(0, 21), FONT_HERSHEY_DUPLEX, 0.8, Scalar(10, 50, 10), 3);
				putText(full_image_roi, obj_str, Point2i(0, 21), FONT_HERSHEY_DUPLEX, 0.8, Scalar(20, 120, 60), 2);
				putText(full_image_roi, obj_str, Point2i(0, 21), FONT_HERSHEY_DUPLEX, 0.8, Scalar(50, 200, 100), 1);
			}

			if (show_help)
			{
				putText(full_image_roi,
					"<- prev_img     -> next_img     space - next_img     c - clear_marks     n - one_object_per_img    0-9 - obj_id",
					Point2i(0, 45), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(50, 10, 10), 2);
				putText(full_image_roi,
					"ESC - exit   w - line width   k - hide obj_name   z - delete last   p - copy last detections", //   h - disable help",
					Point2i(0, 80), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(50, 10, 10), 2);
			}
			else
			{
				putText(full_image_roi,
					"h - show help",
					Point2i(0, 45), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(50, 10, 10), 2);
			}

			// arrows
			{
				Scalar prev_arrow_color(200, 150, 100);
				Scalar next_arrow_color = prev_arrow_color;
				if (prev_img_rect.contains(Point2i(x_end, y_end))) prev_arrow_color = Scalar(220, 190, 170);
				if (next_img_rect.contains(Point2i(x_end, y_end))) next_arrow_color = Scalar(220, 190, 170);

				std::vector<Point> prev_triangle_pts = { Point(5, 50), Point(40, 90), Point(40, 10), Point(5, 50) };
				Mat prev_roi = frame(prev_img_rect);
				line(prev_roi, prev_triangle_pts[0], prev_triangle_pts[1], prev_arrow_color, 5);
				line(prev_roi, prev_triangle_pts[1], prev_triangle_pts[2], prev_arrow_color, 5);
				line(prev_roi, prev_triangle_pts[2], prev_triangle_pts[3], prev_arrow_color, 5);
				line(prev_roi, prev_triangle_pts[3], prev_triangle_pts[0], prev_arrow_color, 5);

				std::vector<Point> next_triangle_pts = { Point(10, 10), Point(10, 90), Point(45, 50), Point(10, 10) };
				Mat next_roi = frame(next_img_rect);
				line(next_roi, next_triangle_pts[0], next_triangle_pts[1], next_arrow_color, 5);
				line(next_roi, next_triangle_pts[1], next_triangle_pts[2], next_arrow_color, 5);
				line(next_roi, next_triangle_pts[2], next_triangle_pts[3], next_arrow_color, 5);
				line(next_roi, next_triangle_pts[3], next_triangle_pts[0], next_arrow_color, 5);
			}
            
			imshow(window_name, frame);

#ifndef CV_VERSION_EPOCH
			int pressed_key = cv::waitKeyEx(20);	// OpenCV 3.x
#else
			int pressed_key = cv::waitKey(20);		// OpenCV 2.x
#endif
			if (pressed_key >= 0)
				for (int i = 0; i < 5; ++i) cv::waitKey(1);
			
			if (exit_flag) break;	// exit after saving
			if (pressed_key == 27 || pressed_key == 1048603) exit_flag = true;// break;  // ESC - save & exit
            if (cv::getWindowProperty(window_name, WND_PROP_AUTOSIZE) == -1) exit_flag = true; // close button pressed

			if (pressed_key >= '0' && pressed_key <= '9') current_obj_id = pressed_key - '0';   // 0 - 9
			if (pressed_key >= 1048624 && pressed_key <= 1048633) current_obj_id = pressed_key - 1048624;   // 0 - 9

			switch (pressed_key)
			{
			case 'z':		// z
			case 1048698:	// z
			    undo = true;
				break;

			case 32:        // SPACE
			case 1048608:	// SPACE
				++trackbar_value;
				break;

			case 2424832:   // <-
			case 65361:     // <-
			case 91:		// [
				--trackbar_value;
				break;
			case 2555904:   // ->
			case 65363:     // ->
			case 93:		// ]
				++trackbar_value;
				break;
			case 'c':       // c
			case 1048675:	// c
				clear_marks = true;
				break;
			case 'm':		// m
			case 1048685:   // m
				show_mouse_coords = !show_mouse_coords;
				full_image.copyTo(full_image_roi);
				break;
			case 'n':       // n
			case 1048686:   // n
				next_by_click = !next_by_click;
				full_image.copyTo(full_image_roi);
				break;
			case 'w':       // w
			case 1048695:   // w
				mark_line_width = mark_line_width % MAX_MARK_LINE_WIDTH + 1;
			break;
			case 'h':		// h
			case 1048680:	// h
				show_help = !show_help;
			break;
			case 'k':
			case 1048683:
				show_mark_class = !show_mark_class;
				break;
            case 'p':
			case 1048688:
                if(trackbar_value > 0){
                    int prev = trackbar_value - 1;
                    readCoordsFromFile(jpg_filenames[prev], current_coord_vec, false);
                }
				break;
			default:
				;
			}

			//if (pressed_key >= 0) std::cout << "pressed_key = " << (int)pressed_key << std::endl;
            
		} while (true);

	}
	catch (std::exception &e) {
		std::cout << "exception: " << e.what() << std::endl;
	}
	catch (...) {
		std::cout << "unknown exception \n";
	}

    return 0;
}

void readCoordsFromFile(std::string const jpg_filename, std::vector<coord_t> &current_coord_vec, bool clear){
    try {
        //std::string const jpg_filename = jpg_filenames[trackbar_value];
        std::string const txt_filename = jpg_filename.substr(0, jpg_filename.find_last_of(".")) + ".txt";
        //std::cout << (images_path + "/" + txt_filename) << std::endl;
        std::ifstream ifs(images_path + "/" + txt_filename);
        if(clear){
            current_coord_vec.clear();
        }
        for (std::string line; getline(ifs, line);)
        {
            std::stringstream ss(line);
            coord_t coord;
            coord.id = -1;
            ss >> coord.id;
            if (coord.id < 0) continue;
            float relative_coord[4] = { -1, -1, -1, -1 };  // rel_center_x, rel_center_y, rel_width, rel_height                          
            for (size_t i = 0; i < 4; i++) if(!(ss >> relative_coord[i])) continue;
            for (size_t i = 0; i < 4; i++) if (relative_coord[i] < 0) continue;
            coord.abs_rect.x = (relative_coord[0] - relative_coord[2] / 2);
            coord.abs_rect.y = (relative_coord[1] - relative_coord[3] / 2);
            coord.abs_rect.width = relative_coord[2];
            coord.abs_rect.height = relative_coord[3];

            current_coord_vec.push_back(coord);
        }
    }
    catch (...) { std::cout << " Exception when try to read txt-file \n"; }
}

cv::Mat resizeKeepAspectRatio(const cv::Mat &input, const cv::Size &dstSize, const cv::Scalar &bgcolor)
{
    cv::Mat output;
    
    double h1 = dstSize.width * (input.rows/(double)input.cols);
    double w2 = dstSize.height * (input.cols/(double)input.rows);

    if( h1 <= dstSize.height) {
        cv::resize( input, output, cv::Size(dstSize.width, h1));
    } else {
        cv::resize( input, output, cv::Size(w2, dstSize.height));
    }

    topPad = (dstSize.height - output.rows) / 2;
    botPad = (dstSize.height - output.rows + 1) / 2;
    leftPad = (dstSize.width - output.cols) / 2;
    rightPad = (dstSize.width - output.cols + 1) / 2;

    cv::copyMakeBorder(output, output, topPad, botPad, leftPad, rightPad, cv::BORDER_CONSTANT, bgcolor);
    
    // multiply by zoom in scale and crop
    //int imgH = input.rows + input.rows * mouseScroll;
    //int imgW = input.cols + input.cols * mouseScroll;
    int maxX0 = dstSize.width - ((float)output.cols * (1.0 - mouseScroll));
    int maxY0 = dstSize.height - ((float)output.rows * (1.0 - mouseScroll));
    scrollWidthPad = scrollWidthPad <= maxX0 ? scrollWidthPad : maxX0;
    scrollHeightPad = scrollHeightPad <= maxY0 ? scrollHeightPad : maxY0;
    int x0 = scrollWidthPad;
    int y0 = scrollHeightPad;

    output = output(Rect(x0, y0, output.cols - (float)output.cols * mouseScroll, output.rows - (float)output.rows * mouseScroll));
    int oldWidth = output.cols;
    int oldHeight = output.rows;
    cv::resize( output, output, cv::Size(dstSize.width, dstSize.height));
    scaleHeight = (float)output.rows / oldHeight;
    scaleWidth = (float)output.cols / oldWidth;
    return output;
}

int getClickedBoxId(const std::vector<coord_t>& current_coord_vec, const int x, int y){
    int id = -1;
    //add preview bar height to y
    y -= PreviewHeight;
    
    //std::vector<coord_t>::iterator begin = current_coord_vec.begin();
    //std::vector<coord_t>::iterator end = current_coord_vec.end();
    //std::vector<coord_t>::iterator it = begin;
    int imgWidth = full_rect_dst.width - rightPad - leftPad;
    int imgHeight = full_rect_dst.height - topPad - botPad;
    
    for (int i = current_coord_vec.size() - 1; i >= 0; --i) // check in reversed order
    {
        Rect_<float> coord = current_coord_vec.at(i).abs_rect;

        coord.x = (leftPad + (float)imgWidth * coord.x) * scaleWidth - scrollWidthPad * scaleWidth;
        coord.y = (botPad + (float)imgHeight * coord.y) * scaleHeight - scrollHeightPad * scaleHeight;
        
        coord.width *= (float)imgWidth * scaleWidth;
        coord.height *= (float)imgHeight * scaleHeight;
        
        if (coord.contains(Point_<float>(x, y))){
            //std::cout << "deleting box at location x: " << x_delete << " y: " << y_delete << "\n";
            //it = current_coord_vec.erase(it);
            id = i;
            break;
        }
    }
    return id;
}