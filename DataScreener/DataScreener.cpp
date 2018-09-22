#include "pch.h"
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
	os << aisEntry.MMSI << "," << TimePointToDateTimeString(aisEntry.BaseDateTime) << "," << aisEntry.LAT << "," << aisEntry.LON;
	return os;
}

struct CommandArgs
{
	std::string inputDataFileName;
	int numberNeighbors;
};

int main(int argc, char* argv[])
{
	if (argc != 5)
	{
		std::cerr << "Usage:\n\n"
			<< "   " << argv[0] << "inputDataFileName numberNeighbors LAT LON\nn"
			<< "  where:\n"
			<< "    inputDataFileName      AIS data in CSV\n"
			<< "    numberNeighbors        number of nearest neighbors\n"
			<< "    LAT                    Latitude to search from\n"
			<< "    LON                    Longitude to search from\n"
			;
	}

	const auto inputDataFileName = argv[1];
	const auto numberNeighbors = std::stoi(argv[2]);
	const auto LAT = std::stof(argv[3]);
	const auto LON = std::stof(argv[4]);

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
	std::cout << "AisEntry count: " << aisEntries.size() << '\n';

	//

	{
		constexpr int dim = 2;
		const int k = numberNeighbors; // number of near neighbor
		constexpr double eps = 0;			// error bound

		ANNpoint queryPt{ annAllocPt(dim) };
		queryPt[0] = LAT;
		queryPt[1] = LON;

		ANNpointArray dataPts{ annAllocPts(aisEntries.size(), dim) };
		std::vector<ANNidx> nnIdx(k);					// near neighbor indices
		std::vector<ANNdist> dists(k);					// near neighbor distance

		for (size_t i = 0, size = aisEntries.size(); i != size; ++i)
		{
			dataPts[i][0] = aisEntries[i].LAT;
			dataPts[i][1] = aisEntries[i].LON;
		}

		ANNkd_tree kdTree(
			dataPts,
			aisEntries.size(),
			dim);

		kdTree.annkSearch(
			queryPt,
			k,
			nnIdx.data(),
			dists.data(),
			eps);

		std::cout << "Nearest " << k << " neighbors:\n";
		for (size_t i = 0; i != k; ++i)
		{
			const auto& aisEntry = aisEntries[i];
			std::cout << "MMSI: " << aisEntry.MMSI << " VesselName: " << aisEntry.VesselName << "; CallSign: " << aisEntry.CallSign << "; Distance: " << dists[i] << '\n';
		}

		annClose();
	}



}
