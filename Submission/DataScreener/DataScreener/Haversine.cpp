#include "pch.h"
#include "Haversine.h"

using namespace std;

namespace
{
	constexpr double M_PI = 3.141592653589794;

	double deg2rad(double deg){
		return (deg * M_PI / 180);
	}

	double rad2deg(double rad) {

		return (rad * 180 / M_PI);
	}
}

double distanceEarth(double lat1d, double lon1d, double lat2d, double lon2d)
{
	double lat1r = deg2rad(lat1d);
	double lon1r = deg2rad(lon1d);
	double lat2r = deg2rad(lat2d);
	double lon2r = deg2rad(lon2d);
	double u = sin((lat2r - lat1r) / 2);
	double v = sin((lon2r - lon1r) / 2);
	return 2.0 * earthRadiusKm * asin(sqrt(u * u + cos(lat1r) * cos(lat2r) * v * v));
}
