// be_decl.cpp,v 1.45 2001/06/01 16:42:02 parsons Exp

// ============================================================================
//
// = LIBRARY
//    TAO IDL
//
// = FILENAME
//    be_decl.cpp
//
// = DESCRIPTION
//    Extension of class AST_Decl that provides additional means for C++
//    mapping.
//
// = AUTHOR
//    Copyright 1994-1995 by Sun Microsystems, Inc.
//    and
//    Aniruddha Gokhale
//
// ============================================================================

#include "idl.h"
#include "idl_extern.h"
#include "be.h"

ACE_RCSID(be, be_decl, "be_decl.cpp,v 1.45 2001/06/01 16:42:02 parsons Exp")

// Default Constructor
be_decl::be_decl (void)
  : cli_hdr_gen_ (I_FALSE),
    cli_stub_gen_ (I_FALSE),
    cli_inline_gen_ (I_FALSE),
    srv_hdr_gen_ (I_FALSE),
    impl_hdr_gen_ (I_FALSE),
    srv_skel_gen_ (I_FALSE),
    impl_skel_gen_ (I_FALSE),
    srv_inline_gen_ (I_FALSE),
    cli_hdr_any_op_gen_ (I_FALSE),
    cli_stub_any_op_gen_ (I_FALSE),
    cli_hdr_cdr_op_gen_ (I_FALSE),
    cli_stub_cdr_op_gen_ (I_FALSE),
    cli_inline_cdr_op_gen_ (I_FALSE),
    cli_inline_cdr_decl_gen_ (I_FALSE),
    flat_name_ (0),
    size_type_ (be_decl::SIZE_UNKNOWN),
    has_constructor_ (I_FALSE)
{
}

// Constructor
be_decl::be_decl (AST_Decl::NodeType type,
                  UTL_ScopedName *n,
                  UTL_StrList *pragmas)
  : AST_Decl (type, 
              n, 
              pragmas),
    cli_hdr_gen_ (I_FALSE),
    cli_stub_gen_ (I_FALSE),
    cli_inline_gen_ (I_FALSE),
    srv_hdr_gen_ (I_FALSE),
    impl_hdr_gen_ (I_FALSE),
    srv_skel_gen_ (I_FALSE),
    impl_skel_gen_ (I_FALSE),
    srv_inline_gen_ (I_FALSE),
    cli_hdr_any_op_gen_ (I_FALSE),
    cli_stub_any_op_gen_ (I_FALSE),
    cli_hdr_cdr_op_gen_ (I_FALSE),
    cli_stub_cdr_op_gen_ (I_FALSE),
    flat_name_ (0),
    size_type_ (be_decl::SIZE_UNKNOWN),
    has_constructor_ (I_FALSE)
{
}

// Destructor
be_decl::~be_decl (void)
{
}

// Return our size type.
be_decl::SIZE_TYPE
be_decl::size_type (void)
{
  if (this->size_type_ == be_decl::SIZE_UNKNOWN)
    {
      (void) this->compute_size_type ();
    }

  return this->size_type_;
}

// Set our size type and that of all our ancestors.
void
be_decl::size_type (be_decl::SIZE_TYPE st)
{
  // Precondition - you cannot set somebody's sizetype to unknown.
  ACE_ASSERT (st != be_decl::SIZE_UNKNOWN);

  // Size type can be VARIABLE or FIXED.
  if (this->size_type_ == be_decl::SIZE_UNKNOWN) // not set yet
    {
      this->size_type_ = st; // set it
    }
  else if ((this->size_type_ == be_decl::FIXED)
           && (st == be_decl::VARIABLE))
    {
      // Once we are VARIABLE, we cannot be FIXED. But if we were FIXED and then
      // get overwritten to VARIABLE, it is fine. Such a situation occurs only
      // when setting the sizes of structures and unions.
      this->size_type_ = st;
    }
}

void
be_decl::compute_full_name  (const char *prefix,
                             const char *suffix,
                             char *&name)
{
  if (prefix == 0 || suffix == 0)
    return;

  ACE_CString prefix_str (prefix);
  ACE_CString suffix_str (suffix);

  ACE_CString result_str;

  // Get parent.
  if (this->defined_in () == 0)
    {
      // Global scope.

      // Prefix.
      result_str = prefix_str;

      // Local name.
      result_str += ACE_CString (this->local_name ()->get_string ());

      // Suffix.
      result_str += suffix_str;
    }
  else
    {
      // Get scope name.
      be_decl *parent = be_scope::narrow_from_scope (this->defined_in ())->decl ();
      if (parent == 0)
        {
          ACE_ERROR ((LM_ERROR,
                      "(%N:%l) be_decl::"
                      "compute_flat_name - "
                      "scope name is nil\n"));
        }

      // Parent name.
      result_str = ACE_CString (parent->full_name ());

      // _
      if (ACE_OS::strcmp (parent->full_name (), "") != 0)
        result_str += ACE_CString ("::");

      // Prefix.
      result_str += prefix_str;

      // Local name.
      result_str += ACE_CString (this->local_name ()->get_string ());

      // Suffix.
      result_str += suffix_str;
    }

  name = result_str.rep ();
}


