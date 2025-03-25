#include <filesystem>
#include <fstream>
#include <chrono>

#include "fmt/format.h"
#include "fmt/ranges.h"
#include "fmt/color.h"
#include "fmt/os.h"
#include "fmt/chrono.h"

#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/cmdlnarg.h"
#include "dcmtk/ofstd/ofconapp.h"

#include "PatientRecord.hpp"
#include "StudyQueryRetriever.hpp"

enum E_OverwriteModalites {
    OVERWRITE_MODALITIES_NONE,
    OVERWRITE_MODALITIES_ALL,
    OVERWRITE_MODALITIES_MISSING
};


int main(int argc, char *argv[]) {
    // const char *FNO_CONSOLE_APPLICATION{ "fnostudyqr" };
    constexpr auto    FNO_CONSOLE_APPLICATION{"fnostudyqr"};
    constexpr auto *  APP_VERSION{"0.5.1"};
    constexpr auto    APP_RELEASE_DATE{"2025-01-05"};
    const std::string rcsid = fmt::format("${}: ver. {} rel. {}\n$dcmtk: ver. {} rel. {}",
                                          FNO_CONSOLE_APPLICATION,
                                          APP_VERSION,
                                          APP_RELEASE_DATE,
                                          OFFIS_DCMTK_VERSION,
                                          OFFIS_DCMTK_RELEASEDATE);
    OFLogger mainLogger = OFLog::getLogger(fmt::format("fno.apps.{}", FNO_CONSOLE_APPLICATION).c_str());

    constexpr int  SHORTCOL{4};
    constexpr int  LONGCOL{20};
    constexpr auto USER_APPLICATION_TITLE{"FNOSTUDYQR"};
    constexpr auto PACS_APPLICATION_TITLE{"STORESCP"};

    OFConsoleApplication app(FNO_CONSOLE_APPLICATION, "DICOM study query/retrieve (C-FIND/C-MOVE) SCU", rcsid.c_str());
    OFCommandLine        cmd;
    QueryRetriever       queryRetriever;

    // const char *        opt_callerIP{ nullptr }; // ip address of application user
    const char *     opt_pacsIP{nullptr};                    // hostname/ip address of PACS (DICOM peer)
    OFCmdUnsignedInt opt_pacsPort{0};                        // tcp/ip port of peer
    OFCmdUnsignedInt opt_recievePort{0};                     // retrieve port to receive data
    const char *     opt_aeCaller{USER_APPLICATION_TITLE};   // ae-caller/aec
    const char *     opt_aePacs{PACS_APPLICATION_TITLE};     // ae-pacs/aep
    const char *     opt_aeReceiver{USER_APPLICATION_TITLE}; // ae-receiver/aer
    const char *     opt_outputDirectory{nullptr};
    const char *     opt_filepath{nullptr};
    bool             opt_overwriteModalities{E_OverwriteModalites::OVERWRITE_MODALITIES_NONE};
    bool             opt_writeMissingStudies{true};


    cmd.setParamColumn(LONGCOL + SHORTCOL + 4);
    cmd.addParam("pacs-ip", "hostname of DICOM peer");
    cmd.addParam("pacs-port", "tcp/ip port number of peer");

    cmd.setOptionColumns(LONGCOL, SHORTCOL);
    cmd.addGroup("general options:", LONGCOL, SHORTCOL + 2);
    cmd.addOption("--help", "-h", "print this help text and exit", OFCommandLine::AF_Exclusive);
    cmd.addOption("--version", "print version information and exit", OFCommandLine::AF_Exclusive);
    OFLog::addOptions(cmd);

    cmd.addGroup("network options:");
    //cmd.addSubGroup("query information model:");
    //cmd.addOption("--patient",  "-P",   "use patient root information model (default)");
    //cmd.addOption("--study",    "-S",   "use study root information model");

    cmd.addSubGroup("application entity titles:");
    cmd.addOption("--ae-caller", "-aec", 1, "[a]etitle: string",
                  fmt::format("set my calling AE title (default: {}", USER_APPLICATION_TITLE).c_str());
    cmd.addOption("--ae-pacs", "-aep", 1, "[a]etitle: string",
                  fmt::format("set called AE title of peer (default: {})", PACS_APPLICATION_TITLE).c_str());
    // TODO maybe add it in the future
    // cmd.addOption("--ae-receiver", "-aer", 1,
    //     "[a]etitle: string",
    //     fmt::format("set move destination AE title (default: {})", APPLICATION_TITLE).c_str());

    cmd.addSubGroup("port for incoming network associations:");
    cmd.addOption("--receive-port", "-port", 1, "[n]umber: integer", "port number for incoming associations");

    cmd.addGroup("input options:");
    cmd.addOption("--patient-list-file", "-plist", 1, "[c]sv: string path",
                  "text file with patient/study information to query/retrieve\nrequired order: PatientName,PatientID,StudyDate,(Modality)");
    cmd.addOption("--overwrite-modality-all", "-oma", "overwrite modalities of all read patient records");
    cmd.addOption("--overwrite-modality-missing", "-omm",
                  "overwrite modality in read patient records with missing modality values");

    cmd.addGroup("output options:");
    cmd.addOption("--output-directory", "-od", 1, R"([d]irectory: string (default: "./download")",
                  "write received data to directory d");

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
        }
        queryRetriever.m_callerAETitle = opt_aeCaller;

        if (cmd.findOption("--ae-pacs")) {
            app.checkValue(cmd.getValue(opt_aePacs));
        }
        queryRetriever.m_calledAETitle = opt_aePacs;

        if (cmd.findOption("--receive-port")) {
            app.checkValue(cmd.getValueAndCheckMinMax(opt_recievePort, 1, 65535));
            queryRetriever.m_retrievePort = OFstatic_cast(unsigned short, opt_recievePort);
        }

        if (cmd.findOption("--output-directory")) {
            app.checkValue(cmd.getValue(opt_outputDirectory));
            queryRetriever.m_outputDirectory = opt_outputDirectory;
        }

        if (cmd.findOption("--patient-list-file"))
            app.checkValue(cmd.getValue(opt_filepath));

        if (cmd.findOption("--overwrite-modality-all"))
            opt_overwriteModalities = true;

        if (cmd.findOption("--overwrite-modality-missing"))
            opt_overwriteModalities = false;

        OFLOG_DEBUG(mainLogger, rcsid.c_str() << OFendl);

        if (queryRetriever.m_retrievePort <= 0) {
            OFLOG_ERROR(mainLogger, "Missing parameter --receiver-port (-port) number");
            return EXITCODE_COMMANDLINE_SYNTAX_ERROR;
        }

        if (!queryRetriever.m_outputDirectory.empty()) {
            if (std::filesystem::exists(queryRetriever.m_outputDirectory))
                OFLOG_DEBUG(mainLogger, "Output directory exists: " << queryRetriever.m_outputDirectory.c_str());
            else {
                (void) std::filesystem::create_directories(queryRetriever.m_outputDirectory);
                OFLOG_DEBUG(mainLogger, "Created output directory: " << queryRetriever.m_outputDirectory.c_str());
            }

            if (!OFStandard::isWriteable(queryRetriever.m_outputDirectory.c_str())) {
                OFLOG_FATAL(mainLogger, "Specified output directory is not writable");
                return EXITCODE_CANNOT_WRITE_OUTPUT_FILE;
            }
        }

        if (opt_filepath == nullptr) {
            OFLOG_ERROR(mainLogger, "No text file specified");
            return EXITCODE_COMMANDLINE_SYNTAX_ERROR;
        }
    }

    const auto filepath = std::filesystem::absolute(opt_filepath);

    if (!std::filesystem::exists(filepath)) {
        OFLOG_ERROR(mainLogger, "Text file not found " << filepath.string().c_str());
        return EXITCODE_TEXT_FILE_ERROR;
    }

    fmt::print("READING TEXT FILE ------------------------- \n");
    std::vector<PatientRecord> recordList = readPatientRecords(filepath.string());

    if (!recordList.empty())
        fmt::print("Found {} records to query\n", recordList.size());
    else {
        OFLOG_FATAL(mainLogger, "Record map is empty");
        return EXITCODE_EMPTY_RECORD_MAP;
    }

    std::cout << R"(Modalities to query for (ex. CT, MR\CT, CR\US\XA): )";
    std::string modalitiesToQuery{};
    std::cin >> modalitiesToQuery;

    if (modalitiesToQuery.empty()) {
        OFLOG_ERROR(mainLogger, "No modalities to query for");
        return EXITCODE_NO_MODALITIES_SPECIFIED;
    }

    std::ranges::replace_if(modalitiesToQuery,
                            [](const char c) { return c == '/'; },
                            '\\');
    if (mainLogger.isEnabledFor(OFLogger::DEBUG_LOG_LEVEL))
        OFLOG_DEBUG(mainLogger, "Specified modalities contain forward slash, modified to backslash\n");

    //TODO: review this
    // if (opt_overwriteModalities == E_OverwriteModalites::OVERWRITE_MODALITIES_ALL)
    // {
    // 	for (auto &record : recordList)
    // 		record.m_modality = modalitiesToQuery;
    // }
    // else if (opt_overwriteModalities == E_OverwriteModalites::OVERWRITE_MODALITIES_MISSING)
    // {
    // 	for (auto &record : recordList)
    // 	{
    // 		if (record.m_modality.empty())
    // 			record.m_modality = modalitiesToQuery;
    // 	}
    // }

    OFCondition cond = queryRetriever.initializeNetwork();
    OFString temp_string;

    if (cond.bad()) {
        OFLOG_ERROR(mainLogger, "Cannot create network: " << DimseCondition::dump(temp_string, cond));
        OFLOG_ERROR(mainLogger, "Exiting program");
        return EXITCODE_CANNOT_INITIALIZE_NETWORK;
    }

    cond = queryRetriever.setupAssociation();

    if (cond.bad()) {
        OFLOG_ERROR(mainLogger, "Failed to setup association: " << DimseCondition::dump(temp_string, cond));
        OFLOG_ERROR(mainLogger, "Exiting program");
        return EXITCODE_CANNOT_NEGOTIATE_NETWORK;
    }

    const auto    time = std::chrono::system_clock::now();
    const auto    tt = std::chrono::system_clock::to_time_t(time);
    const std::tm tm = *std::localtime(&tt);

    const std::string missingStudiesFilename = fmt::format("missing-studies-{:%Y-%m-%d-%H-%M-%S}.txt", tm);

    fmt::ostream missingStudiesFile = fmt::output_file(missingStudiesFilename);
    missingStudiesFile.print("{:%Y-%m-%d %H:%M:%S}\n", tm);
    missingStudiesFile.print("PatientID, StudyDate\n");

    fmt::print("C-FIND -------------------------\n");
    cond = EC_Normal;

    for (auto &record: recordList) {
        cond = queryRetriever.performFindRequest(record, modalitiesToQuery, nullptr);
        const std::string msg = fmt::format("PatientID: {}, StudyDate: {}", record.m_id, record.m_study_date);

        if (record.m_uid_list.empty()) {
            fmt::print("{} - {}\n", msg, fmt::format(fg(fmt::color::red), "FAIL, STUDY NOT FOUND"));
            missingStudiesFile.print("{}, {} - NOT FOUND\n", record.m_id, record.m_study_date);
        } else {
            fmt::print("{} - {}\n", msg,
                       fmt::format(fg(fmt::color::green), "SUCCESS, {} study/ies", record.m_uid_list.size()));
            fmt::print("StudyInstanceUIDs: \n{}\n", record.m_uid_list);
        }
    }
    missingStudiesFile.close();
    fmt::print("Records of missing studies written to {}\n", missingStudiesFilename);

    fmt::print("C-MOVE -------------------------\n");
    cond = EC_Normal;
    for (const auto &record: recordList) {
        if (record.m_uid_list.empty()) {
            const std::string msg = fmt::format("PatientID: {}, StudyDate: {}", record.m_id, record.m_study_date);
            fmt::print("{} - {}\n", msg, fmt::format(fg(fmt::color::red), "FAIL, MISSING StudyInstanceUID"));
            continue;
        }
        cond = queryRetriever.performMoveRequest(record);
    }

    int exitCode = cond.good() ? 0 : 2;
    cond = queryRetriever.dropNetwork();

    if (cond.bad()) {
        OFLOG_FATAL(mainLogger, "Failed to drop network: " << DimseCondition::dump(temp_string, cond));
        if (!exitCode) exitCode = 3;
    }

    OFStandard::shutdownNetwork();

    return exitCode;
}
