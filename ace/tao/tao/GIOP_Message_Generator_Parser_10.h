
// -*- C++ -*-
// ===================================================================
/**
 *  @file   GIOP_Message_Generator_Parser_10.h
 *
 *  GIOP_Message_Generator_Parser_10.h,v 1.4 2001/05/09 19:02:05 parsons Exp
 *
 *  @author Balachandran Natarajan <bala@cs.wustl.edu>
 */
// ===================================================================

#ifndef TAO_GIOP_MESSAGE_GENERATOR_PARSER_10_H
#define TAO_GIOP_MESSAGE_GENERATOR_PARSER_10_H
#include "ace/pre.h"
#include "tao/GIOP_Message_Generator_Parser.h"


#if !defined (ACE_LACKS_PRAGMA_ONCE)
# pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */

#if defined(_MSC_VER)
#if (_MSC_VER >= 1200)
#pragma warning(push)
#endif /* _MSC_VER >= 1200 */
#pragma warning(disable:4250)
#endif /* _MSC_VER */

class TAO_GIOP_Message_State;

/**
 * @class TAO_GIOP_Message_Generator_Parser_10
 *
 * @brief Implementation for GIOP v1.0
 *
 */

class TAO_Export TAO_GIOP_Message_Generator_Parser_10:
  public TAO_GIOP_Message_Generator_Parser
{
public:

  /// Write the request header in to <msg>
  virtual int write_request_header (
      const TAO_Operation_Details &opdetails,
      TAO_Target_Specification &spec,
      TAO_OutputCDR &msg
    );

  /// Write the LocateRequest header
  virtual int write_locate_request_header (
      CORBA::ULong request_id,
      TAO_Target_Specification &spec,
      TAO_OutputCDR &msg
    );

  /// Write the reply header in to <output>
  virtual int write_reply_header (
      TAO_OutputCDR &output,
      TAO_Pluggable_Reply_Params_Base &reply,
      CORBA::Environment &ACE_TRY_ENV
    )
    ACE_THROW_SPEC ((CORBA::SystemException));

  /// Writes the locate _reply message in to the <output>
  virtual int write_locate_reply_mesg (
      TAO_OutputCDR &output,
      CORBA::ULong request_id,
      TAO_GIOP_Locate_Status_Msg &status
    );

  /// Parse the Request Header from the incoming stream. This will do a
  /// version specific parsing of the incoming Request header
  virtual int parse_request_header (TAO_ServerRequest &);

  /// Parse the LocateRequest Header from the incoming stream. This will do a
  /// version specific parsing of the incoming Request header
  virtual int parse_locate_header (
      TAO_GIOP_Locate_Request_Header &
    );

  /// Parse the reply message from the server
  virtual int parse_reply (TAO_InputCDR &input,
                           TAO_Pluggable_Reply_Params &params);

    /// Parse the reply message from the server
  virtual int parse_locate_reply (TAO_InputCDR &input,
                                  TAO_Pluggable_Reply_Params &params);

  /// Our versions
  virtual CORBA::Octet major_version (void);
  virtual CORBA::Octet minor_version (void);

};


#if defined (__ACE_INLINE__)
# include "tao/GIOP_Message_Generator_Parser_10.inl"
#endif /* __ACE_INLINE__ */

#include "ace/post.h"
#endif /*TAO_GIOP_MESSAGE_GENERATOR_PARSER_10_H*/
