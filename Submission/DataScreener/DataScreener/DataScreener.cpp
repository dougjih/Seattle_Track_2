#include "pch.h"
#include "Haversine.h"
#include "ANN/ANN.h"
#pragma warning( push, 2 )
#include "csv/csv.h"
#pragma warning( pop )
#include "date/date.h"
#include "fmt/format.h"
#include "fmt/ostream.h"
#include <sstream>
#include <string>
#include <vector>
#include <iostream>
#include <cstdlib>
#include <algorithm>
#include <fstream>

std::chrono::system_clock::time_point DateTimeStringToTimePoint(const std::string& dateTimeStr)
{
	constexpr const char* DateTimeFormat = "%FT%T";

	std::chrono::system_clock::time_point tp;
	std::istringstream ss{ dateTimeStr };
	ss >> date::parse(DateTimeFormat, tp);
	return tp;
}

//std::string TimePointToDateTimeString(const std::chrono::system_clock::time_point& tp)
//{
//	constexpr const char* DateTimeFormat = "%FT%T";
//	return date::format(DateTimeFormat, tp);
//}

struct AisEntryRaw
{
	long int MMSI;
	std::string BaseDateTime;
	double LAT;
	double LON;
	//double SOG;
	//double COG;
	//double Heading;
	//std::string VesselName;
	//std::string IMO;
	//std::string CallSign;
	//int VesselType;
	//std::string Status;
	//double Length;
	//double Width;
	//double Draft;
	//int Cargo;
};

struct AisEntry
{
	AisEntry(const AisEntryRaw& r, unsigned int fileLineIndex)
		: MMSI{ r.MMSI }, BaseDateTime{ DateTimeStringToTimePoint(r.BaseDateTime) }, LAT{ r.LAT }, LON{ r.LON }
/*		, SOG{ r.SOG }, COG{ r.COG }, Heading{ r.Heading }, VesselName{ r.VesselName }, IMO{ r.IMO }, CallSign{ r.CallSign }
		, VesselType{ r.VesselType }, Status{ r.Status }, Length{ r.Length }, Width{ r.Width }, Draft{ r.Draft }, Cargo{ r.Cargo }*/
		, fileLineIndex{ fileLineIndex }
	{}

	//friend std::ostream& operator<<(std::ostream& os, const AisEntry& aisEntry);

	long int MMSI;
	std::chrono::system_clock::time_point BaseDateTime;
	double LAT;
	double LON;
	//double SOG;
	//double COG;
	//double Heading;
	//std::string VesselName;
	//std::string IMO;
	//std::string CallSign;
	//int VesselType;
	//std::string Status;
	//double Length;
	//double Width;
	//double Draft;
	//int Cargo;
	unsigned int fileLineIndex;
};

//std::ostream& operator<<(std::ostream& os, const AisEntry& a)
//{
//	fmt::print(os, "{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}",
//		a.MMSI, TimePointToDateTimeString(a.BaseDateTime), a.LAT, a.LON,
//		a.SOG, a.COG, a.Heading, a.VesselName, a.IMO, a.CallSign,
//		a.VesselType, a.Status, a.Length, a.Width, a.Draft, a.Cargo);
//	return os;
//}

