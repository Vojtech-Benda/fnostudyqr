//
// Created by Vojtěch on 17.03.2025.
//
#include <filesystem>

#include "StudyQueryRetriever.hpp"
#include "fmt/color.h"


QueryRetriever::QueryRetriever()
	: m_net(nullptr) {}

QueryRetriever::~QueryRetriever() {
	this->dropNetwork();
}

OFCondition QueryRetriever::initializeNetwork() {
	const T_ASC_NetworkRole role = (this->m_retrievePort > 0) ? NET_ACCEPTORREQUESTOR : NET_REQUESTOR;
	return ASC_initializeNetwork(role, this->m_retrievePort, this->m_acseTimeout, &this->m_net);
}

OFCondition QueryRetriever::dropNetwork() {
	if (this->m_net)
		return ASC_dropNetwork(&this->m_net);
	return EC_Normal;
}

OFCondition QueryRetriever::setupAssociation() {
	OFString temp_string;

	OFCondition cond = ASC_createAssociationParameters(&this->m_params, ASC_DEFAULTMAXPDU, dcmConnectionTimeout.get());
	if (cond.bad()) {
		OFLOG_FATAL(qrLogger, "Creating association parameters failed: " << DimseCondition::dump(temp_string, cond));
		return cond;
	}

	ASC_setAPTitles(this->m_params, this->m_callerAETitle.c_str(), this->m_calledAETitle.c_str(), nullptr);

	cond = ASC_setTransportLayerType(this->m_params, this->m_secureConnection);
	if (cond.bad()) {
		OFLOG_FATAL(qrLogger, "Settings transport layer type failed: " << DimseCondition::dump(temp_string, cond));
		(void) ASC_destroyAssociationParameters(&this->m_params);
		return cond;
	}

	cond = ASC_setPresentationAddresses(this->m_params,
	                                    OFStandard::getHostName().c_str(),
	                                    fmt::format("{}:{}", this->m_calledIP, this->m_port).c_str());

	if (cond.bad()) {
		OFLOG_FATAL(qrLogger, "Adding presentation addresses failed: " << DimseCondition::dump(temp_string, cond));
		(void) ASC_destroyAssociationParameters(&this->m_params);
		return cond;
	}

	// EXS_LittleEndianExplicit
	cond = this->addPresentationContext(EXS_LittleEndianExplicit, 1, this->m_abstractSyntax.findSyntax);
	if (cond.bad()) {
		OFLOG_FATAL(qrLogger,
		            "Adding C-FIND presentation contexts failed: " << DimseCondition::dump(temp_string, cond));
		(void) ASC_destroyAssociationParameters(&this->m_params);
		return cond;
	}

	// EXS_LittleEndianExplicit
	cond = this->addPresentationContext(EXS_LittleEndianExplicit, 3, this->m_abstractSyntax.moveSyntax);
	if (cond.bad()) {
		OFLOG_FATAL(qrLogger,
		            "Adding C-MOVE presentation contexts failed: " << DimseCondition::dump(temp_string, cond));
		(void) ASC_destroyAssociationParameters(&this->m_params);
		return cond;
	}

	OFLOG_DEBUG(qrLogger,
	            "Request parameters: " << OFendl << ASC_dumpParameters(temp_string, this->m_params, ASC_ASSOC_RQ));

	OFLOG_INFO(qrLogger, "Requesting association");
	cond = ASC_requestAssociation(this->m_net, this->m_params, &this->m_assoc);
	if (cond.bad()) {
		if (cond == DUL_ASSOCIATIONREJECTED) {
			T_ASC_RejectParameters rejectParams{};
			ASC_getRejectParameters(this->m_params, &rejectParams);
			OFLOG_FATAL(qrLogger, "Association rejected:");
			OFLOG_FATAL(qrLogger, ASC_printRejectParameters(temp_string, &rejectParams));
			(void) ASC_destroyAssociation(&this->m_assoc);
			return cond;
		}
		OFLOG_FATAL(qrLogger, "Association request failed:");
		OFLOG_FATAL(qrLogger, DimseCondition::dump(temp_string, cond));
		(void) ASC_destroyAssociation(&this->m_assoc);
		return cond;
	}

	OFLOG_DEBUG(qrLogger,
	            "Association parameters negotiated: " << OFendl << ASC_dumpParameters(temp_string, this->m_params,
		            ASC_ASSOC_AC));

	if (ASC_countAcceptedPresentationContexts(this->m_params) == 0) {
		OFLOG_FATAL(qrLogger, "No acceptable presentation contexts");
		(void) ASC_destroyAssociation(&this->m_assoc);
		return NET_EC_NoAcceptablePresentationContexts;
	}

	OFLOG_INFO(qrLogger, "Association accepted (max send PDV:" << this->m_assoc->sendPDVLength << ")");
	return cond;
}

