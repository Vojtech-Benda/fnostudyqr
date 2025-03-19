//
// Created by Vojtěch on 17.03.2025.
//

#include "Callbacks.hpp"

void moveCallback(void *             move_callback_data,
                  T_DIMSE_C_MoveRQ * request,
                  int                response_count,
                  int                cancel_after_n_responses,
                  T_DIMSE_C_MoveRSP *response) {
	OFCondition       cond = EC_Normal;
	MoveCallbackInfo *moveCallbackInfo{};
	OFString          temp_string;
	moveCallbackInfo = OFstatic_cast(MoveCallbackInfo*, move_callback_data);


	if (DCM_dcmnetLogger.isEnabledFor(OFLogger::DEBUG_LOG_LEVEL)) {
		DCMNET_INFO("Received Move Response " << response_count << " (" << DU_cmoveStatusString(response->DimseStatus)
		            << ")");
		//DCMNET_INFO("Received Move Response " << response_count);
		DCMNET_DEBUG(DIMSE_dumpMessage(temp_string, *response, DIMSE_INCOMING));
	}

	if (cancel_after_n_responses == response_count) {
		DCMNET_INFO("Sending cancel request (MsgID " << request->MessageID << ", PresID " << OFstatic_cast(unsigned int,
			            moveCallbackInfo->presID) << ")");
		cond = DIMSE_sendCancelRequest(moveCallbackInfo->assoc, moveCallbackInfo->presID, request->MessageID);
		if (cond != EC_Normal)
			DCMNET_ERROR("Cancel Request Failed: " << DimseCondition::dump(temp_string, cond));
	}
}

void subOpMoveCallback(void *,
                       T_ASC_Network *            assoc_net,
                       T_ASC_Association **       sub_assoc,
                       std::string &              output_directory,
                       const T_DIMSE_BlockingMode block_mode,
                       int                        dimse_timeout) {
	if (assoc_net == nullptr)
		return;

	if (*sub_assoc == nullptr)
		acceptSubAssoc(assoc_net, sub_assoc);
	else
		subOpSCP(sub_assoc, output_directory, block_mode, dimse_timeout);
}

void subOpCallback(void * /* subOpCallbackData */,
                   T_ASC_Network *            assoc_net,
                   T_ASC_Association **       sub_assoc,
                   const std::string &        output_directory,
                   const T_DIMSE_BlockingMode block_mode,
                   int                        dimse_timeout) {
	if (assoc_net == nullptr)
		return;

	if (*sub_assoc == nullptr) {
		acceptSubAssoc(assoc_net, sub_assoc);
	} else {
		subOpSCP(sub_assoc, output_directory, block_mode, dimse_timeout);
	}
}

OFCondition acceptSubAssoc(T_ASC_Network *assoc_net, T_ASC_Association **assoc) {
	const char *knownAbstractSyntaxes[] = {UID_VerificationSOPClass};

	// TODO: assuming only these transfer syntaxes
	// may have to change related to opt_in_networkTransferSyntax, opt_acceptAllXfers and gLocalByteOrder
	// dcmtk/dcmnet/movescu.cc line 1010
	const char *transferSyntaxes[] = {nullptr, nullptr, nullptr};

	int      numTransferSyntaxes{0};
	OFString temp_string;

	OFCondition cond = ASC_receiveAssociation(assoc_net, assoc, ASC_DEFAULTMAXPDU);
	if (cond.good()) {
		DCMNET_INFO("Sub-Association Received");
		DCMNET_DEBUG("Parameters:" << OFendl << ASC_dumpParameters(temp_string, (*assoc)->params, ASC_ASSOC_RQ));
		if (gLocalByteOrder == EBO_LittleEndian) {
			transferSyntaxes[0] = UID_LittleEndianExplicitTransferSyntax;
			transferSyntaxes[1] = UID_BigEndianExplicitTransferSyntax;
		} else {
			transferSyntaxes[0] = UID_BigEndianExplicitTransferSyntax;
			transferSyntaxes[1] = UID_LittleEndianExplicitTransferSyntax;
		}

		transferSyntaxes[2] = UID_LittleEndianImplicitTransferSyntax;
		numTransferSyntaxes = 3;

		// accept the verification SOP class if presented, @ dcmtk/dcmnet/movescu.cc line 1246
		cond = ASC_acceptContextsWithPreferredTransferSyntaxes((*assoc)->params,
		                                                       knownAbstractSyntaxes,
		                                                       std::size(knownAbstractSyntaxes),
		                                                       transferSyntaxes,
		                                                       numTransferSyntaxes);

		if (cond.good()) {
			cond = ASC_acceptContextsWithPreferredTransferSyntaxes((*assoc)->params,
			                                                       dcmAllStorageSOPClassUIDs,
			                                                       numberOfDcmAllStorageSOPClassUIDs,
			                                                       transferSyntaxes,
			                                                       numTransferSyntaxes);
		}
	}

	if (cond.good())
		cond = ASC_acknowledgeAssociation(*assoc);
	if (cond.good()) {
		DCMNET_INFO("Sub-Association Acknowledged (Max Send PDV: " << (*assoc)->sendPDVLength << ")");

		if (ASC_countAcceptedPresentationContexts((*assoc)->params) == 0)
			DCMNET_INFO(" (but no valid presentation contexts)");
		DCMNET_DEBUG(ASC_dumpParameters(temp_string, (*assoc)->params, ASC_ASSOC_AC));
	} else {
		DCMNET_ERROR(DimseCondition::dump(temp_string, cond));
		ASC_dropAssociation(*assoc);
		ASC_destroyAssociation(assoc);
	}
	return cond;
}

