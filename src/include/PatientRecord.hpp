//
// Created by Vojtěch on 17.03.2025.
//

#ifndef PATIENTRECORD_HPP
#define PATIENTRECORD_HPP

#include <string>
// #include <string_view>
#include <vector>
// #include <unordered_map>
#include <filesystem>

#include "fmt/format.h"
#include "fmt/color.h"

struct PatientRecord
{
	std::string m_id{};
	std::string m_name{};
	std::string m_study_date{};
	std::string m_modality{};
	std::vector<std::string> m_uid_list{};
	PatientRecord() = default;
	PatientRecord(const std::string& id,
				  const std::string& name,
				  const std::string& study_date,
				  const std::string& modality) :
		m_id{ id },
		m_name{ name },
		m_study_date{ study_date },
		m_modality{ modality } {}
	~PatientRecord() = default;
};

struct RecordKey
{
	std::string m_id;
	std::string m_name;
	std::string m_study_date;

	RecordKey() = default;
	RecordKey(std::string_view id, std::string_view name, std::string_view study_date) :
		m_id{ id }, m_name{ name }, m_study_date{ study_date } {
	};
	~RecordKey() = default;

	bool operator==(const RecordKey& other) const
	{
		return m_id == other.m_id &&
			m_name == other.m_name &&
			m_study_date == other.m_study_date;
	}
};

struct RecordKeyHash
{
	std::size_t operator()(const RecordKey& key) const
	{
		return std::hash<std::string>()(key.m_id) ^
			(std::hash<std::string>()(key.m_name) << 1) ^
			(std::hash<std::string>()(key.m_study_date) << 2);
	}
};

/*
struct StudyRecord
{
	std::string m_studyDates{};
	std::vector<std::string> m_uidList{};
};
*/

//using StudyUIDList = std::vector<std::string>;
//using PatientRecordMap = std::unordered_map<RecordKey, StudyUIDList, RecordKeyHash>;
//using StudyRecordMap = std::unordered_map<std::string, StudyRecord>;

//PatientRecordMap readPatientRecords(const std::filesystem::path& csvFilePath);
std::vector<PatientRecord> readPatientRecords(const std::filesystem::path &csvFilePath);
//StudyRecordMap readPatientRecords(std::ifstream& csvfile);
static std::string nameToDcmFormat(std::string& fullname);
static std::string dateToDcmFormat(const std::string& date);
//static std::string concatenateStudyDate(const std::string& old_study_date, const std::string& new_study_date);


#endif //PATIENTRECORD_HPP
