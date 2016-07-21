#include "csk.h"
#define PI 3.141592653589793

void CircShift(Mat &x){
  int cxl = cvFloor(float(x.cols) / 2);
  int cyl = cvFloor(float(x.rows) / 2);
  int cxh = cvCeil(float(x.cols) / 2);
  int cyh = cvCeil(float(x.rows) / 2);

  Mat q0(x, Rect(0, 0, cxh, cyh));   // Top-Left - Create a ROI per quadrant
  Mat q1(x, Rect(cxh, 0, cxl, cyh));  // Top-Right
  Mat q2(x, Rect(0, cyh, cxh, cyl));  // Bottom-Left
  Mat q3(x, Rect(cxh, cyh, cxl, cyl)); // Bottom-Right

  Mat tmp1, tmp2;                           // swap quadrants (Top-Left with Bottom-Right)
  hconcat(q3, q2, tmp1);
  hconcat(q1, q0, tmp2);
  vconcat(tmp1, tmp2, x);

}

void GetSubWindow(const Mat &frame, Mat &subWindow, Point centraCoor, Size sz, Mat &cos_window){
  
  Point lefttop(centraCoor.x - cvFloor(float(sz.width)/2.0)+1,centraCoor.y - cvFloor(float(sz.height)/2.0)+1);
  Point rightbottom(centraCoor.x + cvCeil(float(sz.width) / 2.0)+1, centraCoor.y + cvCeil(float(sz.height) / 2.0)+1);
    
  Rect border(-min(lefttop.x,0),-min(lefttop.y,0),
                max(rightbottom.x- frame.cols+1,0),max(rightbottom.y- frame.rows+1,0));
    
  Point lefttopLimit(max(lefttop.x,0),max(lefttop.y,0));
  Point rightbottomLimit(min(rightbottom.x,frame.cols-1),min(rightbottom.y,frame.rows-1));
	
  Rect roiRect(lefttopLimit, rightbottomLimit);
  frame(roiRect).copyTo(subWindow);

  if (border != Rect(0,0,0,0))
  {
    cv::copyMakeBorder(subWindow, subWindow, border.y, border.height, border.x, border.width, cv::BORDER_CONSTANT);
  }

  subWindow.convertTo(subWindow, CV_64FC1,1.0/255.0,-0.5);
  subWindow = subWindow.mul(cos_window);
}

void CalculateHann(Mat &cos_window, Size sz){
  Mat temp1(Size(sz.width, 1), CV_64FC1);
  Mat temp2(Size(sz.height, 1), CV_64FC1);
	for (int i = 0; i < sz.width; ++i)
		temp1.at<double>(0, i) = 0.5*(1 - cos(2 * PI*i / (sz.width - 1)));
	for (int i = 0; i < sz.height; ++i)
    temp2.at<double>(0, i) = 0.5*(1 - cos(2 * PI*i / (sz.height - 1)));
	cos_window = temp2.t()*temp1;
}

void DenseGaussKernel(float sigma, const Mat &x,const Mat &y, Mat &k){
  Mat xf, yf;
  dft(x, xf, DFT_COMPLEX_OUTPUT);
  dft(y, yf, DFT_COMPLEX_OUTPUT);
	double xx = norm(x);
	xx = xx*xx;
	double yy = norm(y);
	yy = yy*yy;

  Mat xyf;
  mulSpectrums(xf, yf, xyf, 0, true);

	Mat xy;
	cv::idft(xyf, xy, cv::DFT_SCALE | cv::DFT_REAL_OUTPUT); // Applying IDFT
  CircShift(xy);
	double numelx1 = x.cols*x.rows;
	//exp((-1 / (sigma*sigma)) * abs((xx + yy - 2 * xy) / numelx1), k); //thsi setting is fixed by version 2(KCF)
  exp((-1 / (sigma*sigma)) * max(0,(xx + yy - 2 * xy) / numelx1), k);
}

cv::Mat CreateGaussian1(int n, double sigma, int ktype)
{
	CV_Assert(ktype == CV_32F || ktype == CV_64F);
	Mat kernel(n, 1, ktype);
	float* cf = kernel.ptr<float>();
	double* cd = kernel.ptr<double>();

	double sigmaX = sigma > 0 ? sigma : ((n - 1)*0.5 - 1)*0.3 + 0.8;
	double scale2X = -0.5 / (sigmaX*sigmaX);

	int i;
	for (i = 0; i < n; i++)
	{
		double x = i - floor(n / 2)+1;
		double t = std::exp(scale2X*x*x);
		if (ktype == CV_32F)
		{
			cf[i] = (float)t;
		}
		else
		{
			cd[i] = t;
		}
	}

	return kernel;
}

cv::Mat CreateGaussian2(Size sz, double sigma, int ktype)
{
  Mat a = CreateGaussian1(sz.height, sigma, ktype);
  Mat b = CreateGaussian1(sz.width, sigma, ktype);
	return a*b.t();
}

cv::Mat ComplexMul(const Mat &x1, const Mat &x2)
{
	vector<Mat> planes1;
	split(x1, planes1);
	vector<Mat> planes2;
	split(x2, planes2);
	vector<Mat>complex(2);
	complex[0] = planes1[0].mul(planes2[0]) - planes1[1].mul(planes2[1]);
	complex[1] = planes1[0].mul(planes2[1]) + planes1[1].mul(planes2[0]);
	Mat result;
	merge(complex, result);
	return result;
}

cv::Mat ComplexDiv(const Mat &x1, const Mat &x2)
{
	vector<Mat> planes1;
	split(x1, planes1);
	vector<Mat> planes2;
	split(x2, planes2);
	vector<Mat>complex(2);
	Mat cc = planes2[0].mul(planes2[0]);
	Mat dd = planes2[1].mul(planes2[1]);

	complex[0] = (planes1[0].mul(planes2[0]) + planes1[1].mul(planes2[1])) / (cc + dd);
	complex[1] = (-planes1[0].mul(planes2[1]) + planes1[1].mul(planes2[0])) / (cc + dd);
	Mat result;
	merge(complex, result);
	return result;
}