OFCondition QueryRetriever::removeAssociation(const OFCondition &queryCondition) {
	OFString    temp_string;
	OFCondition cond;
	if (queryCondition == EC_Normal) {
		cond = ASC_abortAssociation(this->m_assoc);
		OFLOG_INFO(qrLogger, "Aborting association");
		if (cond.bad()) {
			OFLOG_FATAL(qrLogger, "Association abort failed: " << DimseCondition::dump(temp_string, cond));
			(void) ASC_destroyAssociation(&this->m_assoc);
			return cond;
		}
	} else if (cond == DUL_PEERREQUESTEDRELEASE) {
		OFLOG_ERROR(qrLogger, "Protocol error: Peer requested release (Aborting)");
		OFLOG_INFO(qrLogger, "Aborting association");
		cond = ASC_abortAssociation(this->m_assoc);
		if (cond.bad()) {
			OFLOG_FATAL(qrLogger, "Association abort failed: " << DimseCondition::dump(temp_string, cond));
			(void) ASC_destroyAssociation(&this->m_assoc);
			return cond;
		}
	} else if (cond == DUL_PEERABORTEDASSOCIATION) {
		OFLOG_INFO(qrLogger, "Peer aborted association");
	} else {
		OFLOG_ERROR(qrLogger, "Find/Move SCU failed: " << DimseCondition::dump(temp_string, cond));
		if (cond.bad()) {
			OFLOG_FATAL(qrLogger, "Association abort failed: " << DimseCondition::dump(temp_string, cond));
			(void) ASC_destroyAssociation(&this->m_assoc);
			return cond;
		}
	}

	cond = ASC_destroyAssociation(&this->m_assoc);
	if (cond.bad())
		OFLOG_FATAL(qrLogger, "Destroying association failed: " << DimseCondition::dump(temp_string, cond));

	return cond;
}

OFCondition QueryRetriever::addPresentationContext(const E_TransferSyntax            outNetworkTransferSyntax,
                                                   const T_ASC_PresentationContextID presID,
                                                   const char *                      abstractSyntax) const {
	const char *transferSyntaxes[]  = {nullptr, nullptr, nullptr, nullptr};
	int         numTransferSyntaxes = 0;

	switch (outNetworkTransferSyntax) {
		case EXS_LittleEndianImplicit:
			// prefer Little Endian Implicit
			transferSyntaxes[0] = UID_LittleEndianImplicitTransferSyntax;
			numTransferSyntaxes = 1;
			break;
		case EXS_LittleEndianExplicit:
			// prefer Little Endian Explicit
			transferSyntaxes[0] = UID_LittleEndianExplicitTransferSyntax;
			transferSyntaxes[1] = UID_BigEndianExplicitTransferSyntax;
			transferSyntaxes[2] = UID_LittleEndianImplicitTransferSyntax;
			numTransferSyntaxes = 3;
			break;
		case EXS_BigEndianExplicit:
			// prefer Big Endian Explicit
			transferSyntaxes[0] = UID_BigEndianExplicitTransferSyntax;
			transferSyntaxes[1] = UID_LittleEndianExplicitTransferSyntax;
			transferSyntaxes[2] = UID_LittleEndianImplicitTransferSyntax;
			numTransferSyntaxes = 3;
			break;
		default:
			// prefer explicit transfer syntax
			if (gLocalByteOrder == EBO_LittleEndian) {
				transferSyntaxes[0] = UID_LittleEndianExplicitTransferSyntax;
				transferSyntaxes[1] = UID_BigEndianExplicitTransferSyntax;
			} else {
				transferSyntaxes[1] = UID_BigEndianExplicitTransferSyntax;
				transferSyntaxes[0] = UID_LittleEndianExplicitTransferSyntax;
			}
			transferSyntaxes[2] = UID_LittleEndianImplicitTransferSyntax;
			numTransferSyntaxes = 3;
			break;
	}

	return ASC_addPresentationContext(this->m_params, presID, abstractSyntax, transferSyntaxes, numTransferSyntaxes);
}

