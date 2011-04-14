/****************************************************************************
 * Copyright (C) 2008-2010 Sourcefire, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License Version 2 as
 * published by the Free Software Foundation.  You may not use, modify or
 * distribute this program under any other version of the GNU General
 * Public License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 **************************************************************************** 
 * Handles processing of events generated by the preprocessor.
 *
 * 8/17/2008 - Initial implementation ... Todd Wease <twease@sourcefire.com>
 *
 ****************************************************************************/

#include "dce2_event.h"
#include "dce2_memory.h"
#include "dce2_config.h"
#include "dce2_stats.h"
#include "smb.h"
#include "dcerpc.h"
#include "sf_dynamic_preprocessor.h"
#include <stdarg.h>
#include <string.h>

/********************************************************************
 * Global variables
 ********************************************************************/
/* Used to print events and their arguments to.  Each event gets
 * a buffer and 255 chars to print to.  The reason for the multiple
 * buffers is that if multiple events fire, we don't want to overwrite
 * one before it's been written via an output plugin.  Only one event
 * type per session is ever logged. */
static char dce2_event_bufs[DCE2_EVENT__MAX][256];
/* Used to hold event information */
static DCE2_EventNode dce2_events[DCE2_EVENT__MAX];
/* Used for matching a command string to a command code */
char *dce2_smb_coms[256];
/* Used for matching a pdu string to a pdu type */
char *dce2_pdu_types[DCERPC_PDU_TYPE__MAX];

/********************************************************************
 * Extern variables
 ********************************************************************/
extern DynamicPreprocessorData _dpd;
extern DCE2_Stats dce2_stats;

/******************************************************************
 * Function: DCE2_EventsInit()
 *
 * Initializes global data.
 *
 * Arguments: None
 *       
 * Returns: None
 *
 ******************************************************************/ 
