//
// Created by Vojtěch on 17.03.2025.
//

#ifndef PATIENTRECORD_HPP
#define PATIENTRECORD_HPP

#include <string>
#include <string_view>
#include <vector>

struct PatientRecord {
	std::string              m_id{};
	std::string              m_name{};
	std::string              m_study_date{};
	std::string              m_modality{};
	std::vector<std::string> m_uid_list{};

	PatientRecord() = default;

	PatientRecord(const std::string_view id,
	              const std::string_view name,
	              const std::string_view study_date) : m_id{id},
	                                                   m_name{name},
	                                                   m_study_date{study_date} {}

	~PatientRecord() = default;
};

std::vector<PatientRecord> readPatientRecords(const std::string &textFilePath);

static std::string nameToDcmFormat(const std::string &fullname);

static std::string dateToDcmFormat(const std::string &date);


#endif //PATIENTRECORD_HPP