const char*
be_decl::flat_name (void)
{
  if (!this->flat_name_)
    {
      this->compute_flat_name ();
    }

  return this->flat_name_;
}


// Compute stringified flattened fully scoped name.
void
be_decl::compute_flat_name (void)
{
  if (this->flat_name_ != 0)
    {
      return;
    }
  else
    {
      long namelen = 0;
      UTL_IdListActiveIterator *i = 0;
      long first = I_TRUE;
      long second = I_FALSE;

      // In the first loop, compute the total length.
      ACE_NEW (i,
               UTL_IdListActiveIterator (this->name ()));

      while (!(i->is_done ()))
        {
          if (!first)
            {
              namelen += 1; // for "_"
            }
          else if (second)
            {
              first = second = I_FALSE;
            }

          // Print the identifier.
          namelen += ACE_OS::strlen (i->item ()->get_string ());

          if (first)
            {
              if (ACE_OS::strcmp (i->item ()->get_string (), "") != 0)
                {
                  // Does not start with a "".
                  first = I_FALSE;
                }
              else
                {
                  second = I_TRUE;
                }
            }

          i->next ();
        }

      delete i;

      ACE_NEW (this->flat_name_,
               char[namelen + 1]);

      this->flat_name_[0] = '\0';
      first = I_TRUE;
      second = I_FALSE;

      ACE_NEW (i,
               UTL_IdListActiveIterator (this->name ()));

      while (!(i->is_done ()))
        {
          if (!first)
            {
              ACE_OS::strcat (this->flat_name_, "_");
            }
          else if (second)
            {
              first = second = I_FALSE;
            }

          // Print the identifier.
          ACE_OS::strcat (this->flat_name_, i->item ()->get_string ());
          if (first)
            {
              if (ACE_OS::strcmp (i->item ()->get_string (), "") != 0)
                {
                  // Does not start with a "".
                  first = I_FALSE;
                }
              else
                {
                  second = I_TRUE;
                }
            }

          i->next ();
        }

      delete i;
    }

  return;
}



void
be_decl::compute_flat_name  (const char *prefix,
                             const char *suffix,
                             char *&name)
{
  if (prefix == 0 || suffix == 0)
    {
      return;
    }

  ACE_CString prefix_str (prefix);
  ACE_CString suffix_str (suffix);

  ACE_CString result_str;

  // Get parent.
  if (this->defined_in () == 0)
    {
      // Global scope.

      // Prefix.
      result_str = prefix_str;

      // Local name.
      result_str += ACE_CString (this->local_name ()->get_string ());

      // Suffix.
      result_str += suffix_str;
    }
  else
    {
      // Get scope name.
      be_decl *parent = be_scope::narrow_from_scope (this->defined_in ())->decl ();
      if (parent == 0)
        {
          ACE_ERROR ((LM_ERROR,
                      "(%N:%l) be_decl::"
                      "compute_flat_name - "
                      "scope name is nil\n"));
        }

      // Parent name.
      result_str = ACE_CString (parent->full_name ());

      // _
      if (ACE_OS::strcmp (parent->full_name (), "") != 0)
        result_str += ACE_CString ("_");

      // Prefix.
      result_str += prefix_str;

      // Local name.
      result_str += ACE_CString (this->local_name ()->get_string ());

      // Suffix.
      result_str += suffix_str;
    }

  name = result_str.rep ();
}