OFCondition QueryRetriever::performFindRequest(PatientRecord &patient_record, const std::string &modalities,
                                               QueryCallback *callback) const {
	T_DIMSE_C_FindRQ  request{};
	T_DIMSE_C_FindRSP response{};
	DcmFileFormat     fileformat;
	OFString          temp_string;

	DcmDataset *requestedDataset = fileformat.getDataset();
	requestedDataset->putAndInsertString(DCM_QueryRetrieveLevel, "STUDY");
	requestedDataset->putAndInsertString(DCM_PatientID, patient_record.m_id.c_str());
	requestedDataset->putAndInsertString(DCM_StudyDate, patient_record.m_study_date.c_str());
	requestedDataset->putAndInsertString(DCM_StudyInstanceUID, "");
	requestedDataset->putAndInsertString(DCM_NumberOfStudyRelatedInstances, "");
	requestedDataset->putAndInsertString(DCM_ModalitiesInStudy, modalities.c_str());

	const T_ASC_PresentationContextID presID = ASC_findAcceptedPresentationContextID(
		 this->m_assoc,
		 this->m_abstractSyntax.findSyntax
		);
	if (presID == 0) {
		OFLOG_FATAL(qrLogger, "No presentation context");
		return DIMSE_NOVALIDPRESENTATIONCONTEXTID;
	}

	std::strncpy(request.AffectedSOPClassUID,
	             this->m_abstractSyntax.findSyntax,
	             sizeof(request.AffectedSOPClassUID) - 1);
	request.AffectedSOPClassUID[sizeof(request.AffectedSOPClassUID) - 1] = '\0';

	request.DataSetType = DIMSE_DATASET_PRESENT;
	request.Priority    = DIMSE_PRIORITY_MEDIUM;

	constexpr int responseCount{0};
	int repeatCount{1};

	QueryDefaultCallback defaultCallback(this->m_cancelAfterNResponses);
	if (callback == nullptr) callback = &defaultCallback;
	callback->setAssociation(this->m_assoc);
	callback->setPresentationContextID(presID);

	OFCondition cond = EC_Normal;
	while (cond.good() && repeatCount--) {
		DcmDataset *statusDetail = nullptr;
		request.MessageID        = this->m_assoc->nextMsgID++;

		OFLOG_INFO(qrLogger, fmt::format("Sending FIND Request (MsgID {})\n", request.MessageID));
		cond = DIMSE_queryUser(this->m_assoc,
		                       presID,
		                       &request,
		                       requestedDataset,
		                       responseCount,
		                       progressCallback,
		                       callback,
		                       this->m_blockMode,
		                       this->m_dimseTimeout,
		                       &response,
		                       &statusDetail,
		                       patient_record.m_uid_list);

		if (cond.bad())
			OFLOG_ERROR(qrLogger, DimseCondition::dump(temp_string, cond).c_str());
	}
	return cond;
}

