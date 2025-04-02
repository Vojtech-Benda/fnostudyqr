//
// Created by Vojtěch on 17.03.2025.
//

#ifndef STUDYQUERYRETRIEVER_HPP
#define STUDYQUERYRETRIEVER_HPP

#include <string>

#include "dcmtk/config/osconfig.h"
#include "dcmtk/ofstd/ofstd.h"
#include "dcmtk/ofstd/ofcond.h"
#include "dcmtk/ofstd/oftimer.h"
#include "dcmtk/ofstd/ofexit.h"
#include "dcmtk/dcmdata/dcxfer.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/dcmdata/dcdeftag.h"
#include "dcmtk/dcmdata/dcmetinf.h"
#include "dcmtk/dcmnet/dicom.h"
#include "dcmtk/dcmnet/dimse.h"
#include "dcmtk/dcmnet/diutil.h"
#include "dcmtk/oflog/oflog.h"

#include "fmt/format.h"

#include "PatientRecord.hpp"
#include "Callbacks.hpp"

constexpr int EXITCODE_EMPTY_RECORD_MAP        = 10;
constexpr int EXITCODE_NO_MODALITIES_SPECIFIED = 11;
constexpr int EXITCODE_TEXT_FILE_ERROR         = 12;

constexpr int EXITCODE_CANNOT_INITIALIZE_NETWORK      = 60;
constexpr int EXITCODE_CANNOT_NEGOTIATE_NETWORK       = 61;
constexpr int EXITCODE_CANNOT_CREATE_ASSOC_PARAMETERS = 65;
constexpr int EXITCODE_NO_PRESENTATION_CONTEXT        = 66;
constexpr int EXITCODE_CANNOT_CLOSE_ASSOCIATION       = 67;
constexpr int EXITCODE_CMOVE_WARNING                  = 68;
constexpr int EXITCODE_CMOVE_ERROR                    = 69;

static int      cmove_status_code = EXITCODE_NO_ERROR;
static OFLogger qrLogger          = OFLog::getLogger("dcmtk.apps.studyQRlogger");

class DcmDataset;
class DcmTransportLayer;
class OFConsoleApplication;
struct T_ASC_Association;
struct T_ASC_Parameters;
struct T_DIMSE_C_FindRQ;
struct T_DIMSE_C_FindRSP;
class QueryCallback;

struct QuerySyntax {
	const char *findSyntax;
	const char *moveSyntax;
};


class QueryRetriever {
public:
	QueryRetriever();

	virtual ~QueryRetriever();

	OFCondition initializeNetwork();

	OFCondition dropNetwork();

	OFCondition setupAssociation();

	OFCondition removeAssociation(const OFCondition &queryCondition);

	OFCondition addPresentationContext(E_TransferSyntax            outNetworkTransferSyntax,
	                                   T_ASC_PresentationContextID presID,
	                                   const char *                abstractSyntax) const;

	OFCondition performFindRequest(PatientRecord &    patient_record,
	                               const std::string &modalities,
	                               QueryCallback *    callback) const;

	OFCondition performMoveRequest(const PatientRecord &patient_record);

	unsigned short m_port{0};    // tcp/ip port of peer
	unsigned short m_retrievePort{0};
	std::string    m_callerIP{}; // ip address of application user
	std::string    m_calledIP{}; // hostname of PACS (DICOM peer)
	std::string    m_callerAETitle{};   // aec
	std::string    m_calledAETitle{};   // aep
	std::string    m_receiverAETitle{}; // aer
	std::string    m_outputDirectory{};
	std::string    m_studyDirectory{};

private:
	T_ASC_Network *    m_net{nullptr};
	T_ASC_Association *m_assoc{nullptr};
	T_ASC_Parameters * m_params{nullptr};
	OFBool             m_secureConnection{OFFalse};
	QuerySyntax        m_abstractSyntax = {
		UID_FINDStudyRootQueryRetrieveInformationModel,
		UID_MOVEStudyRootQueryRetrieveInformationModel
	};
	T_DIMSE_BlockingMode m_blockMode{DIMSE_BLOCKING};
	int                  m_cancelAfterNResponses{-1};
	OFBool               m_ignorePendingDatasets{OFTrue};
	int                  m_acseTimeout{30};
	int                  m_dimseTimeout{0};
};

class QueryCallback {
public:
	QueryCallback();

	virtual ~QueryCallback() = default;

	virtual void callback(T_DIMSE_C_FindRQ *        request,
	                      int                       responseCount,
	                      T_DIMSE_C_FindRSP *       response,
	                      DcmDataset *              responseIdentifiers,
	                      std::vector<std::string> &uid_list) = 0;

	void setAssociation(T_ASC_Association *assoc);

	void setPresentationContextID(T_ASC_PresentationContextID pres_id);

protected:
	T_ASC_Association *         m_assoc;
	T_ASC_PresentationContextID m_presID;
};

class QueryDefaultCallback final : public QueryCallback {
public:
	explicit QueryDefaultCallback(int cancelAfterNResponses);

	~QueryDefaultCallback() override = default;

	void callback(T_DIMSE_C_FindRQ *        request,
	              int                       response_count,
	              T_DIMSE_C_FindRSP *       response,
	              DcmDataset *              response_identifiers,
	              std::vector<std::string> &uid_list) override;

private:
	const int m_cancelAfterNResponses{0};
};


static void progressCallback(void *                    callback_data,
                             T_DIMSE_C_FindRQ *        request,
                             int                       response_count,
                             T_DIMSE_C_FindRSP *       response,
                             DcmDataset *              response_identifiers,
                             std::vector<std::string> &uid_list);

typedef void (*DIMSE_QueryUserCallback)(void *                    callbackData,
                                        T_DIMSE_C_FindRQ *        request,
                                        int                       responseCount,
                                        T_DIMSE_C_FindRSP *       response,
                                        DcmDataset *              responseIdentifiers,
                                        std::vector<std::string> &uid_list);

OFCondition DIMSE_queryUser(T_ASC_Association *         assoc,
                            T_ASC_PresentationContextID pres_id,
                            T_DIMSE_C_FindRQ *          request,
                            DcmDataset *                request_identifiers,
                            int                         response_count,
                            DIMSE_QueryUserCallback     callback,
                            void *                      callback_data,
                            T_DIMSE_BlockingMode        block_mode,
                            int                         timeout,
                            T_DIMSE_C_FindRSP *         response,
                            DcmDataset **               status_detail,
                            std::vector<std::string> &  uid_list);

OFCondition DIMSE_moveUser_(T_ASC_Association *          assoc,
                            T_ASC_PresentationContextID  pres_id,
                            T_DIMSE_C_MoveRQ *           request,
                            DcmDataset *                 request_identifiers,
                            DIMSE_MoveUserCallback_      move_callback,
                            void *                       move_callback_data,
                            T_DIMSE_BlockingMode         block_mode,
                            int                          dimse_timeout,
                            T_ASC_Network *              net,
                            DIMSE_SubOpProviderCallback_ sub_op_callback,
                            void *                       sub_op_callback_data,
                            T_DIMSE_C_MoveRSP *          response,
                            int                          cancel_after_n_responses,
                            DcmDataset **                status_detail,
                            DcmDataset **                response_ids,
                            OFBool                       ignore_pending_datasets,
                            const std::string &          output_directory);



#endif //STUDYQUERYRETRIEVER_HPP
