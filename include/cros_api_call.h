#ifndef _CROS_API_CALL_H_
#define _CROS_API_CALL_H_

#include "cros_api.h"
#include "xmlrpc_params_vector.h"

#define CROS_API_TCPROS_STRING "TCPROS"

typedef void (*ResultCallback)(int callid, void *result, void *context);
typedef void * (*FetchResultCallback)(XmlrpcParamVector *response);
typedef void (*FreeResultCallback)(void *result);

typedef struct RosApiCall RosApiCall;
typedef struct ApiCallNode ApiCallNode;
typedef struct ApiCallQueue ApiCallQueue;

struct RosApiCall
{
  int id;                                     //! Progressive id of the call
  CROS_API_METHOD method;                     //! ROS api method
  XmlrpcParamVector params;                   //! Method arguments
  char *host;                                 //! Host to contact for the api
  int port;                                   //! Tcp port of the host to contact for the api
  int provider_idx;                           //! Provider (sub, pub or service provider) index
  ResultCallback result_callback;             //! Response callback
  void *context_data;                         //! Result callback context
  FetchResultCallback fetch_result_callback;  //! Callback to fetch the result
  FreeResultCallback free_result_callback;
};

struct ApiCallNode
{
  RosApiCall *call;
  ApiCallNode* next;
};

struct ApiCallQueue
{
  ApiCallNode* head;
  ApiCallNode* tail;
  size_t count;
};

RosApiCall * newRosApiCall();
void freeRosApiCall(RosApiCall *call);

void initApiCallQueue(ApiCallQueue *queue);
int enqueueApiCall(ApiCallQueue *queue, RosApiCall* apiCall);
RosApiCall * peekApiCallQueue(ApiCallQueue *queue);
RosApiCall * dequeueApiCall(ApiCallQueue *queue);
void releaseApiCallQueue(ApiCallQueue *queue);
size_t getQueueCount(ApiCallQueue *queue);
int isQueueEmpty(ApiCallQueue *queue);

#endif // _CROS_API_CALL_H_