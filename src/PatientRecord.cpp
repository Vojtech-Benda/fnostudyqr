//
// Created by Vojtěch on 17.03.2025.
//

#include "PatientRecord.hpp"
#include <algorithm>
#include <fstream>
#include "fmt/format.h"
#include "fmt/color.h"

auto splitString = [](const std::string &line, const char delimiter = ',', const std::size_t parts = 4) {
	std::vector<std::string> tokens{};
	tokens.reserve(parts);
	std::size_t                start = 0, end;
	int                        i     = 0;
	while ((end = line.find(delimiter, start)) != std::string::npos && tokens[i].empty()) {
		tokens[i] = line.substr(start, end - start);
		start     = end + 1;
		++i;
	}
	tokens[i] = line.substr(start);
	return tokens;
};

auto checkTokens = [](std::vector<std::string> &tokens) {
	std::string msg;

	// check all
	if (tokens[0].empty() || tokens[1].empty() || tokens[2].empty()) {
		msg = fmt::format("PatientName, PatientID or StudyDate is empty");
		fmt::print("{} - {}\n", msg, fmt::format(fg(fmt::color::yellow), "LINE SKIPPED"));
		return true;
	}

	if (std::ranges::any_of(tokens[1], ::isalpha)) {
		msg = fmt::format(R"(ID "{}" contains non-numeric characters)", tokens[1]);
		fmt::print("{} - {}\n", msg, fmt::format(fg(fmt::color::yellow), "LINE SKIPPED"));
		return true;
	}

	// check study date
	if (std::ranges::any_of(tokens[2], ::isalpha)) {
		msg = fmt::format(R"(ID "{}" with Study Date "{}" contains alphabet characters)",
		                  tokens[1],
		                  tokens[2]);
		fmt::print("{} - {}\n", msg, fmt::format(fg(fmt::color::yellow), "LINE SKIPPED"));
		return true;
	}

	return false;
};

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

			auto tokens = splitString(line);

			if (checkTokens(tokens))
				continue;

			PatientRecord record{};

			// modify id
			std::erase(tokens[1], '/');
			if (tokens[1].length() == 9) {
				tokens[1].insert(0, 1, '0');
			}

			record.m_id = tokens[1];
			record.m_name = nameToDcmFormat(tokens[0]);
			record.m_study_date = dateToDcmFormat(tokens[2]);

			if (!tokens[3].empty()) {
				record.m_modality = tokens[3];
			}

			/*const std::size_t firstPeriodPos  = line.find(',');
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

			// sanitizing IDs with forward slash
			std::erase(record.m_id, '/');

			if (std::ranges::any_of(record.m_id, ::isalpha)) {
				const std::string msg = fmt::format(R"(ID "{}" contains non-numeric characters)", record.m_id);
				fmt::print("{} - {}\n", msg, fmt::format(fg(fmt::color::yellow), "LINE SKIPPED"));
				continue;
			}

			// pad 9-digit IDs with leading zero, usually old IDs
			if (record.m_id.length() == 9) {
				record.m_id.insert(0, 1, '0');
			}

			if (std::ranges::any_of(record.m_study_date, ::isalpha)) {
				const std::string msg = fmt::format(R"(ID "{}" with Study Date "{}" contains alphabet characters)",
				                                    record.m_id,
				                                    record.m_study_date);
				fmt::print("{} - {}\n", msg, fmt::format(fg(fmt::color::yellow), "LINE SKIPPED"));
				continue;
			}

			record.m_name       = nameToDcmFormat(record.m_name);
			record.m_study_date = dateToDcmFormat(record.m_study_date);*/

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

static std::string nameToDcmFormat(const std::string &fullname) {
	auto tokens = splitString(fullname, ' ', 2); // tokens[lastname, firstname]
	// const std::size_t ws_pos = fullname.find(' ');
	//
	// if (ws_pos == std::string::npos) {
	// 	return fullname;
	// }
	// std::string firstname = fullname.substr(0, ws_pos);
	// std::string lastname  = fullname.substr(ws_pos + 1, std::string::npos);
	//
	// if (firstname.empty() || lastname.empty()) {
	// 	return fullname;
	// }

	if (std::ranges::all_of(tokens, &std::string::empty))
		return "MISSING^MISSING";

	std::ranges::for_each(tokens, [&](std::string &token) {
		std::erase_if(token, ::isspace);
	});

	// for (auto& token : nameTokens) {
	// 	std::erase_if(token, ::iswspace);
	// }

	return fmt::format("{}^{}", tokens[1], tokens[0]);
}

static std::string dateToDcmFormat(const std::string &date) {
	auto tokens = splitString(date, '.', 3);

	std::ranges::for_each(tokens, [&](std::string &token) {
		std::erase_if(token, ::isalpha);
	});

	return fmt::format("{}{:02}{:02}",
	                   std::stoi(tokens[2]),
	                   std::stoi(tokens[1]),
	                   std::stoi(tokens[0]));
}