void DCE2_EventsInit(void)
{
    DCE2_Event event;
    char gname[100];
    unsigned int i;
    static const DCE2_EventNode events[DCE2_EVENT__MAX] =
    {
        {
            DCE2_EVENT_FLAG__NONE,
            DCE2_EVENT__NO_EVENT,
            "Have to use this because can't have an event sid of zero"
        },
        {
            DCE2_EVENT_FLAG__MEMCAP,
            DCE2_EVENT__MEMCAP,
            "Memory cap exceeded"
        },
        {
            DCE2_EVENT_FLAG__SMB,
            DCE2_EVENT__SMB_BAD_NBSS_TYPE,
            "SMB - Bad NetBIOS Session Service session type"
        },
        {
            DCE2_EVENT_FLAG__SMB,
            DCE2_EVENT__SMB_BAD_TYPE,
            "SMB - Bad SMB message type"
        },
        {
            DCE2_EVENT_FLAG__SMB,
            DCE2_EVENT__SMB_BAD_ID,
            "SMB - Bad SMB Id (not \\xffSMB)"
        },
        {
            DCE2_EVENT_FLAG__SMB,
            DCE2_EVENT__SMB_BAD_WCT,
            "SMB - %s: Bad word count: %u"
        },
        {
            DCE2_EVENT_FLAG__SMB,
            DCE2_EVENT__SMB_BAD_BCC,
            "SMB - %s: Bad byte count: %u"
        },
        {
            DCE2_EVENT_FLAG__SMB,
            DCE2_EVENT__SMB_BAD_FORMAT,
            "SMB - %s: Bad format type: %u"
        },
        {
            DCE2_EVENT_FLAG__SMB,
            DCE2_EVENT__SMB_BAD_OFF,
            "SMB - %s: Bad offset: %p not between %p and %p"
        },
        {
            DCE2_EVENT_FLAG__SMB,
            DCE2_EVENT__SMB_TDCNT_ZERO,
            "SMB - %s: Zero total data count"
        },
        {
            DCE2_EVENT_FLAG__SMB,
            DCE2_EVENT__SMB_NB_LT_SMBHDR,
            "SMB - NetBIOS data length (%u) less than SMB header length (%u)"
        },
        {
            DCE2_EVENT_FLAG__SMB,
            DCE2_EVENT__SMB_NB_LT_COM,
            "SMB - %s: Remaining NetBIOS data length (%u) less than command length (%u)"
        },
        {
            DCE2_EVENT_FLAG__SMB,
            DCE2_EVENT__SMB_NB_LT_BCC,
            "SMB - %s: Remaining NetBIOS data length (%u) less than command byte count (%u)"
        },
        {
            DCE2_EVENT_FLAG__SMB,
            DCE2_EVENT__SMB_NB_LT_DSIZE,
            "SMB - %s: Remaining NetBIOS data length (%u) less than command data size (%u)"
        },
        {
            DCE2_EVENT_FLAG__SMB,
            DCE2_EVENT__SMB_TDCNT_LT_DSIZE,
            "SMB - %s: Remaining total data count (%u) less than this command data size (%u)"
        },
        {
            DCE2_EVENT_FLAG__SMB,
            DCE2_EVENT__SMB_DSENT_GT_TDCNT,
            "SMB - %s: Total data sent (%u) greater than command total data expected (%u)"
        },
        {
            DCE2_EVENT_FLAG__SMB,
            DCE2_EVENT__SMB_BCC_LT_DSIZE,
            "SMB - %s: Byte count (%u) less than command data size (%u)"
        },
        {
            DCE2_EVENT_FLAG__SMB,
            DCE2_EVENT__SMB_INVALID_DSIZE,
            "SMB - %s: Invalid command data size (%u) for byte count (%u)"
        },
        {
            DCE2_EVENT_FLAG__SMB,
            DCE2_EVENT__SMB_EXCESSIVE_TREE_CONNECTS,
            "SMB - %s: Excessive Tree Connect requests (>%u) with pending Tree Connect responses"
        },
        {
            DCE2_EVENT_FLAG__SMB,
            DCE2_EVENT__SMB_EXCESSIVE_READS,
            "SMB - %s: Excessive Read requests (>%u) with pending Read responses"
        },
        {
            DCE2_EVENT_FLAG__SMB,
            DCE2_EVENT__SMB_EXCESSIVE_CHAINING,
            "SMB - Excessive command chaining (>%u)"
        },
        {
            DCE2_EVENT_FLAG__SMB,
            DCE2_EVENT__SMB_MULT_CHAIN_SS,
            "SMB - Multiple chained login requests"
        },
        {
            DCE2_EVENT_FLAG__SMB,
            DCE2_EVENT__SMB_MULT_CHAIN_TC,
            "SMB - Multiple chained tree connect requests"
        },
        {
            DCE2_EVENT_FLAG__SMB,
            DCE2_EVENT__SMB_CHAIN_SS_LOGOFF,
            "SMB - Chained login followed by logoff"
        },
        {
            DCE2_EVENT_FLAG__SMB,
            DCE2_EVENT__SMB_CHAIN_TC_TDIS,
            "SMB - Chained tree connect followed by tree disconnect"
        },
        {
            DCE2_EVENT_FLAG__SMB,
            DCE2_EVENT__SMB_CHAIN_OPEN_CLOSE,
            "SMB - Chained open pipe followed by close pipe"
        },
        {
            DCE2_EVENT_FLAG__SMB,
            DCE2_EVENT__SMB_INVALID_SHARE,
            "SMB - Invalid share access: %s"
        },
        {
            DCE2_EVENT_FLAG__CO,
            DCE2_EVENT__CO_BAD_MAJ_VERSION,
            "Connection-oriented DCE/RPC - Invalid major version: %u"
        },
        {
            DCE2_EVENT_FLAG__CO,
            DCE2_EVENT__CO_BAD_MIN_VERSION,
            "Connection-oriented DCE/RPC - Invalid minor version: %u"
        },
        {
            DCE2_EVENT_FLAG__CO,
            DCE2_EVENT__CO_BAD_PDU_TYPE,
            "Connection-oriented DCE/RPC - Invalid pdu type: 0x%02x"
        },
        {
            DCE2_EVENT_FLAG__CO,
            DCE2_EVENT__CO_FLEN_LT_HDR,
            "Connection-oriented DCE/RPC - Fragment length (%u) less than header size (%u)"
        },
        {
            DCE2_EVENT_FLAG__CO,
            DCE2_EVENT__CO_FLEN_LT_SIZE,
            "Connection-oriented DCE/RPC - %s: Remaining fragment length (%u) less than size needed (%u)"
        },
        {
            DCE2_EVENT_FLAG__CO,
            DCE2_EVENT__CO_ZERO_CTX_ITEMS,
            "Connection-oriented DCE/RPC - %s: No context items specified"
        },
        {
            DCE2_EVENT_FLAG__CO,
            DCE2_EVENT__CO_ZERO_TSYNS,
            "Connection-oriented DCE/RPC - %s: No transfer syntaxes specified"
        },
        {
            DCE2_EVENT_FLAG__CO,
            DCE2_EVENT__CO_FRAG_LT_MAX_XMIT_FRAG,
            "Connection-oriented DCE/RPC - %s: Fragment length on non-last fragment (%u) less than "
                "maximum negotiated fragment transmit size for client (%u)"
        },
        {
            DCE2_EVENT_FLAG__CO,
            DCE2_EVENT__CO_FRAG_GT_MAX_XMIT_FRAG,
            "Connection-oriented DCE/RPC - %s: Fragment length (%u) greater than "
                "maximum negotiated fragment transmit size (%u)"
        },
        {
            DCE2_EVENT_FLAG__CO,
            DCE2_EVENT__CO_ALTER_CHANGE_BYTE_ORDER,
            "Connection-oriented DCE/RPC - Alter Context byte order different from Bind"
        },
        {
            DCE2_EVENT_FLAG__CO,
            DCE2_EVENT__CO_FRAG_DIFF_CALL_ID,
            "Connection-oriented DCE/RPC - Call id (%u) of non first/last fragment different "
                "from call id established for fragmented request (%u)"
        },
        {
            DCE2_EVENT_FLAG__CO,
            DCE2_EVENT__CO_FRAG_DIFF_OPNUM,
            "Connection-oriented DCE/RPC - Opnum (%u) of non first/last fragment different "
                "from opnum established for fragmented request (%u)"
        },
        {
            DCE2_EVENT_FLAG__CO,
            DCE2_EVENT__CO_FRAG_DIFF_CTX_ID,
            "Connection-oriented DCE/RPC - Context id (%u) of non first/last fragment different "
                "from context id established for fragmented request (%u)"
        },
        {
            DCE2_EVENT_FLAG__CL,
            DCE2_EVENT__CL_BAD_MAJ_VERSION,
            "Connection-less DCE/RPC - Invalid major version: %u"
        },
        {
            DCE2_EVENT_FLAG__CL,
            DCE2_EVENT__CL_BAD_PDU_TYPE,
            "Connection-less DCE/RPC - Invalid pdu type: 0x%02x"
        },
        {
            DCE2_EVENT_FLAG__CL,
            DCE2_EVENT__CL_DATA_LT_HDR,
            "Connection-less DCE/RPC - Data length (%u) less than header size (%u)"
        },
        {
            DCE2_EVENT_FLAG__CL,
            DCE2_EVENT__CL_BAD_SEQ_NUM,
            "Connection-less DCE/RPC - %s: Bad sequence number"
        },
    };

    snprintf(gname, sizeof(gname) - 1, "(%s) ", DCE2_GNAME);
    gname[sizeof(gname) - 1] = '\0';

    for (event = DCE2_EVENT__NO_EVENT; event < DCE2_EVENT__MAX; event++)
    {
        int size = strlen(gname) + strlen(events[event].format) + 1;

        /* This is a check to make sure all of the events in the array are
         * in the same order as the enum, so we index the right thing when
         * alerting - DO NOT REMOVE THIS CHECK */
        if (events[event].event != event)
        {
            DCE2_Die("%s(%d) Events are not in the right order.",
                     __FILE__, __LINE__);
        }

        dce2_events[event].format = (char *)DCE2_Alloc(size, DCE2_MEM_TYPE__INIT);
        if (dce2_events[event].format == NULL)
        {
            DCE2_Die("%s(%d) Could not allocate memory for events array.",
                     __FILE__, __LINE__);
        }

        dce2_events[event].format[size - 1] = '\0';
        snprintf(dce2_events[event].format, size, "%s%s", gname, events[event].format);
        if (dce2_events[event].format[size - 1] != '\0')
        {
            DCE2_Die("%s(%d) Event string truncated.", __FILE__, __LINE__);
        }

        dce2_events[event].eflag = events[event].eflag;
        dce2_events[event].event = events[event].event;
    }

    for (i = 0; i < (sizeof(dce2_smb_coms) / sizeof(char *)); i++)
    {
        char *com;

        switch (i)
        {
            case SMB_COM_OPEN:
                com = "Open";
                break;
            case SMB_COM_CLOSE:
                com = "Close";
                break;
            case SMB_COM_READ:
                com = "Read";
                break;
            case SMB_COM_WRITE:
                com = "Write";
                break;
            case SMB_COM_READ_BLOCK_RAW:
                com = "Read Block Raw";
                break;
            case SMB_COM_WRITE_BLOCK_RAW:
                com = "Write Block Raw";
                break;
            case SMB_COM_WRITE_COMPLETE:
                com = "Write Complete";
                break;
            case SMB_COM_TRANS:
                com = "Transaction";
                break;
            case SMB_COM_TRANS_SEC:
                com = "Transaction Secondary";
                break;
            case SMB_COM_WRITE_AND_CLOSE:
                com = "Write and Close";
                break;
            case SMB_COM_OPEN_ANDX:
                com = "Open AndX";
                break;
            case SMB_COM_READ_ANDX:
                com = "Read AndX";
                break;
            case SMB_COM_WRITE_ANDX:
                com = "Write AndX";
                break;
            case SMB_COM_NT_CREATE_ANDX:
                com = "Nt Create AndX";
                break;
            case SMB_COM_TREE_CON:
                com = "Tree Connect";
                break;
            case SMB_COM_TREE_DIS:
                com = "Tree Disconnect";
                break;
            case SMB_COM_NEGPROT:
                com = "Negotiate Protocol";
                break;
            case SMB_COM_SESS_SETUP_ANDX:
                com = "Session Setup AndX";
                break;
            case SMB_COM_LOGOFF_ANDX:
                com = "Logoff AndX";
                break;
            case SMB_COM_TREE_CON_ANDX:
                com = "Tree Connect AndX";
                break;
            case SMB_COM_RENAME:
                com = "Rename";
                break;
            default:
                com = "Unknown SMB command";
                break;
        }

        dce2_smb_coms[i] = (char *)DCE2_Alloc(strlen(com) + 1, DCE2_MEM_TYPE__INIT);
        strncpy(dce2_smb_coms[i], com, strlen(com));
        dce2_smb_coms[i][strlen(com)] = '\0';
#ifdef DCE2_EVENT_PRINT_DEBUG
        printf("%s\n", dce2_smb_coms[i]);
#endif
    }

    for (i = 0; i < (sizeof(dce2_pdu_types) / sizeof(char *)); i++)
    {
        char *type;

        switch (i)
        {
            case DCERPC_PDU_TYPE__REQUEST:
                type = "Request";
                break;
            case DCERPC_PDU_TYPE__PING:
                type = "Ping";
                break;
            case DCERPC_PDU_TYPE__RESPONSE:
                type = "Response";
                break;
            case DCERPC_PDU_TYPE__FAULT:
                type = "Fault";
                break;
            case DCERPC_PDU_TYPE__WORKING:
                type = "Working";
                break;
            case DCERPC_PDU_TYPE__NOCALL:
                type = "NoCall";
                break;
            case DCERPC_PDU_TYPE__REJECT:
                type = "Reject";
                break;
            case DCERPC_PDU_TYPE__ACK:
                type = "Ack";
                break;
            case DCERPC_PDU_TYPE__CL_CANCEL:
                type = "Cancel";
                break;
            case DCERPC_PDU_TYPE__FACK:
                type = "Fack";
                break;
            case DCERPC_PDU_TYPE__CANCEL_ACK:
                type = "Cancel Ack";
                break;
            case DCERPC_PDU_TYPE__BIND:
                type = "Bind";
                break;
            case DCERPC_PDU_TYPE__BIND_ACK:
                type = "Bind Ack";
                break;
            case DCERPC_PDU_TYPE__BIND_NACK:
                type = "Bind Nack";
                break;
            case DCERPC_PDU_TYPE__ALTER_CONTEXT:
                type = "Alter Context";
                break;
            case DCERPC_PDU_TYPE__ALTER_CONTEXT_RESP:
                type = "Alter Context Response";
                break;
            case DCERPC_PDU_TYPE__AUTH3:
                type = "Auth3";
                break;
            case DCERPC_PDU_TYPE__SHUTDOWN:
                type = "Shutdown";
                break;
            case DCERPC_PDU_TYPE__CO_CANCEL:
                type = "Cancel";
                break;
            case DCERPC_PDU_TYPE__ORPHANED:
                type = "Orphaned";
                break;
            case DCERPC_PDU_TYPE__MICROSOFT_PROPRIETARY_OUTLOOK2003_RPC_OVER_HTTP:
                type = "Microsoft Exchange/Outlook 2003";
                break;
            default:
                type = "Unknown DCE/RPC type";
                break;
        }

        dce2_pdu_types[i] = (char *)DCE2_Alloc(strlen(type) + 1, DCE2_MEM_TYPE__INIT);
        strncpy(dce2_pdu_types[i], type, strlen(type));
        dce2_pdu_types[i][strlen(type)] = '\0';
#ifdef DCE2_EVENT_PRINT_DEBUG
        printf("%s\n", dce2_pdu_types[i]);
#endif
    }
}

