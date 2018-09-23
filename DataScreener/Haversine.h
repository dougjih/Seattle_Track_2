#pragma once

#include <cmath>

constexpr double earthRadiusKm = 6371.0;

double distanceEarth(double lat1d, double lon1d, double lat2d, double lon2d);