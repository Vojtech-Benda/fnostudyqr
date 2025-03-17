//
// Created by Vojtěch on 17.03.2025.
//

#ifndef CALLBACKS_HPP
#define CALLBACKS_HPP

#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/dcmdata/dcmetinf.h"
#include "dcmtk/dcmdata/dcdeftag.h"
#include "dcmtk/dcmnet/dimse.h"
#include "dcmtk/dcmnet/diutil.h"

#include <fmt/format.h>

typedef struct
{
	T_ASC_Association *assoc;
	T_ASC_PresentationContextID presID;
} MoveCallbackInfo;

struct StoreCallbackData
{
	OFString m_filename;
	DcmFileFormat *m_fileformat{ nullptr };
	T_ASC_Association *m_assoc{ nullptr };
};

typedef void (*DIMSE_MoveUserCallback_)(
	/* in */
	void *callbackData,
	T_DIMSE_C_MoveRQ *request,
	int responseCount,
	int cancelAfterNResponses,
	T_DIMSE_C_MoveRSP *response);

typedef void (*DIMSE_SubOpProviderCallback_)(
	void *subOpCallbackData,
	T_ASC_Network *net,
	T_ASC_Association **subOpAssoc,
	std::string& output_directory,
	T_DIMSE_BlockingMode block_mode,
	int dimse_timeout);

void moveCallback(void *move_callback_data,
				  T_DIMSE_C_MoveRQ *request,
				  int response_count,
				  int cancel_after_n_responses,
				  T_DIMSE_C_MoveRSP *response);

void subOpMoveCallback(void *,
					   T_ASC_Network *assoc_net,
					   T_ASC_Association **sub_assoc,
					   std::string &output_directory,
					   T_DIMSE_BlockingMode block_mode,
					   int dimse_timeout);

void storeSCPCallback(/* in */
					  void *store_callback_data,
					  T_DIMSE_StoreProgress *progress,
					  T_DIMSE_C_StoreRQ *in_request,
					  char *filename,
					  DcmDataset **in_dataset,
					  /* out */
					  T_DIMSE_C_StoreRSP *out_response,
					  DcmDataset **status_detail);

void subOpCallback(void * /* subOpCallbackData */,
				   T_ASC_Network *assoc_net,
				   T_ASC_Association **sub_assoc,
				   std::string &output_directory,
				   T_DIMSE_BlockingMode block_mode,
				   int dimse_timeout);


OFCondition storeSCP(T_ASC_Association *assoc,
					 T_DIMSE_Message *message,
					 T_ASC_PresentationContextID presID,
					 std::string &output_directory,
					 T_DIMSE_BlockingMode block_mode,
					 int dimse_timeout);

OFCondition echoSCP(T_ASC_Association *assoc,
					T_DIMSE_Message *message,
					T_ASC_PresentationContextID presID);

OFCondition acceptSubAssoc(T_ASC_Network *assoc_net,
						   T_ASC_Association **assoc);

OFCondition subOpSCP(T_ASC_Association **sub_assoc,
					 std::string &output_directory,
					 T_DIMSE_BlockingMode block_mode,
					 int dimse_timeout);

int selectReadable(T_ASC_Association *assoc,
				  T_ASC_Network *net,
				  T_ASC_Association *sub_assoc,
				  T_DIMSE_BlockingMode block_mode,
				  int timeout);


#endif //CALLBACKS_HPP
