// main.h
#ifndef MAIN_H // include guard
#define MAIN_H

struct coord_t {
    Rect_<float> abs_rect;
    int id;
};

void readCoordsFromFile(std::string const jpg_filename, std::vector<coord_t> &current_coord_vec, bool clear);
cv::Mat resizeKeepAspectRatio(const cv::Mat &input, const cv::Size &dstSize, const cv::Scalar &bgcolor);
int getClickedBoxId(const std::vector<coord_t>& current_coord_vec, const int x, const int y);

#endif /* MAIN_H */