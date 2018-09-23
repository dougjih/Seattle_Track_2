#include "pch.h"
#include "Haversine.h"
#include "ANN/ANN.h"
#pragma warning( push, 2 )
#include "csv/csv.h"
#pragma warning( pop )
#include "date/date.h"
#include <sstream>
#include <string>
#include <vector>
#include <iostream>
#include <cstdlib>
#include <algorithm>

std::chrono::system_clock::time_point DateTimeStringToTimePoint(const std::string& dateTimeStr)
{
	constexpr const char* DateTimeFormat = "%FT%T";

	std::chrono::system_clock::time_point tp;
	std::istringstream ss{ dateTimeStr };
	ss >> date::parse(DateTimeFormat, tp);
	return tp;
}

std::string TimePointToDateTimeString(const std::chrono::system_clock::time_point& tp)
{
	constexpr const char* DateTimeFormat = "%FT%T";
	return date::format(DateTimeFormat, tp);
}

struct AisEntryRaw
{
	int MMSI;
	std::string BaseDateTime;
	double LAT;
	double LON;
	std::string VesselName;
	std::string CallSign;
};

struct AisEntry
{
	AisEntry(const AisEntryRaw& aisEntryRaw)
		: MMSI{ aisEntryRaw.MMSI }, BaseDateTime{ DateTimeStringToTimePoint(aisEntryRaw.BaseDateTime) }, LAT{ aisEntryRaw.LAT }, LON{ aisEntryRaw.LON }
		, VesselName{ aisEntryRaw.VesselName }, CallSign{ aisEntryRaw.CallSign }
	{}

	friend std::ostream& operator<<(std::ostream& os, const AisEntry& aisEntry);

	int MMSI;
	std::chrono::system_clock::time_point BaseDateTime;
	double LAT;
	double LON;
	std::string VesselName;
	std::string CallSign;
};

std::ostream& operator<<(std::ostream& os, const AisEntry& aisEntry)
{
	os << aisEntry.MMSI << "," << TimePointToDateTimeString(aisEntry.BaseDateTime) << "," << aisEntry.LAT << "," << aisEntry.LON << "," << aisEntry.VesselName << "," << aisEntry.CallSign;
	return os;
}

int main(int argc, char* argv[])
{
	if (argc != 7)
	{
		std::cerr << "Usage:\n\n"
			<< "   " << argv[0] << "inputDataFileName distance numberOfMinutes LAT LON time\nn"
			<< "  where:\n"
			<< "    inputDataFileName      AIS data in CSV\n"
			<< "    distance               search distance in yards\n"
			<< "    numberOfMinutes        number of adjacent minutes\n"
			<< "    LAT                    Latitude to search from\n"
			<< "    LON                    Longitude to search from\n"
			<< "    time                   time to search from\n"
			;
	}

	const auto inputDataFileName = argv[1];
	const auto distance = std::stof(argv[2]);
	const auto numberOfMinutes = std::stoi(argv[3]);
	const auto LAT = std::stof(argv[4]);
	const auto LON = std::stof(argv[5]);
	const auto time = DateTimeStringToTimePoint(argv[6]);

	io::CSVReader<6/*16*/> in(inputDataFileName);
	in.read_header(io::ignore_extra_column, "MMSI", "BaseDateTime", "LAT", "LON"/*, "SOG", "COG", "Heading"*/, "VesselName"/*, "IMO"*/, "CallSign"/*, "VesselType", "Status", "Length", "Width", "Draft", "Cargo"*/);

	std::vector<AisEntry> aisEntries;
	{
		AisEntryRaw aisEntryRaw;
		while (in.read_row(aisEntryRaw.MMSI, aisEntryRaw.BaseDateTime, aisEntryRaw.LAT, aisEntryRaw.LON, aisEntryRaw.VesselName, aisEntryRaw.CallSign))
		{
			aisEntries.push_back(AisEntry{ aisEntryRaw });
		}
	}
//	std::sort(aisEntries.begin(), aisEntries.end(), [](const auto& x, const auto& y) { return x.BaseDateTime < y.BaseDateTime; });
	std::cout << "AisEntry count: " << aisEntries.size() << '\n';

	//

	const auto durationBound = std::chrono::minutes(numberOfMinutes);
	const auto lower_time_it = std::lower_bound(aisEntries.begin(), aisEntries.end(), time - durationBound, [](const auto& a, const auto& t) { return a.BaseDateTime < t; });
	const auto upper_time_it = std::upper_bound(aisEntries.begin(), aisEntries.end(), time + durationBound, [](const auto& t, const auto& a) { return t < a.BaseDateTime; });
	const auto time_range_count = std::distance(lower_time_it, upper_time_it);
	std::cout << "numberOfMinutes: " << numberOfMinutes << '\n';
	std::cout << "AisEntry in time range count: " << time_range_count << '\n';


	//

	{
		constexpr int dim = 2;
		constexpr double eps = 0;

		ANNpoint queryPt{ annAllocPt(dim) };
		queryPt[0] = LAT;
		queryPt[1] = LON;

		ANNpointArray dataPts{ annAllocPts(time_range_count, dim) };

		auto aisEntryIt = lower_time_it;
		for (size_t i = 0, size = time_range_count; i != size; ++i)
		{
			dataPts[i][0] = aisEntryIt->LAT;
			dataPts[i][1] = aisEntryIt->LON;
			++aisEntryIt;
		}

		ANNkd_tree kdTree(
			dataPts,
			time_range_count,
			dim);

		const double distInDegree = distance / 1093.613 / 111;
		const auto k = kdTree.annkFRSearch(
			queryPt,
			distInDegree * distInDegree,
			0);

		std::vector<ANNidx> nnIdx(k);
		std::vector<ANNdist> dists(k);

		const auto numNeighbors = kdTree.annkFRSearch(
			queryPt,
			distInDegree * distInDegree,
			k,
			nnIdx.data(),
			dists.data(),
			eps);

		std::cout << "Nearest " << k << " neighbors:\n";
		for (size_t i = 0; i != k; ++i)
		{
			const auto& aisEntry = aisEntries[nnIdx[i]];
			constexpr double YardsInKm = 1093.613;
			std::cout << aisEntry << "; Distance (raw): " << dists[i] << "; Distance (yards): " << YardsInKm * distanceEarth(aisEntry.LAT, aisEntry.LON, LAT, LON) << '\n';
		}

		annClose();
	}



}
