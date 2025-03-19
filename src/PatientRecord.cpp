//
// Created by Vojtěch on 17.03.2025.
//

#include "PatientRecord.hpp"

#include <algorithm>
#include <fstream>

#include "fmt/color.h"

std::vector<PatientRecord> readPatientRecords(const std::filesystem::path &csvFilePath) {
	std::ifstream              fileObject{csvFilePath, std::ios::in};
	std::vector<PatientRecord> recordList;
	if (!fileObject.is_open()) {
		fmt::print("Unable to open CSV file {}\n", csvFilePath.string());
		return {};
	}

	if (fileObject.is_open()) {
		std::string line;
		while (std::getline(fileObject, line)) {
			if (line.empty()) continue;

			PatientRecord     record;

			const std::size_t firstPeriodPos  = line.find(',');
			const std::size_t secondPeriodPos = line.find(',', firstPeriodPos + 1);
			const std::size_t thirdPeriodPos  = line.find(',', secondPeriodPos + 1);

			record.m_name       = line.substr(0, firstPeriodPos);
			record.m_id         = line.substr(firstPeriodPos + 1, secondPeriodPos - firstPeriodPos - 1);
			record.m_study_date = line.substr(secondPeriodPos + 1, thirdPeriodPos - secondPeriodPos - 1);

			if (thirdPeriodPos != std::string::npos) {
				record.m_modality = line.substr(thirdPeriodPos + 1, std::string::npos);
			}

			if (record.m_id.empty() || record.m_name.empty() || record.m_study_date.empty()) {
				const std::string msg = fmt::format("Patient Name, Patient ID or Study Date is empty");
				fmt::print("{} - {}\n", msg, fmt::format(fg(fmt::color::yellow), "LINE SKIPPED"));
				continue;
			}

			if (std::ranges::any_of(record.m_id, ::isalpha)) {
				const std::string msg = fmt::format(R"(ID "{}" contains non-numeric characters)", record.m_id);
				fmt::print("{} - {}\n", msg, fmt::format(fg(fmt::color::yellow), "LINE SKIPPED"));
				continue;
			}

			if (std::ranges::any_of(record.m_study_date, ::isalpha)) {
				const std::string msg = fmt::format(R"(ID "{}" with Study Date "{}" contains alphabet characters)",
				                                    record.m_id,
				                                    record.m_study_date);
				fmt::print("{} - {}\n", msg, fmt::format(fg(fmt::color::yellow), "LINE SKIPPED"));
				continue;
			}

			record.m_name       = nameToDcmFormat(record.m_name);
			record.m_study_date = dateToDcmFormat(record.m_study_date);

			bool recordExists = std::ranges::any_of(recordList,
			                                        [&](const PatientRecord &rec) {
				                                        return rec.m_id == record.m_id && rec.m_study_date == record.
						                                        m_study_date;
			                                        });

			if (recordExists) {
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

static std::string nameToDcmFormat(std::string &fullname) {
	std::erase_if(fullname, ::isdigit);
	const std::size_t ws_pos = fullname.find(' ');

	if (ws_pos == std::string::npos) {
		return fullname;
	}
	std::string firstname = fullname.substr(0, ws_pos);
	std::string lastname  = fullname.substr(ws_pos + 1, std::string::npos);
	std::erase_if(firstname, ::iswspace);
	std::erase_if(lastname, ::iswspace);
	return fmt::format("{}^{}", lastname, firstname);
}

static std::string dateToDcmFormat(const std::string &date) {
	const std::size_t firstPeriodPos = date.find('.');
	const std::size_t secondPeriodPos = date.find('.', firstPeriodPos + 1);
	int               day = std::stoi(date.substr(0, firstPeriodPos));
	int               month = std::stoi(date.substr(firstPeriodPos + 1, secondPeriodPos - firstPeriodPos - 1));
	int               year = std::stoi(date.substr(secondPeriodPos + 1, std::string::npos));
	return fmt::format("{}{:02}{:02}", year, month, day);
}
