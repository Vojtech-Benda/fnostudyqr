//
// Created by Vojtěch on 17.03.2025.
//

#include "PatientRecord.hpp"

#include <algorithm>
#include <fstream>

/*
PatientRecordMap readPatientRecords(const std::filesystem::path& csvFilePath)
{
	std::ifstream fileObject{ csvFilePath, std::ios::in };

	PatientRecordMap recordMap;
	if (!fileObject.is_open())
	{
		fmt::print("Unable to open CSV file {}, exiting program\n", csvFilePath.string());
		return {};
	}

	if (fileObject.is_open())
	{
		std::string line;
		while (std::getline(fileObject, line))
		{
			if (line.empty()) continue;

			RecordKey record;

			std::size_t firstPeriodPos{ line.find(",") };
			std::size_t secondPeriodPos{ line.find(",", firstPeriodPos + 1) };
			record.m_name = line.substr(0, firstPeriodPos);
			record.m_id = line.substr(firstPeriodPos + 1, secondPeriodPos - firstPeriodPos - 1);
			record.m_study_date = line.substr(secondPeriodPos + 1, std::string::npos);


			if (std::any_of(record.m_id.begin(), record.m_id.end(), ::isalpha))
			{
				fmt::print("ID \"{}\" contains non-numeric characters, skipping this line\n", record.m_id);
				continue;
			}

			if (std::any_of(record.m_study_date.begin(), record.m_study_date.end(), ::isalpha))
			{
				fmt::print("ID \"{}\" with Study Date \"{}\" contains alphabet characters, skipping this line\n", record.m_id, record.m_study_date);
				continue;
			}

			record.m_name = nameToDcmFormat(record.m_name);
			record.m_study_date = dateToDcmFormat(record.m_study_date);

			StudyUIDList uidlist;
			if (recordMap.contains(record))
				fmt::print("Possible duplicate for PatientID: {}, StudyDate: {} in CSV file, skipping this line\n", record.m_id, record.m_study_date);
			else
				recordMap[record] = uidlist;
		}
		fileObject.close();
	}
	return recordMap;

}
*/


std::vector<PatientRecord> readPatientRecords(const std::filesystem::path &csvFilePath)
{
	std::ifstream fileObject{ csvFilePath, std::ios::in };
	std::vector<PatientRecord> recordList;
	if (!fileObject.is_open())
	{
		fmt::print("Unable to open CSV file {}, exiting program\n", csvFilePath.string());
		return {};
	}
	if (fileObject.is_open())
	{
		std::string line;
		while (std::getline(fileObject, line))
		{
			if (line.empty()) continue;

			PatientRecord record;
			std::size_t firstPeriodPos{ line.find(",") };
			std::size_t secondPeriodPos{ line.find(",", firstPeriodPos + 1) };
			std::size_t thirdPeriodPos{ line.find(",", secondPeriodPos + 1) };
			record.m_name = line.substr(0, firstPeriodPos);
			record.m_id = line.substr(firstPeriodPos + 1, secondPeriodPos - firstPeriodPos - 1);
			record.m_study_date = line.substr(secondPeriodPos + 1, thirdPeriodPos - secondPeriodPos - 1);

			if (thirdPeriodPos != std::string::npos)
			{
				record.m_modality = line.substr(thirdPeriodPos + 1, std::string::npos);
			}

			if (record.m_id.empty() || record.m_name.empty() || record.m_study_date.empty())
			{
				const std::string msg = fmt::format("Patient Name, Patient ID or Study Date is empty");
				fmt::print("{} - {}\n", msg, fmt::format(fg(fmt::color::yellow), "LINE SKIPPED"));
				continue;
			}

			if (std::any_of(record.m_id.begin(), record.m_id.end(), ::isalpha))
			{
				const std::string msg = fmt::format("ID \"{}\" contains non-numeric characters", record.m_id);
				fmt::print("{} - {}\n", msg, fmt::format(fg(fmt::color::yellow), "LINE SKIPPED"));
				continue;
			}

			if (std::any_of(record.m_study_date.begin(), record.m_study_date.end(), ::isalpha))
			{
				const std::string msg = fmt::format("ID \"{}\" with Study Date \"{}\" contains alphabet characters", record.m_id, record.m_study_date);
				fmt::print("{} - {}\n", msg, fmt::format(fg(fmt::color::yellow), "LINE SKIPPED"));
				continue;
			}

			record.m_name = nameToDcmFormat(record.m_name);
			record.m_study_date = dateToDcmFormat(record.m_study_date);

			if (recordExists(recordList, record))
			{
				const std::string msg = fmt::format("PatientID {}: StudyDate: {}", record.m_id, record.m_study_date);
				fmt::print("{} - {}\n", msg, fmt::format(fg(fmt::color::yellow), "POSSIBLE DUPLICATE IN TEXT FILE"));
				continue;
			}

			recordList.push_back(record);
		}
	}
	fileObject.close();
	return recordList;
}