// Apply the prefix and suffix to the local name and compute the
// repoID.  Both the parameters should be non-null.
void
be_decl::compute_repoID (const char *prefix,
                         const char *suffix,
                         char *&name)
{
  // Prefix and suffix should be valid.
  if (prefix == 0 || suffix == 0)
    {
      return;
    }

  // First prepare the result without IDL: and  :1.0 strings.

  // repoID without IDL: and :1.0 strings.
  char *result = 0;

  long namelen = 8; // for the prefix "IDL:" and suffix ":1.0"
  UTL_IdListActiveIterator *i = 0;
  long first = I_TRUE;
  long second = I_FALSE;

  // In the first loop compute the total length.
  namelen += ACE_OS::strlen (this->prefix ()) + 1;

  ACE_NEW (i,
           UTL_IdListActiveIterator (this->name ()));

  while (!(i->is_done ()))
    {
      if (!first)
        {
          namelen += 1; // for "/"
        }
      else if (second)
        {
          first = second = I_FALSE;
        }

      // Print the identifier.
      namelen += ACE_OS::strlen (i->item ()->get_string ());

      if (first)
        {
          if (ACE_OS::strcmp (i->item ()->get_string (), "") != 0)
            {
              // Does not start with a "".
              first = I_FALSE;
            }
          else
            {
              second = I_TRUE;
            }
        }

      i->next ();
    }

  delete i;

  // Get the result.
  ACE_NEW (result,
           char[namelen + 1]);
  result[0] = '\0';

  // Start the result with prefix.
  ACE_OS::sprintf (result, "%s", this->prefix ());

  // Add the "/" only if there is a prefix
  if (ACE_OS::strcmp (this->prefix (), "") != 0)
    {
      ACE_OS::strcat (result, "/");
    }

  ACE_NEW (i,
           UTL_IdListActiveIterator (this->name ()));

  first = I_TRUE;
  second = I_FALSE;

  while (!(i->is_done ()))
    {
      if (!first)
        {
          ACE_OS::strcat (result, "/");
        }
      else if (second)
        {
          first = second = I_FALSE;
        }

      // Print the identifier
      ACE_OS::strcat (result, i->item ()->get_string ());

      if (first)
        {
          if (ACE_OS::strcmp (i->item ()->get_string (), "") != 0)
            {
              // Does not start with a ""
              first = I_FALSE;
            }
          else
            {
              second = I_TRUE;
            }
        }

      i->next ();
    }

  delete i;

  // Add prefix and suffix.

  // Search where the last / is. If it is not there put AMI_ right
  // after IDL: and _Handler after that and then the :1.0 string.
  // Otherwise put AMI after the last / and _Handler at the end and
  // then the 1.0 string.

  // CStrings are cool.

  ACE_CString result_str (result);

  // Return val. Release = 0 so that we can return the <c_str>.
  ACE_CString repoID ("IDL:", 0, 0);

  // Useful CStrings.
  ACE_CString prefix_str (prefix);
  ACE_CString suffix_str (suffix);
  ACE_CString version_str (":1.0");

  // Find the last /.
  int last_name_pos = result_str.rfind ('/');

  if (last_name_pos == ACE_CString::npos)
    {
      // '/' not present.

      // Add prefix + result.
      repoID += prefix_str;

      repoID += result_str;

      // Add suffix.
      repoID += suffix_str;
    }
  else
    {
      // '/' present.

      // IDl:<result except the last name>'/'. Watch get the last /
      // also.
      repoID += result_str.substring (0, last_name_pos+1);

      // Put prefix.
      repoID += prefix_str;

      // Put the last name.
      repoID += result_str.substr (last_name_pos + 1);

      // Add suffix.
      repoID += suffix_str;
    }

  // Add 1.0.
  repoID += version_str;

  // Delete result.
  delete [] result;
  result = 0;

  name = repoID.rep ();
}

void
be_decl::destroy (void)
{
  if (this->flat_name_ != 0)
    {
      delete [] this->flat_name_;
      this->flat_name_ = 0;
    }
}

idl_bool
be_decl::is_nested (void)
{
  be_decl *d;

  d = be_scope::narrow_from_scope (this->defined_in ())->decl ();

  // If we have an outermost scope and if that scope is not that of the Root,
  // then we are defined at some nesting level.
  if (d != 0 && d->node_type () != AST_Decl::NT_root)
    {
      return I_TRUE;
    }

  return I_FALSE;
}

// Compute the size type of the node in question
int
be_decl::compute_size_type (void)
{
  return 0;
}

// Return the scope created by this node (if one exists, else NULL).
be_scope *
be_decl::scope (void)
{
  be_decl *d = this;

   switch (this->node_type ()) 
   {
     case AST_Decl::NT_interface_fwd:
        // Resolve forward declared interface by looking at full_definition()
        // field and iterating.
        d = 
          be_interface::narrow_from_decl (
              (be_interface_fwd::narrow_from_decl (this))->full_definition ()
            );
     // Fall through
     case AST_Decl::NT_interface:
        return be_interface::narrow_from_decl (d);
     case AST_Decl::NT_module:
        return be_module::narrow_from_decl (d);
     case AST_Decl::NT_root:
        return be_root::narrow_from_decl (d);
     case AST_Decl::NT_except:
        return be_exception::narrow_from_decl (d);
     case AST_Decl::NT_union:
        return be_union::narrow_from_decl (d);
     case AST_Decl::NT_struct:
        return be_structure::narrow_from_decl (d);
     case AST_Decl::NT_enum:
        return be_enum::narrow_from_decl (d);
     case AST_Decl::NT_op:
        return be_operation::narrow_from_decl (d);
     case AST_Decl::NT_sequence:
        return be_sequence::narrow_from_decl (d);
     default:
        return (be_scope *)0;
   }
}

