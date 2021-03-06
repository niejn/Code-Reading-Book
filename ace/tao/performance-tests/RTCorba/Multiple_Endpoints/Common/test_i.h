// test_i.h,v 1.2 2001/05/20 17:38:49 fhunleth Exp

// ============================================================================
//
// = LIBRARY
//   TAO/tests/TPP
//
// = FILENAME
//   test_i.h
//
// = AUTHOR
//   Carlos O'Ryan
//
// ============================================================================

#ifndef TAO_TPP_TEST_I_H
#define TAO_TPP_TEST_I_H

#include "testS.h"

class RTCORBA_COMMON_Export Test_i : public POA_Test
{
  // = TITLE
  //   An implementation for the Test interface
  //
  // = DESCRIPTION
  //   Implements the Test interface in test.idl
  //
public:
  Test_i (void);
  // ctor

  // = The Test methods.
  void test_method (CORBA::Long id,
                    CORBA::Environment&)
    ACE_THROW_SPEC ((CORBA::SystemException));

  void shutdown (const char *orb_id,
                 CORBA::Environment&)
    ACE_THROW_SPEC ((CORBA::SystemException));
};

#if defined(__ACE_INLINE__)
#include "test_i.i"
#endif /* __ACE_INLINE__ */

#endif /* TAO_LATENCY_TEST_I_H */