OFCondition QueryRetriever::performMoveRequest(const PatientRecord &patient_record) {
	OFCondition cond = EC_Normal;

	T_ASC_PresentationContextID presID;
	T_DIMSE_C_MoveRQ            request{};
	T_DIMSE_C_MoveRSP           response{};
	DIC_US                      msgID = this->m_assoc->nextMsgID;
	std::string                 sopClass;
	MoveCallbackInfo            moveCallbackInfo{};
	OFString                    temp_string;

	DcmFileFormat fileformat;
	DcmDataset *  requestedDataset = fileformat.getDataset();
	requestedDataset->putAndInsertString(DCM_PatientID, patient_record.m_id.c_str());

	presID = ASC_findAcceptedPresentationContextID(this->m_assoc, this->m_abstractSyntax.moveSyntax);
	if (presID == 0) return DIMSE_NOVALIDPRESENTATIONCONTEXTID;

	moveCallbackInfo.assoc  = this->m_assoc;
	moveCallbackInfo.presID = presID;
	request.MessageID       = msgID;
	std::strncpy(request.AffectedSOPClassUID,
	             this->m_abstractSyntax.moveSyntax,
	             sizeof(request.AffectedSOPClassUID));

	if (this->m_moveDestination.empty())
		ASC_getAPTitles(this->m_assoc->params,
		                request.MoveDestination,
		                sizeof(request.MoveDestination),
		                nullptr,
		                0,
		                nullptr,
		                0);
	else
		std::strncpy(request.MoveDestination, this->m_moveDestination.c_str(), sizeof(request.MoveDestination));

	request.Priority    = DIMSE_PRIORITY_MEDIUM;
	request.DataSetType = DIMSE_DATASET_PRESENT;

	if (qrLogger.isEnabledFor(OFLogger::DEBUG_LOG_LEVEL)) {
		OFLOG_INFO(qrLogger, "Sending Move Request");
		OFLOG_DEBUG(qrLogger, DIMSE_dumpMessage(temp_string, request, DIMSE_OUTGOING, nullptr, presID));
	} else {
		OFLOG_INFO(qrLogger, fmt::format("Sending Move Request (MsID: {})", msgID));
	}

	for (const auto &uid: patient_record.m_uid_list) {
		DcmDataset *responseIDs  = nullptr;
		DcmDataset *statusDetail = nullptr;

		requestedDataset->putAndInsertString(DCM_StudyInstanceUID, uid.c_str());
		OFLOG_INFO(qrLogger, "Request Identifiers: " << OFendl << DcmObject::PrintHelper(*fileformat.getDataset()));

		const std::string studyDirectory = fmt::format("{}/{}", this->m_outputDirectory, uid);

		if (std::filesystem::exists(studyDirectory))
			fmt::print("Study directory {} exits - {}\n",
			           studyDirectory,
			           fmt::format(fg(fmt::color::yellow), "OVERWRITING"));

		(void) std::filesystem::create_directories(studyDirectory);

		cond = DIMSE_moveUser_(this->m_assoc,
		                       presID,
		                       &request,
		                       requestedDataset,
		                       moveCallback,
		                       &moveCallbackInfo,
		                       this->m_blockMode,
		                       this->m_dimseTimeout,
		                       this->m_net,
		                       subOpCallback,
		                       nullptr,
		                       &response,
		                       this->m_cancelAfterNResponses,
		                       &statusDetail,
		                       &responseIDs,
		                       this->m_ignorePendingDatasets,
		                       studyDirectory);

		if (cond == EC_Normal) {
			if ((response.DimseStatus == STATUS_Success) ||
				(response.DimseStatus == STATUS_MOVE_Cancel_SubOperationsTerminatedDueToCancelIndication)) {
				// status is "success" or "cancel", nothing to do
				const std::string msg = fmt::format("PatientID: {}, StudyDate: {}, StudyUID: {}",
				                                    patient_record.m_id,
				                                    patient_record.m_study_date,
				                                    uid);
				fmt::print("{} - {}", msg, fmt::format(fg(fmt::color::green), "SUCCESS\n"));
			} else if (response.DimseStatus == STATUS_MOVE_Warning_SubOperationsCompleteOneOrMoreFailures) {
				if (cmove_status_code == EXITCODE_NO_ERROR)
					cmove_status_code = EXITCODE_CMOVE_WARNING;
				OFLOG_WARN(qrLogger,
				           "Move response with warning status (" << DU_cmoveStatusString(response.DimseStatus) << ")");
			} else {
				cmove_status_code = EXITCODE_CMOVE_ERROR;
				OFLOG_WARN(qrLogger,
				           "Move response with error status (" << DU_cmoveStatusString(response.DimseStatus) << ")");
			}

			if (qrLogger.isEnabledFor(OFLogger::DEBUG_LOG_LEVEL)) {
				OFLOG_INFO(qrLogger, "Received Final Move Response");
				OFLOG_DEBUG(qrLogger, DIMSE_dumpMessage(temp_string, response, DIMSE_INCOMING));
				if (responseIDs != nullptr)
					OFLOG_DEBUG(qrLogger, "Response Identifiers:" << OFendl << DcmObject::PrintHelper(*responseIDs));
				else
					OFLOG_INFO(qrLogger,
				           "Received Final Move Response (" << DU_cmoveStatusString(response.DimseStatus) << ")");
			}
		} else {
			OFLOG_ERROR(qrLogger, "Move Request Failed: " << DimseCondition::dump(temp_string, cond));
		}

		if (statusDetail != nullptr) {
			OFLOG_DEBUG(qrLogger, "Status Detail:" << OFendl << DcmObject::PrintHelper(*statusDetail));
			delete statusDetail;
		}
	}
	return cond;
}