// Boolean methods to test if code was already generated.
idl_bool
be_decl::cli_hdr_gen (void)
{
  return this->cli_hdr_gen_;
}

idl_bool
be_decl::cli_stub_gen (void)
{
  return this->cli_stub_gen_;
}

idl_bool
be_decl::cli_hdr_any_op_gen (void)
{
  return this->cli_hdr_any_op_gen_;
}

idl_bool
be_decl::cli_stub_any_op_gen (void)
{
  return this->cli_stub_any_op_gen_;
}

idl_bool
be_decl::cli_hdr_cdr_op_gen (void)
{
  return this->cli_hdr_cdr_op_gen_;
}

idl_bool
be_decl::cli_stub_cdr_op_gen (void)
{
  return this->cli_stub_cdr_op_gen_;
}

idl_bool
be_decl::cli_inline_cdr_op_gen (void)
{
  return this->cli_inline_cdr_op_gen_;
}

idl_bool
be_decl::cli_inline_cdr_decl_gen (void)
{
  return this->cli_inline_cdr_decl_gen_;
}

idl_bool
be_decl::cli_inline_gen (void)
{
  return this->cli_inline_gen_;
}

idl_bool
be_decl::srv_hdr_gen (void)
{
  return this->srv_hdr_gen_;
}

idl_bool
be_decl::impl_hdr_gen (void)
{
  return this->impl_hdr_gen_;
}

idl_bool
be_decl::srv_skel_gen (void)
{
  return this->srv_skel_gen_;
}

idl_bool
be_decl::impl_skel_gen (void)
{
  return this->impl_skel_gen_;
}

idl_bool
be_decl::srv_inline_gen (void)
{
  return this->srv_inline_gen_;
}

// Set the flag indicating that code generation is done.
void
be_decl::cli_hdr_gen (idl_bool val)
{
  this->cli_hdr_gen_ = val;
}

void
be_decl::cli_stub_gen (idl_bool val)
{
  this->cli_stub_gen_ = val;
}

void
be_decl::cli_hdr_any_op_gen (idl_bool val)
{
  this->cli_hdr_any_op_gen_ = val;
}

void
be_decl::cli_stub_any_op_gen (idl_bool val)
{
  this->cli_stub_any_op_gen_ = val;
}

void
be_decl::cli_hdr_cdr_op_gen (idl_bool val)
{
  this->cli_hdr_cdr_op_gen_ = val;
}

void
be_decl::cli_stub_cdr_op_gen (idl_bool val)
{
  this->cli_stub_cdr_op_gen_ = val;
}

void
be_decl::cli_inline_cdr_op_gen (idl_bool val)
{
  this->cli_inline_cdr_op_gen_ = val;
}

void
be_decl::cli_inline_cdr_decl_gen (idl_bool val)
{
  this->cli_inline_cdr_decl_gen_ = val;
}

void
be_decl::cli_inline_gen (idl_bool val)
{
  this->cli_inline_gen_ = val;
}

void
be_decl::srv_hdr_gen (idl_bool val)
{
  this->srv_hdr_gen_ = val;
}

void
be_decl::impl_hdr_gen (idl_bool val)
{
  this->impl_hdr_gen_ = val;
}


void
be_decl::srv_skel_gen (idl_bool val)
{
  this->srv_skel_gen_ = val;
}

void
be_decl::srv_inline_gen (idl_bool val)
{
  this->srv_inline_gen_ = val;
}

int
be_decl::accept (be_visitor *visitor)
{
  return visitor->visit_decl (this);
}

idl_bool
be_decl::has_constructor (void)
{
  return this->has_constructor_;
}

void
be_decl::has_constructor (idl_bool value)
{
  // Similarly to be_decl::size_type_, once this
  // gets set to I_TRUE, we don't want it to
  // change back.
  if (this->has_constructor_ == 0)
    {
      this->has_constructor_ = value;
    }
}

// Narrowing methods.
IMPL_NARROW_METHODS1 (be_decl, AST_Decl)
IMPL_NARROW_FROM_DECL (be_decl)
