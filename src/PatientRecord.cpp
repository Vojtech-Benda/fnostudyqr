//
// Created by Vojtěch on 17.03.2025.
//

#include "PatientRecord.hpp"
#include <algorithm>
#include <fstream>
#include "fmt/format.h"
#include "fmt/color.h"

auto splitString = [](const std::string &line, const char delimiter = ',', const std::size_t parts = 4) {
	std::vector<std::string> tokens(parts, "");

	std::size_t start{0}, end;
	int i{0};
	while ((end = line.find(delimiter, start)) != std::string::npos) {
		tokens[i] = line.substr(start, end - start);
		start = end + 1;
		++i;
	}
	tokens[i] = line.substr(start);
	return tokens;
};

auto checkRecord = [](const PatientRecord &record) {
	bool checkFailed{false};
	std::string msg;


	// check all
	if (record.m_name.empty() || record.m_id.empty() || record.m_study_date.empty()) {
		msg = fmt::format("PatientName, PatientID or StudyDate is empty");
		fmt::print("{} - {}\n", msg, fmt::format(fg(fmt::color::yellow), "LINE SKIPPED"));
		checkFailed = true;
	}

	if (std::ranges::any_of(record.m_id, ::isalpha)) {
		msg = fmt::format(R"(ID "{}" contains non-numeric characters)", record.m_id);
		fmt::print("{} - {}\n", msg, fmt::format(fg(fmt::color::yellow), "LINE SKIPPED"));
		checkFailed = true;
	}

	// check study date
	if (std::ranges::any_of(record.m_study_date, ::isalpha)) {
		msg = fmt::format(R"(ID "{}" with Study Date "{}" contains alphabet characters)",
		                  record.m_id, record.m_study_date);
		fmt::print("{} - {}\n", msg, fmt::format(fg(fmt::color::yellow), "LINE SKIPPED"));
		checkFailed = true;
	}

	return checkFailed;
};

std::vector<PatientRecord> readPatientRecords(const std::filesystem::path &textFilePath) {
	std::ifstream fileObject{textFilePath, std::ios::in};

	if (!fileObject.is_open()) {
		fmt::print("Unable to open text file {}\n", textFilePath.string());
		return {};
	}

	std::vector<PatientRecord> recordList{};
	if (fileObject.is_open()) {
		std::string line;
		while (std::getline(fileObject, line)) {
			if (line.empty()) continue;

			auto tokens = splitString(line); // tokens[name, id, study_date, modality]

			// remove possible '/' in id and alphabet characters
			std::erase(tokens[1], '/');
			std::erase_if(tokens[1], ::isalpha);

			// IMPORTANT FOR PACS:
			// ids must be 10-digit length, otherwise querying could fail
			// pad 9-digit IDs with leading zero
			if (tokens[1].length() == 9) {
				tokens[1].insert(0, 1, '0');
			}

			PatientRecord record{};

			record.m_id = tokens[1];
			record.m_name = nameToDcmFormat(tokens[0]);
			record.m_study_date = dateToDcmFormat(tokens[2]);

			if (!tokens[3].empty()) {
				record.m_modality = tokens[3];
			}

			// sanity check record for invalid characters
			if (checkRecord(record))
				continue;

			bool recordExists = std::ranges::any_of(recordList,
			                                        [&](const PatientRecord &rec) {
				                                        return rec.m_id == record.m_id && rec.m_study_date == record.
						                                        m_study_date;
			                                        });

			if (recordExists) {
				const std::string msg = fmt::format("PatientID {}: StudyDate: {}", record.m_id, record.m_study_date);
				fmt::print("{} - {}\n", msg, fmt::format(fg(fmt::color::yellow), "SKIPPING DUPLICATE IN TEXT FILE"));
				continue;
			}

			recordList.push_back(record);
		}
	}
	fileObject.close();
	return recordList;
}

static std::string nameToDcmFormat(const std::string &fullname) {
	auto tokens = splitString(fullname, ' ', 2); // tokens[lastname, firstname]

	if (std::ranges::all_of(tokens, &std::string::empty))
		return "MISSING^MISSING";

	for (auto &token : tokens) {
		std::erase_if(token, ::iswspace);
	}

	return fmt::format("{}^{}", tokens[1], tokens[0]);
}

static std::string dateToDcmFormat(const std::string &date) {
	auto tokens = splitString(date, '.', 3); // tokens[day, month, year]

	for (auto &token : tokens) {
		std::erase_if(token, ::iswspace);
		std::erase_if(token, ::isalpha);
	}

	return fmt::format("{}{:02}{:02}",
	                   std::stoi(tokens[2]),
	                   std::stoi(tokens[1]),
	                   std::stoi(tokens[0]));
}