QueryCallback::QueryCallback() : m_assoc(nullptr),
                                 m_presID(0) {}

void QueryCallback::setAssociation(T_ASC_Association *assoc) {
	this->m_assoc = assoc;
}

void QueryCallback::setPresentationContextID(const T_ASC_PresentationContextID pres_id) {
	this->m_presID = pres_id;
}

QueryDefaultCallback::QueryDefaultCallback(int cancelAfterNResponses) : m_cancelAfterNResponses(cancelAfterNResponses) {
}

void QueryDefaultCallback::callback(T_DIMSE_C_FindRQ *        request,
                                    int                       response_count,
                                    T_DIMSE_C_FindRSP *       response,
                                    DcmDataset *              response_identifiers,
                                    std::vector<std::string> &uid_list) {
	if (DCM_dcmnetLogger.isEnabledFor(OFLogger::DEBUG_LOG_LEVEL)) {
		OFString temp_string;
		DCMNET_INFO("Received Find Response " << response_count);
		DCMNET_DEBUG(DIMSE_dumpMessage(temp_string, *response, DIMSE_INCOMING));
		if (qrLogger.isEnabledFor(OFLogger::DEBUG_LOG_LEVEL))
			DCMNET_DEBUG("Response Identifiers:" << OFendl << DcmObject::PrintHelper(*response_identifiers));
	} else if (qrLogger.isEnabledFor(OFLogger::INFO_LOG_LEVEL)) {
		OFLOG_INFO(qrLogger, "---------------------------");
		OFLOG_INFO(qrLogger,
		           "Find Response: " << response_count << " (" << DU_cfindStatusString(response->DimseStatus) << ")");
		OFLOG_INFO(qrLogger, DcmObject::PrintHelper(*response_identifiers));
	} else {
		DCMNET_INFO("Received Find Response " << response_count << " (" << DU_cfindStatusString(response->DimseStatus)
		            << ")");
	}

	OFString studyuid;
	if (response_identifiers->findAndGetOFString(DCM_StudyInstanceUID, studyuid).good()) {
		if (!studyuid.empty())
			uid_list.emplace_back(studyuid.c_str());
	}

	if (this->m_cancelAfterNResponses == response_count) {
		DCMNET_INFO("Sending Cancel Request (MsgID " << request->MessageID << ", PresID " << OFstatic_cast(unsigned int,
			            this->m_presID) << ")");
		OFCondition cond = DIMSE_sendCancelRequest(this->m_assoc, this->m_presID, request->MessageID);
		if (cond.bad()) {
			OFString temp_string;
			DCMNET_ERROR("Cancel Request Failed: " << DimseCondition::dump(temp_string, cond));
		}
	}
}

static void progressCallback(void *                    callback_data,
                             T_DIMSE_C_FindRQ *        request,
                             int                       response_count,
                             T_DIMSE_C_FindRSP *       response,
                             DcmDataset *              response_identifiers,
                             std::vector<std::string> &uid_list) {
	QueryCallback *callback = OFreinterpret_cast(QueryCallback*, callback_data);
	if (callback)
		callback->callback(request, response_count, response, response_identifiers, uid_list);
}

