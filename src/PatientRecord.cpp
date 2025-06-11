//
// Created by Vojtěch on 17.03.2025.
//

#include "PatientRecord.hpp"

#include <algorithm>
#include <fstream>

#include "fmt/format.h"
#include "fmt/color.h"

auto splitString = [](std::string_view line, const char delimiter = ';', const std::size_t parts = 3) {
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
	// record.m_name.empty() ||
	if (record.m_id.empty() || record.m_study_date.empty()) {
		msg = fmt::format("PatientName, PatientID or StudyDate is empty");
		fmt::print("{} - {}\n", msg, fmt::format(fg(fmt::color::yellow), "LINE SKIPPED"));
		checkFailed = true;
		msg.clear();
	}

	if (std::ranges::any_of(record.m_id, ::isalpha)) {
		msg = fmt::format(R"(ID "{}" contains non-numeric characters)", record.m_id);
		fmt::print("{} - {}\n", msg, fmt::format(fg(fmt::color::yellow), "LINE SKIPPED"));
		checkFailed = true;
		msg.clear();
	}

	// check study date
	if (std::ranges::any_of(record.m_study_date, ::isalpha)) {
		msg = fmt::format(R"(ID "{}" with Study Date "{}" contains alphabet characters)",
		                  record.m_id, record.m_study_date);
		fmt::print("{} - {}\n", msg, fmt::format(fg(fmt::color::yellow), "LINE SKIPPED"));
		checkFailed = true;
		msg.clear();
	}

	return checkFailed;
};

std::vector<PatientRecord> readPatientRecords(const std::string &textFilePath) {
	std::ifstream fileObject{textFilePath, std::ios::in};

	if (!fileObject.is_open()) {
		fmt::print("Unable to open text file {}\n", textFilePath);
		return {};
	}

	std::vector<PatientRecord> recordList{};
	if (fileObject.is_open()) {
		std::string line;
		while (std::getline(fileObject, line)) {
			if (line.empty()) continue;

			auto tokens = splitString(line, ';', 3); // tokens[name, id, study_date, modality]

			PatientRecord record{};
			// record.m_name       = nameToDcmFormat(tokens[0]);
			record.m_id         = idToDcmFormat(tokens[0]);
			record.m_study_date = dateToDcmFormat(tokens[1]);

			if (!tokens[2].empty()) {
				record.m_modality = tokens[2];
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

std::string nameToDcmFormat(std::string_view fullname) {
	auto tokens = splitString(fullname, ' ', 2); // tokens[lastname, firstname]

	if (std::ranges::all_of(tokens, &std::string::empty))
		return "MISSING^MISSING";

	for (auto &token : tokens) {
		std::erase_if(token, ::iswspace);
	}

	return fmt::format("{}^{}", tokens[1], tokens[0]);
}

std::string dateToDcmFormat(std::string_view date) {
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

std::string idToDcmFormat(std::string_view id) {
	std::string mod_id{id};

	// allow reading ids with forward slash
	// remove possible alphabet characters
	std::erase(mod_id, '/');
	std::erase_if(mod_id, ::isalpha);

	// IMPORTANT FOR PACS:
	// ids must be 10-digit length, otherwise querying could fail
	// pad 9-digit IDs with leading zero
	if (mod_id.length() == 9) {
		mod_id.insert(0, 1, '0');
	}
	return mod_id;
}