/******************************************************************
 * Function: DCE2_Alert()
 *
 * Potentially generates an alert if an event is triggered.
 *
 * Arguments:
 *  DCE2_SsnData *
 *      This is the current session data structure being used
 *      when the event was triggered.  It is not a necessary
 *      argument if no session data is currently available, for
 *      example if the event is a memcap event - pass in NULL in
 *      this case.
 *  DCE2_Event
 *      The event type that was triggered.
 *  ...
 *      The arguments to the format for the event.
 *       
 * Returns: None
 *
 ******************************************************************/ 
void DCE2_Alert(DCE2_SsnData *sd, DCE2_Event e, ...)
{
    va_list ap;

    if (sd != NULL)
    {
        /* Only log a specific alert once per session */
        if (sd->alert_mask & (1 << e))
            return;

        /* set bit for this alert so we don't alert on again
         * in this session */
        sd->alert_mask |= (1 << e);
    }

    if (!DCE2_GcAlertOnEvent(dce2_events[e].eflag))
        return;

    dce2_stats.events++;

    va_start(ap, e);
    vsnprintf(dce2_event_bufs[e], sizeof(dce2_event_bufs[e]) - 1, dce2_events[e].format, ap);
    va_end(ap);

    /* Make sure it's NULL terminated */
    dce2_event_bufs[e][sizeof(dce2_event_bufs[e]) - 1] = '\0';

    DEBUG_WRAP(DCE2_DebugMsg(DCE2_DEBUG__ALL, "DCE2 Alert => %s\n", dce2_event_bufs[e]));

    _dpd.alertAdd(GENERATOR_DCE2, e, 1, 0, 3, dce2_event_bufs[e], 0);
}

