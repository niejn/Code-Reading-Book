//
// cdr_op_cs.cpp,v 1.8 2000/05/10 20:52:45 nanbor Exp
//

// ============================================================================
//
// = LIBRARY
//    TAO IDL
//
// = FILENAME
//    cdr_op_cs.cpp
//
// = DESCRIPTION
//    Visitor generating code for CDR operators for structures
//
// = AUTHOR
//    Aniruddha Gokhale
//
// ============================================================================

#include "idl.h"
#include "idl_extern.h"
#include "be.h"

#include "be_visitor_structure.h"
#include "be_visitor_field.h"

ACE_RCSID(be_visitor_structure, cdr_op_cs, "cdr_op_cs.cpp,v 1.8 2000/05/10 20:52:45 nanbor Exp")

be_visitor_structure_cdr_op_cs::be_visitor_structure_cdr_op_cs (be_visitor_context *ctx)
  : be_visitor_structure (ctx)
{
}

be_visitor_structure_cdr_op_cs::~be_visitor_structure_cdr_op_cs (void)
{
}

int
be_visitor_structure_cdr_op_cs::visit_structure (be_structure *node)
{
  if (node->cli_stub_cdr_op_gen () ||
      node->imported () ||
      node->is_local ())
    return 0;

  // set the substate as generating code for the types defined in our scope
  this->ctx_->sub_state(TAO_CodeGen::TAO_CDR_SCOPE);
  // all we have to do is to visit the scope and generate code
  if (this->visit_scope (node) == -1)
    {
      ACE_ERROR_RETURN ((LM_ERROR,
                         "(%N:%l) be_visitor_structure_cdr_op_cs::"
                         "visit_structure - "
                         "codegen for scope failed\n"), -1);
    }


  node->cli_stub_cdr_op_gen (1);
  return 0;
}
