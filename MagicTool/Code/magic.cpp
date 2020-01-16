double IMAGIC::dist(const Vec3b& a, const Vec3b& b) {
  return sqrt(
    (a[0] - b[0]) * (a[0] - b[0]) +
    (a[1] - b[1]) * (a[1] - b[1]) +
    (a[2] - b[2]) * (a[2] - b[2])
  );
}

vector<Vec3b> IMAGIC::get_keys(const Mat& im) {
  int n = im.rows;
  int m = im.cols;
  return {
    im.at<Vec3b>(0, 0),
    im.at<Vec3b>(0, m - 1),
    im.at<Vec3b>(n - 1, 0),
    im.at<Vec3b>(n - 1, m - 1)
  };
}

void IMAGIC::remove_treshold(Mat& im, int rude) {
	int lowThreshold = 100;
	int ratio = 3;
	int kernelSize = 3;
	
	Mat cannyEdges, blurred;
  
  if (rude < 3) {
	  erode(im, im, getStructuringElement(MORPH_RECT, Size(3, 3)));
  }
	
	blur(im, cannyEdges, Size(3, 3));
	Canny(cannyEdges, cannyEdges, lowThreshold, lowThreshold * ratio, kernelSize);
	
	int dilation_size = rude;
	Mat element = getStructuringElement(MORPH_CROSS,
	Size(2 * dilation_size + 1, 2 * dilation_size + 1),
	Point(dilation_size, dilation_size));
	
	dilate(cannyEdges, cannyEdges, element);
	im.copyTo(blurred);
	blur(blurred, blurred, Size(3, 3));
	blurred.copyTo(im, cannyEdges);
}

Mat IMAGIC::get_mask(const Mat im, const vector<Vec3b>& keys, int sensivity) {
  double dst = 0;
  for (auto key1 : keys) {
    for (auto key2 : keys) {
      dst = max(dst, dist(key1, key2));
    }
  }
  //Mat res(im.size(), CV_8UC1, Scalar::all(255));
  
  //ADDED {  
    if (sensivity == -1) {
      sensivity = 64;
    }
    Vec3b mn = keys[0], mx = keys[0];
    for (Vec3b key : keys) {
      for (int i = 0; i < 3; ++i) {
        mn[i] = min(int(mn[i]), max(0, int(key[i]) - 50));
        mx[i] = max(int(mx[i]), min(int(key[i]) + 50, 255));
      }
    }
    Mat res, tmp(im.size(), CV_8UC1, Scalar::all(255));
    inRange(im, mn, mx, res);
    tmp -= res;
    return tmp;
  //} ADDED
  
  /*
  for (auto key : keys) {
    #pragma omp parallel for num_threads(8)
		for (int i = 0; i < im.rows; ++i) {
      for (int j = 0; j < im.cols; ++j) {
        if (sensivity == -1) {
          if (dist(im.at<Vec3b>(i, j), key) < max(64, int(dst))) {
            res.at<unsigned char>(i, j) = 0;
          }
        } else if (dist(im.at<Vec3b>(i, j), key) < sensivity) {
          res.at<unsigned char>(i, j) = 0;
        }
      }
    }
  }
  */
  return res;
}

void IMAGIC::solve(Mat& im, Mat& bg, const Mat& mask0, const Mat& mask1) {
  im.convertTo(im, CV_32FC3);
  bg.convertTo(bg, CV_32FC3);
  
  Mat alpha;
  merge({mask0, mask1, mask0}, alpha);
  alpha.convertTo(alpha, CV_32FC3, 1.0/255);

  Mat res = Mat::zeros(im.size(), im.type());
  multiply(alpha, im, im); 
  multiply(Scalar::all(1.0)-alpha, bg, bg); 
  add(im, bg, res);

  res.convertTo(res, CV_8UC3, 1);
  swap(res, im);
}

void IMAGIC::equalize(Mat& im) {
 	Mat hist_equalized_image;
  cvtColor(im, hist_equalized_image, COLOR_BGR2YCrCb);

  vector<Mat> vec_channels;
  split(hist_equalized_image, vec_channels); 

  equalizeHist(vec_channels[0], vec_channels[0]);

  merge(vec_channels, hist_equalized_image); 
      
  cvtColor(hist_equalized_image, hist_equalized_image, COLOR_YCrCb2BGR);
	swap(im, hist_equalized_image);
}

void IMAGIC::fit(const Mat& a, Mat& b) {
  double aa = double(a.rows) / a.cols;
  double bb = double(b.rows) / b.cols;
  
  double scale;
  if (aa > bb) {
    scale = double(a.rows) / b.rows;
  } else {
    scale = double(a.cols) / b.cols;
  }
	/*
	int startY = (-a.rows / 2) / scale + b.rows / 2,
			startX = (-a.cols / 2) / scale + b.cols / 2,
			width  = a.cols
			height = a.rows;

	Mat ROI(b, Rect(startX,startY,width,height));
	ROI.copyTo(b);
	*/
  Mat res = Mat::zeros(a.size(), a.type());
	#pragma omp parallel for num_threads(8)
  for (int i = 0; i < res.rows; ++i) {
    for (int j = 0; j < res.cols; ++j) {
      res.at<Vec3b>(i, j) =
        b.at<Vec3b>((i - a.rows / 2) / scale + b.rows / 2, (j - a.cols / 2) / scale + b.cols / 2);
    }
  }
  swap(b, res);
}

Mat IMAGIC::ChromaKey(int quality, Mat im, Mat bg, int sensivity) {// sensivity = -1 means maximum of (64) and (corner dif)
  if (im.size() != bg.size()) {
    fit(im, bg);
  }

  Mat for_mask = im.clone();
  //equalize(for_mask);
	if (quality == 1) {
  	adaptiveBilateralFilter(im, for_mask, Size(3, 3), 150); // Size(5, 5), 150
  }
  vector<Vec3b> keys = get_keys(for_mask);
	Mat mask = get_mask(for_mask, keys, sensivity);

	if (quality == 1) {
		Mat mask1 = mask.clone();
		#pragma omp parallel sections	
			#pragma omp section
				remove_treshold(mask, 5); // 5
			#pragma omp section
				remove_treshold(mask1, 3); // 2
  	
		solve(im, bg, mask, mask1);
	} else {
  	solve(im, bg, mask, mask);
	}
	return im;
}