OFCondition subOpSCP(T_ASC_Association ** sub_assoc,
                     const std::string &  output_directory,
                     T_DIMSE_BlockingMode block_mode,
                     int                  dimse_timeout) {
	T_DIMSE_Message             message{};
	T_ASC_PresentationContextID presID;

	if (!ASC_dataWaiting(*sub_assoc, 0))
		return DIMSE_NODATAAVAILABLE;

	OFCondition cond = DIMSE_receiveCommand(*sub_assoc, block_mode, dimse_timeout, &presID, &message, nullptr);

	if (cond == EC_Normal) {
		switch (message.CommandField) {
			case DIMSE_C_ECHO_RQ:
				cond = echoSCP(*sub_assoc, &message, presID);
				break;
			case DIMSE_C_STORE_RQ:
				cond = storeSCP(*sub_assoc,
				                &message,
				                presID,
				                output_directory,
				                block_mode,
				                dimse_timeout);
				break;
			default:
				OFString temp_string;
				cond = DIMSE_BADCOMMANDTYPE;
				DCMNET_ERROR(fmt::format("Expected C-ECHO or C-STORE request but received DIMSE command {:#04x}",
					             static_cast<unsigned>(message.CommandField)));
				DCMNET_DEBUG(DIMSE_dumpMessage(temp_string, message, DIMSE_INCOMING, nullptr, presID));
				break;
		}
	}

	if (cond == DUL_PEERREQUESTEDRELEASE) {
		cond = ASC_acknowledgeRelease(*sub_assoc);
		ASC_dropSCPAssociation(*sub_assoc);
		ASC_destroyAssociation(sub_assoc);
		return cond;
	}
	if (cond == DUL_PEERABORTEDASSOCIATION) {}
	else if (cond != EC_Normal) {
		OFString temp_string;
		DCMNET_ERROR("DIMSE failure (aborting sub-association): " << DimseCondition::dump(temp_string, cond));
		cond = ASC_abortAssociation(*sub_assoc);
	}

	if (cond != EC_Normal) {
		ASC_dropAssociation(*sub_assoc);
		ASC_destroyAssociation(sub_assoc);
	}
	return cond;
}

void storeSCPCallback(
	/* in */
	void *                 store_callback_data,
	T_DIMSE_StoreProgress *progress,
	T_DIMSE_C_StoreRQ *    in_request,
	char *                 filename,
	DcmDataset **          in_dataset,
	/* out */
	T_DIMSE_C_StoreRSP *out_response,
	DcmDataset **       status_detail) {
	DIC_UI sopClass;
	DIC_UI sopInstance;

	OFLogger progressLogger = OFLog::getLogger("dcmtk.apps.fnostudyqr.progress");
	if (progressLogger.getChainedLogLevel() == OFLogger::DEBUG_LOG_LEVEL) {
		switch (progress->state) {
			case DIMSE_StoreBegin:
				COUT << "RECV: ";
				break;
			case DIMSE_StoreEnd:
				COUT << OFendl;
				break;
			default:
				COUT << ".";
				break;
		}
		COUT.flush();
	}

	if (progress->state == DIMSE_StoreEnd) {
		*status_detail = nullptr;
		if ((in_dataset != nullptr) && (*in_dataset != nullptr)) {
			StoreCallbackData *storecbdata = OFstatic_cast(StoreCallbackData *, store_callback_data);
			const OFString     ofname(storecbdata->m_filename);
			if (OFStandard::fileExists(ofname))
				DCMNET_WARN("DICOM file already exists, overwriting: " << ofname);

			const E_TransferSyntax xfer = (*in_dataset)->getOriginalXfer();
			//if (xfer == EXS_Unknown)
			//	xfer = (*in_dataset)->getOriginalXfer();
			OFCondition cond = storecbdata->m_fileformat->saveFile(ofname, xfer);

			if (cond.bad()) {
				DCMNET_ERROR("Cannot write DICOM file: " << ofname);
				out_response->DimseStatus = STATUS_STORE_Refused_OutOfResources;
				OFStandard::deleteFile(ofname);
			}

			// sanity checking matching SOP Class and SOP Instance UIDs
			if (out_response->DimseStatus == STATUS_Success) {
				if (!DU_findSOPClassAndInstanceInDataSet(*in_dataset,
				                                         sopClass,
				                                         sizeof(sopClass),
				                                         sopInstance,
				                                         sizeof(sopInstance))) {
					// FIXME: replace with OFLOG
					DCMNET_ERROR("bad DICOM file: " << filename);
					out_response->DimseStatus = STATUS_STORE_Error_CannotUnderstand;
				} else if (strcmp(sopClass, in_request->AffectedSOPClassUID) != 0) {
					out_response->DimseStatus = STATUS_STORE_Error_DataSetDoesNotMatchSOPClass;
				}
			}
		}
	}
}

