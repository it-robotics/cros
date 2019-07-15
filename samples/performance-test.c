/*! \file listener.c
 *  \brief This file implements a cROS program for measuring the performance of the cROS library.
 *
 *  To exit safely press Ctrl-C or 'kill' the process once. If this actions are repeated, the process
 *  will be finished immediately.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <direct.h>

#  define DIR_SEPARATOR_STR "\\"
#else
#  include <unistd.h>
#  include <errno.h>
#  include <signal.h>

#  define DIR_SEPARATOR_STR "/"
#endif

#include "cros.h"

#define ROS_MASTER_PORT 11311
#define ROS_MASTER_ADDRESS "127.0.0.1"

CrosNode *node; //! Pointer to object storing the ROS node. This object includes all the ROS-node state variables
static unsigned char exit_flag = 0; //! ROS-node loop exit flag. When set to 1 the cRosNodeStart() function exits

#define MAX_TIME_STAMPS 20
#define MAX_REPS 30

double sub_time_stamps[MAX_TIME_STAMPS][MAX_REPS];
size_t n_sub_time_stamps = 0;
size_t n_sub_reps = 0;

void array_diff(double *values, size_t n_values, double *diffs)
{
  size_t n_val;
  for (n_val = 0; n_val+1 < n_values; n_val++)
  {
    diffs[n_val] = values[n_val+1] - values[n_val];
  }
}

int print_array(FILE *out_fd, double *values, size_t n_values)
{
  size_t n_val;
  int tot_chars;

  tot_chars=0;
  for (n_val = 0; n_val < n_values; n_val++)
    tot_chars+=fprintf(out_fd, "%f ", values[n_val]);
  tot_chars+=fprintf(out_fd, "\n");

  return(tot_chars);
}

double array_mean(double *values, size_t n_values)
{
  size_t n_val;
  double val_sum=0.0;
  for (n_val = 0; n_val < n_values; n_val++)
    val_sum+=values[n_val];
  return(val_sum/n_values);
}

// corrected = 1 for a Corrected sample standard deviation or =0 for a Uncorrected sample standard deviation
double array_std_dev(double *values, size_t n_values, int corrected)
{
  size_t n_val;
  double sq_sum=0.0;
  double mean = array_mean(values, n_values);
  for (n_val = 0; n_val < n_values; n_val++)
  {
    double val_diff = values[n_val]-mean;
    sq_sum+=val_diff*val_diff;
  }
  return( sqrt(sq_sum/(n_values-corrected)) );
}

int store_times(const char *output_file_name)
{
  size_t n_stamp;
  FILE *fd;
  int ret;
  double time_stamp_diffs[MAX_REPS-1];

  fd=fopen(output_file_name, "wt");
  if(fd != NULL)
  {
    for (n_stamp = 0; n_stamp < n_sub_time_stamps; n_stamp++)
    {
      array_diff(sub_time_stamps[n_stamp], MAX_REPS, time_stamp_diffs); // We meause time MAX_REPS times
      print_array(fd, time_stamp_diffs, MAX_REPS-1);
    }
    fclose(fd);
    ret=0;
  }
  else
    ret=-1;
  return(ret);
}

void compute_times(void)
{
  size_t n_stamp;
  double time_stamp_diffs[MAX_REPS-1];
  printf("Mean and std.dev. pairs of time differences between reception times: ");
  for (n_stamp = 0; n_stamp < n_sub_time_stamps; n_stamp++)
  {
    double mean, std_dev;
    array_diff(sub_time_stamps[n_stamp], MAX_REPS, time_stamp_diffs); // We meause time MAX_REPS times
    mean = array_mean(time_stamp_diffs, MAX_REPS-1); // So, we only have MAX_REPS-1 time differences to average

    std_dev = array_std_dev(time_stamp_diffs, MAX_REPS-1, 1); // 1 for corrected sample standard deviation

    printf("%f %f   ", mean, std_dev);
  }
  printf("\n");
}

// This callback will be invoked when the subscriber receives a message
static CallbackResponse callback_sub(cRosMessage *message, void *data_context)
{
  sub_time_stamps[n_sub_time_stamps][n_sub_reps++] = cRosClockTimeStampToUSec(cRosClockGetTimeStamp());
  if(n_sub_reps >= MAX_REPS)
  {
    n_sub_reps=0;
    n_sub_time_stamps++;
  }

  cRosMessageField *data_field = cRosMessageGetField(message, "data");
  if (data_field != NULL)
  {
    printf("Heard %lu\n",strlen(data_field->data.as_string));
    //ROS_INFO(node, "I heard: [%s]\n", data_field->data.as_string);
  }

  if(n_sub_time_stamps>=MAX_TIME_STAMPS)
    exit_flag = 1;
  return 0; // 0=success
}

static CallbackResponse callback_pub(cRosMessage *message, void *data_context)
{
  static int pub_count = 0;
  char buf[1024];
  // We need to index into the message structure and then assign to fields
  cRosMessageField *data_field;

  data_field = cRosMessageGetField(message, "data");
  if(data_field)
  {
    snprintf(buf, sizeof(buf), "periodic hello world %d", pub_count);
    if(cRosMessageSetFieldValueString(data_field, buf) == 0)
    {
      ROS_INFO(node, "%s\n", buf);
    }
    pub_count+=10;
  }

  return 0; // 0=success
}

// This callback will be invoked when the service provider receives a service call
static CallbackResponse callback_provider_add_two_ints(cRosMessage *request, cRosMessage *response, void* data_context)
{
  sub_time_stamps[n_sub_time_stamps][n_sub_reps++] = cRosClockTimeStampToUSec(cRosClockGetTimeStamp());
  if(n_sub_reps >= MAX_REPS)
  {
    n_sub_reps=0;
    n_sub_time_stamps++;
  }

  cRosMessageField *a_field = cRosMessageGetField(request, "a");
  cRosMessageField *b_field = cRosMessageGetField(request, "b");
  //cRosMessageFieldsPrint(request, 0);

  if(a_field != NULL && a_field != NULL)
  {
    int64_t a = a_field->data.as_int64;
    int64_t b = b_field->data.as_int64;

    int64_t response_val = a+b;

    cRosMessageField *sum_field = cRosMessageGetField(response, "sum");
    if(sum_field != NULL)
    {
      sum_field->data.as_int64 = response_val;
      //ROS_INFO(node,"Service add 2 ints. Args: {a: %lld, b: %lld}. Resp: %lld\n", (long long)a, (long long)b, (long long)response_val);
    }
  }

  if(n_sub_time_stamps>=MAX_TIME_STAMPS)
  exit_flag = 1;

  return 0; // 0=success
}

static CallbackResponse callback_caller_add_two_ints(cRosMessage *request, cRosMessage *response, int call_resp_flag, void* context)
{
  static int call_count = 0;

  if(!call_resp_flag) // Check if this callback function has been called to provide the service call arguments or to collect the response
  {
    cRosMessageField *a_field = cRosMessageGetField(request, "a");
    cRosMessageField *b_field = cRosMessageGetField(request, "b");
    if(a_field != NULL && b_field != NULL)
    {
      a_field->data.as_int64=10;
      b_field->data.as_int64=call_count;
      ROS_INFO(node, "Service add 2 ints call arguments: {a: %lld, b: %lld}\b\n", (long long)a_field->data.as_int64, (long long)b_field->data.as_int64);
    }
  }
  else // Service call response available
  {
    cRosMessageField *sum_field = cRosMessageGetField(response, "sum");
    if(sum_field != NULL)
      ROS_INFO(node, "Service add 2 ints response: %lld (call_count: %i)\n", (long long)sum_field->data.as_int64, call_count++);
  }

  if(call_count > 10) exit_flag=1;
  return 0; // 0=success
}


// Ctrl-C-and-'kill' event/signal handler: (this code is no strictly necessary for a simple example and can be removed)
#ifdef _WIN32
// This callback function will be called when the console process receives a CTRL_C_EVENT or
// CTRL_CLOSE_EVENT signal.
// Function set_signal_handler() should be called before calling cRosNodeStart() to set function
// exit_deamon_handler() as the handler of these signals.
// These functions are declared as 'static' to allow the declaration of other (independent) functions with
// the same name in this project.
static BOOL WINAPI exit_deamon_handler(DWORD sig)
{
  BOOL sig_handled;

  switch(sig)
  {
    case CTRL_C_EVENT:
    case CTRL_CLOSE_EVENT:
      SetConsoleCtrlHandler(exit_deamon_handler, FALSE); // Remove the handler
      printf("Signal %u received: exiting safely.\n", sig);
      exit_flag = 1; // Cause the exit of cRosNodeStart loop (safe exit)
      sig_handled = TRUE; // Indicate that this signal is handled by this function
      break;
    default:
      sig_handled = FALSE; // Indicate that this signal is not handled by this functions, so the next handler function of the list will be called
      break;
  }
  return(sig_handled);
}

// Sets the signal handler functions of CTRL_C_EVENT and CTRL_CLOSE_EVENT: exit_deamon_handler
static DWORD set_signal_handler(void)
  {
   DWORD ret;

   if(SetConsoleCtrlHandler(exit_deamon_handler, TRUE))
      ret=0; // Success setting the control handler
   else
     {
      ret=GetLastError();
      printf("Error setting termination signal handler. Error code=%u\n",ret);
     }
   return(ret);
  }
#else
struct sigaction old_int_signal_handler, old_term_signal_handler; //! Structures codifying the original handlers of SIGINT and SIGTERM signals (e.g. used when pressing Ctrl-C for the second time);

// This callback function will be called when the main process receives a SIGINT or
// SIGTERM signal.
// Function set_signal_handler() should be called to set this function as the handler of
// these signals
static void exit_deamon_handler(int sig)
{
  printf("Signal %i received: exiting safely.\n", sig);
  sigaction(SIGINT, &old_int_signal_handler, NULL);
  sigaction(SIGTERM, &old_term_signal_handler, NULL);
  exit_flag = 1; // Indicate the exit of cRosNodeStart loop (safe exit)
}

// Sets the signal handler functions of SIGINT and SIGTERM: exit_deamon_handler
static int set_signal_handler(void)
  {
   int ret;
   struct sigaction act;

   memset (&act, '\0', sizeof(act));

   act.sa_handler = exit_deamon_handler;
   // If the signal handler is invoked while a system call or library function call is blocked,
   // then the we want the call to be automatically restarted after the signal handler returns
   // instead of making the call fail with the error EINTR.
   act.sa_flags=SA_RESTART;
   if(sigaction(SIGINT, &act, &old_int_signal_handler) == 0 && sigaction(SIGTERM, &act,  &old_term_signal_handler) == 0)
      ret=0;
   else
     {
      ret=errno;
      printf("Error setting termination signal handler. errno=%d\n",ret);
     }
   return(ret);
  }
#endif

int main(int argc, char **argv)
{
  char path[4097]; // We need to tell our node where to find the .msg files that we'll be using
  const char *node_name;
  int subidx, pubidx, calleridx; // Index (identifier) of the subscriber, publisher and service caller to be created

  cRosErrCodePack err_cod;
  int op_mode;

  getcwd(path, sizeof(path));
  strncat(path, DIR_SEPARATOR_STR"rosdb", sizeof(path) - strlen(path) - 1);

  printf("PATH ROSDB: %s\n", path);

  printf("Press s for subscriber, p for publisher, r for service server or c for service client: ");
  op_mode = getchar();
  if (op_mode == 's')
    node_name = "/node_sub";
  else if (op_mode == 'r')
    node_name = "/node_server";
  else if (op_mode == 'p')
    node_name = "/node_pub";
  else if (op_mode == 'c')
    node_name = "/node_caller";
  else
  {
    printf("Invalid option");
    return EXIT_FAILURE;
  }

  // Create a new node and tell it to connect to roscore in the usual place
  node = cRosNodeCreate(node_name, "127.0.0.1", ROS_MASTER_ADDRESS, ROS_MASTER_PORT, path);
  if( node == NULL )
  {
    printf("cRosNodeCreate() failed; is this program already being run?");
    return EXIT_FAILURE;
  }

  err_cod = cRosWaitPortOpen(ROS_MASTER_ADDRESS, ROS_MASTER_PORT, 0);
  if(err_cod != CROS_SUCCESS_ERR_PACK)
  {
    cRosPrintErrCodePack(err_cod, "Port %s:%hu cannot be opened: ROS Master does not seems to be running", ROS_MASTER_ADDRESS, ROS_MASTER_PORT);
    return EXIT_FAILURE;
  }

  printf("Node RPCROS port: %i\n", node->rpcros_port);

  // Function exit_deamon_handler() will be called when Ctrl-C is pressed or kill is executed
  set_signal_handler();

  if (op_mode == 's')
  {
    // Create a subscriber to topic /chatter of type "std_msgs/String" and supply a callback for received messages (callback_sub)
    err_cod = cRosApiRegisterSubscriber(node, "/chatter", "std_msgs/String", callback_sub, NULL, NULL, 0, &subidx);
    if (err_cod != CROS_SUCCESS_ERR_PACK)
    {
      cRosPrintErrCodePack(err_cod, "cRosApiRegisterSubscriber() failed; did you run this program one directory above 'rosdb'?");
      cRosNodeDestroy(node);
      return EXIT_FAILURE;
    }

    // Run the main loop until exit_flag is 1
    err_cod = cRosNodeStart( node, CROS_INFINITE_TIMEOUT, &exit_flag );
    if(err_cod != CROS_SUCCESS_ERR_PACK)
      cRosPrintErrCodePack(err_cod, "cRosNodeStart() returned an error code");

  }
  else if (op_mode == 'r')
  {
    // Create a service provider named /sum of type "roscpp_tutorials/TwoInts" and supply a callback for received calls
    err_cod = cRosApiRegisterServiceProvider(node,"/sum","roscpp_tutorials/TwoInts", callback_provider_add_two_ints, NULL, NULL, NULL);
    if(err_cod != CROS_SUCCESS_ERR_PACK)
    {
      cRosPrintErrCodePack(err_cod, "cRosApiRegisterServiceProvider() failed; did you run this program one directory above 'rosdb'?");
      cRosNodeDestroy( node );
      return EXIT_FAILURE;
    }

    // Run the main loop until exit_flag is 1
    err_cod = cRosNodeStart( node, CROS_INFINITE_TIMEOUT, &exit_flag );
    if(err_cod != CROS_SUCCESS_ERR_PACK)
      cRosPrintErrCodePack(err_cod, "cRosNodeStart() returned an error code");

  }
  else if (op_mode == 'p')
  {
    cRosMessage *msg;
    cRosMessageField *data_field;

    // Create a publisher to topic /chatter of type "std_msgs/String"
    err_cod = cRosApiRegisterPublisher(node, "/chatter", "std_msgs/String", -1, NULL, NULL, NULL, &pubidx); // callback_pub
    if (err_cod != CROS_SUCCESS_ERR_PACK)
    {
      cRosPrintErrCodePack(err_cod, "cRosApiRegisterPublisher() failed; did you run this program one directory above 'rosdb'?");
      cRosNodeDestroy(node);
      return EXIT_FAILURE;
    }

    msg = cRosApiCreatePublisherMessage(node, pubidx);
    // Popullate msg
    data_field = cRosMessageGetField(msg, "data");
    if (data_field != NULL)
    {
      char buf[1024*MAX_TIME_STAMPS+1]={0};
      int pub_count;

      printf("Publishing strings...\n");
      cRosMessageSetFieldValueString(data_field, buf);
      err_cod = cRosNodeStart( node, 200, &exit_flag );
      for (pub_count = 0; pub_count < MAX_TIME_STAMPS && err_cod == CROS_SUCCESS_ERR_PACK && exit_flag == 0; pub_count++)
      {
        int rep_count;
        //snprintf(buf, sizeof(buf), "hello world %d", pub_count);
        memset(buf+1024*pub_count,' ',1024);
        cRosMessageSetFieldValueString(data_field, buf);
        for(rep_count=0;rep_count<MAX_REPS && err_cod == CROS_SUCCESS_ERR_PACK;rep_count++)
        {
          err_cod = cRosNodeSendTopicMsg(node, pubidx, msg, 1000);
          if (err_cod == CROS_SUCCESS_ERR_PACK)
          {
            printf("Published string %d\n", pub_count);
            //err_cod = cRosNodeStart( node, 400, &exit_flag );
          }
          else
            cRosPrintErrCodePack(err_cod, "cRosNodeSendTopicMsg() failed: message not sent");
        }
      }
      printf("End of message publication.\n");

    }
    else
      printf("Error accessing message fields\n");

    cRosMessageFree(msg);
  }
  else if (op_mode == 'c')
  {
    cRosMessage *msg_req, msg_res;
    cRosMessageField *a_field, *b_field;

    // Create a service caller named /sum of type "roscpp_tutorials/TwoInts" and request that the associated callback be invoked every 200ms (5Hz)
    err_cod = cRosApiRegisterServiceCaller(node,"/sum","roscpp_tutorials/TwoInts", -1, NULL, NULL, NULL, 1, 1, &calleridx); // callback_caller_add_two_ints
    if(err_cod != CROS_SUCCESS_ERR_PACK)
    {
      cRosPrintErrCodePack(err_cod, "cRosApiRegisterServiceCaller() failed; did you run this program one directory above 'rosdb'?");
      cRosNodeDestroy( node );
      return EXIT_FAILURE;
    }


    msg_req = cRosApiCreateServiceCallerRequest(node, calleridx);
    cRosMessageInit(&msg_res);

    a_field = cRosMessageGetField(msg_req, "a");
    b_field = cRosMessageGetField(msg_req, "b");
    if (a_field != NULL && b_field != NULL)
    {
      char buf[1024*MAX_TIME_STAMPS+1]={0};
      int call_count = 0;

      printf("Calling service...\n");

      err_cod = cRosNodeStart( node, 200, &exit_flag );
      for (call_count = 0; call_count < MAX_TIME_STAMPS && err_cod == CROS_SUCCESS_ERR_PACK && exit_flag == 0; call_count++)
      {
        int rep_count;
        //snprintf(buf, sizeof(buf), "hello world %d", call_count);
        memset(buf+1024*call_count,' ',1024);

        a_field->data.as_int64 = call_count;
        b_field->data.as_int64 = 10;

        for(rep_count=0;rep_count<MAX_REPS && err_cod == CROS_SUCCESS_ERR_PACK;rep_count++)
        {
          err_cod = cRosNodeServiceCall(node, calleridx, msg_req, &msg_res, 5000);
          if (err_cod == CROS_SUCCESS_ERR_PACK)
          {
            printf("Called service %d\n", call_count);
            //err_cod = cRosNodeStart( node, 1000, &exit_flag );
          }
          else
            cRosPrintErrCodePack(err_cod, "cRosNodeServiceCall() failed: service call not made");
        }
      }

    }
    else
      printf("Error accessing message fields\n");


    cRosMessageFree(msg_req);
    cRosMessageRelease(&msg_res);

    printf("End of service call.\n");

    // Run the main loop until exit_flag is 1
    err_cod = cRosNodeStart( node, 200, &exit_flag );
    if(err_cod != CROS_SUCCESS_ERR_PACK)
      cRosPrintErrCodePack(err_cod, "cRosNodeStart() returned an error code");
  }


  printf("Unregistering in ROS master\n");
  // Free memory and unregister
  err_cod=cRosNodeDestroy( node );
  if(err_cod != CROS_SUCCESS_ERR_PACK)
  {
    cRosPrintErrCodePack(err_cod, "cRosNodeDestroy() failed; Error unregistering from ROS master");
    return EXIT_FAILURE;
  }

  printf("Node end. Current n_sub_time_stamps: %lu n_sub_reps: %lu.\n", n_sub_time_stamps, n_sub_reps);

  if(op_mode == 's' || op_mode == 'r')
  {
    compute_times();
    store_times("times.txt");
  }

  return EXIT_SUCCESS;
}
