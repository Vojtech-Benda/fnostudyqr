//
// Created by Vojtěch on 17.03.2025.
//

#ifndef PATIENTRECORD_HPP
#define PATIENTRECORD_HPP

#include <string>
#include <string_view>
#include <vector>
#include <set>

struct PatientRecord {
	std::string           m_id{};
	std::string           m_name{};
	std::string           m_study_date{};
	std::string           m_modality{};
	std::set<std::string> m_uid_list{};

	PatientRecord() = default;

	PatientRecord(const std::string_view id,
	              const std::string_view name,
	              const std::string_view study_date) : m_id{id},
	                                                   m_name{name},
	                                                   m_study_date{study_date} {}

	~PatientRecord() = default;
};

struct studyDateRangeExtend {
	bool         rangeMatch{false};
	int          byYear{0};
	unsigned int byMonth{0};
};

std::vector<PatientRecord> readPatientRecords(const std::string &         textFilePath,
                                              const studyDateRangeExtend &studyDateRange);

static std::string nameToDcmFormat(std::string_view fullname);

static std::string dateToDcmFormat(std::string_view            date,
                                   const studyDateRangeExtend &study_date_range);

static std::string idToDcmFormat(std::string_view id);

#endif //PATIENTRECORD_HPP