OFCondition storeSCP(T_ASC_Association *         assoc,
                     T_DIMSE_Message *           message,
                     T_ASC_PresentationContextID pres_id,
                     const std::string &         output_directory,
                     T_DIMSE_BlockingMode        block_mode,
                     int                         dimse_timeout) {
	OFCondition        cond    = EC_Normal;
	T_DIMSE_C_StoreRQ *request = &message->msg.CStoreRQ;

	char filename[2048];
	OFStandard::snprintf(filename,
	                     sizeof(filename),
	                     "%s.%s",
	                     dcmSOPClassUIDToModality(request->AffectedSOPClassUID),
	                     request->AffectedSOPInstanceUID);
	OFStandard::sanitizeFilename(filename);
	OFString ofname;
	OFStandard::combineDirAndFilename(ofname, output_directory, filename, OFTrue);

	StoreCallbackData storeCallbackData;
	storeCallbackData.m_assoc    = assoc;
	storeCallbackData.m_filename = ofname;
	DcmFileFormat fileformat;
	storeCallbackData.m_fileformat = &fileformat;

	if (assoc && assoc->params) {
		const char *aet = assoc->params->DULparams.calledAPTitle;
		if (aet)
			fileformat.getMetaInfo()->putAndInsertString(DCM_SourceApplicationEntityTitle, aet);
	}

	cond = DIMSE_storeProvider(assoc,
	                           pres_id,
	                           request,
	                           ofname.c_str(),
	                           OFTrue /* write file with meta header */,
	                           nullptr,
	                           storeSCPCallback,
	                           OFreinterpret_cast(void *, &storeCallbackData),
	                           block_mode,
	                           dimse_timeout);

	if (cond.bad()) {
		OFString temp_string;
		DCMNET_ERROR("Store SCP Failed: " << DimseCondition::dump(temp_string, cond));
		if (strcmp(filename, NULL_DEVICE_NAME) != 0)
			OFStandard::deleteFile(ofname);
	}

	return cond;
}


OFCondition echoSCP(T_ASC_Association *assoc,
						   T_DIMSE_Message *message,
						   const T_ASC_PresentationContextID pres_id)
{
	OFString temp_string;
	T_DIMSE_C_EchoRQ *request = &message->msg.CEchoRQ;
	if (DCM_dcmnetLogger.isEnabledFor(OFLogger::DEBUG_LOG_LEVEL))
	{
		DCMNET_INFO("Received ECHO Request (MsgID " << request->MessageID << ")");
		DCMNET_DEBUG(DIMSE_dumpMessage(temp_string, *request, DIMSE_INCOMING, nullptr, pres_id));
	}
	else
	{
		DCMNET_INFO("Received Echo Request (MsgID " << request->MessageID << ")");
	}

	OFCondition cond = DIMSE_sendEchoResponse(assoc, pres_id, request, STATUS_Success, nullptr);
	if (cond.bad())
		DCMNET_ERROR("Echo SCP Failed: " << DimseCondition::dump(temp_string, cond));

	return cond;
}

int selectReadable(T_ASC_Association *        assoc,
                   T_ASC_Network *            net,
                   T_ASC_Association *        sub_assoc,
                   const T_DIMSE_BlockingMode block_mode,
                   int                        timeout) {
	T_ASC_Association *assocList[2] = {nullptr, nullptr};
	int                assocCount{0};
	if (net != nullptr && sub_assoc == nullptr) {
		if (ASC_associationWaiting(net, 0))
			return 2;
	}

	assocList[0] = assoc;
	assocCount   = 1;
	assocList[1] = sub_assoc;

	if (sub_assoc != nullptr)
		assocCount++;

	if (sub_assoc == nullptr)
		timeout = 1;
	else {
		if (block_mode == DIMSE_BLOCKING)
			timeout = 10000;
	}

	if (!ASC_selectReadableAssociation(assocList, assocCount, timeout))
		return 0; // none readable

	if (assocList[0] != nullptr)
		return 1; // main association readable

	if (assocList[1] != nullptr)
		return 2; // sub association readable

	return 0; // should not be reached
}
