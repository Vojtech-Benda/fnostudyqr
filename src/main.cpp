#include <chrono>
#include <filesystem>
#include <fstream>

#include "fmt/chrono.h"
#include "fmt/color.h"
#include "fmt/format.h"
#include "fmt/os.h"
#include "fmt/ranges.h"

#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/cmdlnarg.h"
#include "dcmtk/ofstd/ofconapp.h"

#include "PatientRecord.hpp"
#include "StudyQueryRetriever.hpp"

enum E_addModalities { ADD_MODALITIES_ALL, ADD_MODALITIES_MISSING };

int main(int argc, char *argv[]) {
  constexpr auto FNO_CONSOLE_APPLICATION{"fnostudyqr"};
  constexpr auto *APP_VERSION{"0.7.2"};
  constexpr auto APP_RELEASE_DATE{"2025-01-05"};

  const std::string rcsid =
      fmt::format("${}: ver. {} rel. {}\n$dcmtk: ver. {} rel. {}",
                  FNO_CONSOLE_APPLICATION, APP_VERSION, APP_RELEASE_DATE,
                  OFFIS_DCMTK_VERSION, OFFIS_DCMTK_RELEASEDATE);
  OFLogger mainLogger = OFLog::getLogger(
      fmt::format("fno.apps.{}", FNO_CONSOLE_APPLICATION).c_str());

  constexpr int SHORTCOL{4};
  constexpr int LONGCOL{20};

  constexpr auto USER_APPLICATION_TITLE{"FNOSTUDYQR"};
  constexpr auto PACS_APPLICATION_TITLE{"STORESCP"};

  OFConsoleApplication app(FNO_CONSOLE_APPLICATION,
                           "DICOM study query/retrieve (C-FIND/C-MOVE) SCU",
                           rcsid.c_str());
  OFCommandLine cmd;
  QueryRetriever queryRetriever;

  // const char *        opt_callerIP{ nullptr }; // ip address of application
  // user
  const char *opt_pacsIP{nullptr};  // hostname/ip address of PACS (DICOM peer)
  OFCmdUnsignedInt opt_pacsPort{0}; // tcp/ip port of peer
  OFCmdUnsignedInt opt_recievePort{0}; // retrieve port to receive data

  const char *opt_aeCaller{USER_APPLICATION_TITLE};   // ae-caller/aet
  const char *opt_aePacs{PACS_APPLICATION_TITLE};     // ae-pacs/aec
  const char *opt_aeReceiver{USER_APPLICATION_TITLE}; // ae-receiver/aem

  OFString opt_outputDirectory{"./download"};
  const char *opt_filepath{nullptr}; // filepath to queried patient list

  const char *opt_queryModality{nullptr};
  E_addModalities opt_addModalities{E_addModalities::ADD_MODALITIES_MISSING};

  std::vector<OFString> opt_overrideTags{};
  OFBool opt_retrieveTags{OFFalse};
  OFBool opt_retrieveFiles{OFFalse};

  OFString opt_dumpFilepath{"./dumped_tags"};
  OFBool opt_logMissingStudies{OFTrue};
  studyDateRangeExtend opt_extendStudyDate{};

  cmd.setParamColumn(LONGCOL + SHORTCOL + 4);
  cmd.addParam("pacs-ip", "hostname of DICOM peer");
  cmd.addParam("pacs-port", "tcp/ip port number of peer");

  cmd.setOptionColumns(LONGCOL, SHORTCOL);
  cmd.addGroup("general options:", LONGCOL, SHORTCOL + 2);
  cmd.addOption("--help", "-h", "print this help text and exit",
                OFCommandLine::AF_Exclusive);
  cmd.addOption("--version", "print version information and exit",
                OFCommandLine::AF_Exclusive);
  OFLog::addOptions(cmd);

  cmd.addGroup("network options:");
  // cmd.addSubGroup("query information model:");
  // cmd.addOption("--patient",  "-P",   "use patient root information model
  // (default)"); cmd.addOption("--study",    "-S",   "use study root
  // information model");

  cmd.addSubGroup("application entity titles:");
  cmd.addOption("--ae-caller", "-aet", 1, "[a]etitle: string",
                fmt::format("set my calling AE title (default: {}",
                            USER_APPLICATION_TITLE)
                    .c_str());
  cmd.addOption("--ae-pacs", "-aec", 1, "[a]etitle: string",
                fmt::format("set called AE title of peer (default: {})",
                            PACS_APPLICATION_TITLE)
                    .c_str());
  cmd.addOption("--ae-receiver", "-aem", 1, "[a]etitle: string",
                fmt::format("set move destination AE title (default: {})",
                            USER_APPLICATION_TITLE)
                    .c_str());

  cmd.addSubGroup("port for incoming network associations:");
  cmd.addOption("--receive-port", "-port", 1, "[n]umber: integer",
                "port number for incoming associations");

  cmd.addGroup("input options:");
  cmd.addOption(
      "--patient-list-file", "-plist", 1, "filepath: string path",
      "text file with patient/study information to query/retrieve\nrequired "
      "order: PatientName,PatientID,StudyDate,(Modality)");
  cmd.addOption(
      "--add-modality-missing", "-am", 1, "modality: string",
      "add modality to missing modalities in read patient records (default)");
  cmd.addOption(
      "--add-modality-all", "+am", 1, "modality: string",
      "add modality to all read patient records, overwrites read modalities");
  cmd.addOption("--tag", "-t", 1, "[t]ag: gggg,eeee=\"str\" or name=\"str\"",
                "additional query tags");
  cmd.addOption(
      "--extend-date", 1, "month: integer (default: 0)",
      "extend all study dates to range match <date1> - <date2>\n<date1> = "
      "StudyDate - month\n<date2> = StudyDate + month");

  cmd.addGroup("output options:");
  cmd.addOption("--output-directory", "-od", 1,
                "[d]irectory: string (default: \"./download\"",
                "write received data to directory d");
  cmd.addOption(
      "--dump-filepath", "-df", 1,
      "[f]ilepath: string (default: \"<output-directory>/dumped_tags.csv\")",
      "CSV filepath to write retrieved tags, excluding extension");
  cmd.addOption("--retrieve-tags", "-rt",
                "retrieve queried tags and store them to CSV");
  cmd.addOption("--retrieve-files", "-rf",
                "perform C-MOVE request for queried tags");
  cmd.addOption("--no-missing-file", "-nf",
                "disable writing missing studies to file");

  prepareCmdLineArgs(argc, argv, FNO_CONSOLE_APPLICATION);
  if (app.parseCommandLine(cmd, argc, argv)) {
    if (cmd.hasExclusiveOption()) {
      if (cmd.findOption("--version")) {
        app.printHeader(OFTrue);
        return EXITCODE_NO_ERROR;
      }
    }

    cmd.getParam(1, opt_pacsIP); // ip address of PACS
    queryRetriever.m_calledIP = opt_pacsIP;

    app.checkParam(cmd.getParamAndCheckMinMax(2, opt_pacsPort, 1, 65535));
    queryRetriever.m_port = OFstatic_cast(unsigned short, opt_pacsPort);

    OFLog::configureFromCommandLine(cmd, app);

    if (cmd.findOption("--ae-caller")) {
      app.checkValue(cmd.getValue(opt_aeCaller));
      queryRetriever.m_callerAETitle = opt_aeCaller;
    }

    if (cmd.findOption("--ae-pacs")) {
      app.checkValue(cmd.getValue(opt_aePacs));
      queryRetriever.m_calledAETitle = opt_aePacs;
    }

    if (cmd.findOption("--ae-receiver")) {
      app.checkValue(cmd.getValue(opt_aeReceiver));
      queryRetriever.m_receiverAETitle = opt_aeReceiver;
    }

    if (cmd.findOption("--receive-port")) {
      app.checkValue(cmd.getValueAndCheckMinMax(opt_recievePort, 1, 65535));
      queryRetriever.m_retrievePort =
          OFstatic_cast(unsigned short, opt_recievePort);
    }

    if (cmd.findOption("--output-directory")) {
      app.checkValue(cmd.getValue(opt_outputDirectory));
    }

    if (cmd.findOption("--patient-list-file"))
      app.checkValue(cmd.getValue(opt_filepath));

    if (cmd.findOption("--add-modality-missing")) {
      opt_addModalities = E_addModalities::ADD_MODALITIES_MISSING;
      app.checkValue(cmd.getValue(opt_queryModality));
    }

    if (cmd.findOption("--add-modality-all")) {
      opt_addModalities = E_addModalities::ADD_MODALITIES_ALL;
      app.checkValue(cmd.getValue(opt_queryModality));
    }

    if (cmd.findOption("--tag", 0, OFCommandLine::FOM_FirstFromLeft)) {
      OFString ovTag{};
      do {
        app.checkValue(cmd.getValue(ovTag));
        if (ovTag == "PatientID" || ovTag == "StudyInstanceUID" ||
            ovTag == "SeriesDescription") {
          OFLOG_WARN(mainLogger, "Ignoring -t "
                                     << ovTag
                                     << "; already requested by default");
          continue;
        }
        opt_overrideTags.push_back(ovTag);
      } while (cmd.findOption("--tag", 0, OFCommandLine::FOM_NextFromLeft));
    }

    if (cmd.findOption("--dump-filepath")) {
      app.checkValue(cmd.getValue(opt_dumpFilepath));
    }

    if (cmd.findOption("--retrieve-tags")) {
      opt_retrieveTags = OFTrue;
    }

    if (cmd.findOption("--retrieve-files")) {
      opt_retrieveFiles = OFTrue;
    }

    if (cmd.findOption("--no-missing-log")) {
      opt_logMissingStudies = OFFalse;
    }

    if (cmd.findOption("--extend-date")) {
      // OFCmdUnsignedInt year{};
      OFCmdUnsignedInt month{};
      // app.checkValue(cmd.getValueAndCheckMin(year, 0));
      app.checkValue(cmd.getValueAndCheckMin(month, 0));
      opt_extendStudyDate.rangeMatch = true;
      // opt_extendStudyDate.byYear = OFstatic_cast(Uint8, year);
      opt_extendStudyDate.byMonth = OFstatic_cast(unsigned int, month);
    }

    OFLOG_DEBUG(mainLogger, rcsid.c_str() << OFendl);

    if (queryRetriever.m_retrievePort <= 0 &&
        queryRetriever.m_receiverAETitle.empty()) {
      OFLOG_ERROR(mainLogger,
                  "Missing parameter --receiver-port (-port) number");
      return EXITCODE_COMMANDLINE_SYNTAX_ERROR;
    }

    if (!queryRetriever.m_receiverAETitle.empty() &&
        queryRetriever.m_retrievePort > 0) {
      fmt::print("Setting local receiver port (-port) with AE title of third "
                 "party destination (-aem) not required\n");
    }

    if (!queryRetriever.m_receiverAETitle.empty() &&
        !opt_outputDirectory.empty()) {
      fmt::print("Setting local output directory (-od) with AE title of third "
                 "party destination (-aem) not required\n");
    }

    if (OFStandard::dirExists(opt_outputDirectory))
      OFLOG_DEBUG(mainLogger,
                  "Output directory exists: " << opt_outputDirectory);
    else {
      (void)OFStandard::createDirectory(opt_outputDirectory, "");
      OFLOG_DEBUG(mainLogger,
                  "Created output directory: " << opt_outputDirectory);
    }

    if (!OFStandard::isWriteable(opt_outputDirectory)) {
      OFLOG_FATAL(mainLogger, "Specified output directory is not writable");
      return EXITCODE_CANNOT_WRITE_OUTPUT_FILE;
    }
    queryRetriever.m_outputDirectory = opt_outputDirectory.c_str();

    if (opt_filepath == nullptr) {
      OFLOG_ERROR(mainLogger, "No text file specified");
      return EXITCODE_COMMANDLINE_SYNTAX_ERROR;
    }
  }

  const auto filepath = std::filesystem::absolute(opt_filepath);

  if (!std::filesystem::exists(filepath)) {
    OFLOG_ERROR(mainLogger,
                "Text file not found " << filepath.string().c_str());
    return EXITCODE_TEXT_FILE_ERROR;
  }

  fmt::print("READING TEXT FILE ------------------------- \n");
  std::vector<PatientRecord> recordList =
      readPatientRecords(filepath.string(), opt_extendStudyDate);

  if (!recordList.empty())
    fmt::print("Found {} records to query\n", recordList.size());
  else {
    OFLOG_FATAL(mainLogger, "Record list is empty");
    return EXITCODE_EMPTY_RECORD_LIST;
  }

  std::string queryModality{};
  if (opt_queryModality == nullptr) {
    std::cout << R"(Specify query modalities (ex. CT, MR\CT, CR\US\MG): )";
    std::cin >> queryModality;

    if (queryModality.empty()) {
      OFLOG_ERROR(mainLogger, "Specified no modalities to query");
      return EXITCODE_NO_MODALITIES_SPECIFIED;
    }
  } else {
    queryModality = opt_queryModality;
  }

  std::ranges::replace_if(
      queryModality, [](const char c) { return c == '/'; }, '\\');

  for (auto &record : recordList) {
    // add modality specified on cmd line
    // reason: some records may have modality specified, some may not

    // add to all
    if (opt_addModalities == E_addModalities::ADD_MODALITIES_ALL) {
      record.m_modality = queryModality;
    } else {
      // otherwise add only to records with no modality specified in text file
      if (record.m_modality.empty())
        record.m_modality = queryModality;
    }
  }

  OFCondition cond = queryRetriever.initializeNetwork();
  OFString temp_string;

  if (cond.bad()) {
    OFLOG_ERROR(mainLogger, "Cannot create network: "
                                << DimseCondition::dump(temp_string, cond));
    OFLOG_ERROR(mainLogger, "Exiting program");
    return EXITCODE_CANNOT_INITIALIZE_NETWORK;
  }

  cond = queryRetriever.setupAssociation();

  if (cond.bad()) {
    OFLOG_ERROR(mainLogger, "Failed to setup association: "
                                << DimseCondition::dump(temp_string, cond));
    OFLOG_ERROR(mainLogger, "Exiting program");
    return EXITCODE_CANNOT_NEGOTIATE_NETWORK;
  }

  const auto time = std::chrono::system_clock::now();
  const auto tt = std::chrono::system_clock::to_time_t(time);
  const std::tm tm = *std::localtime(&tt);

  const std::string missingStudiesFilename =
      fmt::format("missing-studies-{:%Y-%m-%d-%H-%M-%S}.txt", tm);

  fmt::ostream missingStudiesFile = fmt::output_file(missingStudiesFilename);
  if (opt_logMissingStudies) {
    missingStudiesFile.print("{:%Y-%m-%d %H:%M:%S}\n", tm);
    missingStudiesFile.print("PatientID, StudyDate\n");
  }

  if (opt_retrieveTags) {
    OFLOG_INFO(mainLogger, "QueryRetriever set up for dumping tags");
  }

  if (opt_retrieveFiles) {
    OFLOG_INFO(mainLogger, "QueryRetriever set up for storing files");
  }

  fmt::print("C-FIND ---------- FIND STUDIES\n");
  cond = EC_Normal;

  for (auto &record : recordList) {
    cond = queryRetriever.performFindRequest(record, queryModality, nullptr);
    const std::string msg = fmt::format("PatientID: {}, StudyDate: {}",
                                        record.m_id, record.m_study_date);

    if (record.m_uid_list.empty()) {
      fmt::print("{} - {}\n", msg,
                 fmt::format(fg(fmt::color::red), "FAIL, STUDY NOT FOUND"));
      missingStudiesFile.print("{}, {} - NOT FOUND\n", record.m_id,
                               record.m_study_date);
    } else {
      fmt::print("{} - {}\n", msg,
                 fmt::format(fg(fmt::color::green), "SUCCESS, {} study/ies",
                             record.m_uid_list.size()));
      if (opt_logMissingStudies) {
        fmt::print("StudyInstanceUIDs: \n{}\n", record.m_uid_list);
      }
    }
  }

  missingStudiesFile.close();

  // remove missing-studies file
  if (!opt_logMissingStudies) {
    OFLOG_DEBUG(mainLogger,
                "Removing file \"" << missingStudiesFilename.c_str() << "\"");
    std::filesystem::remove(missingStudiesFilename);
  }

  if (opt_logMissingStudies) {
    fmt::print("Records of missing studies written to {}\n",
               missingStudiesFilename);
  }

  if (opt_retrieveTags) {
    fmt::print("C-FIND ---------- DUMP TAGS\n");

    if (opt_overrideTags.empty()) {
      fmt::print("no additional tags to query for, writing only defaults: "
                 "PatientID, StudyInstanceUID, SeriesDescription\n");
    }

    OFString header{"PatientID;StudyInstanceUID;SeriesDescription"};
    auto iter = opt_overrideTags.begin();
    while (iter != opt_overrideTags.end()) {
      if (iter != opt_overrideTags.end()) {
        header += ";";
      }
      header += *iter;
      ++iter;
    }

    const std::string dumpFilePath = fmt::format("{}-{:%Y-%m-%d-%H-%M-%S}.csv",
                                                 opt_dumpFilepath.c_str(), tm);
    fmt::ostream fileStream =
        fmt::output_file(dumpFilePath, fmt::file::CREATE | fmt::file::WRONLY |
                                           fmt::file::APPEND);
    fileStream.print("{}\n", header.c_str());
    fileStream.close();

    cond = EC_Normal;
    for (const auto &record : recordList) {
      if (record.m_uid_list.empty()) {
        OFLOG_DEBUG(
            mainLogger,
            fmt::format("not querying tags for \"{}\", no study instance uids",
                        record.m_id));
        continue;
      }

      std::vector<TagValuePair> queryTags;
      for (const auto &ov_tag : opt_overrideTags) {
        DcmTag tag = prepareQueryTag(app, ov_tag.c_str());
        queryTags.emplace_back(tag, ""); // adds TagValuePar<tag, "">
      }

      cond = queryRetriever.dumpTags(record, dumpFilePath, queryTags, nullptr);
    }
    fmt::print("Writing tags: {}\n", header.c_str());
    fmt::print("Tags written to: {}\n", dumpFilePath);
  }

  // TODO: Perhaps add in the future? Replace with current opt_overrideTags
  // querying? DcmPathProcessor proc; for (const auto &tag : m_overrideTags) {
  //     cond = proc.applyPathWithValue(requestedDataset, tag);
  //     if (cond.bad()) {
  //         DCMNET_ERROR("bad override tag: " << tag);
  //         return cond;
  //     }
  // }

  if (opt_retrieveFiles) {
    fmt::print("C-MOVE ---------- MOVE STUDIES\n");
    cond = EC_Normal;
    for (const auto &record : recordList) {
      if (record.m_uid_list.empty()) {
        const std::string msg = fmt::format("PatientID: {}, StudyDate: {}",
                                            record.m_id, record.m_study_date);
        fmt::print(
            "{} - {}\n", msg,
            fmt::format(fg(fmt::color::red), "FAIL, MISSING StudyInstanceUID"));
        continue;
      }
      cond = queryRetriever.performMoveRequest(record);
    }
  }

  int exitCode = cond.good() ? 0 : 2;
  cond = queryRetriever.dropNetwork();

  if (cond.bad()) {
    OFLOG_FATAL(mainLogger, "Failed to drop network: "
                                << DimseCondition::dump(temp_string, cond));
    if (!exitCode)
      exitCode = 3;
  }

  OFStandard::shutdownNetwork();

  return exitCode;
}