int main(int argc, char* argv[])
{
	if (argc != 5)
	{
		std::cerr << "Usage:\n\n"
			<< "   " << argv[0] << "inputDataFileName distance numberOfMinutes\nn"
			<< "  where:\n"
			<< "    inputDataFileName      AIS data in CSV\n"
			<< "    distance               search distance in yards\n"
			<< "    numberOfMinutes        number of adjacent minutes\n"
			<< "    MMSI                   MMSI to search interactions for\n"
			;
	}

	const auto inputDataFileName = argv[1];
	const auto distance = std::stof(argv[2]);
	const auto numberOfMinutes = std::stoi(argv[3]);
	const auto mmsi = std::stol(argv[4]);


	io::CSVReader<4/*16*/> in(inputDataFileName);
	in.read_header(io::ignore_extra_column, "MMSI", "BaseDateTime", "LAT", "LON"/*, "SOG", "COG", "Heading", "VesselName", "IMO", "CallSign", "VesselType", "Status", "Length", "Width", "Draft", "Cargo"*/);

	std::vector<AisEntry> aisEntries;
	{
		AisEntryRaw r;
		while (in.read_row(r.MMSI, r.BaseDateTime, r.LAT, r.LON/*, r.SOG, r.COG, r.Heading, r.VesselName, r.IMO, r.CallSign, r.VesselType, r.Status, r.Length, r.Width, r.Draft, r.Cargo*/))
		{
			aisEntries.push_back(AisEntry{ r, in.get_file_line() - 1 });
		}
	}
	std::sort(aisEntries.begin(), aisEntries.end(), [](const auto& x, const auto& y) { return x.BaseDateTime < y.BaseDateTime; });

	//

	std::vector<std::pair<const AisEntry*, const AisEntry*>> aisEntryPairs;

	const auto durationBound = std::chrono::minutes(numberOfMinutes);
	
	for (size_t i = aisEntries.size(); i-- > 1;)
	{
		const auto& aisEntry = aisEntries[i];

		if (aisEntry.MMSI != mmsi) continue;


		const auto time = aisEntry.BaseDateTime;
		const auto lower_time_it = std::lower_bound(aisEntries.begin(), aisEntries.end(), time - durationBound, [](const auto& a, const auto& t) { return a.BaseDateTime < t; });
		const auto upper_time_it = std::upper_bound(aisEntries.begin(), aisEntries.end(), time + durationBound, [](const auto& t, const auto& a) { return t < a.BaseDateTime; });
		const auto time_range_count = std::distance(lower_time_it, upper_time_it);

		{
			constexpr int dim = 2;
			constexpr double eps = 0;

			ANNpoint queryPt{ annAllocPt(dim) };
			queryPt[0] = aisEntry.LAT;
			queryPt[1] = aisEntry.LON;

			// remove entries of self (same MMSI)
			std::vector<const AisEntry*> aisEntriesSelfExluded;
			for (auto it = lower_time_it; it != upper_time_it; ++it)
			{
				if (it->MMSI != aisEntry.MMSI)
				{
					aisEntriesSelfExluded.push_back(&*it);
				}
			}
			const auto data_count = aisEntriesSelfExluded.size();

			ANNpointArray dataPts{ annAllocPts(data_count, dim) };
			for (size_t i = 0, size = data_count; i != size; ++i)
			{
				const auto& a = aisEntriesSelfExluded[i];
				dataPts[i][0] = a->LAT;
				dataPts[i][1] = a->LON;
			}

			ANNkd_tree kdTree(
				dataPts,
				data_count,
				dim);

			const double distInDegree = distance / 1093.613 / 111;
			const auto k = kdTree.annkFRSearch(
				queryPt,
				distInDegree * distInDegree,
				0);

			if (k != 0)
			{
				std::vector<ANNidx> nnIdx(k);
				std::vector<ANNdist> dists(k);

				const auto numNeighbors = kdTree.annkFRSearch(
					queryPt,
					distInDegree * distInDegree,
					k,
					nnIdx.data(),
					dists.data(),
					eps);

				for (size_t i = 0; i != k; ++i)
				{
					const auto pairingAisEntry = aisEntriesSelfExluded[nnIdx[i]];
					aisEntryPairs.push_back(std::make_pair(&aisEntry, pairingAisEntry));
/*

					constexpr double YardsInKm = 1093.613*/;
					//std::cout << aisEntry << "," << *pairingAisEntry << "," << YardsInKm * distanceEarth(pairingAisEntry->LAT, pairingAisEntry->LON, aisEntry.LAT, aisEntry.LON) << '\n';
				}
			}

			annDeallocPt(queryPt);
			annDeallocPts(dataPts);
			annClose();
		}

		aisEntries.pop_back();
	}
	
	fmt::print("ID,Distance(yards),MMSI_1,Timestamp_1,LAT_1,LON_1,SOG_1,COG_1,Heading_1,Vessel Name_1,IMO_1,Call Sign_1,Vessel Type_1,Status_1,Length_1,Width_1,Draft_1,Cargo_1,MMSI_2,Timestamp_2,LAT_2,LON_2,SOG_2,COG_2,Heading_2,Vessel Name_2,IMO_2,Call Sign_2,Vessel Type_2,Status_2,Length_2,Width_2,Draft_2,Cargo_2\n");

	std::ifstream infile(inputDataFileName);
	std::vector<std::string> infileLines;
	{
		std::string line;
		while (std::getline(infile, line))
		{
			infileLines.push_back(line);
		}
	}

	{
		unsigned int id = 1;
		for (const auto& pair : aisEntryPairs)
		{
			constexpr double YardsInKm = 1093.613;
			fmt::print("{},{},{},{}\n",
				id,
				YardsInKm * distanceEarth(pair.first->LAT, pair.first->LON, pair.second->LAT, pair.second->LON)
				, infileLines[pair.first->fileLineIndex], infileLines[pair.second->fileLineIndex]);
			//std::cout << *pair.first << "," << *pair.second << '\n';
			id++;
		}
	}


	//




}
