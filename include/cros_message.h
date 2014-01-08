#ifndef _CROS_MESSAGE_H_
#define _CROS_MESSAGE_H_

#include "cros_node.h"

/*! \defgroup cros_message cROS TCPROS 
 * 
 *  Implemenation of the TCPROS protocol for topic message exchanges
 */

/*! \addtogroup cros_message
 *  @{
 */

typedef enum
{
  TCPROS_PARSER_ERROR = 0,
  TCPROS_PARSER_HEADER_INCOMPLETE,
  TCPROS_PARSER_DATA_INCOMPLETE,
  TCPROS_PARSER_DONE
}TcprosParserState;

/*! \brief Parse a TCPROS header sent initially from a subscriber
 * 
 *  \param n Ponter to the CrosNode object
 *  \param server_idx Index of the TcprosProcess ( tcpros_server_proc[server_idx] ) to be considered for the parsing
 * 
 *  \return Returns TCPROS_PARSER_DONE if the header is successfully parsed,
 *          TCPROS_PARSER_HEADER_INCOMPLETE if the header is incomplete,  
 *          or TCPROS_PARSER_ERROR on failure
 */
TcprosParserState cRosMessageParseSubcriptionHeader( CrosNode *n, int server_idx );

/*! \brief Prepare a TCPROS header to be initially sent back to a subscriber
 * 
 *  \param n Ponter to the CrosNode object
 *  \param server_idx Index of the TcprosProcess ( tcpros_server_proc[server_idx] ) to be considered for the parsing
 */
void cRosMessagePreparePublicationHeader( CrosNode *n, int server_idx );

/*! \brief Prepare a TCPROS message (with data) to be sent to a subscriber
 * 
 *  \param n Ponter to the CrosNode object
 *  \param server_idx Index of the TcprosProcess ( tcpros_server_proc[server_idx] ) to be considered for the parsing
 */
void cRosMessagePreparePublicationPacket( CrosNode *n, int server_idx );


/*! @}*/

#endif