/******************************************************************
 * Function: DCE2_EventsFree()
 *
 * Frees any global data that was dynamically allocated.
 *
 * Arguments: None
 *       
 * Returns: None
 *
 ******************************************************************/ 
void DCE2_EventsFree(void)
{
    unsigned int i;

    for (i = 0; i < DCE2_EVENT__MAX; i++)
    {
        if (dce2_events[i].format != NULL)
        {
            DCE2_Free((void *)dce2_events[i].format, strlen(dce2_events[i].format) + 1, DCE2_MEM_TYPE__INIT);
            dce2_events[i].format = NULL;
        }
    }

    for (i = 0; i < (sizeof(dce2_smb_coms) / sizeof(char *)); i++)
    {
        if (dce2_smb_coms[i] != NULL)
        {
            DCE2_Free((void *)dce2_smb_coms[i], strlen(dce2_smb_coms[i]) + 1, DCE2_MEM_TYPE__INIT);
            dce2_smb_coms[i] = NULL;
        }
    }

    for (i = 0; i < (sizeof(dce2_pdu_types) / sizeof(char *)); i++)
    {
        if (dce2_pdu_types[i] != NULL)
        {
            DCE2_Free((void *)dce2_pdu_types[i], strlen(dce2_pdu_types[i]) + 1, DCE2_MEM_TYPE__INIT);
            dce2_pdu_types[i] = NULL;
        }
    }
}
