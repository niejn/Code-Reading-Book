//
// status_i.cpp,v 1.2 2000/11/17 18:18:48 coryan Exp
//

#include "status_i.h"

corbaname_Status_i::corbaname_Status_i (CORBA::Environment &)
{
  // Constructor
}

CORBA::Boolean
corbaname_Status_i::print_status (CORBA::Environment &)
  ACE_THROW_SPEC ((CORBA::SystemException))
{
  // If the client makes a succesful request, return a true value
  // indicating that it has successfully reached the server.
  return 0;
}