/*
StudyRecordMap readPatientRecords(std::ifstream& csvfile)
{
	StudyRecordMap recordMap;
	std::string line;
	unsigned short studyCounter{ 0 };
	while (std::getline(csvfile, line))
	{
		if (line.empty()) continue;

		const std::size_t	firstSeparator	{ line.find(",") };
		const std::size_t	secondSeparator	{ line.find(",", firstSeparator + 1) };
		std::string			name			{ line.substr(0, firstSeparator) };
		std::string			id				{ line.substr(firstSeparator + 1, secondSeparator - firstSeparator - 1) };
		std::string			study_date		{ line.substr(secondSeparator + 1, std::string::npos) };

		if (std::any_of(id.begin(), id.end(), ::isalpha))
		{
			spdlog::warn("ID \"{}\" contains non-numeric characters, skipping this line", id);
			continue;
		}

		if (std::any_of(study_date.begin(), study_date.end(), ::isalpha))
		{
			spdlog::warn("ID \"{}\" with Study Date \"{}\" contains alphabet characters, skipping this line", id, study_date);
			continue;
		}

		name = nameToDcmFormat(name);
		study_date = dateToDcmFormat(study_date);

		if (recordMap.find(id) == recordMap.end())
		{
			StudyRecord studyRecord{ study_date };
			recordMap[id] = studyRecord;
		}
		else
		{
			recordMap[id].m_studyDates = concatenateStudyDate(recordMap[id].m_studyDates, study_date);
		}
		++studyCounter;
	}
	return recordMap;
}
*/

static std::string nameToDcmFormat(std::string& fullname)
{
	fullname.erase(std::remove_if(fullname.begin(), fullname.end(), ::isdigit), fullname.end());
	std::size_t ws_pos = fullname.find(" ");

	if (ws_pos == std::string::npos)
	{
		return fullname;
	}
	std::string firstname = fullname.substr(0, ws_pos);
	std::string lastname = fullname.substr(ws_pos + 1, std::string::npos);
	firstname.erase(std::remove_if(firstname.begin(), firstname.end(), ::iswspace), firstname.end());
	lastname.erase(std::remove_if(lastname.begin(), lastname.end(), ::iswspace), lastname.end());
	return fmt::format("{}^{}", lastname, firstname);
}

static std::string dateToDcmFormat(const std::string& date)
{
	std::size_t firstPeriodPos = date.find(".");
	std::size_t secondPeriodPos = date.find(".", firstPeriodPos + 1);
	int day = std::stoi(date.substr(0, firstPeriodPos));
	int month = std::stoi(date.substr(firstPeriodPos + 1, secondPeriodPos - firstPeriodPos - 1));
	int year = std::stoi(date.substr(secondPeriodPos + 1, std::string::npos));
	return fmt::format("{}{:02}{:02}", year, month, day);
}

static bool recordExists(const std::vector<PatientRecord> &record_list, const PatientRecord &new_record)
{
	return std::any_of(record_list.begin(), record_list.end(), [&](const PatientRecord &rec)
	{
		return rec.m_id == new_record.m_id && rec.m_study_date == new_record.m_study_date;
	});
}

//static std::string concatenateStudyDate(const std::string& old_study_date, const std::string& new_study_date)
//{
//	return old_study_date + "\\" + new_study_date;
//}