OFCondition DIMSE_queryUser(T_ASC_Association *         assoc,
                            T_ASC_PresentationContextID pres_id,
                            T_DIMSE_C_FindRQ *          request,
                            DcmDataset *                request_identifiers,
                            int                         response_count,
                            DIMSE_QueryUserCallback     callback, void *callback_data,
                            T_DIMSE_BlockingMode        block_mode,
                            int                         timeout,
                            T_DIMSE_C_FindRSP *         response,
                            DcmDataset **               status_detail,
                            std::vector<std::string> &  uid_list) {
	T_DIMSE_Message req{}, rsp{};
	DIC_US          msgID;
	DcmDataset *    rspIDs = nullptr;
	DIC_US          status = STATUS_FIND_Pending_MatchesAreContinuing;

	if (request_identifiers == nullptr) return DIMSE_NULLKEY;

	req.CommandField     = DIMSE_C_FIND_RQ;
	request->DataSetType = DIMSE_DATASET_PRESENT;
	req.msg.CFindRQ      = *request;

	msgID = request->MessageID;

	OFCondition cond = DIMSE_sendMessageUsingMemoryData(assoc,
	                                                    pres_id,
	                                                    &req,
	                                                    nullptr,
	                                                    request_identifiers,
	                                                    nullptr,
	                                                    nullptr);

	if (cond.bad()) return cond;

	while (cond == EC_Normal && DICOM_PENDING_STATUS(status)) {
		if (rspIDs != nullptr) {
			delete rspIDs;
			rspIDs = nullptr;
		}

		// try to recieve C-FIND-RSP over the network
		cond = DIMSE_receiveCommand(assoc, block_mode, timeout, &pres_id, &rsp, status_detail);
		if (cond.bad())
			return cond;

		if (rsp.CommandField != DIMSE_C_FIND_RSP) {
			std::string buf{
				fmt::format("DIMSE: Unexpected Response Command Field: {:#04x}",
				            static_cast<unsigned>(rsp.CommandField))
			};
			return makeDcmnetCondition(DIMSEC_UNEXPECTEDRESPONSE, OF_error, buf.c_str());
		}

		*response = rsp.msg.CFindRSP;
		if (response->MessageIDBeingRespondedTo != msgID) {
			std::string buf{
				fmt::format("DIMSE: Unexpected Response MsgID: {} (expected: {})",
				            response->MessageIDBeingRespondedTo,
				            msgID)
			};
			return makeDcmnetCondition(DIMSEC_UNEXPECTEDRESPONSE, OF_error, buf.c_str());
		}

		status = response->DimseStatus;
		response_count++;

		switch (status) {
			case STATUS_FIND_Pending_MatchesAreContinuing:
			case STATUS_FIND_Pending_WarningUnsupportedOptionalKeys:
				if (*status_detail != nullptr) {
					DCMNET_WARN(DIMSE_warn_str(assoc) << "findUser: Pending with statusDetail, ignoring detail");
					delete*status_detail;
					*status_detail = nullptr;
				}

				if (response->DataSetType == DIMSE_DATASET_NULL) {
					DCMNET_WARN(DIMSE_warn_str(assoc) << "findUser: Status Pending, but DataSetType==nullptr");
					DCMNET_WARN(DIMSE_warn_str(assoc) << "Assuming response identifiers are present");
				}

				cond = DIMSE_receiveDataSetInMemory(assoc, block_mode, timeout, &pres_id, &rspIDs, nullptr, nullptr);
				if (cond != EC_Normal)
					return cond;

				if (callback)
					callback(callback_data, request, response_count, response, rspIDs, uid_list);

				break;
			case STATUS_FIND_Success:
				if (response->DataSetType != DIMSE_DATASET_NULL) {
					DCMNET_WARN(DIMSE_warn_str(assoc) << "findUser: Status Success, but DataSetType!=nullptr");
					DCMNET_WARN(DIMSE_warn_str(assoc) << "Assuming no response identifiers are present");
				}
				break;
			default:
				if (response->DataSetType != DIMSE_DATASET_NULL) {
					DCMNET_WARN(DIMSE_warn_str(assoc) << "findUser: Status " << DU_cfindStatusString(status) <<
					            ", but DataSetType != nullptr");
					DCMNET_WARN(DIMSE_warn_str(assoc) << "Assuming no response identifiers are present");
				}
				break;
		} // switch end
	}     // while end

	return cond;
}

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
                            const std::string &          output_directory) {
	T_DIMSE_Message    req{}, rsp{};
	DIC_US             msgID;
	int                responseCount{0};
	T_ASC_Association *subAssoc = nullptr;
	DIC_US             status   = STATUS_MOVE_Pending_SubOperationsAreContinuing;
	OFBool             firstLoop{OFTrue};

	if (request_identifiers == nullptr)
		return DIMSE_NULLKEY;

	req.CommandField     = DIMSE_C_MOVE_RQ;
	request->DataSetType = DIMSE_DATASET_PRESENT;
	req.msg.CMoveRQ      = *request;
	msgID                = request->MessageID;

	OFCondition cond = DIMSE_sendMessageUsingMemoryData(assoc,
	                                                    pres_id,
	                                                    &req,
	                                                    nullptr,
	                                                    request_identifiers,
	                                                    nullptr,
	                                                    nullptr);
	if (cond != EC_Normal)
		return cond;

	// receive responses
	OFTimer timer;
	while (cond == EC_Normal && status == STATUS_MOVE_Pending_SubOperationsAreContinuing) {
		// int readable = selectReadable(assoc, net, subAssoc, block_mode, dimse_timeout);

		switch (selectReadable(assoc, net, subAssoc, block_mode, dimse_timeout)) {
			case 0:
				// none are readable, timeout
				if ((block_mode == DIMSE_BLOCKING) || firstLoop)
					firstLoop = OFFalse;
				else if ((block_mode == DIMSE_NONBLOCKING) && (timer.getDiff() > dimse_timeout)) {
					DCMNET_DEBUG(fmt::format("Timeout of {} seconds elapsed while waiting for C-MOVE Responses",
						             dimse_timeout));
					return DIMSE_NODATAAVAILABLE;
				}
				continue; // continue with main loop
			case 1:
				// main association readable
				firstLoop = OFFalse;
				break;
			case 2:
				// net/subAssoc readable
				if (sub_op_callback) {
					sub_op_callback(sub_op_callback_data, net, &subAssoc, output_directory, block_mode, dimse_timeout);
				}
				firstLoop = OFFalse;
				continue; // continue with main loop
		}

		cond = DIMSE_receiveCommand(assoc, block_mode, dimse_timeout, &pres_id, &rsp, status_detail);
		if (cond != EC_Normal)
			return cond;

		if (rsp.CommandField != DIMSE_C_MOVE_RSP) {
			std::string buf{
				fmt::format("DIMSE: Unexpected Response Command Field: {:#04x}",
				            static_cast<unsigned>(rsp.CommandField))
			};
			fmt::print("{}", buf);
			return makeDcmnetCondition(DIMSEC_UNEXPECTEDRESPONSE, OF_error, buf.c_str());
		}

		*response = rsp.msg.CMoveRSP;

		if (response->MessageIDBeingRespondedTo != msgID) {
			std::string buf{
				fmt::format("DIMSE: Unexpected Response MsgID : {} (expected {})",
				            response->MessageIDBeingRespondedTo,
				            msgID)
			};
			fmt::print("{}", buf);
			return makeDcmnetCondition(DIMSEC_UNEXPECTEDRESPONSE, OF_error, buf.c_str());
		}
		status = response->DimseStatus;
		responseCount++;

		switch (status) {
			case STATUS_MOVE_Pending_SubOperationsAreContinuing:
				if (*status_detail != nullptr) {
					DCMNET_WARN(DIMSE_warn_str(assoc) << "moveUser: Pending with statusDetail, ignoring detail");
					delete *status_detail;
					*status_detail = nullptr;
				}

				if (response->DataSetType != DIMSE_DATASET_NULL) {
					DCMNET_WARN(DIMSE_warn_str(assoc) << "moveUser: Status Pending, but DataSetType != NULL");
					if (!ignore_pending_datasets) {
						/*
						Some systems send a "wrong/illegal" dataset following C-MOVE-RSP messages with pending status.
						This needs to be handled too. @ dcmtk/dcmnet/dimmove.cc line 241
						*/
						DCMNET_WARN(DIMSE_warn_str(assoc) << "Reading but ignoring response identifier set");
						DcmDataset *tempset = nullptr;
						cond                = DIMSE_receiveDataSetInMemory(assoc,
						                                                   block_mode,
						                                                   dimse_timeout,
						                                                   &pres_id,
						                                                   &tempset,
						                                                   nullptr,
						                                                   nullptr);
						delete tempset;
						if (cond != EC_Normal) return cond;
					} else {
						DCMNET_WARN(DIMSE_warn_str << "Assuming NO response identifiers are present");
					}
				}

				if (move_callback)
					move_callback(move_callback_data, request, responseCount, cancel_after_n_responses, response);
				break;
			default:
				if (response->DataSetType != DIMSE_DATASET_NULL) {
					cond = DIMSE_receiveDataSetInMemory(assoc,
					                                    block_mode,
					                                    dimse_timeout,
					                                    &pres_id,
					                                    response_ids,
					                                    nullptr,
					                                    nullptr);
					if (cond != EC_Normal)
						return cond;
				}
				break;
		}

		timer.reset();
		while (subAssoc != nullptr) {
			if (sub_op_callback)
				sub_op_callback(sub_op_callback_data, net, &subAssoc, output_directory, block_mode, dimse_timeout);
		}
	}
	return cond;
}
